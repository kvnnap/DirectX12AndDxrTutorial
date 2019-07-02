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
#include "Camera.h"
#include "IRenderer.h"

namespace Engine {
	class RTGraphics 
		: public IRenderer
	{
	public:
		RTGraphics(HWND hWnd);
		RTGraphics(const RTGraphics&) = delete;
		RTGraphics& operator=(const RTGraphics&) = delete;
		virtual ~RTGraphics();

		void clearBuffer(float red, float green, float blue) override;
		void init() override;
		void draw(uint64_t timeMs = 0) override;
		void endFrame() override;

	private:

		static const UINT numBackBuffers = 2;

		DxgiInfoManager infoManager;
		int winWidth, winHeight;

		Microsoft::WRL::ComPtr<ID3D12Device5> pDevice;
		std::unique_ptr<CommandQueue> pCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCurrentCommandList;

		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain;

		// Heaps are similar to views in DirectX11 - essentially a list of views?
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDepthDescriptorHeap;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;

		// Tracks the backbuffers used in the swapchain
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffers[numBackBuffers];

		BOOL tearingSupported;

		UINT pRTVDescriptorSize;
		UINT pDSVDescriptorSize;
		UINT pCurrentBackBufferIndex;
		uint64_t frameFenceValues[numBackBuffers];

		// Temporary triangle stuff here
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;

		// Root signature
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

		// Pipeline state object.
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;

		std::unique_ptr<Camera> camera;
	};
}