// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Buffer.cpp: D3D Common code for buffers.
=============================================================================*/

#include "D3D12RHIPrivate.h"

// Forward declarations are required for the template functions
template void* FD3D12DynamicRHI::LockBuffer<FD3D12VertexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12VertexBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);
template void* FD3D12DynamicRHI::LockBuffer<FD3D12IndexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12IndexBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);
template void* FD3D12DynamicRHI::LockBuffer<FD3D12StructuredBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12StructuredBuffer* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode);

template void FD3D12DynamicRHI::UnlockBuffer<FD3D12VertexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12VertexBuffer* Buffer);
template void FD3D12DynamicRHI::UnlockBuffer<FD3D12IndexBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12IndexBuffer* Buffer);
template void FD3D12DynamicRHI::UnlockBuffer<FD3D12StructuredBuffer>(FRHICommandListImmediate* RHICmdList, FD3D12StructuredBuffer* Buffer);

template FD3D12VertexBuffer* FD3D12Adapter::CreateRHIBuffer<FD3D12VertexBuffer>(FRHICommandListImmediate* RHICmdList,
	const D3D12_RESOURCE_DESC& Desc,
	uint32 Alignment, uint32 Stride, uint32 Size, uint32 InUsage,
	FRHIResourceCreateInfo& CreateInfo,
	bool SkipCreate);

template FD3D12IndexBuffer* FD3D12Adapter::CreateRHIBuffer<FD3D12IndexBuffer>(FRHICommandListImmediate* RHICmdList,
	const D3D12_RESOURCE_DESC& Desc,
	uint32 Alignment, uint32 Stride, uint32 Size, uint32 InUsage,
	FRHIResourceCreateInfo& CreateInfo,
	bool SkipCreate);

template FD3D12StructuredBuffer* FD3D12Adapter::CreateRHIBuffer<FD3D12StructuredBuffer>(FRHICommandListImmediate* RHICmdList,
	const D3D12_RESOURCE_DESC& Desc,
	uint32 Alignment, uint32 Stride, uint32 Size, uint32 InUsage,
	FRHIResourceCreateInfo& CreateInfo,
	bool SkipCreate);

struct FRHICommandUpdateBuffer : public FRHICommand<FRHICommandUpdateBuffer>
{
	FD3D12ResourceLocation Source;
	FD3D12ResourceLocation* Destination;
	uint32 NumBytes;
	uint32 DestinationOffset;

	FORCEINLINE_DEBUGGABLE FRHICommandUpdateBuffer(FD3D12ResourceLocation* InDest, FD3D12ResourceLocation& InSource, uint32 InDestinationOffset, uint32 InNumBytes)
		: Source(nullptr)
		, Destination(InDest)
		, NumBytes(InNumBytes)
		, DestinationOffset(InDestinationOffset)
	{
		FD3D12ResourceLocation::TransferOwnership(Source, InSource);
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		FD3D12DynamicRHI::GetD3DRHI()->UpdateBuffer(Destination->GetResource(), Destination->GetOffsetFromBaseOfResource() + DestinationOffset, Source.GetResource(), Source.GetOffsetFromBaseOfResource(), NumBytes);
	}
};

// This allows us to rename resources from the RenderThread i.e. all the 'hard' work of allocating a new resource
// is done in parallel and this small function is called to switch the resource to point to the correct location
// a the correct time.
template<typename ResourceType>
struct FRHICommandRenameUploadBuffer : public FRHICommand<FRHICommandRenameUploadBuffer<ResourceType>>
{
	ResourceType* Resource;
	FD3D12ResourceLocation NewResource;

	FORCEINLINE_DEBUGGABLE FRHICommandRenameUploadBuffer(ResourceType* InResource, FD3D12Device* Device)
		: Resource(InResource)
		, NewResource(Device) 
	{}

	void Execute(FRHICommandListBase& CmdList)
	{ 
		Resource->Rename(NewResource);
	}
};

void FD3D12Adapter::AllocateBuffer(FD3D12Device* Device,
	const D3D12_RESOURCE_DESC& InDesc,
	uint32 Size,
	uint32 InUsage,
	FRHIResourceCreateInfo& CreateInfo,
	uint32 Alignment,
	FD3D12TransientResource& TransientResource,
	FD3D12ResourceLocation& ResourceLocation)
{
	// Explicitly check that the size is nonzero before allowing CreateBuffer to opaquely fail.
	check(Size > 0);

	const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;

	if (bIsDynamic)
	{
		void* pData = GetUploadHeapAllocator().AllocUploadResource(Size, Alignment, ResourceLocation);
		check(ResourceLocation.GetSize() == Size);

		if (CreateInfo.ResourceArray)
		{
			const void* InitialData = CreateInfo.ResourceArray->GetResourceData();

			check(Size == CreateInfo.ResourceArray->GetResourceDataSize());
			// Handle initial data
			FMemory::Memcpy(pData, InitialData, Size);
		}
	}
	else
	{
		VERIFYD3D12RESULT(Device->GetDefaultBufferAllocator().AllocDefaultResource(InDesc, ResourceLocation, Alignment));
		check(ResourceLocation.GetSize() == Size);
	}
}

// This is a templated function used to create FD3D12VertexBuffers, FD3D12IndexBuffers and FD3D12StructuredBuffers
template<class BufferType>
BufferType* FD3D12Adapter::CreateRHIBuffer(FRHICommandListImmediate* RHICmdList,
	const D3D12_RESOURCE_DESC& InDesc,
	uint32 Alignment,
	uint32 Stride,
	uint32 Size,
	uint32 InUsage,
	FRHIResourceCreateInfo& CreateInfo,
	bool SkipCreate)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12CreateBufferTime);

	const bool bIsDynamic = (InUsage & BUF_AnyDynamic) ? true : false;

	BufferType* BufferOut = CreateLinkedObject<BufferType>([&](FD3D12Device* Device)
	{
		BufferType* NewBuffer = new BufferType(Device, Stride, Size, InUsage);
		NewBuffer->BufferAlignment = Alignment;

		if (!SkipCreate)
		{
			AllocateBuffer(Device, InDesc, Size, InUsage, CreateInfo, Alignment, *NewBuffer, NewBuffer->ResourceLocation);
		}

		return NewBuffer;
	});

	if (CreateInfo.ResourceArray)
	{
		if (bIsDynamic == false && BufferOut->ResourceLocation.IsValid())
		{
			check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

			// Get an upload heap and initialize data
			FD3D12ResourceLocation SrcResourceLoc(BufferOut->GetParentDevice());
			void* pData = SrcResourceLoc.GetParentDevice()->GetDefaultFastAllocator().Allocate<FD3D12ScopeLock>(Size, 4UL, &SrcResourceLoc);
			check(pData);
			FMemory::Memcpy(pData, CreateInfo.ResourceArray->GetResourceData(), Size);

			const auto& pfnUpdateBuffer = [&]()
			{
				BufferType* CurrentBuffer = BufferOut;
				while (CurrentBuffer != nullptr)
				{
					FD3D12Resource* Destination = CurrentBuffer->ResourceLocation.GetResource();
					FD3D12Device* Device = Destination->GetParentDevice();

					FD3D12CommandListHandle& hCommandList = Device->GetDefaultCommandContext().CommandListHandle;
					// Copy from the temporary upload heap to the default resource
					{
						// Writable structured bufferes are sometimes initialized with inital data which means they sometimes need tracking.
						FConditionalScopeResourceBarrier ConditionalScopeResourceBarrier(hCommandList, Destination, D3D12_RESOURCE_STATE_COPY_DEST, 0);

						Device->GetDefaultCommandContext().numCopies++;
						hCommandList.FlushResourceBarriers();
						hCommandList->CopyBufferRegion(
							Destination->GetResource(),
							CurrentBuffer->ResourceLocation.GetOffsetFromBaseOfResource(),
							SrcResourceLoc.GetResource()->GetResource(),
							SrcResourceLoc.GetOffsetFromBaseOfResource(), Size);

						hCommandList.UpdateResidency(Destination);
					}

					CurrentBuffer = CurrentBuffer->GetNextObject();
				}
			};

			//TODO: This should be a deferred op like the buffer lock/unlocks
			// We only need to synchronize when creating default resource buffers (because we need a command list to initialize them)
			if (RHICmdList)
			{
				FScopedRHIThreadStaller StallRHIThread(*RHICmdList);
				pfnUpdateBuffer();
			}
			else
			{
				pfnUpdateBuffer();
			}
		}

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return BufferOut;
}

template<class BufferType>
void* FD3D12DynamicRHI::LockBuffer(FRHICommandListImmediate* RHICmdList, BufferType* Buffer, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12LockBufferTime);

	FD3D12LockedResource& LockedData = Buffer->LockedData;
	check(LockedData.bLocked == false);
	FD3D12Device* Device = GetRHIDevice();
	FD3D12Adapter& Adapter = GetAdapter();

	// Determine whether the buffer is dynamic or not.
	const bool bIsDynamic = (Buffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	void* Data = nullptr;

	if (bIsDynamic)
	{
		check(LockMode == RLM_WriteOnly);

		BufferType* CurrentBuffer = Buffer;

		// Update all of the resources in the LDA chain
		while (CurrentBuffer)
		{
			// Allocate a new resource

			// If on the RenderThread, queue up a command on the RHIThread to rename this buffer at the correct time
			if (ShouldDeferBufferLockOperation(RHICmdList))
			{
				FRHICommandRenameUploadBuffer<BufferType>* Command = new (RHICmdList->AllocCommand<FRHICommandRenameUploadBuffer<BufferType>>()) FRHICommandRenameUploadBuffer<BufferType>(CurrentBuffer, Device);

				Data = Adapter.GetUploadHeapAllocator().AllocUploadResource(Buffer->GetSize(), Buffer->BufferAlignment, Command->NewResource);
			}
			else
			{
				FD3D12ResourceLocation Location(CurrentBuffer->GetParentDevice());
				Data = Adapter.GetUploadHeapAllocator().AllocUploadResource(Buffer->GetSize(), Buffer->BufferAlignment, Location);
				CurrentBuffer->Rename(Location);
			}

			CurrentBuffer = CurrentBuffer->GetNextObject();
		}
	}
	else
	{
		FD3D12Resource* pResource = Buffer->ResourceLocation.GetResource();

		// Locking for read must occur immediately so we can't queue up the operations later.
		if (LockMode == RLM_ReadOnly)
		{
			LockedData.bLockedForReadOnly = true;
			// If the static buffer is being locked for reading, create a staging buffer.
			FD3D12Resource* StagingBuffer = nullptr;

			const GPUNodeMask Node = Device->GetNodeMask();
			VERIFYD3D12RESULT(Adapter.CreateBuffer(D3D12_HEAP_TYPE_READBACK, Node, Node, Offset + Size, &StagingBuffer));

			// Copy the contents of the buffer to the staging buffer.
			{
				const auto& pfnCopyContents = [&]()
				{
					FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();

					FD3D12CommandListHandle& hCommandList = DefaultContext.CommandListHandle;
					FScopeResourceBarrier ScopeResourceBarrierSource(hCommandList, pResource, pResource->GetDefaultResourceState(), D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
					// Don't need to transition upload heaps

					uint64 SubAllocOffset = Buffer->ResourceLocation.GetOffsetFromBaseOfResource();

					DefaultContext.numCopies++;
					hCommandList.FlushResourceBarriers();	// Must flush so the desired state is actually set.
					hCommandList->CopyBufferRegion(
						StagingBuffer->GetResource(),
						0,
						pResource->GetResource(),
						SubAllocOffset + Offset, Size);

					hCommandList.UpdateResidency(StagingBuffer);
					hCommandList.UpdateResidency(pResource);

					DefaultContext.FlushCommands(true);
				};

				if (ShouldDeferBufferLockOperation(RHICmdList))
				{
					// Sync when in the render thread implementation
					check(IsInRHIThread() == false);

					RHICmdList->ImmediateFlush(EImmediateFlushType::FlushRHIThread);
					pfnCopyContents();
				}
				else
				{
					check(IsInRenderingThread() && !GRHIThreadId);
					pfnCopyContents();
				}
			}

			LockedData.ResourceLocation.AsStandAlone(StagingBuffer, Size);
			Data = LockedData.ResourceLocation.GetMappedBaseAddress();
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			Data = Device->GetDefaultFastAllocator().Allocate<FD3D12ScopeLock>(Size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, &LockedData.ResourceLocation);
		}
	}

	LockedData.LockedOffset = Offset;
	LockedData.LockedPitch = Size;
	LockedData.bLocked = true;

	// Return the offset pointer
	check(Data != nullptr);
	return Data;
}

template<class BufferType>
void FD3D12DynamicRHI::UnlockBuffer(FRHICommandListImmediate* RHICmdList, BufferType* Buffer)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D12UnlockBufferTime);

	FD3D12LockedResource& LockedData = Buffer->LockedData;
	check(LockedData.bLocked == true);

	// Determine whether the buffer is dynamic or not.
	const bool bIsDynamic = (Buffer->GetUsage() & BUF_AnyDynamic) ? true : false;

	if (bIsDynamic)
	{
		// If the Buffer is dynamic, its upload heap memory can always stay mapped. Don't do anything.
	}
	else
	{
		if (LockedData.bLockedForReadOnly)
		{
			//Nothing to do, just release the locked data at the end of the function
		}
		else
		{
			// Copy the contents of the temporary memory buffer allocated for writing into the Buffer.
			BufferType* CurrentBuffer = Buffer;

			// Update all of the resources in the LDA chain
			while (CurrentBuffer)
			{
				// If we are on the render thread, queue up the copy on the RHIThread so it happens at the correct time.
				if (ShouldDeferBufferLockOperation(RHICmdList))
				{
					new (RHICmdList->AllocCommand<FRHICommandUpdateBuffer>()) FRHICommandUpdateBuffer(&CurrentBuffer->ResourceLocation, LockedData.ResourceLocation, LockedData.LockedOffset, LockedData.LockedPitch);
				}
				else
				{
					UpdateBuffer(CurrentBuffer->ResourceLocation.GetResource(),
						CurrentBuffer->ResourceLocation.GetOffsetFromBaseOfResource() + LockedData.LockedOffset,
						LockedData.ResourceLocation.GetResource(),
						LockedData.ResourceLocation.GetOffsetFromBaseOfResource(),
						LockedData.LockedPitch);
				}

				CurrentBuffer = CurrentBuffer->GetNextObject();
			}
		}
	}

	LockedData.Reset();
}
