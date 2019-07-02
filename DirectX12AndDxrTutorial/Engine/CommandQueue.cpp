#include "CommandQueue.h"

#include "../Exception/WindowException.h"
#include <chrono>

using namespace Engine;
namespace wrl = Microsoft::WRL;

CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device5> pDevice, D3D12_COMMAND_LIST_TYPE listType)
	: listType (listType), pDevice (pDevice), fenceValue(), fenceEvent(NULL)
{
	HRESULT hr;
	D3D12_COMMAND_QUEUE_DESC cQueueDesc = {};
	cQueueDesc.Type = listType;
	cQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	GFXTHROWIFFAILED(pDevice->CreateCommandQueue(&cQueueDesc, IID_PPV_ARGS(&pCommandQueue)));
	GFXTHROWIFFAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence)));

	fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	if (fenceEvent == NULL) {
		ThrowException("Failed to create windows event");
	}
}

Engine::CommandQueue::~CommandQueue()
{
	if (fenceEvent != NULL) {
		::CloseHandle(fenceEvent);
	}
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> Engine::CommandQueue::getCommandQueue() const
{
	return pCommandQueue;
}

wrl::ComPtr<ID3D12GraphicsCommandList> Engine::CommandQueue::getCommandList()
{
	HRESULT hr;

	wrl::ComPtr<ID3D12CommandAllocator> commandAllocator;
	wrl::ComPtr<ID3D12GraphicsCommandList> commandList;

	// Grab a command allocator
	if (!commandAllocatorQueue.empty() && isFenceComplete(commandAllocatorQueue.front().fenceValue)) {
		commandAllocator = commandAllocatorQueue.front().commandAllocator;
		commandAllocatorQueue.pop();

		GFXTHROWIFFAILED(commandAllocator->Reset());
	}
	else {
		commandAllocator = createCommandAllocator();
	}

	// Create list with allocator
	if (commandListQueue.empty()) {
		commandList = createCommandList(commandAllocator);
	}
	else {
		commandList = commandListQueue.front();
		commandListQueue.pop();

		commandList->Reset(commandAllocator.Get(), nullptr);
	}

	GFXTHROWIFFAILED(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator.Get()));

	return commandList;
}

std::uint64_t Engine::CommandQueue::executeCommandList(wrl::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	HRESULT hr;
	GFXTHROWIFFAILED(commandList->Close());

	// Get command allocator from list
	ID3D12CommandAllocator* pCommandAllocator;
	UINT pointerSize = sizeof(pCommandAllocator);
	commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &pointerSize, &pCommandAllocator);

	ID3D12CommandList* const commandLists[] = {
		commandList.Get()
	};

	pCommandQueue->ExecuteCommandLists(std::size(commandLists), commandLists);

	const uint64_t fV = signal();

	commandAllocatorQueue.emplace(CommandAllocatorEntry{ fV, pCommandAllocator });
	commandListQueue.push(commandList);

	pCommandAllocator->Release();

	return fV;
}

std::uint64_t Engine::CommandQueue::signal()
{
	HRESULT hr;
	GFXTHROWIFFAILED(pCommandQueue->Signal(pFence.Get(), ++fenceValue));
	return fenceValue;
}

bool Engine::CommandQueue::isFenceComplete(std::uint64_t fenceValue)
{
	return pFence->GetCompletedValue() >= fenceValue;
}

void Engine::CommandQueue::waitForFenceValue(std::uint64_t fenceValue)
{
	if (!isFenceComplete(fenceValue)) {
		pFence->SetEventOnCompletion(fenceValue, fenceEvent);
		waitForEvent(fenceEvent);
	}
}

void Engine::CommandQueue::flush()
{
	auto fV = signal();
	waitForFenceValue(fV);
}

void Engine::CommandQueue::waitForEvent(HANDLE handleEvent)
{
	using namespace std::chrono;
	milliseconds d = milliseconds::max();

	::WaitForSingleObject(handleEvent, d.count());
}

wrl::ComPtr<ID3D12CommandAllocator> Engine::CommandQueue::createCommandAllocator()
{
	HRESULT hr;

	wrl::ComPtr<ID3D12CommandAllocator> commandAllocator;
	GFXTHROWIFFAILED(pDevice->CreateCommandAllocator(listType, IID_PPV_ARGS(&commandAllocator)));
	return commandAllocator;
}

wrl::ComPtr<ID3D12GraphicsCommandList> Engine::CommandQueue::createCommandList(wrl::ComPtr<ID3D12CommandAllocator> commandAllocator)
{
	HRESULT hr;

	wrl::ComPtr<ID3D12GraphicsCommandList> commandList;
	GFXTHROWIFFAILED(pDevice->CreateCommandList(0, listType, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	return commandList;
}