#include "DXUtil.h"

#include "../Exception/WindowException.h"
#include "../Exception/DxgiInfoException.h"
#include "../Libraries/d3dx12.h"

#include <iostream>

namespace wrl = Microsoft::WRL;

using namespace Util;

void Util::DXUtil::enableDebugLayer()
{
#ifdef _DEBUG
	HRESULT hr;
	wrl::ComPtr<ID3D12Debug> debugInterface;
	GFXTHROWIFFAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

void Util::DXUtil::setupDebugLayer(wrl::ComPtr<ID3D12Device5> pDevice)
{
#ifdef _DEBUG
	HRESULT hr;
	wrl::ComPtr<ID3D12InfoQueue> infoQueue;
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

bool Util::DXUtil::checkTearingSupport()
{
	BOOL allowTearing = false;
	UINT createFactoryFlags = 0;

	wrl::ComPtr<IDXGIFactory7> dxgiFactory = createDXGIFactory();

	HRESULT hr;

	GFXTHROWIFFAILED(dxgiFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)));

	return allowTearing == TRUE;
}

wrl::ComPtr<IDXGIAdapter4> Util::DXUtil::getAdapterLatestFeatureLevel(D3D_FEATURE_LEVEL *fl, bool useWarp)
{
	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	wrl::ComPtr<IDXGIAdapter4> adapter;
	for (auto featureLevel : featureLevels) {
		std::cout << "Trying Feature Level: " << featureLevel << "... ";
		adapter = getAdapter(featureLevel);
		if (adapter) {
			*fl = featureLevel;
			std::cout << "OK" << std::endl;
			break;
		} else {
			std::cout << "FAILED" << std::endl;
		}
	}

	if (!adapter) {
		ThrowException("Cannot find a compatible DX12 hardware device");
	}

	return adapter;
}

wrl::ComPtr<IDXGIAdapter4> Util::DXUtil::getAdapter(D3D_FEATURE_LEVEL featureLevel, bool useWarp)
{
	wrl::ComPtr<IDXGIFactory7> dxgiFactory = createDXGIFactory();

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

wrl::ComPtr<ID3D12Device5> Util::DXUtil::createDeviceFromAdapter(wrl::ComPtr<IDXGIAdapter4> adapter, D3D_FEATURE_LEVEL featureLevel)
{
	HRESULT hr;
	wrl::ComPtr<ID3D12Device5> pDevice;

	GFXTHROWIFFAILED(D3D12CreateDevice(adapter.Get(), featureLevel, IID_PPV_ARGS(&pDevice)));

	return pDevice;
}

wrl::ComPtr<IDXGIFactory7> Util::DXUtil::createDXGIFactory()
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

wrl::ComPtr<ID3D12DescriptorHeap> Util::DXUtil::createDescriptorHeap(wrl::ComPtr<ID3D12Device5> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, UINT numDescriptors)
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

wrl::ComPtr<IDXGISwapChain4> Util::DXUtil::createSwapChain(wrl::ComPtr<ID3D12CommandQueue> commandQueue, HWND hWnd, UINT numBuffers)
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
	sd.BufferCount = numBuffers;
	sd.Scaling = DXGI_SCALING_NONE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	sd.Flags = checkTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	wrl::ComPtr<IDXGISwapChain4> swapChain;
	wrl::ComPtr<IDXGISwapChain1> swapChain1;

	HRESULT hr;
	auto factory = createDXGIFactory();
	GFXTHROWIFFAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), hWnd, &sd, nullptr, nullptr, &swapChain1));
	GFXTHROWIFFAILED(swapChain1.As(&swapChain));
	// TODO: pCurrentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

	return swapChain;
}

std::vector<wrl::ComPtr<ID3D12Resource>> Util::DXUtil::createRenderTargetViews(
	wrl::ComPtr<ID3D12Device5> device,
	wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
	wrl::ComPtr<IDXGISwapChain4> swapChain,
	UINT numRTV)
{
	std::vector<wrl::ComPtr<ID3D12Resource>> backBuffers (numRTV);
	
	auto pRTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	HRESULT hr;
	for (int i = 0; i < numRTV; ++i) {
		GFXTHROWIFFAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])));
		// MAY FAIL -------
		/*ThrowDxgiInfoExceptionIfFailed*/(device->CreateRenderTargetView(backBuffers[i].Get(), nullptr, rtvHandle));
		// MAY FAIL END ---
		rtvHandle.Offset(pRTVDescriptorSize);
	}

	return backBuffers;
}

std::vector<wrl::ComPtr<ID3D12Resource>> Util::DXUtil::createDepthStencilView(
	wrl::ComPtr<ID3D12Device5> device,
	wrl::ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap,
	UINT winWidth, UINT winHeight,
	UINT numDSV)
{
	std::vector<wrl::ComPtr<ID3D12Resource>> depthBuffers(numDSV);

	auto pDSVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(depthDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

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
	for (int i = 0; i < numDSV; ++i) {
		// Create depth buffer
		GFXTHROWIFFAILED(device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, winWidth, winHeight, 1u, 0u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&clearValue,
			IID_PPV_ARGS(&depthBuffers[i])
		));

		// Create view for resource
		/*ThrowDxgiInfoExceptionIfFailed*/(device->CreateDepthStencilView(depthBuffers[i].Get(), &dsv, dsvHandle));
		dsvHandle.Offset(pDSVDescriptorSize);
	}

	return depthBuffers;
}

wrl::ComPtr<ID3D12Resource> Util::DXUtil::createCommittedResource(wrl::ComPtr<ID3D12Device5> device, D3D12_HEAP_TYPE heapType, UINT64 size, D3D12_RESOURCE_STATES resourceState)
{
	wrl::ComPtr<ID3D12Resource> buffer;
	
	HRESULT hr;
	GFXTHROWIFFAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size),
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)
	));

	return buffer;
}
