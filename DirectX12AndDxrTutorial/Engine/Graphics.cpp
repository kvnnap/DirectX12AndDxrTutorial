#include "Graphics.h"

#include "../Util/DXUtil.h"

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
using namespace Util;
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
	DXUtil::enableDebugLayer();
	
	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevel;
	auto adapter = DXUtil::getAdapterLatestFeatureLevel(&featureLevel);
	pDevice = DXUtil::createDeviceFromAdapter(adapter, featureLevel);

	// Enable debug messages in debug mode
	DXUtil::setupDebugLayer(pDevice);

	// Create command queue, with command list and command allocators
	pCommandQueue = make_unique<CommandQueue>(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// Create swap chain
	pSwap = DXUtil::createSwapChain(pCommandQueue->getCommandQueue(), hWnd, std::size(pBackBuffers));

	// Create descriptor heap for render target view
	pDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, std::size(pBackBuffers));

	// Create render target Views
	pRTVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto backBuffers = DXUtil::createRenderTargetViews(pDevice, pDescriptorHeap, pSwap, std::size(pBackBuffers));
	for (int i = 0; i < backBuffers.size(); ++i) {
		pBackBuffers[i] = backBuffers[i];
	}

	// And for depth stencil view
	pDepthDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, std::size(pDepthBuffers));
	pDSVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	auto depthBuffers = DXUtil::createDepthStencilView(pDevice, pDepthDescriptorHeap, winWidth, winHeight, std::size(pDepthBuffers));
	for (int i = 0; i < backBuffers.size(); ++i) {
		pDepthBuffers[i] = depthBuffers[i];
	}

	// Save tearing support
	tearingSupported = DXUtil::checkTearingSupport();

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
