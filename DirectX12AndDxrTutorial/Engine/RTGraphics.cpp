#include "RTGraphics.h"

#include "../Util/DXUtil.h"

#include <chrono>
#include <array>
#include <limits>
#include <vector>
#include <iostream>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "Libraries/d3dx12.h"


#include "../Exception/DxgiInfoException.h"
#include "../Exception/Exception.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include "../Libraries/imgui/imgui.h"
#include "../Libraries/imgui/imgui_impl_win32.h"
#include "../Libraries/imgui/imgui_impl_dx12.h"

#include "../Shaders/RTShaders.hlsli"

namespace wrl = Microsoft::WRL;
namespace dx = DirectX;

using namespace std;
using namespace Util;
using namespace Engine;

RTGraphics::RTGraphics(HWND hWnd)
	: winWidth(), winHeight(), pRTVDescriptorSize(), pCurrentBackBufferIndex(), frameFenceValues{},
	scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)), viewport()
{
	RECT rect;
	GetClientRect(hWnd, &rect);
	winWidth = rect.right;
	winHeight = rect.bottom;

	// DX12 viewport
	viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight));

	// Enable debugging
	DXUtil::enableDebugLayer();

	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevel;
	auto adapter = DXUtil::getAdapterLatestFeatureLevel(&featureLevel);

	// Create a device supporting ray tracing
	pDevice = DXUtil::createRTDeviceFromAdapter(adapter, featureLevel);

	// Enable debug messages in debug mode
	DXUtil::setupDebugLayer(pDevice);

	// Create command queue
	pCommandQueue = make_unique<CommandQueue>(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// Create swap chain 
	pSwapChain = DXUtil::createSwapChain(pCommandQueue->getCommandQueue(), hWnd, numBackBuffers);

	// Create descriptor heap for render target view
	pRTVDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numBackBuffers);

	// Create render target Views
	pRTVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto backBuffers = DXUtil::createRenderTargetViews(pDevice, pRTVDescriptorHeap, pSwapChain, std::size(pBackBuffers));
	std::copy(backBuffers.begin(), backBuffers.end(), pBackBuffers);

	// Init camera
	camera = make_unique<Camera>(
		dx::XMVectorSet(0.f, 1.f, 3.5f, 1.f),
		dx::XMVectorSet(0.f, 0.f, -1.f, 0.f),
		(float)winWidth / winHeight,
		1.0f,
		1.f,
		10.f);
}

Engine::RTGraphics::~RTGraphics()
{
	pCommandQueue->flush();
}

void Engine::RTGraphics::init()
{
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

	scene.loadScene("CornellBox-Original.obj");
	//scene.flattenGroups();
	scene.transformLightPosition(dx::XMMatrixTranslation(0.f, -0.02f, 0.f));

	const auto& geometry = scene.getVertices();

	pCurrentCommandList = pCommandQueue->getCommandList();

	Shaders::ConstBuff cBuff = {};

	wrl::ComPtr<ID3D12Resource> cBuffIntBuffer;
	pConstantBuffer = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		cBuffIntBuffer,
		&cBuff,
		sizeof(cBuff),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	std::vector<wrl::ComPtr<ID3D12Resource>> intermediateBuffers;
	intermediateBuffers.resize(geometry.size());
	vertexBuffers.clear();

	for (size_t i = 0; i < geometry.size(); ++i) {
		const auto& geo = geometry[i];
		vertexBuffers.push_back(DXUtil::uploadDataToDefaultHeap(
			pDevice,
			pCurrentCommandList,
			intermediateBuffers[i], 
			geo.data(),
			geo.size() * sizeof(dx::XMFLOAT3),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	}

	std::vector<size_t> vertexCounts;
	std::transform(geometry.begin(), geometry.end(), std::back_inserter(vertexCounts), [](const std::vector<dx::XMFLOAT3>& v) -> size_t {return v.size(); });

	blasBuffers = DXUtil::createBottomLevelAS(pDevice, pCurrentCommandList, vertexBuffers, vertexCounts, sizeof(dx::XMFLOAT3));
	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers.pResult, tlasTempBuffer, 0.f, false, tlasBuffers);

	pStateObject = createRtPipeline();

	createShaderResources();
	createMaterialsAndFaceAttributes();

	wrl::ComPtr<ID3D12Resource> shaderTableTempBuffer;
	pShadingTable = createShaderTable(shaderTableTempBuffer);

	pCommandQueue->executeCommandList(pCurrentCommandList);
	pCommandQueue->flush();

	pCurrentCommandList.Reset();
}

// Our begin frame
void Engine::RTGraphics::clearBuffer(float red, float green, float blue)
{
	pCurrentCommandList = pCommandQueue->getCommandList();

	pCurrentCommandList->SetDescriptorHeaps(1u, srvDescriptorHeap.GetAddressOf());

	pCurrentCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(outputRTTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Transition the back buffer from the present state to the render target state
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	pCurrentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

	// Clear the RTV with the specified colour
	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pRTVDescriptorSize);
	//FLOAT color[] = { red, green, blue, 1.0f };
	//pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);
}

void Engine::RTGraphics::draw(uint64_t timeMs, bool clear)
{
	// Transform vertices in TLAS
	//DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers.pResult, pTlasTempBuffer[pCurrentBackBufferIndex], (timeMs % 8000) / 8000.f * 6.28f, true, tlasBuffers);

	Shaders::ConstBuff cBuff = {};

	// Setup camera - Simulating Nikon's one
	Shaders::Camera& shaderCamera = cBuff.camera;
	shaderCamera.position = camera->getPosition();
	shaderCamera.direction = camera->getDirection();
	shaderCamera.up = dx::XMVectorSet(0, 1, 0, 0);
	shaderCamera.objectPlane.width = 0.0235f;
	shaderCamera.objectPlane.height = 0.0156f;
	shaderCamera.objectPlane.distance = 0.018f;
	shaderCamera.objectPlane.apertureSize = 0.018f / 1.4f;

	// Setup area lights
	cBuff.numLights = std::min(std::size(cBuff.areaLights), scene.getLights().size());
	memcpy(cBuff.areaLights, scene.getLights().data(), sizeof(Shaders::AreaLight) * cBuff.numLights);

	// seed
	cBuff.seed = 0;
	cBuff.clear = clear ? 1 : 0;

	//
	DXUtil::updateDataInDefaultHeap(
		pDevice,
		pCurrentCommandList,
		pConstantBuffer,
		pTlasTempBuffer[pCurrentBackBufferIndex],
		&cBuff,
		sizeof(cBuff),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// bind empty root signature 
	pCurrentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	pCurrentCommandList->SetPipelineState1(pStateObject.Get());

	// Launch rays
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = {};

	// Dimensions of the compute grid
	dispatchRaysDesc.Width = winWidth;
	dispatchRaysDesc.Height = winHeight;
	dispatchRaysDesc.Depth = 1;

	// Ray generation shader record
	dispatchRaysDesc.RayGenerationShaderRecord.StartAddress = pShadingTable->GetGPUVirtualAddress();
	dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = 64;

	// Miss ray record
	dispatchRaysDesc.MissShaderTable.StartAddress = pShadingTable->GetGPUVirtualAddress() + 64;
	dispatchRaysDesc.MissShaderTable.SizeInBytes = 64;  
	dispatchRaysDesc.MissShaderTable.StrideInBytes = 64;

	// Hit ray record
	dispatchRaysDesc.HitGroupTable.StartAddress = pShadingTable->GetGPUVirtualAddress() + 128;
	dispatchRaysDesc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT * 3 * scene.getVertices().size() * 2;
	dispatchRaysDesc.HitGroupTable.StrideInBytes = D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT * 3;

	pCurrentCommandList->DispatchRays(&dispatchRaysDesc);
}

void Engine::RTGraphics::endFrame()
{
	pCurrentCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(outputRTTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

	// Change the back buffer from the render target state to the present state
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	// perform copy
	pCurrentCommandList->CopyResource(backBuffer.Get(), outputRTTexture.Get());
	pCurrentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));

	// Execute command list
	frameFenceValues[pCurrentBackBufferIndex] = pCommandQueue->executeCommandList(pCurrentCommandList);

	// Release pointer to this command list (Comptr reset is being called here)
	pCurrentCommandList.Reset();

	HRESULT hr;
	GFXTHROWIFFAILED(pSwapChain->Present(1u, 0u));

	// Set current back buffer and Wait for any fence values associated to it
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
	pCommandQueue->waitForFenceValue(frameFenceValues[pCurrentBackBufferIndex]);
}

Camera& Engine::RTGraphics::getCamera()
{
	return *camera;
}

wrl::ComPtr<ID3D12StateObject> Engine::RTGraphics::createRtPipeline()
{
	HRESULT hr;

	// Define State Object Descriptor (EXTENDED version from d3dx)
	CD3DX12_STATE_OBJECT_DESC stateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Construct sub objects

	// First is DXIL - to load shader and Load symbols from the shader and identify the entry points
	wrl::ComPtr<ID3DBlob> pRTShadersBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/RTShaders.cso", &pRTShadersBlob));
	CD3DX12_DXIL_LIBRARY_SUBOBJECT dxilSubObject (stateObjectDesc);
	dxilSubObject.SetDXILLibrary(&CD3DX12_SHADER_BYTECODE(pRTShadersBlob.Get()));
	const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs", L"shadowChs" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject (stateObjectDesc);
	hitSubObject.SetClosestHitShaderImport(L"chs");
	hitSubObject.SetHitGroupExport(L"HitGroup");

	CD3DX12_HIT_GROUP_SUBOBJECT shadowHitSubObject(stateObjectDesc);
	shadowHitSubObject.SetClosestHitShaderImport(L"shadowChs");
	shadowHitSubObject.SetHitGroupExport(L"ShadowHitGroup");

	// Third - Local Root Signature for Ray Gen shader
	// Build the root signature descriptor and create root signature
	array<CD3DX12_DESCRIPTOR_RANGE1, 2> descriptorRanges = {
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE), //gOutput, gRadiance
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE)  //gRtScene
	};
	
	std::vector<CD3DX12_ROOT_PARAMETER1> rayGenRootParams;
	rayGenRootParams.resize(2);
	rayGenRootParams[0].InitAsDescriptorTable(descriptorRanges.size(), descriptorRanges.data());
	rayGenRootParams[1].InitAsConstantBufferView(0);
	
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(rayGenRootParams.size(), rayGenRootParams.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
	wrl::ComPtr<ID3D12RootSignature> rootSignature = DXUtil::createRootSignature(pDevice, rootSignatureDesc);

	// Set the local root signature sub object
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT rgLocalRootSignatureSubObject(stateObjectDesc);
	rgLocalRootSignatureSubObject.SetRootSignature(rootSignature.Get());

	// Fourth - Associate the local root signature to registers in shaders (in the rayGen program) using Export Association
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT rgAssociation(stateObjectDesc);
	rgAssociation.AddExport(L"rayGen");
	rgAssociation.SetSubobjectToAssociate(rgLocalRootSignatureSubObject);

	// Fifth - create empty lrs for miss program
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC emptyRootSignatureDesc(0, static_cast<CD3DX12_ROOT_PARAMETER1*>(nullptr), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
	wrl::ComPtr<ID3D12RootSignature> emptyRootSignature = DXUtil::createRootSignature(pDevice, emptyRootSignatureDesc);
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT emptyLocalRootSignatureSubObject(stateObjectDesc);
	emptyLocalRootSignatureSubObject.SetRootSignature(emptyRootSignature.Get());

	// Sixth - Associate the empty local root signature with the miss programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT emptyAssociation(stateObjectDesc);
	emptyAssociation.AddExport(L"miss");
	emptyAssociation.AddExport(L"ShadowHitGroup");
	emptyAssociation.SetSubobjectToAssociate(emptyLocalRootSignatureSubObject);

	// Create root signature having constant buffer and SRV
	std::vector<CD3DX12_ROOT_PARAMETER1> chsRootParams;
	chsRootParams.resize(5);
	chsRootParams[0].InitAsConstantBufferView(0);
	chsRootParams[1].InitAsShaderResourceView(1);
	chsRootParams[2] = rayGenRootParams[0];
	chsRootParams[3].InitAsShaderResourceView(2);
	chsRootParams[4].InitAsShaderResourceView(3);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC cbvRootSignatureDesc(chsRootParams.size(), chsRootParams.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
	wrl::ComPtr<ID3D12RootSignature> cbvRootSignature = DXUtil::createRootSignature(pDevice, cbvRootSignatureDesc);
	// Set the local root signature sub object
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT cbvLocalRootSignatureSubObject(stateObjectDesc);
	cbvLocalRootSignatureSubObject.SetRootSignature(cbvRootSignature.Get());

	// Associate cbv signature with hit program
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT hitAssociation(stateObjectDesc);
	hitAssociation.AddExport(L"HitGroup");
	hitAssociation.SetSubobjectToAssociate(cbvLocalRootSignatureSubObject);

	// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
	shaderConfig.Config(3 * sizeof(float), 2 * sizeof(float));

	// Eighth - Associate the shader configuration with all shader programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT shaderConfigAssociation(stateObjectDesc);
	shaderConfigAssociation.AddExport(L"rayGen");
	shaderConfigAssociation.AddExport(L"miss");
	shaderConfigAssociation.AddExport(L"HitGroup");
	shaderConfigAssociation.AddExport(L"ShadowHitGroup");
	shaderConfigAssociation.SetSubobjectToAssociate(shaderConfig);

	// Ninth - Configure the RAY TRACING PIPELINE
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT rtPipelineConfig(stateObjectDesc);
	rtPipelineConfig.Config(2);

	// Tenth - Global Root Signature
	CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT globalRootSignature (stateObjectDesc);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC globalEmptyRootSignatureDesc(0, static_cast<CD3DX12_ROOT_PARAMETER1*>(nullptr), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	globalEmptyRootSignature = DXUtil::createRootSignature(pDevice, globalEmptyRootSignatureDesc);
	globalRootSignature.SetRootSignature(globalEmptyRootSignature.Get());

	// Finally - Create the state
	wrl::ComPtr<ID3D12StateObject> stateObject;
	GFXTHROWIFFAILED(pDevice->CreateStateObject(stateObjectDesc, IID_PPV_ARGS(&stateObject)));

	return stateObject;
}

void Engine::RTGraphics::createShaderResources()
{
	// The output resource
	outputRTTexture = DXUtil::createTextureCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, winWidth, winHeight, D3D12_RESOURCE_STATE_COPY_SOURCE);
	radianceTexture = DXUtil::createTextureCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, winWidth, winHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R32G32B32A32_FLOAT);

	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	srvDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, 3, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Create the UAV descriptor first (needs to be same order as in root signature)
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; // In below, set it at the first location of this heap

	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	pDevice->CreateUnorderedAccessView(outputRTTexture.Get(), nullptr, &uavDesc, cpuDescHandle);

	cpuDescHandle.ptr += pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	pDevice->CreateUnorderedAccessView(radianceTexture.Get(), nullptr, &uavDesc, cpuDescHandle);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlasBuffers.pResult->GetGPUVirtualAddress();

	cpuDescHandle.ptr += pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	pDevice->CreateShaderResourceView(nullptr, &srvDesc, cpuDescHandle);
}

void Engine::RTGraphics::createMaterialsAndFaceAttributes()
{
	// create constant buffer view - not on descriptor heap
	pMaterials = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		pTlasTempBuffer[0],
		scene.getMaterials().data(),
		sizeof(Shaders::Material) * scene.getMaterials().size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// Get Face attributes
	const std::vector<Shaders::FaceAttributes>& faceAttributes = scene.getFaceAttributes();

	// Copy..
	pFaceAttributes = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		pTlasTempBuffer[1],
		faceAttributes.data(),
		sizeof(Shaders::FaceAttributes) * faceAttributes.size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

wrl::ComPtr<ID3D12Resource> Engine::RTGraphics::createShaderTable(wrl::ComPtr<ID3D12Resource>& shaderTableTempResource)
{
	// Extract the properties interface
	wrl::ComPtr<ID3D12StateObjectProperties> pStateObjectProps;
	pStateObject.As(&pStateObjectProps);

	// Table layout is ProgramID + constants/descriptors/descriptor-tables
	// Calculate size for shading table
	UINT64 hitGroupTableSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8 + 8 + 8 + 8 + 8;
	auto t = numeric_limits<size_t>::max();
	std::align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, hitGroupTableSize, (void*&)hitGroupTableSize, t); // align using largest record size (rayGen)
	const UINT64 hitGroupTableRecordSize = hitGroupTableSize;
	const size_t hitGroupRecords = scene.getVertices().size();
	hitGroupTableSize *= hitGroupRecords * 2; // 2 Records (rayGen/miss) + n hit group records
	const size_t totalTableSize = 128 + hitGroupTableSize;

	// Create host side buffer - it should be zero initialised when using make_unique..
	unique_ptr<uint8_t[]> bufferManager = make_unique<uint8_t[]>(totalTableSize);
	uint8_t* buffer = bufferManager.get();

	// Entry 0,1 - Program Id's
	size_t i = 0;
	for (const auto& program : { L"rayGen", L"miss" }) {
		memcpy(buffer + i++ * 64, pStateObjectProps->GetShaderIdentifier(program), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	// Fill in Entry 0 Descriptor table entry
	UINT64 descriptorTableHandle = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	memcpy(buffer + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &descriptorTableHandle, sizeof(descriptorTableHandle));

	auto cbHandle = pConstantBuffer->GetGPUVirtualAddress();
	memcpy(buffer + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + 8, &cbHandle, sizeof(cbHandle));

	size_t faceAttributeIndex = 0;

	for (i = 0; i < hitGroupRecords; ++i) {

		size_t index = i * 2;

		uint8_t* address = buffer + 128 + index * hitGroupTableRecordSize;

		memcpy(address, pStateObjectProps->GetShaderIdentifier(L"HitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

		// Fill in Entry 2 - CBV entry
		auto handle = pConstantBuffer->GetGPUVirtualAddress();
		memcpy(address += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &handle, sizeof(handle));

		// Vertices ptr
		handle = vertexBuffers[i]->GetGPUVirtualAddress();
		memcpy(address += 8, &handle, sizeof(handle));

		// descriptor table pointer
		memcpy(address += 8, &descriptorTableHandle, sizeof(descriptorTableHandle));

		// Face attributes
		handle = pFaceAttributes->GetGPUVirtualAddress() + faceAttributeIndex * sizeof(Shaders::FaceAttributes);
		memcpy(address += 8, &handle, sizeof(handle));
		faceAttributeIndex += scene.getVertices(i).size() / 3;

		// Material
		handle = pMaterials->GetGPUVirtualAddress();
		memcpy(address += 8, &handle, sizeof(handle));

		// SHADOW
		++index;
		memcpy(buffer + 128 + index * hitGroupTableRecordSize, pStateObjectProps->GetShaderIdentifier(L"ShadowHitGroup"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	// Upload buffer to gpu
	return DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, shaderTableTempResource, buffer, totalTableSize, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

