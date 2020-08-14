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
#include "UniformSampler.h"

#include "RootSignatureManager.h"
#include "ShadingTable.h"

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
		void draw(uint64_t timeMs, bool& clear) override;
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
		std::vector<Util::DXUtil::AccelerationStructureBuffers> blasBuffers;
		Util::DXUtil::AccelerationStructureBuffers tlasBuffers;
		Microsoft::WRL::ComPtr<ID3D12Resource> pConstTempBuffer[numBackBuffers];
		Microsoft::WRL::ComPtr<ID3D12Resource> pTlasTempBuffer[numBackBuffers];

		Microsoft::WRL::ComPtr<ID3D12StateObject> pStateObject;
		Microsoft::WRL::ComPtr<ID3D12Resource> outputRTTexture;
		Microsoft::WRL::ComPtr<ID3D12Resource> radianceTexture;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textures;
		Microsoft::WRL::ComPtr<ID3D12Resource> pConstantBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> pMaterials;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTexCoords;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTempBufferMatrices[numBackBuffers];
		Microsoft::WRL::ComPtr<ID3D12Resource> pMatrices;
		Microsoft::WRL::ComPtr<ID3D12Resource> pFaceAttributes;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> globalEmptyRootSignature;

		Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain;

		// Heaps are similar to views in DirectX11 - essentially a list of views?
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pRTVDescriptorHeap;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pImGuiDescriptorHeap;

		// Tracks the backbuffers used in the swapchain
		Microsoft::WRL::ComPtr<ID3D12Resource> pBackBuffers[numBackBuffers];

		// Resources for Anvil
		Microsoft::WRL::ComPtr<ID3D12Resource> outputAnvilBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> readbackAnvilBuffer[numBackBuffers];

		UINT pRTVDescriptorSize;
		UINT pCurrentBackBufferIndex;
		uint64_t frameFenceValues[numBackBuffers];

		// Temporary triangle stuff here
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;

		D3D12_RECT scissorRect;
		D3D12_VIEWPORT viewport;

		std::unique_ptr<Camera> camera;

		Scene scene;
		UniformSampler sampler;

		std::shared_ptr<RootSignatureManager> rootSignatureManager;
		std::unique_ptr<ShadingTable> shadingTable;

		std::vector<DirectX::XMFLOAT3X4> groupMatrices;
	};
}