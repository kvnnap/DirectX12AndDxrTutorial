#include "Graphics.h"

#include <chrono>
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
using namespace Engine;

Graphics::Graphics(HWND hWnd)
	: winWidth(), winHeight(), pRTVDescriptorSize(), pDSVDescriptorSize(), pCurrentBackBufferIndex(), frameFenceValues{},
	scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)), viewport()
{
	HRESULT hr;

	RECT rect;
	GetClientRect(hWnd, &rect);
	winWidth = rect.right;
	winHeight = rect.bottom;

	viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight));

	// Enable debugging
	enableDebugLayer();
	
	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	wrl::ComPtr<IDXGIAdapter4> adapter;
	for (auto featureLevel : featureLevels) {
		cout << "Trying Feature Level: " << featureLevel << "... ";
		adapter = getAdapter(featureLevel);
		if (adapter) {
			// Create d3d12 device - A device gives us access to create resources on the GPU
			GFXTHROWIFFAILED(D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&pDevice)));
			cout << "OK" << endl;
			break;
		}
		else {
			cout << "FAILED" << endl;
		}
	}

	if (!adapter) {
		ThrowException("Cannot find a compatible DX12 hardware device");
	}

	// Enable debug messages in debug mode
	setupDebugLayer();

	// Create command queue, with command list and command allocators
	pCommandQueue = make_unique<CommandQueue>(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// Create swap chain
	pSwap = createSwapChain(hWnd);

	// Create descriptor heap for render target view
	pDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, std::size(pBackBuffers));

	// Create render target Views
	createRenderTargetViews();

	// And for depth stencil view
	pDepthDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, std::size(pBackBuffers));
	createDepthStencilView();

	// Save tearing support
	tearingSupported = checkTearingSupport();

	// Init camera
	camera = make_unique<Camera>(
		dx::XMVectorSet(0.f, 0.f, -5.f, 1.f),
		dx::XMVectorSet(0.f, 0.f, 1.f, 0.f),
		(float)winWidth / winHeight,
		1.0f,
		1.f,
		10.f);

	// Setup ImGui - TODO DX12
	/*bool valid = IMGUI_CHECKVERSION();
	pImGuiDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, std::size(pBackBuffers));
	ImGuiContext * context = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX12_Init(
		pDevice.Get(), 
		std::size(pBackBuffers), 
		DXGI_FORMAT_R8G8B8A8_UNORM, 
		pImGuiDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		pImGuiDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	ImGui::StyleColorsDark();*/
}

Graphics::~Graphics() {
	/*ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();*/

	// Ensure that all GPU stuff is finished before releasing resources
	pCommandQueue->flush();
}

void Graphics::clearBuffer(float red, float green, float blue)
{
	// begins a frame
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];

	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pCurrentCommandList = pCommandQueue->getCommandList();
	pCurrentCommandList->ResourceBarrier(1, &barrier);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pRTVDescriptorSize);
	FLOAT color[] = { red, green, blue, 1.0f };
	pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);

	// Clear depth buffer
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(pDepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pDSVDescriptorSize);
	pCurrentCommandList->ClearDepthStencilView(dsvDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
}

void Engine::Graphics::init()
{
	// Let's draw a triangle.. 
	// Vertex data for a colored cube.
	struct VertexPosColor
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Color;
	};

	VertexPosColor vertices[] = {
		{DirectX::XMFLOAT3(-1.f, 1.f, 0.f), DirectX::XMFLOAT3(1.f, 0.f, 0.f)},
		{DirectX::XMFLOAT3( 1.f,-1.f, 0.f), DirectX::XMFLOAT3(0.f, 1.f, 0.f)},
		{DirectX::XMFLOAT3(-1.f,-1.f, 0.f), DirectX::XMFLOAT3(0.f, 0.f, 1.f)}
	};

	HRESULT hr;
	GFXTHROWIFFAILED(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&vertexBuffer)
	));


	wrl::ComPtr<ID3D12Resource> intermediateBuffer;
	GFXTHROWIFFAILED(pDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&intermediateBuffer)
	));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = vertices;
	subresourceData.RowPitch = sizeof(vertices);
	subresourceData.SlicePitch = subresourceData.RowPitch;

	auto commandList = pCommandQueue->getCommandList();

	// Upload vertices using intermediate upload heap - using helper method
	UpdateSubresources(
		commandList.Get(),
		vertexBuffer.Get(),
		intermediateBuffer.Get(),
		0, 0, 1, &subresourceData
	);

	// setup vertex buffer view
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(vertices);
	vertexBufferView.StrideInBytes = sizeof(VertexPosColor);

	// Load shaders
	wrl::ComPtr<ID3DBlob> pVertexShaderBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/VertexShader.cso", &pVertexShaderBlob));

	wrl::ComPtr<ID3DBlob> pPixelShaderBlob;
	GFXTHROWIFFAILED(D3DReadFileToBlob(L"./Shaders/PixelShader.cso", &pPixelShaderBlob));

	// Input Assembler config
	D3D12_INPUT_ELEMENT_DESC ied[] = {
		{ "Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "Color", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Root signature
	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	// A single 32-bit constant root parameter that is used by the vertex shader.
	CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};
	rootParameters[0].InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
	rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	// Serialize the root signature.
	wrl::ComPtr<ID3DBlob> rootSignatureBlob;
	wrl::ComPtr<ID3DBlob> errorBlob;
	GFXTHROWIFFAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription,
		featureData.HighestVersion, &rootSignatureBlob, &errorBlob));
	// Create the root signature.
	GFXTHROWIFFAILED(pDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	// Setup pipeline
	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	pipelineStateStream.pRootSignature = rootSignature.Get();
	pipelineStateStream.InputLayout = { ied, std::size(ied) };
	pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(pVertexShaderBlob.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(pPixelShaderBlob.Get());
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RTVFormats = rtvFormats;

	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};
	GFXTHROWIFFAILED(pDevice->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&pipelineState)));


	pCommandQueue->executeCommandList(commandList);
	pCommandQueue->flush();
}

void Graphics::draw(uint64_t timeMs, float zTrans)
{
	pCurrentCommandList->SetPipelineState(pipelineState.Get());
	pCurrentCommandList->SetGraphicsRootSignature(rootSignature.Get());

	pCurrentCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pCurrentCommandList->IASetVertexBuffers(0u, 1u, &vertexBufferView);

	pCurrentCommandList->RSSetScissorRects(1u, &scissorRect);
	pCurrentCommandList->RSSetViewports(1u, &viewport);

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pRTVDescriptorSize);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvDescriptorHandle(pDepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pDSVDescriptorSize);
	pCurrentCommandList->OMSetRenderTargets(1u, &rtvDescriptorHandle, FALSE, &dsvDescriptorHandle);


	// Update the MVP matrix
	DirectX::XMMATRIX mvpMatrix = camera->getViewPerspectiveMatrix();
	pCurrentCommandList->SetGraphicsRoot32BitConstants(0u, sizeof(dx::XMMATRIX) / 4, &mvpMatrix, 0u);

	pCurrentCommandList->DrawInstanced(3u, 1u, 0u, 0u);
	// Start the Dear ImGui frame
	//ImGui_ImplDX12_NewFrame();
	//ImGui_ImplWin32_NewFrame();
	//ImGui::NewFrame();

	//// Create ImGui Window
	//ImGui::Begin("Drawables");

	//ImGui::End();

	//// Create ImGui Test Window
	////ImGui::Begin("Test");
	////ImGui::Text("Hello, world %d", 123);
	////ImGui::End();

	//// Assemble together draw data
	//ImGui::Render();
	////ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList.Get());
}

void Graphics::endFrame()
{
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pCurrentCommandList->ResourceBarrier(1, &barrier);

	frameFenceValues[pCurrentBackBufferIndex] = pCommandQueue->executeCommandList(pCurrentCommandList);
	pCurrentCommandList.Reset();

	HRESULT hr;
	GFXTHROWIFFAILED(pSwap->Present(1u, 0u));
	pCurrentBackBufferIndex = pSwap->GetCurrentBackBufferIndex();
	pCommandQueue->waitForFenceValue(frameFenceValues[pCurrentBackBufferIndex]);
}

void Engine::Graphics::simpleDraw(uint64_t timeMs)
{
	
}

int Engine::Graphics::getWinWidth() const
{
	return winWidth;
}

int Engine::Graphics::getWinHeight() const
{
	return winHeight;
}

Microsoft::WRL::ComPtr<ID3D12Device5>& Engine::Graphics::getDevice()
{
	return pDevice;
}

DxgiInfoManager& Engine::Graphics::getInfoManager()
{
	return infoManager;
}

void Engine::Graphics::enableDebugLayer()
{
#ifdef _DEBUG
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
	GFXTHROWIFFAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void Engine::Graphics::setupDebugLayer()
{
#ifdef _DEBUG
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
	GFXTHROWIFFAILED(pDevice.As(&infoQueue));
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
	infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

	D3D12_INFO_QUEUE_FILTER queueFilter = {};
	D3D12_MESSAGE_SEVERITY severities[] = {
		D3D12_MESSAGE_SEVERITY_INFO
	};
	D3D12_MESSAGE_ID denyIds[] = {
		D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
	};

	// TODO comment the below to see if we get more messages
	queueFilter.DenyList.pSeverityList = severities;
	queueFilter.DenyList.NumSeverities = std::size(severities);
	queueFilter.DenyList.pIDList = denyIds;
	queueFilter.DenyList.NumIDs = std::size(denyIds);

	GFXTHROWIFFAILED(infoQueue->PushStorageFilter(&queueFilter));

#endif
}

bool Engine::Graphics::checkTearingSupport()
{
	BOOL allowTearing = false;
	UINT createFactoryFlags = 0;

	wrl::ComPtr<IDXGIFactory7> dxgiFactory = createFactory();

	HRESULT hr;

	GFXTHROWIFFAILED(dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));

	return allowTearing == TRUE;
}

Microsoft::WRL::ComPtr<IDXGIFactory7> Engine::Graphics::createFactory()
{
	HRESULT hr;
	wrl::ComPtr<IDXGIFactory7> dxgiFactory;
	//dxgiFactory->
	UINT createFactoryFlags = 0;
#ifdef _DEBUG
	createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	GFXTHROWIFFAILED(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
	return dxgiFactory;
}

wrl::ComPtr<IDXGISwapChain4> Engine::Graphics::createSwapChain(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC1 sd = {};
	// Use the window (hWnd) dimensions
	sd.Width = 0;
	sd.Height = 0;

	// Pixel format (TODO: Try srgb too)
	sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.Stereo = FALSE;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = std::size(pBackBuffers);
	sd.Scaling = DXGI_SCALING_NONE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd.Flags = checkTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	
	wrl::ComPtr<IDXGISwapChain4> swapChain;
	wrl::ComPtr<IDXGISwapChain1> swapChain1;

	HRESULT hr;
	auto factory = createFactory();
	GFXTHROWIFFAILED(factory->CreateSwapChainForHwnd(pCommandQueue->getCommandQueue().Get(), hWnd, &sd, nullptr, nullptr, &swapChain1));
	GFXTHROWIFFAILED(swapChain1.As(&swapChain));
	pCurrentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	return swapChain;
}

wrl::ComPtr<ID3D12DescriptorHeap> Engine::Graphics::createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, UINT numDescriptors)
{
	wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.Type = descriptorHeapType;
	dhd.NumDescriptors = numDescriptors;
	dhd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr;
	GFXTHROWIFFAILED(pDevice->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void Engine::Graphics::createRenderTargetViews()
{
	pRTVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	HRESULT hr;
	for (int i = 0; i < std::size(pBackBuffers); ++i) {
		GFXTHROWIFFAILED(pSwap->GetBuffer(i, IID_PPV_ARGS(&pBackBuffers[i])));
		ThrowDxgiInfoExceptionIfFailed(pDevice->CreateRenderTargetView(pBackBuffers[i].Get(), nullptr, rtvHandle));
		rtvHandle.Offset(pRTVDescriptorSize);
	}
}

void Engine::Graphics::createDepthStencilView()
{
	pDSVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(pDepthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	
	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.f;
	clearValue.DepthStencil.Stencil = 0;

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;

	HRESULT hr;
	for (int i = 0; i < std::size(pBackBuffers); ++i) {
		// Create depth buffer
		GFXTHROWIFFAILED(pDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, winWidth, winHeight, 1u, 0u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&pDepthBuffers[i])
		));

		// Create view for resource
		ThrowDxgiInfoExceptionIfFailed(pDevice->CreateDepthStencilView(pDepthBuffers[i].Get(), &dsv, dsvHandle));
		dsvHandle.Offset(pDSVDescriptorSize);
	}
}

wrl::ComPtr<IDXGIAdapter4> Engine::Graphics::getAdapter(D3D_FEATURE_LEVEL featureLevel, bool useWarp)
{
	wrl::ComPtr<IDXGIFactory7> dxgiFactory = createFactory();

	wrl::ComPtr<IDXGIAdapter1> dxgiAdapter1;
	wrl::ComPtr<IDXGIAdapter4> dxgiAdapter4;
	HRESULT hr;
	if (useWarp) {
		GFXTHROWIFFAILED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter4)));
	}
	else {
		for (UINT adapterIndex = 0; dxgiFactory->EnumAdapters1(adapterIndex, &dxgiAdapter1) != DXGI_ERROR_NOT_FOUND; ++adapterIndex) {
			DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
			dxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

			const bool isHardware = (dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
			const bool d3d12DeviceCreationSuccess = SUCCEEDED(D3D12CreateDevice(dxgiAdapter1.Get(), featureLevel, __uuidof(ID3D12Device5), nullptr));
			if (isHardware && d3d12DeviceCreationSuccess) {
				GFXTHROWIFFAILED(dxgiAdapter1.As(&dxgiAdapter4));
				break;
			}
		}
	}

	return dxgiAdapter4;
}

