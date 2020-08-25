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
#include "../Libraries/stb/stb_image.h"


namespace wrl = Microsoft::WRL;
namespace dx = DirectX;

using namespace std;
using namespace Util;
using namespace Engine;
using Shaders::PathTracingPath;
using feanor::io::IMouseReader;
using feanor::anvil::Trace;
using feanor::anvil::Anvil;

RTGraphics::RTGraphics(HWND hWnd, IMouseReader* mouseReader)
	: winWidth(), winHeight(), debugMode(), debugPixelX(), debugPixelY(), mouseReader(mouseReader), pRTVDescriptorSize(), pCurrentBackBufferIndex(), frameFenceValues{},
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
	pRTVDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, numBackBuffers, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

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

	rootSignatureManager = make_shared<RootSignatureManager>();
	shadingTable = make_unique<ShadingTable>(rootSignatureManager);

	// Setup ImGui
	bool valid = IMGUI_CHECKVERSION();
	pImGuiDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, 1u, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
	ImGuiContext* context = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX12_Init(
		pDevice.Get(),
		numBackBuffers,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		pImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		pImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui::StyleColorsDark();
}

Engine::RTGraphics::~RTGraphics()
{
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	pCommandQueue->flush();
}

void Engine::RTGraphics::init(const std::string& sceneFileName)
{
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

	scene.loadScene(sceneFileName);
	//scene.flattenGroups();
	//scene.transformLightPosition(dx::XMMatrixTranslation(0.f, -0.02f, 0.f));

	const auto& shapes = scene.getShapes();

	pCurrentCommandList = pCommandQueue->getCommandList();

	Shaders::ConstBuff cBuff = {};

	// DEBUG-ANVIL SECTION
	for (size_t i = 0; i < std::size(readbackAnvilBuffer); ++i) {
		readbackAnvilBuffer[i] = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_READBACK, sizeof(PathTracingPath), D3D12_RESOURCE_STATE_COPY_DEST);
	}
	// END DEBUG-ANVIL SECTION

	wrl::ComPtr<ID3D12Resource> cBuffIntBuffer;
	pConstantBuffer = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		cBuffIntBuffer,
		&cBuff,
		sizeof(cBuff),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	std::vector<wrl::ComPtr<ID3D12Resource>> intermediateBuffers;
	intermediateBuffers.resize(1);

	auto flattenedVerts = scene.getFlattenedVertices();
	vertexBuffer = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		intermediateBuffers[0], 
		flattenedVerts.data(),
		flattenedVerts.size() * sizeof(dx::XMFLOAT3),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	std::vector<size_t> vertexCounts;
	std::transform(shapes.begin(), shapes.end(), std::back_inserter(vertexCounts), [](const Shape& s) -> size_t {return s.getVertices().size(); });

	blasBuffers.resize(shapes.size());

	for (size_t i = 0; i < blasBuffers.size(); ++i) {
		size_t vertexCount = scene.getShape(i).getVertices().size();
		blasBuffers[i] = DXUtil::createBottomLevelAS(pDevice, pCurrentCommandList, 
			{ vertexBuffer->GetGPUVirtualAddress() + sizeof(dx::XMFLOAT3) * scene.getFaceOffsets()[i] * 3 },
			{ vertexCount }, sizeof(dx::XMFLOAT3));
	}

	// Setup matrices
	groupMatrices.resize(shapes.size());
	
	for (size_t i = 0; i < groupMatrices.size(); ++i) {
		groupMatrices[i] = shapes[i].getTransform();
	}

	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers, tlasTempBuffer, scene.getFaceOffsets(), groupMatrices, false, tlasBuffers);

	// load textures
	vector<wrl::ComPtr<ID3D12Resource>> texTempBuffer;
	texTempBuffer.resize(scene.getTextures().size());
	size_t dTCount = 0;
	for (const auto& texture : scene.getTextures()) {
		textures.push_back(DXUtil::uploadTextureDataToDefaultHeap(
			pDevice, 
			pCurrentCommandList,
			texTempBuffer[dTCount++],
			texture.data.get(),
			texture.width,
			texture.height,
			texture.channels,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
	}

	if (textures.empty()) {
		textures.push_back(DXUtil::createTextureCommittedResource(
			pDevice, D3D12_HEAP_TYPE_DEFAULT, 1, 1, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R8G8B8A8_UNORM));
	}

	// load texture coordinates
	wrl::ComPtr<ID3D12Resource> texCoordsTempBuffer;
	pTexCoords = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		texCoordsTempBuffer,
		scene.getTextureVertices().data(),
		scene.getTextureVertices().size() * sizeof(dx::XMFLOAT2),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	pStateObject = createRtPipeline();
	
	createShaderResources();
	createMaterialsAndFaceAttributes();

	// Copy matrices
	pMatrices = DXUtil::uploadDataToDefaultHeap(
		pDevice,
		pCurrentCommandList,
		pTempBufferMatrices[pCurrentBackBufferIndex],
		groupMatrices.data(),
		sizeof(decltype(groupMatrices)::value_type) * groupMatrices.size(),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	wrl::ComPtr<ID3D12Resource> shaderTableTempBuffer;
	createShaderTable(shaderTableTempBuffer);

	pCommandQueue->executeCommandList(pCurrentCommandList);
	pCommandQueue->flush();

	pCurrentCommandList.Reset();
}

// Our begin frame
void Engine::RTGraphics::clearBuffer(float red, float green, float blue)
{
	pCurrentCommandList = pCommandQueue->getCommandList();

	pCurrentCommandList->SetDescriptorHeaps(1u, descriptorHeap.GetAddressOf());

	pCurrentCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(outputRTTexture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Transition the back buffer from the present state to the render target state
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	pCurrentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

	// Clear the RTV with the specified colour
	//CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pRTVDescriptorSize);
	//FLOAT color[] = { red, green, blue, 1.0f };
	//pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);
}

void Engine::RTGraphics::draw(uint64_t timeMs, bool& clear)
{
	// process mouse quickly
	if (debugMode && mouseReader != nullptr && mouseReader->hasKeyChanged(0) && mouseReader->isKeyPressed(0)) {
		debugPixelX = mouseReader->getX();
		debugPixelY = mouseReader->getY();
	}

	// Transform vertices in TLAS
	//DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers.pResult, pTlasTempBuffer[pCurrentBackBufferIndex], (timeMs % 8000) / 8000.f * 6.28f, true, tlasBuffers);

	Shaders::ConstBuff cBuff = {};

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Cameras");
	camera->drawUI();
	clear |= camera->hasChanged();
	ImGui::End();

	// Setup camera - Simulating Nikon's one
	Shaders::Camera& shaderCamera = cBuff.camera;
	shaderCamera.position = camera->getPosition();
	shaderCamera.direction = camera->getDirection();
	shaderCamera.up = camera->getUp();
	shaderCamera.cameraType = camera->isThinLensEnabled() ? Shaders::ThinLens : Shaders::Pinhole;
	shaderCamera.focalLength = camera->getFocalLength();
	shaderCamera.filmPlane.width = 0.0235f; // 1.5 : 1
	shaderCamera.filmPlane.height = 0.0156f;

	if (camera->isThinLensEnabled()) {
		shaderCamera.apertureRadius = 0.5f * camera->getApertureSize();
		shaderCamera.focalLength *= camera->getMagnification();
		shaderCamera.filmPlane.width *= camera->getMagnification();
		shaderCamera.filmPlane.height *= camera->getMagnification();
	}

	//// Create ImGui Window
	ImGui::Begin("Shapes");

	bool structureChanged = false;
	for (int i = 0; i < scene.getShapes().size(); ++i) {
		auto& shape = scene.getShape(i);
		shape.drawUI();
		if (shape.hasChanged()) {
			structureChanged = true;
			groupMatrices[i] = shape.getTransform();
		}
	}

	ImGui::End();

	if (structureChanged) {
		clear = true;
		DXUtil::buildTopLevelAS(pDevice, pCurrentCommandList, blasBuffers, pTlasTempBuffer[pCurrentBackBufferIndex], scene.getFaceOffsets(), groupMatrices, true, tlasBuffers);
		DXUtil::updateDataInDefaultHeap(
			pDevice,
			pCurrentCommandList,
			pMatrices,
			pTempBufferMatrices[pCurrentBackBufferIndex],
			groupMatrices.data(),
			sizeof(decltype(groupMatrices)::value_type) * groupMatrices.size(),
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}

	ImGui::Begin("Lights");

	for (int i = 0; i < scene.getLights().size(); ++i) {
		ImGui::PushID(&scene.getLights()[i]);
		clear |= ImGui::SliderFloat3("Radiance Multiplier", scene.getLight(i).intensity.m128_f32, 0.f, 10.f);
		ImGui::PopID();
	}

	ImGui::End();

	// Setup area lights
	cBuff.numLights = std::min(std::size(cBuff.areaLights), scene.getLights().size());
	memcpy(cBuff.areaLights, scene.getLights().data(), sizeof(Shaders::AreaLight) * cBuff.numLights);

	// seed
	cBuff.seed1 = sampler.nextUInt32();
	cBuff.seed2 = sampler.nextUInt32();
	cBuff.clear = clear ? 1 : 0;
	cBuff.debugPixel = debugMode ? 1 : 0;
	cBuff.debugPixelX = debugPixelX;
	cBuff.debugPixelY = debugPixelY;
	//
	DXUtil::updateDataInDefaultHeap(
		pDevice,
		pCurrentCommandList,
		pConstantBuffer,
		pConstTempBuffer[pCurrentBackBufferIndex],
		&cBuff,
		sizeof(cBuff),
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	// bind empty root signature 
	pCurrentCommandList->SetComputeRootSignature(globalEmptyRootSignature.Get());
	pCurrentCommandList->SetPipelineState1(pStateObject.Get());

	// Launch rays
	D3D12_DISPATCH_RAYS_DESC dispatchRaysDesc = shadingTable->getDispatchRaysDescriptor(winWidth, winHeight);

	pCurrentCommandList->DispatchRays(&dispatchRaysDesc);

	

	// Create ImGui Test Window
	//ImGui::Begin("Test");
	//ImGui::Text("Hello, world %d", 123);
	//ImGui::End();

	// Assemble together draw data
	ImGui::Render();
}

void Engine::RTGraphics::endFrame()
{
	// Anvil
	if (debugMode) {
		//pCurrentCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(outputAnvilBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE));
		pCurrentCommandList->CopyResource(readbackAnvilBuffer[pCurrentBackBufferIndex].Get(), outputAnvilBuffer.Get());
	}

	pCurrentCommandList->ResourceBarrier(1u, &CD3DX12_RESOURCE_BARRIER::Transition(outputRTTexture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE));

	// Change the back buffer from the render target state to the present state
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	// perform copy
	pCurrentCommandList->CopyResource(backBuffer.Get(), outputRTTexture.Get());
	pCurrentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Draw imgui
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pRTVDescriptorSize);
	pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandle, FALSE, nullptr);

	pCurrentCommandList->SetDescriptorHeaps(1u, pImGuiDescriptorHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCurrentCommandList.Get());

	pCurrentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	// Execute command list
	frameFenceValues[pCurrentBackBufferIndex] = pCommandQueue->executeCommandList(pCurrentCommandList);

	// Release pointer to this command list (Comptr reset is being called here)
	pCurrentCommandList.Reset();

	HRESULT hr;
	GFXTHROWIFFAILED(pSwapChain->Present(0u, 0u));

	// Set current back buffer and Wait for any fence values associated to it
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
	pCommandQueue->waitForFenceValue(frameFenceValues[pCurrentBackBufferIndex]);

	if (debugMode) {
		// Anvil
		D3D12_RANGE readbackAnvilBufferRange{ 0, sizeof(PathTracingPath) };
		PathTracingPath* debugPathTracingPath{};
		readbackAnvilBuffer[pCurrentBackBufferIndex]->Map(0, &readbackAnvilBufferRange, reinterpret_cast<void**>(&debugPathTracingPath));
		if (debugPathTracingPath->debugId == 0) {
			cout << "AAA" << endl;
		}
		memcpy(&localDebugPathTracingPath, debugPathTracingPath, sizeof(PathTracingPath));

		vector<Shaders::PathTracingIntersectionContext> v;
		v.insert(v.end(), begin(localDebugPathTracingPath.pathTracingIntersectionContext), begin(localDebugPathTracingPath.pathTracingIntersectionContext) + debugPathTracingPath->numRays);
		
		readbackAnvilBufferRange.End = 0;
		readbackAnvilBuffer[pCurrentBackBufferIndex]->Unmap(0, &readbackAnvilBufferRange);

		// Anvil stuff
		trace.clear();
		trace.add("ray", v);
		Anvil::getInstance().tick();
	}
}

void Engine::RTGraphics::setDebugMode(bool debugEnabled)
{
	debugMode = debugEnabled;
}

void Engine::RTGraphics::setPixelToObserve(uint32_t x, uint32_t y)
{
	debugPixelX = x;
	debugPixelY = y;
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
	const WCHAR* entryPoints[] = { L"rayGen", L"miss", L"chs", L"shadowChs", L"indirectChs", L"indirectMiss" };
	dxilSubObject.DefineExports(entryPoints);

	// Second - Hit Program - link to entry point names
	CD3DX12_HIT_GROUP_SUBOBJECT hitSubObject (stateObjectDesc);
	hitSubObject.SetClosestHitShaderImport(L"chs");
	hitSubObject.SetHitGroupExport(L"HitGroup");

	CD3DX12_HIT_GROUP_SUBOBJECT shadowHitSubObject(stateObjectDesc);
	shadowHitSubObject.SetClosestHitShaderImport(L"shadowChs");
	shadowHitSubObject.SetHitGroupExport(L"ShadowHitGroup");

	CD3DX12_HIT_GROUP_SUBOBJECT indirectHitSubObject(stateObjectDesc);
	indirectHitSubObject.SetClosestHitShaderImport(L"indirectChs");
	indirectHitSubObject.SetHitGroupExport(L"IndirectHitGroup");

	
	// Third - Local Root Signature for Ray Gen shader
	// Build the root signature descriptor and create root signature
	rootSignatureManager->addDescriptorRange("BVHAndTextures", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE));//gOutput, gRadiance, gAnvilBuffer
	rootSignatureManager->addDescriptorRange("BVHAndTextures", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE)); //gRtScene

	if (!textures.empty()) {
		rootSignatureManager->addDescriptorRange("BVHAndTextures", CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, textures.size(), 6, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE));
	}

	rootSignatureManager->setDescriptorTableParameter("BVHAndTexturesDescTable", "BVHAndTextures");

	CD3DX12_ROOT_PARAMETER1 param;
	param.InitAsConstantBufferView(0);
	rootSignatureManager->setParameter("ConstBuff", param);

	rootSignatureManager->addParametersToRootSignature("RayGenRootSignature", { "BVHAndTexturesDescTable", "ConstBuff" });
	rootSignatureManager->generateRootSignature("RayGenRootSignature", pDevice);

	// Fourth - Associate the local root signature to registers in shaders (in the rayGen program) using Export Association
	shadingTable->addProgram(L"rayGen", RayGeneration, "RayGenRootSignature");

	// Fifth - create empty lrs for miss program
	rootSignatureManager->addRootSignature("EmptyRootSignature");
	rootSignatureManager->generateRootSignature("EmptyRootSignature", pDevice);

	// Sixth - Associate the empty local root signature with the miss programs
	shadingTable->addProgram(L"miss", Miss, "EmptyRootSignature");
	shadingTable->addProgram(L"indirectMiss", Miss, "EmptyRootSignature");

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// Create Hit root signature parameters
	param.InitAsShaderResourceView(1); rootSignatureManager->setParameter("verts", param);
	param.InitAsShaderResourceView(2); rootSignatureManager->setParameter("faceAttributes", param);
	param.InitAsShaderResourceView(3); rootSignatureManager->setParameter("materials", param);
	param.InitAsShaderResourceView(4); rootSignatureManager->setParameter("texVerts", param);
	param.InitAsShaderResourceView(5); rootSignatureManager->setParameter("matrices", param);

	rootSignatureManager->addParametersToRootSignature("HitRootSignature", { "ConstBuff",  "verts",  "BVHAndTexturesDescTable", "faceAttributes", "materials", "texVerts", "matrices" });
	rootSignatureManager->setSamplerForRootSignature("HitRootSignature", sampler);
	rootSignatureManager->generateRootSignature("HitRootSignature", pDevice);

	// Associate cbv signature with hit program
	shadingTable->addProgram(L"HitGroup", HitGroup, "HitRootSignature");
	shadingTable->addProgram(L"ShadowHitGroup", HitGroup, "EmptyRootSignature");
	shadingTable->addProgram(L"IndirectHitGroup", HitGroup, "EmptyRootSignature");

	// Generate/add subobjects
	rootSignatureManager->addRootSignaturesToSubObject(stateObjectDesc);
	shadingTable->addProgramAssociationsToSubobject(stateObjectDesc);

	// Seventh - Shader Configuration (set payload sizes - the actual program parameters)
	CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT shaderConfig(stateObjectDesc);
	shaderConfig.Config(5 * sizeof(float), 2 * sizeof(float));

	// Eighth - Associate the shader configuration with all shader programs
	CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT shaderConfigAssociation(stateObjectDesc);
	shaderConfigAssociation.AddExport(L"rayGen");
	shaderConfigAssociation.AddExport(L"miss");
	shaderConfigAssociation.AddExport(L"indirectMiss");
	shaderConfigAssociation.AddExport(L"HitGroup");
	shaderConfigAssociation.AddExport(L"ShadowHitGroup");
	shaderConfigAssociation.AddExport(L"IndirectHitGroup");
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

// TODO: Automate where possible
void Engine::RTGraphics::createShaderResources()
{
	// The descriptor heap to store SRV (Shader resource View) and UAV (Unordered access view) descriptors
	auto& descHeapManager = shadingTable->generateDescriptorHeap("BVHAndTexturesDescTable", "BVHTextures1", pDevice);
	descriptorHeap = descHeapManager.getDescriptorHeap();

	// The output resource
	outputRTTexture = DXUtil::createTextureCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, winWidth, winHeight, D3D12_RESOURCE_STATE_COPY_SOURCE);
	radianceTexture = DXUtil::createTextureCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, winWidth, winHeight, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_NONE, DXGI_FORMAT_R32G32B32A32_FLOAT);
	outputAnvilBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, sizeof(PathTracingPath), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	//outputAnvilBuffer = DXUtil::createTextureCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, sizeof(PathTracingPath), 1, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, DXGI_FORMAT_UNKNOWN);

	// Create the UAV descriptor first (needs to be same order as in root signature)
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; // In below, set it at the first location of this heap

	size_t entryNumber = 0;

	descHeapManager.setUAV(entryNumber++, uavDesc, pDevice, outputRTTexture);
	descHeapManager.setUAV(entryNumber++, uavDesc, pDevice, radianceTexture);

	// Anvil
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc2 = {};
	uavDesc2.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc2.Buffer.NumElements = 1;
	uavDesc2.Buffer.StructureByteStride = sizeof(PathTracingPath);

	descHeapManager.setUAV(entryNumber++, uavDesc2, pDevice, outputAnvilBuffer);

	// Create the SRV descriptor in second place (following same order as in root signature)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = tlasBuffers.pResult->GetGPUVirtualAddress();

	descHeapManager.setSRV(entryNumber++, srvDesc, pDevice);

	srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	for (const auto& texture : textures) {
		descHeapManager.setSRV(entryNumber++, srvDesc, pDevice, texture);
	}
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
	// Link elements
	shadingTable->setInputForDescriptorTableParameter(L"rayGen", "BVHAndTexturesDescTable", "BVHTextures1");
	shadingTable->setInputForViewParameter(L"rayGen", "ConstBuff", pConstantBuffer);

	shadingTable->setInputForViewParameter(L"HitGroup", "ConstBuff", pConstantBuffer);
	shadingTable->setInputForViewParameter(L"HitGroup", "verts", vertexBuffer);
	shadingTable->setInputForDescriptorTableParameter(L"HitGroup", "BVHAndTexturesDescTable", "BVHTextures1");
	shadingTable->setInputForViewParameter(L"HitGroup", "faceAttributes", pFaceAttributes);
	shadingTable->setInputForViewParameter(L"HitGroup", "materials", pMaterials);
	shadingTable->setInputForViewParameter(L"HitGroup", "texVerts", pTexCoords);
	shadingTable->setInputForViewParameter(L"HitGroup", "matrices", pMatrices);

	return shadingTable->generateShadingTable(pDevice, pCurrentCommandList, pStateObject, shaderTableTempResource);
}
