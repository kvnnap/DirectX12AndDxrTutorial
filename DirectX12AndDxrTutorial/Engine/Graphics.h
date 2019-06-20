#pragma once

#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include "DxgiInfoManager.h"

#include "../Exception/WindowException.h"
#include "CommandQueue.h"

namespace Engine {

	class Graphics
	{
	public:
		Graphics(HWND hWnd);
		Graphics(const Graphics&) = delete;
		Graphics& operator=(const Graphics&) = delete;
		virtual ~Graphics();

		void clearBuffer(float red, float green, float blue);
		void init();
		void draw(uint64_t timeMs = 0, float zTrans = 0.f);
		void endFrame();

		void simpleDraw(uint64_t timeMs = 0);
		int getWinWidth() const;
		int getWinHeight() const;

		Microsoft::WRL::ComPtr<ID3D12Device5>& getDevice();
		DxgiInfoManager& getInfoManager();

	private:

		void enableDebugLayer();
		void setupDebugLayer();
		bool checkTearingSupport();

		Microsoft::WRL::ComPtr<IDXGIFactory7> createFactory();
		Microsoft::WRL::ComPtr<IDXGISwapChain4> createSwapChain(HWND hWnd);
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, UINT numDescriptors);
		
		void createRenderTargetViews();

		Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapter(D3D_FEATURE_LEVEL featureLevel, bool useWarp = false);

		DxgiInfoManager infoManager;
		int winWidth, winHeight;

		Microsoft::WRL::ComPtr<ID3D12Device5> pDevice;
		std::unique_ptr<CommandQueue> pCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCurrentCommandList;

		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwap;

		// Heaps are similar to views in DirectX11 - essentially a list of views?
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescriptorHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;

		// Tracks the backbuffers used in the swapchain
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffers[2];

		BOOL tearingSupported;

		UINT pRTVDescriptorSize;
		UINT pCurrentBackBufferIndex;
		uint64_t frameFenceValues[2];
	};

}