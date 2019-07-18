#include "DXUtil.h"

#include "../Exception/WindowException.h"
#include "../Exception/DxgiInfoException.h"
#include "../Libraries/d3dx12.h"

#include <DirectXMath.h>
#include "Libraries/d3dx12.h"

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

	auto dxgiFactory = createDXGIFactory();

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
	auto dxgiFactory = createDXGIFactory();

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

wrl::ComPtr<ID3D12DescriptorHeap> Util::DXUtil::createDescriptorHeap(wrl::ComPtr<ID3D12Device5> device, UINT count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
{
	HRESULT hr;
	wrl::ComPtr<ID3D12DescriptorHeap> descriptorHeap;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = count;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	GFXTHROWIFFAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

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

wrl::ComPtr<ID3D12Resource> Util::DXUtil::createCommittedResource(wrl::ComPtr<ID3D12Device5> device, D3D12_HEAP_TYPE heapType, UINT64 size, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags)
{
	wrl::ComPtr<ID3D12Resource> buffer;
	
	HRESULT hr;
	GFXTHROWIFFAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(size, resourceFlags),
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)
	));

	return buffer;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Util::DXUtil::createTextureCommittedResource(Microsoft::WRL::ComPtr<ID3D12Device5> device, D3D12_HEAP_TYPE heapType, UINT64 width, UINT64 height, D3D12_RESOURCE_STATES resourceState, D3D12_RESOURCE_FLAGS resourceFlags, DXGI_FORMAT format)
{
	wrl::ComPtr<ID3D12Resource> buffer;

	// TODO: Check - Do we need mip level 1 for Ray Tracing?
	HRESULT hr;
	GFXTHROWIFFAILED(device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(heapType),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1u, 1u, 1u, 0u, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		resourceState,
		nullptr,
		IID_PPV_ARGS(&buffer)
	));

	return buffer;
}

wrl::ComPtr<ID3D12Resource> Util::DXUtil::uploadDataToDefaultHeap(wrl::ComPtr<ID3D12Device5> pDevice, wrl::ComPtr<ID3D12GraphicsCommandList4> pCommandList, wrl::ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES finalState)
{
	// Upload buffer to gpu
	wrl::ComPtr<ID3D12Resource> defaultResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, dataSize, D3D12_RESOURCE_STATE_COPY_DEST);

	updateDataInDefaultHeap(pDevice, pCommandList, defaultResource, tempResource, ptData, dataSize, D3D12_RESOURCE_STATE_COPY_DEST, finalState);

	return defaultResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Util::DXUtil::uploadTextureDataToDefaultHeap(Microsoft::WRL::ComPtr<ID3D12Device5> device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCommandList, Microsoft::WRL::ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t width, std::size_t height, std::size_t sizePerPixel, DXGI_FORMAT format, D3D12_RESOURCE_STATES finalState)
{
	// create texture
	wrl::ComPtr<ID3D12Resource> texResource = createTextureCommittedResource(device, D3D12_HEAP_TYPE_DEFAULT, width, height, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, format);

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texResource.Get(), 0, 1);
	//const UINT64 uploadBufferSize = width * height * 4;

	// Upload buffer to gpu
	tempResource = DXUtil::createCommittedResource(device, D3D12_HEAP_TYPE_UPLOAD, uploadBufferSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = ptData;
	subresourceData.RowPitch = width * sizePerPixel;
	subresourceData.SlicePitch = subresourceData.RowPitch * height;
	UpdateSubresources(pCommandList.Get(), texResource.Get(), tempResource.Get(), 0, 0, 1, &subresourceData);

	// Change state so that it can be read
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(texResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState));

	return texResource;
}

void Util::DXUtil::updateDataInDefaultHeap(Microsoft::WRL::ComPtr<ID3D12Device5> pDevice, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCommandList, Microsoft::WRL::ComPtr<ID3D12Resource>& resource, Microsoft::WRL::ComPtr<ID3D12Resource>& tempResource, const void* ptData, std::size_t dataSize, D3D12_RESOURCE_STATES previousState, D3D12_RESOURCE_STATES finalState)
{
	// Transition to correct state
	if (previousState != D3D12_RESOURCE_STATE_COPY_DEST) {
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), previousState, D3D12_RESOURCE_STATE_COPY_DEST));
	}

	// Upload buffer to gpu
	tempResource = DXUtil::createCommittedResource(pDevice, D3D12_HEAP_TYPE_UPLOAD, dataSize, D3D12_RESOURCE_STATE_GENERIC_READ);
	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = ptData;
	subresourceData.RowPitch = dataSize;
	subresourceData.SlicePitch = subresourceData.RowPitch;
	UpdateSubresources(pCommandList.Get(), resource.Get(), tempResource.Get(), 0, 0, 1, &subresourceData);

	// Change state so that it can be read
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState));
}


wrl::ComPtr<ID3D12RootSignature> Util::DXUtil::createRootSignature(wrl::ComPtr<ID3D12Device5> pDevice, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& rootSignatureDesc)
{
	// Check which root signature version we support - 1.1 is better than 1.0...
	// Root signature - https://docs.microsoft.com/en-us/windows/desktop/direct3d12/root-signatures-overview
	HRESULT hr;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Serialise the signature
	wrl::ComPtr<ID3DBlob> rootSignatureBlob;
	wrl::ComPtr<ID3DBlob> errorBlob;
	GFXTHROWIFFAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &rootSignatureBlob, &errorBlob));

	// Create the root signature.
	wrl::ComPtr<ID3D12RootSignature> rootSignature;
	GFXTHROWIFFAILED(pDevice->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

	return rootSignature;
}

wrl::ComPtr<ID3D12Device5> Util::DXUtil::createRTDeviceFromAdapter(wrl::ComPtr<IDXGIAdapter4> adapter, D3D_FEATURE_LEVEL featureLevel)
{
	HRESULT hr;
	wrl::ComPtr<ID3D12Device5> pDevice = createDeviceFromAdapter(adapter, featureLevel);

	D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5 = {};
	GFXTHROWIFFAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5)));
	if (features5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
		ThrowException("Ray tracing is not supported on this device");
	}

	return pDevice;
}

DXUtil::AccelerationStructureBuffers Util::DXUtil::createBottomLevelAS(
	Microsoft::WRL::ComPtr<ID3D12Device5> pDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCommandList,
	const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& pVertexBuffers,
	const std::vector<size_t>& vertexCounts,
	UINT vertexSize)
{
	AccelerationStructureBuffers blasBuffers;

	// Create geometry descriptors
	std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> rtGeoDescriptors;
	rtGeoDescriptors.reserve(pVertexBuffers.size());

	for (const auto& pVertexBuffer : pVertexBuffers) {
		// Acceleration Structure setup - describes our geometry (similar to ied)
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDescriptor = {};
		geometryDescriptor.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDescriptor.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		geometryDescriptor.Triangles.VertexBuffer.StartAddress = pVertexBuffer->GetGPUVirtualAddress();
		geometryDescriptor.Triangles.VertexBuffer.StrideInBytes = vertexSize;
		geometryDescriptor.Triangles.VertexCount = vertexCounts[rtGeoDescriptors.size()];
		geometryDescriptor.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;

		rtGeoDescriptors.push_back(geometryDescriptor);
	}

	// Bottom level?
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS rtStructureDescriptor = {};
	rtStructureDescriptor.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	rtStructureDescriptor.NumDescs = rtGeoDescriptors.size();
	rtStructureDescriptor.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	rtStructureDescriptor.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	rtStructureDescriptor.pGeometryDescs = rtGeoDescriptors.data();

	// Query the buffer sizes that we need to allocate
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&rtStructureDescriptor, &prebuildInfo);

	// Create the buffers..
	blasBuffers.pScratch = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	blasBuffers.pResult = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasDesc = {};
	blasDesc.Inputs = rtStructureDescriptor;
	blasDesc.DestAccelerationStructureData = blasBuffers.pResult->GetGPUVirtualAddress();
	blasDesc.ScratchAccelerationStructureData = blasBuffers.pScratch->GetGPUVirtualAddress();
	pCommandList->BuildRaytracingAccelerationStructure(&blasDesc, 0, nullptr);

	return blasBuffers;
}

void Util::DXUtil::buildTopLevelAS(
	Microsoft::WRL::ComPtr<ID3D12Device5> pDevice,
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> pCommandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> blasBuffer,
	Microsoft::WRL::ComPtr<ID3D12Resource>& tlasTempBuffer,
	float rotation,
	bool update,
	DXUtil::AccelerationStructureBuffers& tlasBuffers)
{
	// Query the buffer sizes that we need to allocate
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS rtStructureDescriptor = {};
	rtStructureDescriptor.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	rtStructureDescriptor.NumDescs = 1;
	rtStructureDescriptor.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	rtStructureDescriptor.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
	
	// Query the buffer sizes that we need to allocate
	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo = {};
	pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&rtStructureDescriptor, &prebuildInfo);

	if (update) {
		pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(tlasBuffers.pResult.Get()));
	}
	else {
		// Create the buffers..
		tlasBuffers.pScratch = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		tlasBuffers.pResult = createCommittedResource(pDevice, D3D12_HEAP_TYPE_DEFAULT, prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	}

	D3D12_RAYTRACING_INSTANCE_DESC rtInstanceDesc = {};
	rtInstanceDesc.InstanceID = 0;
	rtInstanceDesc.InstanceContributionToHitGroupIndex = 0;
	rtInstanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
	rtInstanceDesc.AccelerationStructure = blasBuffer->GetGPUVirtualAddress();
	DirectX::XMMATRIX identity = DirectX::XMMatrixRotationZ(rotation);
	memcpy(rtInstanceDesc.Transform, &identity, sizeof(rtInstanceDesc.Transform));
	rtInstanceDesc.InstanceMask = 0xFF;

	// Upload ray tracing instance desc to GPU
	if (update) {
		updateDataInDefaultHeap(pDevice, pCommandList, tlasBuffers.pInstanceDesc, tlasTempBuffer, &rtInstanceDesc, sizeof(rtInstanceDesc), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	else {
		tlasBuffers.pInstanceDesc = uploadDataToDefaultHeap(pDevice, pCommandList, tlasTempBuffer, &rtInstanceDesc, sizeof(rtInstanceDesc), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	}
	rtStructureDescriptor.InstanceDescs = tlasBuffers.pInstanceDesc->GetGPUVirtualAddress();

	// Descriptor for building TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasDesc = {};
	tlasDesc.Inputs = rtStructureDescriptor;
	tlasDesc.DestAccelerationStructureData = tlasBuffers.pResult->GetGPUVirtualAddress();
	tlasDesc.ScratchAccelerationStructureData = tlasBuffers.pScratch->GetGPUVirtualAddress();

	if (update) {
		tlasDesc.Inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
		tlasDesc.SourceAccelerationStructureData = tlasBuffers.pResult->GetGPUVirtualAddress();
	}

	pCommandList->BuildRaytracingAccelerationStructure(&tlasDesc, 0, nullptr);

	// Insert barrier for uav access..
	pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(tlasBuffers.pResult.Get()));
}
