#pragma once

#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>

#include "DxgiInfoManager.h"

#include "../Exception/WindowException.h"

#include <vector>
#include <memory>

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
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> createCommandQueue(D3D12_COMMAND_LIST_TYPE listType);
		Microsoft::WRL::ComPtr<IDXGISwapChain4> createSwapChain(HWND hWnd);
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, UINT numDescriptors);
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> createCommandAllocator();
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> createCommandList();
		Microsoft::WRL::ComPtr<ID3D12Fence> createFence();
		HANDLE createFenceEvent();
		uint64_t signal();
		void waitForFenceValue();

		void createRenderTargetViews();

		void flush();


		Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapter(D3D_FEATURE_LEVEL featureLevel, bool useWarp = false);

		DxgiInfoManager infoManager;
		int winWidth, winHeight;

		Microsoft::WRL::ComPtr<ID3D12Device5> pDevice;
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwap;

		// Heaps are similar to views in DirectX11 - essentially a list of views?
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescriptorHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;

		// Tracks the backbuffers used in the swapchain
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffers[2];

		// Submit this to the command queue
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> pCommandList;

		// Command allocators - only use one per in-flight render frame
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocators[2];

		BOOL tearingSupported;

		UINT pRTVDescriptorSize;
		UINT pCurrentBackBufferIndex = 0;


		// Synchronization objects
		Microsoft::WRL::ComPtr<ID3D12Fence> pFence;
		uint64_t fenceValue = 0;
		uint64_t frameFenceValues[2] = {};
		HANDLE fenceEvent;
	};

}