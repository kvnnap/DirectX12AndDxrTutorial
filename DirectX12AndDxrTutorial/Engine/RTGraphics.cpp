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
}

Engine::RTGraphics::~RTGraphics()
{
	pCommandQueue->flush();
}

void Engine::RTGraphics::init()
{
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

	// Upload Geometry
	DirectX::XMFLOAT3 triangle[3] = {
		DirectX::XMFLOAT3(    0.f,   1.f, 0.f),
		DirectX::XMFLOAT3( 0.866f, -0.5f, 0.f),
		DirectX::XMFLOAT3(-0.866f, -0.5f, 0.f)
	};

	pCurrentCommandList = pCommandQueue->getCommandList();

	wrl::ComPtr<ID3D12Resource> intermediateBuffer;
	vertexBuffer = DXUtil::uploadDataToDefaultHeap(pDevice, pCurrentCommandList, intermediateBuffer, triangle, sizeof(triangle), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	blasBuffers = DXUtil::createBottomLevelAS(pDevice, pCurrentCommandList, vertexBuffer, std::size(triangle), sizeof(dx::XMFLOAT3));
	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers.pResult, tlasTempBuffer, 0.f, false, tlasBuffers);

	pStateObject = createRtPipeline(pDevice, globalEmptyRootSignature);

	createShaderResources();
	createConstantBuffer();

	wrl::ComPtr<ID3D12Resource> shaderTableTempBuffer;
	pShadingTable = createShaderTable(
		pDevice,
		pStateObject,
		pCurrentCommandList,
		srvDescriptorHeap,
		pConstantBuffer,
		shaderTableTempBuffer);

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

void Engine::RTGraphics::draw(uint64_t timeMs)
{
	// Transform vertices in TLAS
	//DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers.pResult, pTlasTempBuffer[pCurrentBackBufferIndex], (timeMs % 2000) / 2000.f * 6.28f, true, tlasBuffers);

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
	dispatchRaysDesc.RayGenerationShaderRecord.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT; // THIS IS A QUICK HACK

	// Miss ray record
	dispatchRaysDesc.MissShaderTable.StartAddress = pShadingTable->GetGPUVirtualAddress() + D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	dispatchRaysDesc.MissShaderTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;   // THIS IS A QUICK HACK
	dispatchRaysDesc.MissShaderTable.StrideInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT; // THIS IS A QUICK HACK

	// Hit ray record
	dispatchRaysDesc.HitGroupTable.StartAddress = pShadingTable->GetGPUVirtualAddress() + 2 * D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	dispatchRaysDesc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;   // THIS IS A QUICK HACK
	dispatchRaysDesc.HitGroupTable.StrideInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT; // THIS IS A QUICK HACK


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

wrl::ComPtr<ID3D12StateObject> Engine::RTGraphics::createRtPipeline(wrl::ComPtr<ID3D12Device5> pDevice, wrl::ComPtr<ID3D12RootSignature>& globalEmptyRootSignature)
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
	const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject (stateObjectDesc);
	hitSubObject.SetClosestHitShaderImport(L"chs");
	hitSubObject.SetHitGroupExport(L"HitGroup");

	// Third - Local Root Signature
	// Build the root signature descriptor and create root signature
	array<CD3DX12_DESCRIPTOR_RANGE1, 2> descriptorRanges = {
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 0), //gOutput
		CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, 1) //gRtScene
	};
	
	CD3DX12_ROOT_PARAMETER1 rootParameter;
	rootParameter.InitAsDescriptorTable(descriptorRanges.size(), descriptorRanges.data());
	
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(1, &rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
	wrl::ComPtr<ID3D12RootSignature> rootSignature = DXUtil::createRootSignature(pDevice, rootSignatureDesc);

	// Set the local root signature sub object
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT rgLocalRootSignatureSubObject(stateObjectDesc);
	rgLocalRootSignatureSubObject.SetRootSignature(rootSignature.Get());

	// Fourth - Associate the local root signature to registers in shaders (in the rayGen program) using Export Association
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT rgAssociation(stateObjectDesc);
	rgAssociation.AddExport(L"rayGen");
	rgAssociation.SetSubobjectToAssociate(rgLocalRootSignatureSubObject);

	// Fifth - create empty lrs for miss and hit programs
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC emptyRootSignatureDesc(0, static_cast<CD3DX12_ROOT_PARAMETER1*>(nullptr), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
	wrl::ComPtr<ID3D12RootSignature> emptyRootSignature = DXUtil::createRootSignature(pDevice, emptyRootSignatureDesc);
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT emptyLocalRootSignatureSubObject(stateObjectDesc);
	emptyLocalRootSignatureSubObject.SetRootSignature(emptyRootSignature.Get());

	// Sixth - Associate the empty local root signature with the miss and hit programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT emptyAssociation(stateObjectDesc);
	emptyAssociation.AddExport(L"miss");
	emptyAssociation.SetSubobjectToAssociate(emptyLocalRootSignatureSubObject);

	// Create root signature having constant buffer
	CD3DX12_ROOT_PARAMETER1 cbvRootParameter;
	cbvRootParameter.InitAsConstantBufferView(0);
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC cbvRootSignatureDesc(1, &cbvRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE);
	wrl::ComPtr<ID3D12RootSignature> cbvRootSignature = DXUtil::createRootSignature(pDevice, cbvRootSignatureDesc);
	// Set the local root signature sub object
	CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT cbvLocalRootSignatureSubObject(stateObjectDesc);
	cbvLocalRootSignatureSubObject.SetRootSignature(cbvRootSignature.Get());

	// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
	shaderConfig.Config(3 * sizeof(float), 2 * sizeof(float));

	// Eighth - Associate the shader configuration with all shader programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT shaderConfigAssociation(stateObjectDesc);
	shaderConfigAssociation.AddExport(L"rayGen");
	shaderConfigAssociation.AddExport(L"miss");
	shaderConfigAssociation.AddExport(L"chs");
	shaderConfigAssociation.SetSubobjectToAssociate(shaderConfig);

	// Ninth - Configure the RAY TRACING PIPELINE
	CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT rtPipelineConfig(stateObjectDesc);
	rtPipelineConfig.Config(1);

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

	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	srvDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

	// Create the UAV descriptor first (needs to be same order as in root signature)
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; // In below, set it at the first location of this heap
	pDevice->CreateUnorderedAccessView(outputRTTexture.Get(), nullptr, &uavDesc, srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlasBuffers.pResult->GetGPUVirtualAddress();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	cpuDescHandle.ptr += pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	pDevice->CreateShaderResourceView(nullptr, &srvDesc, cpuDescHandle);
}

void Engine::RTGraphics::createConstantBuffer()
{
	// create constant buffer view - not on descriptor heap
	DirectX::XMFLOAT4 cols[3] = {
		DirectX::XMFLOAT4(1.f, 0.f, 0.f,0.f),
		DirectX::XMFLOAT4(0.f,1.f,0.f,0.f),
		DirectX::XMFLOAT4(0.f,0.f,1.f,0.f)
	};

	pConstantBuffer = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		pTlasTempBuffer[0],
		cols,
		sizeof(cols), 
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

wrl::ComPtr<ID3D12Resource> Engine::RTGraphics::createShaderTable(
	wrl::ComPtr<ID3D12Device5> pDevice,
	wrl::ComPtr<ID3D12StateObject> pipelineStateObject,
	wrl::ComPtr<ID3D12GraphicsCommandList4> pCommandList,
	wrl::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap,
	wrl::ComPtr<ID3D12Resource> constantBuffer,
	wrl::ComPtr<ID3D12Resource>& shaderTableTempResource)
{
	// Extract the properties interface
	wrl::ComPtr<ID3D12StateObjectProperties> pStateObjectProps;
	pipelineStateObject.As(&pStateObjectProps);

	// Table layout is ProgramID + constants/descriptors/descriptor-tables
	// Calculate size for shading table
	UINT64 shaderTableSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	shaderTableSize += 8; // Descriptor Table for ray gen
	auto t = numeric_limits<size_t>::max();
	std::align(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, shaderTableSize, (void*&)shaderTableSize, t); // align using largest record size (rayGen)
	UINT64 shaderTableRecordSize = shaderTableSize;
	shaderTableSize *= 3; // 3 Records

	// Create host side buffer - it should be zero initialised when using make_unique..
	unique_ptr<uint8_t[]> bufferManager = make_unique<uint8_t[]>(shaderTableSize);
	uint8_t* buffer = bufferManager.get();

	// Entry 0,1,2 - Program Id's
	auto programs = { L"rayGen", L"miss", L"HitGroup" };
	auto i = 0;
	for (const auto& program : programs) {
		memcpy(buffer + i++ * shaderTableRecordSize, pStateObjectProps->GetShaderIdentifier(program), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
	}

	// Fill in Entry 0 Descriptor table entry
	UINT64 descriptorTableHandle = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;
	memcpy(buffer + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, &descriptorTableHandle, sizeof(descriptorTableHandle));

	// Fill in Entry 2 - CBV entry
	auto cbvHandle = constantBuffer->GetGPUVirtualAddress();
	memcpy(buffer + 2 * shaderTableRecordSize + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES,
		&cbvHandle, sizeof(cbvHandle));

	// Upload buffer to gpu
	return DXUtil::uploadDataToDefaultHeap(pDevice, pCommandList, shaderTableTempResource, buffer, shaderTableSize, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

