// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"

#define DEBUG_FENCES 0

extern int32 GCommandListBatchingMode;

enum ECommandListBatchMode
{
	CLB_NormalBatching = 1,			// Submits work on explicit Flush and at the end of a context container batch
	CLB_AggressiveBatching = 2,		// Submits work on explicit Flush (after Occlusion queries, and before Present) - Least # of submits.
};

enum class CommandListState
{
	kOpen,
	kQueued,
	kFinished
};


struct FD3D12CommandListPayload
{
	FD3D12CommandListPayload() : NumCommandLists(0)
	{
		FMemory::Memzero(CommandLists);
		FMemory::Memzero(ResidencySets);
	}

	void Reset();
	void Append(ID3D12CommandList* CL, FD3D12ResidencySet* Set);

	static const uint32 MaxCommandListsPerPayload = 256;
	ID3D12CommandList* CommandLists[MaxCommandListsPerPayload];
	FD3D12ResidencySet* ResidencySets[MaxCommandListsPerPayload];
	uint32 NumCommandLists;
};

class FD3D12FenceCore : public FD3D12AdapterChild
{
public:
	FD3D12FenceCore(FD3D12Adapter* Parent, uint64 InitialValue);
	~FD3D12FenceCore();

	inline ID3D12Fence* GetFence() const { return Fence.GetReference(); }
	inline HANDLE GetCompletionEvent() const { return hFenceCompleteEvent; }
	inline bool IsAvailable() const { return FenceValueAvailableAt <= Fence->GetCompletedValue(); }

	uint64 FenceValueAvailableAt;
private:
	TRefCountPtr<ID3D12Fence> Fence;
	HANDLE hFenceCompleteEvent;
};

class FD3D12FenceCorePool : public FD3D12AdapterChild
{
public:

	FD3D12FenceCorePool(FD3D12Adapter* Parent) : FD3D12AdapterChild(Parent) {};

	FD3D12FenceCore* ObtainFenceCore();
	void ReleaseFenceCore(FD3D12FenceCore* Fence, uint64 CurrentFenceValue);
	void Destroy();

private:
	FCriticalSection CS;
	TQueue<FD3D12FenceCore*> AvailableFences;
};

// Automatically increments the current fence value after Signal.
class FD3D12Fence : public FRHIComputeFence, public FNoncopyable, public FD3D12AdapterChild
{
public:
	FD3D12Fence(FD3D12Adapter* Parent = nullptr, const FName& Name = L"<unnamed>");
	~FD3D12Fence();

	void CreateFence();
	uint64 Signal(ID3D12CommandQueue* pCommandQueue);
	void GpuWait(ID3D12CommandQueue* pCommandQueue, uint64 FenceValue);
	bool IsFenceComplete(uint64 FenceValue);
	void WaitForFence(uint64 FenceValue);

	uint64 GetCurrentFence() const { return CurrentFence; }
	uint64 GetLastSignaledFence() const { return LastSignaledFence; }
	uint64 GetLastCompletedFence();

	// Might not be the most up to date value but avoids querying the fence.
	uint64 GetCachedLastCompletedFence() const { return LastCompletedFence; };

	void Destroy();

	// NvFlow begin
	inline FD3D12FenceCore* GetFenceCore() const { return FenceCore; }
	// NvFlow end

protected:
	void InternalSignal(ID3D12CommandQueue* pCommandQueue, uint64 FenceToSignal);

protected:
	uint64 CurrentFence;
	uint64 LastSignaledFence;
	uint64 LastCompletedFence;
	FCriticalSection WaitForFenceCS;

	FD3D12FenceCore* FenceCore;
};

// Fence value must be incremented manually. Useful when you need incrementing and signaling to happen at different times.
class FD3D12ManualFence : public FD3D12Fence
{
public:
	explicit FD3D12ManualFence(FD3D12Adapter* Parent = nullptr, const FName& Name = L"<unnamed>")
		: FD3D12Fence(Parent, Name)
	{}

	// Signals the specified fence value.
	uint64 Signal(ID3D12CommandQueue* pCommandQueue, uint64 FenceToSignal);

	// Increments the current fence and returns the previous value.
	inline uint64 IncrementCurrentFence() { return CurrentFence++; }
};

class FD3D12CommandAllocatorManager : public FD3D12DeviceChild
{
public:
	FD3D12CommandAllocatorManager(FD3D12Device* InParent, const D3D12_COMMAND_LIST_TYPE& InType);

	~FD3D12CommandAllocatorManager()
	{
		// Go through all command allocators owned by this manager and delete them.
		for (auto Iter = CommandAllocators.CreateIterator(); Iter; ++Iter)
		{
			FD3D12CommandAllocator* pCommandAllocator = *Iter;
			delete pCommandAllocator;
		}
	}

	FD3D12CommandAllocator* ObtainCommandAllocator();
	void ReleaseCommandAllocator(FD3D12CommandAllocator* CommandAllocator);

private:
	TArray<FD3D12CommandAllocator*> CommandAllocators;		// List of all command allocators owned by this manager
	TQueue<FD3D12CommandAllocator*> CommandAllocatorQueue;	// Queue of available allocators. Note they might still be in use by the GPU.
	FCriticalSection CS;	// Must be thread-safe because multiple threads can obtain/release command allocators
	const D3D12_COMMAND_LIST_TYPE Type;
};

class FD3D12CommandListManager : public FD3D12DeviceChild, public FD3D12SingleNodeGPUObject
{
public:
	FD3D12CommandListManager(FD3D12Device* InParent, D3D12_COMMAND_LIST_TYPE CommandListType);
	virtual ~FD3D12CommandListManager();

	void Create(const TCHAR* Name, uint32 NumCommandLists = 0, uint32 Priority = 0);
	void Destroy();

	inline bool IsReady()
	{
		return D3DCommandQueue.GetReference() != nullptr;
	}

	// This use to also take an optional PSO parameter so that we could pass this directly to Create/Reset command lists,
	// however this was removed as we generally can't actually predict what PSO we'll need until draw due to frequent
	// state changes. We leave PSOs to always be resolved in ApplyState().
	FD3D12CommandListHandle ObtainCommandList(FD3D12CommandAllocator& CommandAllocator);
	void ReleaseCommandList(FD3D12CommandListHandle& hList);

	void ExecuteCommandList(FD3D12CommandListHandle& hList, bool WaitForCompletion = false);
	virtual void ExecuteCommandLists(TArray<FD3D12CommandListHandle>& Lists, bool WaitForCompletion = false);

	uint32 GetResourceBarrierCommandList(FD3D12CommandListHandle& hList, FD3D12CommandListHandle& hResourceBarrierList);

	CommandListState GetCommandListState(const FD3D12CLSyncPoint& hSyncPoint);

	bool IsComplete(const FD3D12CLSyncPoint& hSyncPoint, uint64 FenceOffset = 0);
	void WaitForCompletion(const FD3D12CLSyncPoint& hSyncPoint)
	{
		hSyncPoint.WaitForCompletion();
	}

	inline HRESULT GetTimestampFrequency(uint64* Frequency)
	{
		return D3DCommandQueue->GetTimestampFrequency(Frequency);
	}

	inline ID3D12CommandQueue* GetD3DCommandQueue()
	{
		return D3DCommandQueue.GetReference();
	}

	inline FD3D12Fence& GetFence() { return CommandListFence; }

	void WaitForCommandQueueFlush();

	void ReleaseResourceBarrierCommandListAllocator();

protected:
	// Returns signaled Fence
	uint64 ExecuteAndIncrementFence(FD3D12CommandListPayload& Payload, FD3D12Fence &Fence);
	FD3D12CommandListHandle CreateCommandListHandle(FD3D12CommandAllocator& CommandAllocator);

	TRefCountPtr<ID3D12CommandQueue>		D3DCommandQueue;

	FThreadsafeQueue<FD3D12CommandListHandle> ReadyLists;

	// Command allocators used exclusively for resource barrier command lists.
	FD3D12CommandAllocatorManager ResourceBarrierCommandAllocatorManager;
	FD3D12CommandAllocator* ResourceBarrierCommandAllocator;

	FD3D12Fence CommandListFence;

	D3D12_COMMAND_LIST_TYPE					CommandListType;
	FCriticalSection						ResourceStateCS;
	FCriticalSection						FenceCS;
};
