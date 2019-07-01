#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <vector>

namespace Util 
{
	class DXUtil {
	public:
		DXUtil() = delete;
		~DXUtil() = delete;

		// Helper methods
		static void enableDebugLayer();
		static void setupDebugLayer(Microsoft::WRL::ComPtr<ID3D12Device5> pDevice);
		static bool checkTearingSupport();

		static Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapterLatestFeatureLevel(D3D_FEATURE_LEVEL* featureLevel, bool useWarp = false);
		static Microsoft::WRL::ComPtr<IDXGIAdapter4> getAdapter(D3D_FEATURE_LEVEL featureLevel, bool useWarp = false);
		static Microsoft::WRL::ComPtr<ID3D12Device5> createDeviceFromAdapter(Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter, D3D_FEATURE_LEVEL featureLevel);
		static Microsoft::WRL::ComPtr<IDXGIFactory7> createDXGIFactory();
		static Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device5> pDevice, D3D12_DESCRIPTOR_HEAP_TYPE descriptorHeapType, UINT numDescriptors);
		static Microsoft::WRL::ComPtr<IDXGISwapChain4> createSwapChain(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue, HWND hWnd, UINT numBuffers);

		static std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> createRenderTargetViews(
			Microsoft::WRL::ComPtr<ID3D12Device5> device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap,
			Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
			UINT numRTV);

		static std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> createDepthStencilView(
			Microsoft::WRL::ComPtr<ID3D12Device5> device,
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthDescriptorHeap,
			UINT winWidth, UINT winHeight,
			UINT numDSV);
	};
}