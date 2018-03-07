// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12CommandList.h: Implementation of D3D12 Command List functions
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"

class FD3D12Device;

class FD3D12CommandAllocator : public FNoncopyable
{
public:
	explicit FD3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);
	~FD3D12CommandAllocator();

	// The command allocator is ready to be reset when all command lists have been executed (or discarded) AND the GPU not using it.
	inline bool IsReady() const { return (PendingCommandListCount.GetValue() == 0) && SyncPoint.IsComplete(); }
	inline bool HasValidSyncPoint() const { return SyncPoint.IsValid(); }
	inline void SetSyncPoint(const FD3D12SyncPoint& InSyncPoint) { check(InSyncPoint.IsValid()); SyncPoint = InSyncPoint; }
	inline void Reset() { check(IsReady()); VERIFYD3D12RESULT(CommandAllocator->Reset()); }

	operator ID3D12CommandAllocator*() { return CommandAllocator.GetReference(); }

	// Called to indicate a command list is using this command alloctor
	inline void IncrementPendingCommandLists()
	{
		check(PendingCommandListCount.GetValue() >= 0);
		PendingCommandListCount.Increment();
	}

	// Called to indicate a command list using this allocator has been executed OR discarded (closed with no intention to execute it).
	inline void DecrementPendingCommandLists()
	{
		check(PendingCommandListCount.GetValue() > 0);
		PendingCommandListCount.Decrement();
	}

private:
	void Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType);

private:
	TRefCountPtr<ID3D12CommandAllocator> CommandAllocator;
	FD3D12SyncPoint SyncPoint;	// Indicates when the GPU is finished using the command allocator.
	FThreadSafeCounter PendingCommandListCount;	// The number of command lists using this allocator but haven't been executed yet.
};

class FD3D12CommandListHandle
{
private:

	class FD3D12CommandListData : public FD3D12DeviceChild, public FD3D12SingleNodeGPUObject, public FNoncopyable
	{
	public:

		FD3D12CommandListData(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager);

		~FD3D12CommandListData();

		void Close()
		{
			if (!IsClosed)
			{
				FlushResourceBarriers();
				VERIFYD3D12RESULT(CommandList->Close());

				D3DX12Residency::Close(ResidencySet);
				IsClosed = true;
			}
		}

		// Reset the command list with a specified command allocator and optional initial state.
		// Note: Command lists can be reset immediately after they are submitted for execution.
		void Reset(FD3D12CommandAllocator& CommandAllocator)
		{
			VERIFYD3D12RESULT(CommandList->Reset(CommandAllocator, nullptr));

			CurrentCommandAllocator = &CommandAllocator;
			IsClosed = false;

			// Indicate this command allocator is being used.
			CurrentCommandAllocator->IncrementPendingCommandLists();

			CleanupActiveGenerations();

			// Remove all pendering barriers from the command list
			PendingResourceBarriers.Reset();

			// Empty tracked resource state for this command list
			TrackedResourceState.Empty();

			// If this fails there are too many concurrently open residency sets. Increase the value of MAX_NUM_CONCURRENT_CMD_LISTS
			// in the residency manager. Beware, this will increase the CPU memory usage of every tracked resource.
			D3DX12Residency::Open(ResidencySet);

			// If this fails then some previous resource barriers were never submitted.
			check(ResourceBarrierBatcher.GetBarriers().Num() == 0);

#if DEBUG_RESOURCE_STATES
			ResourceBarriers.Reset();
#endif
		}

		bool IsComplete(uint64 Generation)
		{
			if (Generation >= CurrentGeneration)
			{
				// Have not submitted this generation for execution yet.
				return false;
			}

			check(Generation < CurrentGeneration);
			if (Generation > LastCompleteGeneration)
			{
				FScopeLock Lock(&ActiveGenerationsCS);
				GenerationSyncPointPair GenerationSyncPoint;
				if (ActiveGenerations.Peek(GenerationSyncPoint))
				{
					if (Generation < GenerationSyncPoint.Key)
					{
						// The requested generation is older than the oldest tracked generation, so it must be complete.
						return true;
					}
					else
					{
						if (GenerationSyncPoint.Value.IsComplete())
						{
							// Oldest tracked generation is done so clean the queue and try again.
							CleanupActiveGenerations();
							return IsComplete(Generation);
						}
						else
						{
							// The requested generation is newer than the older track generation but the old one isn't done.
							return false;
						}
					}
				}
			}

			return true;
		}

		void WaitForCompletion(uint64 Generation)
		{
			if (Generation > LastCompleteGeneration)
			{
				CleanupActiveGenerations();
				if (Generation > LastCompleteGeneration)
				{
					FScopeLock Lock(&ActiveGenerationsCS);
					ensureMsgf(Generation < CurrentGeneration, TEXT("You can't wait for an unsubmitted command list to complete.  Kick first!"));
					GenerationSyncPointPair GenerationSyncPoint;
					while (ActiveGenerations.Peek(GenerationSyncPoint) && (Generation > LastCompleteGeneration))
					{
						check(Generation >= GenerationSyncPoint.Key);
						ActiveGenerations.Dequeue(GenerationSyncPoint);

						// Unblock other threads while we wait for the command list to complete
						ActiveGenerationsCS.Unlock();

						GenerationSyncPoint.Value.WaitForCompletion();

						ActiveGenerationsCS.Lock();
						LastCompleteGeneration = FMath::Max(LastCompleteGeneration, GenerationSyncPoint.Key);
					}
				}
			}
		}

		inline void CleanupActiveGenerations()
		{
			FScopeLock Lock(&ActiveGenerationsCS);

			// Cleanup the queue of active command list generations.
			// Only remove them from the queue when the GPU has completed them.
			GenerationSyncPointPair GenerationSyncPoint;
			while (ActiveGenerations.Peek(GenerationSyncPoint) && GenerationSyncPoint.Value.IsComplete())
			{
				// The GPU is done with the work associated with this generation, remove it from the queue.
				ActiveGenerations.Dequeue(GenerationSyncPoint);

				check(GenerationSyncPoint.Key > LastCompleteGeneration);
				LastCompleteGeneration = GenerationSyncPoint.Key;
			}
		}

		void SetSyncPoint(const FD3D12SyncPoint& SyncPoint)
		{
			{
				FScopeLock Lock(&ActiveGenerationsCS);

				// Only valid sync points should be set otherwise we might not wait on the GPU correctly.
				check(SyncPoint.IsValid());

				// Track when this command list generation is completed on the GPU.
				GenerationSyncPointPair CurrentGenerationSyncPoint;
				CurrentGenerationSyncPoint.Key = CurrentGeneration;
				CurrentGenerationSyncPoint.Value = SyncPoint;
				ActiveGenerations.Enqueue(CurrentGenerationSyncPoint);

				// Move to the next generation of the command list.
				CurrentGeneration++;
			}

			// Update the associated command allocator's sync point so it's not reset until the GPU is done with all command lists using it.
			CurrentCommandAllocator->SetSyncPoint(SyncPoint);
		}

		void FlushResourceBarriers()
		{
#if DEBUG_RESOURCE_STATES
			// Keep track of all the resource barriers that have been submitted to the current command list.
			const TArray<D3D12_RESOURCE_BARRIER>& Barriers = ResourceBarrierBatcher.GetBarriers();
			if (Barriers.Num())
			{
				ResourceBarriers.Append(Barriers.GetData(), Barriers.Num());
			}
#endif

			ResourceBarrierBatcher.Flush(CommandList);
		}

		uint32 AddRef() const
		{
			int32 NewValue = NumRefs.Increment();
			check(NewValue > 0);
			return uint32(NewValue);
		}

		uint32 Release() const
		{
			int32 NewValue = NumRefs.Decrement();
			check(NewValue >= 0);
			return uint32(NewValue);
		}

		mutable FThreadSafeCounter				NumRefs;
		FD3D12CommandListManager*				CommandListManager;
		FD3D12CommandContext*					CurrentOwningContext;
		const D3D12_COMMAND_LIST_TYPE			CommandListType;
		TRefCountPtr<ID3D12GraphicsCommandList>	CommandList;		// Raw D3D command list pointer
		FD3D12CommandAllocator*					CurrentCommandAllocator;	// Command allocator currently being used for recording the command list
		uint64									CurrentGeneration;
		uint64									LastCompleteGeneration;
		bool									IsClosed;
		typedef TPair<uint64, FD3D12SyncPoint>	GenerationSyncPointPair;	// Pair of command list generation to a sync point
		TQueue<GenerationSyncPointPair>			ActiveGenerations;	// Queue of active command list generations and their sync points. Used to determine what command lists have been completed on the GPU.
		FCriticalSection						ActiveGenerationsCS;	// While only a single thread can record to a command list at any given time, multiple threads can ask for the state of a given command list. So the associated tracking must be thread-safe.

		// Array of resources who's state needs to be synced between submits.
		TArray<FD3D12PendingResourceBarrier>	PendingResourceBarriers;

		/**
		*	A map of all D3D resources, and their states, that were state transitioned with tracking.
		*/
		class FCommandListResourceState
		{
		private:
			TMap<FD3D12Resource*, CResourceState> ResourceStates;
			void inline ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState);

		public:
			CResourceState& GetResourceState(FD3D12Resource* pResource);

			// Empty the command list's resource state map after the command list is executed
			void Empty();
		};

		FCommandListResourceState TrackedResourceState;

		// Used to track which resources are used on this CL so that they may be made resident when appropriate
		FD3D12ResidencySet* ResidencySet;

		// Batches resource barriers together until it's explicitly flushed
		FD3D12ResourceBarrierBatcher ResourceBarrierBatcher;

#if DEBUG_RESOURCE_STATES
		// Tracks all the resources barriers being issued on this command list in order
		TArray<D3D12_RESOURCE_BARRIER> ResourceBarriers;
#endif
	};

public:

	FD3D12CommandListHandle() : CommandListData(nullptr) {}

	FD3D12CommandListHandle(const FD3D12CommandListHandle& CL) : CommandListData(CL.CommandListData)
	{
		if (CommandListData)
			CommandListData->AddRef();
	}

	virtual ~FD3D12CommandListHandle()
	{
		if (CommandListData && CommandListData->Release() == 0)
		{
			delete CommandListData;
		}
	}
	
	FD3D12CommandListHandle& operator = (const FD3D12CommandListHandle& CL)
	{
		if (this != &CL)
		{
			if (CommandListData && CommandListData->Release() == 0)
			{
				delete CommandListData;
			}

			CommandListData = nullptr;

			if (CL.CommandListData)
			{
				CommandListData = CL.CommandListData;
				CommandListData->AddRef();
			}
		}

		return *this;
	}

	bool operator!() const
	{
		return CommandListData == 0;
	}

	inline friend bool operator==(const FD3D12CommandListHandle& lhs, const FD3D12CommandListHandle& rhs)
	{
		return lhs.CommandListData == rhs.CommandListData;
	}

	inline friend bool operator==(const FD3D12CommandListHandle& lhs, const FD3D12CommandListData* rhs)
	{
		return lhs.CommandListData == rhs;
	}

	inline friend bool operator==(const FD3D12CommandListData* lhs, const FD3D12CommandListHandle& rhs)
	{
		return lhs == rhs.CommandListData;
	}

	inline friend bool operator!=(const FD3D12CommandListHandle& lhs, const FD3D12CommandListData* rhs)
	{
		return lhs.CommandListData != rhs;
	}

	inline friend bool operator!=(const FD3D12CommandListData* lhs, const FD3D12CommandListHandle& rhs)
	{
		return lhs != rhs.CommandListData;
	}

	FORCEINLINE ID3D12GraphicsCommandList* operator->() const
	{
		check(CommandListData && !CommandListData->IsClosed);
	
		return CommandListData->CommandList;
	}

	void Create(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager);

	void Execute(bool WaitForCompletion = false);

	void Close()
	{
		check(CommandListData);
		CommandListData->Close();
	}

	// Reset the command list with a specified command allocator and optional initial state.
	// Note: Command lists can be reset immediately after they are submitted for execution.
	void Reset(FD3D12CommandAllocator& CommandAllocator)
	{
		check(CommandListData);
		CommandListData->Reset(CommandAllocator);
	}

	ID3D12CommandList* CommandList() const
	{
		check(CommandListData);
		return CommandListData->CommandList.GetReference();
	}

	ID3D12GraphicsCommandList* GraphicsCommandList() const
	{
		check(CommandListData && (CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || CommandListData->CommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE));
		return reinterpret_cast<ID3D12GraphicsCommandList*>(CommandListData->CommandList.GetReference());
	}

	uint64 CurrentGeneration() const
	{
		check(CommandListData);
		return CommandListData->CurrentGeneration;
	}

	FD3D12CommandAllocator* CurrentCommandAllocator()
	{
		check(CommandListData);
		return CommandListData->CurrentCommandAllocator;
	}

	void SetSyncPoint(const FD3D12SyncPoint& SyncPoint)
	{
		check(CommandListData);
		CommandListData->SetSyncPoint(SyncPoint);
	}

	bool IsClosed() const
	{
		check(CommandListData);
		return CommandListData->IsClosed;
	}

	bool IsComplete(uint64 Generation) const
	{
		check(CommandListData);
		return CommandListData->IsComplete(Generation);
	}

	void WaitForCompletion(uint64 Generation) const
	{
		check(CommandListData);
		return CommandListData->WaitForCompletion(Generation);
	}

	// Get the state of a resource on this command lists.
	// This is only used for resources that require state tracking.
	CResourceState& GetResourceState(FD3D12Resource* pResource)
	{
		check(CommandListData);
		return CommandListData->TrackedResourceState.GetResourceState(pResource);
	}

	void AddPendingResourceBarrier(FD3D12Resource* Resource, D3D12_RESOURCE_STATES State, uint32 SubResource)
	{
		check(CommandListData);

		FD3D12PendingResourceBarrier PRB = { Resource, State, SubResource };

		CommandListData->PendingResourceBarriers.Add(PRB);
	}

	TArray<FD3D12PendingResourceBarrier>& PendingResourceBarriers()
	{
		check(CommandListData);
		return CommandListData->PendingResourceBarriers;
	}

	// Empty all the resource states being tracked on this command list
	void EmptyTrackedResourceState()
	{
		check(CommandListData);

		CommandListData->TrackedResourceState.Empty();
	}

	void SetCurrentOwningContext(FD3D12CommandContext* context)
	{
		CommandListData->CurrentOwningContext = context;
	}

	FD3D12CommandContext* GetCurrentOwningContext()
	{
		return CommandListData->CurrentOwningContext;
	}

	D3D12_COMMAND_LIST_TYPE GetCommandListType() const
	{
		check(CommandListData);
		return CommandListData->CommandListType;
	}

	FD3D12ResidencySet& GetResidencySet()
	{
		check(CommandListData);
		return *CommandListData->ResidencySet;
	}

	inline void UpdateResidency(FD3D12Resource* Resource)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		Resource->UpdateResidency(*this);
#endif
	}

	inline void UpdateResidency(FD3D12ResidencyHandle* Resource)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		if (D3DX12Residency::IsInitialized(Resource))
		{
			D3DX12Residency::Insert(GetResidencySet(), Resource);
		}
#endif
	}

	inline void UpdateResidency(FD3D12ResidencyHandle** Resources, uint32 Count)
	{
#if ENABLE_RESIDENCY_MANAGEMENT
		for (uint32 i = 0; i < Count; i++)
		{
			UpdateResidency(Resources[i]);
		}
#endif
	}

	// Adds a transition barrier to the barrier batch
	void AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource);

	// Adds a UAV barrier to the barrier batch
	void AddUAVBarrier();

	void AddAliasingBarrier(FD3D12Resource* pResource);

	// Flushes the batched resource barriers to the current command list
	void FlushResourceBarriers()
	{
		check(CommandListData);
		CommandListData->FlushResourceBarriers();
	}

	void LogResourceBarriers()
	{
#if DEBUG_RESOURCE_STATES
		::LogResourceBarriers(CommandListData->ResourceBarriers.Num(), CommandListData->ResourceBarriers.GetData(), CommandList());
#endif
	}

private:

	FD3D12CommandListHandle& operator*()
	{
		return *this;
	}

	FD3D12CommandListData* CommandListData;
};

class FD3D12CLSyncPoint
{
public:

	FD3D12CLSyncPoint() : Generation(0) {}

	FD3D12CLSyncPoint(FD3D12CommandListHandle& CL) : CommandList(CL), Generation(CL.CommandList() ? CL.CurrentGeneration() : 0) {}

	FD3D12CLSyncPoint(const FD3D12CLSyncPoint& SyncPoint) : CommandList(SyncPoint.CommandList), Generation(SyncPoint.Generation) {}

	FD3D12CLSyncPoint& operator = (FD3D12CommandListHandle& CL)
	{
		CommandList = CL;
		Generation = (CL != nullptr) ? CL.CurrentGeneration() : 0;

		return *this;
	}

	FD3D12CLSyncPoint& operator = (const FD3D12CLSyncPoint& SyncPoint)
	{
		CommandList = SyncPoint.CommandList;
		Generation = SyncPoint.Generation;

		return *this;
	}

	bool operator!() const
	{
		return CommandList == 0;
	}

	bool IsOpen() const
	{
		return Generation == CommandList.CurrentGeneration();
	}

	bool IsComplete() const
	{
		return CommandList.IsComplete(Generation);
	}

	void WaitForCompletion() const
	{
		CommandList.WaitForCompletion(Generation);
	}

	uint64 GetGeneration() const
	{
		return Generation;
	}

private:

	friend class FD3D12CommandListManager;

	FD3D12CommandListHandle CommandList;
	uint64                  Generation;
};