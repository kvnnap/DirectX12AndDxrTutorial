#include "RTGraphics.h"

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

RTGraphics::RTGraphics(HWND hWnd)
	: winWidth(), winHeight(), pRTVDescriptorSize(), pCurrentBackBufferIndex(), frameFenceValues{},
	scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)), viewport()
{
	HRESULT hr;

	RECT rect;
	GetClientRect(hWnd, &rect);
	winWidth = rect.right;
	winHeight = rect.bottom;

	// DX12 viewport
	viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(winWidth), static_cast<float>(winHeight));

	// Enable debugging
	DXUtil::enableDebugLayer();

	// Get DX12 compatible hardware device - Adapter contains info about the actual device
	D3D_FEATURE_LEVEL featureLevel;
	auto adapter = DXUtil::getAdapterLatestFeatureLevel(&featureLevel);

	// Create a device supporting ray tracing
	pDevice = DXUtil::createRTDeviceFromAdapter(adapter, featureLevel);

	// Enable debug messages in debug mode
	DXUtil::setupDebugLayer(pDevice);

	// Create command queue
	pCommandQueue = make_unique<CommandQueue>(pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// Create swap chain 
	pSwapChain = DXUtil::createSwapChain(pCommandQueue->getCommandQueue(), hWnd, numBackBuffers);

	// Create descriptor heap for render target view
	pRTVDescriptorHeap = DXUtil::createDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, numBackBuffers);

	// Create render target Views
	pRTVDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto backBuffers = DXUtil::createRenderTargetViews(pDevice, pRTVDescriptorHeap, pSwapChain, std::size(pBackBuffers));
	std::copy(backBuffers.begin(), backBuffers.end(), pBackBuffers);
}

Engine::RTGraphics::~RTGraphics()
{
	pCommandQueue->flush();
}

void Engine::RTGraphics::init()
{
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();

	// Upload Geometry
	DirectX::XMFLOAT3 triangle[3] = {
		DirectX::XMFLOAT3(    0.f,   1.f, 0.f),
		DirectX::XMFLOAT3( 0.866f, -0.5f, 0.f),
		DirectX::XMFLOAT3(-0.866f, -0.5f, 0.f)
	};

	vertexBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, sizeof(triangle), D3D12_RESOURCE_STATE_COPY_DEST);
	wrl::ComPtr<ID3D12Resource> intermediateBuffer = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, sizeof(triangle), D3D12_RESOURCE_STATE_GENERIC_READ);

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = triangle;
	subresourceData.RowPitch = sizeof(triangle);
	subresourceData.SlicePitch = subresourceData.RowPitch;

	auto commandList = pCommandQueue->getCommandList();

	// Upload vertices using intermediary upload heap - using helper method
	UpdateSubresources(
		commandList.Get(),
		vertexBuffer.Get(),
		intermediateBuffer.Get(),
		0, 0, 1, &subresourceData
	);

	// Change vertexBuffer state so that it can be read for acceleration structure construction
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrier);

	blasBuffers = DXUtil::createBottomLevelAS(pDevice, commandList, vertexBuffer, 1u, sizeof(dx::XMFLOAT3));
	wrl::ComPtr<ID3D12Resource> tlasTempBuffer;
	tlasBuffers = DXUtil::createTopLevelAS(pDevice, commandList, blasBuffers.pResult, tlasTempBuffer);

	pCommandQueue->executeCommandList(commandList);
	pCommandQueue->flush();
}

void Engine::RTGraphics::clearBuffer(float red, float green, float blue)
{
	pCurrentCommandList = pCommandQueue->getCommandList();

	// Transition the back buffer from the present state to the render target state
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	pCurrentCommandList->ResourceBarrier(1, &barrier);

	// Clear the RTV with the specified colour
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(pRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), pCurrentBackBufferIndex, pRTVDescriptorSize);
	FLOAT color[] = { red, green, blue, 1.0f };
	pCurrentCommandList->ClearRenderTargetView(rtvDescriptorHandle, color, 0, nullptr);
}

void Engine::RTGraphics::draw(uint64_t timeMs)
{
}

void Engine::RTGraphics::endFrame()
{
	// Change the back buffer from the render target state to the present state
	auto backBuffer = pBackBuffers[pCurrentBackBufferIndex];
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	pCurrentCommandList->ResourceBarrier(1, &barrier);

	// Execute command list
	frameFenceValues[pCurrentBackBufferIndex] = pCommandQueue->executeCommandList(pCurrentCommandList);

	// Release pointer to this command list (Comptr reset is being called here)
	pCurrentCommandList.Reset();

	HRESULT hr;
	GFXTHROWIFFAILED(pSwapChain->Present(1u, 0u));

	// Set current back buffer and Wait for any fence values associated to it
	pCurrentBackBufferIndex = pSwapChain->GetCurrentBackBufferIndex();
	pCommandQueue->waitForFenceValue(frameFenceValues[pCurrentBackBufferIndex]);
}
