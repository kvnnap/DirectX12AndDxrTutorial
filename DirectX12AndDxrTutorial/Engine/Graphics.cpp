#include "Graphics.h"

#include <chrono>
#include <iostream>
#include <d3dcompiler.h>
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
	: winWidth(), winHeight(), pRTVDescriptorSize(), pCurrentBackBufferIndex(), frameFenceValues{}
{
	HRESULT hr;

	RECT rect;
	GetClientRect(hWnd, &rect);
	winWidth = rect.right;
	winHeight = rect.bottom;

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

	// Save tearing support
	tearingSupported = checkTearingSupport();

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
}

void Engine::Graphics::init()
{
	
}

void Graphics::draw(uint64_t timeMs, float zTrans)
{
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

