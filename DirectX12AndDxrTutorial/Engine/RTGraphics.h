#pragma once

#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

#include "Util/DXUtil.h"

#include "DxgiInfoManager.h"

#include "../Exception/WindowException.h"
#include "CommandQueue.h"
#include "Camera.h"
#include "IRenderer.h"

#include "Scene.h"

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
		void draw(uint64_t timeMs = 0, bool clear = false) override;
		void endFrame() override;
		Camera& getCamera() override;

	private:

		static const UINT numBackBuffers = 2;

		Microsoft::WRL::ComPtr<ID3D12StateObject> createRtPipeline();
		void createShaderResources();
		void createMaterialsAndFaceAttributes();
		Microsoft::WRL::ComPtr<ID3D12Resource> createShaderTable(Microsoft::WRL::ComPtr<ID3D12Resource>& shaderTableTempResource);

		DxgiInfoManager infoManager;
		int winWidth, winHeight;

		Microsoft::WRL::ComPtr<ID3D12Device5> pDevice;
		std::unique_ptr<CommandQueue> pCommandQueue;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCurrentCommandList;
		Util::DXUtil::AccelerationStructureBuffers blasBuffers, tlasBuffers;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTlasTempBuffer[numBackBuffers];

		Microsoft::WRL::ComPtr<ID3D12StateObject> pStateObject;
		Microsoft::WRL::ComPtr<ID3D12Resource> outputRTTexture;
		Microsoft::WRL::ComPtr<ID3D12Resource> radianceTexture;
		Microsoft::WRL::ComPtr<ID3D12Resource> pConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> pMaterials;
		Microsoft::WRL::ComPtr<ID3D12Resource> pFaceAttributes;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;

		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain;

		// Heaps are similar to views in DirectX11 - essentially a list of views?
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;

		// Tracks the backbuffers used in the swapchain
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffers[numBackBuffers];

		UINT pRTVDescriptorSize;
		UINT pCurrentBackBufferIndex;
		uint64_t frameFenceValues[numBackBuffers];

		// Temporary triangle stuff here
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> vertexBuffers;

		// Root signature
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

		// Pipeline state object.
		Microsoft::WRL::ComPtr<ID3D12Resource> pShadingTable;

		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;

		std::unique_ptr<Camera> camera;

		Scene scene;
	};
}