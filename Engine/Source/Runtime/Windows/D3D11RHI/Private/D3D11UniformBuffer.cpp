// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11UniformBuffer.cpp: D3D uniform buffer RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "UniformBuffer.h"

/** Describes a uniform buffer in the free pool. */
struct FPooledUniformBuffer
{
	TRefCountPtr<ID3D11Buffer> Buffer;
	uint32 CreatedSize;
	uint32 FrameFreed;
};

/** 
 * Number of size buckets to use for the uniform buffer free pool. 
 * This needs to be enough to cover the valid uniform buffer size range combined with the heuristic used to map sizes to buckets.
 */
const int32 NumPoolBuckets = 17;

/** 
 * Number of frames that a uniform buffer will not be re-used for, after being freed. 
 * This is done as a workaround for what appears to be an AMD driver bug with 11.10 drivers and a 6970 HD,
 * Where reusing a constant buffer with D3D11_MAP_WRITE_DISCARD still in use by the GPU will result in incorrect contents randomly.
 */
const int32 NumSafeFrames = 3;

/** Returns the size in bytes of the bucket that the given size fits into. */
uint32 GetPoolBucketSize(uint32 NumBytes)
{
	return FMath::RoundUpToPowerOfTwo(NumBytes);
}

/** Returns the index of the bucket that the given size fits into. */
uint32 GetPoolBucketIndex(uint32 NumBytes)
{
	return FMath::CeilLogTwo(NumBytes);
}

/** Pool of free uniform buffers, indexed by bucket for constant size search time. */
TArray<FPooledUniformBuffer> UniformBufferPool[NumPoolBuckets];

/** Uniform buffers that have been freed more recently than NumSafeFrames ago. */
TArray<FPooledUniformBuffer> SafeUniformBufferPools[NumSafeFrames][NumPoolBuckets];

/** Does per-frame global updating for the uniform buffer pool. */
void UniformBufferBeginFrame()
{
	int32 NumCleaned = 0;

	SCOPE_CYCLE_COUNTER(STAT_D3D11CleanUniformBufferTime);

	// Clean a limited number of old entries to reduce hitching when leaving a large level
	for (int32 BucketIndex = 0; BucketIndex < NumPoolBuckets; BucketIndex++)
	{
		for (int32 EntryIndex = UniformBufferPool[BucketIndex].Num() - 1; EntryIndex >= 0 && NumCleaned < 10; EntryIndex--)
		{
			FPooledUniformBuffer& PoolEntry = UniformBufferPool[BucketIndex][EntryIndex];

			check(IsValidRef(PoolEntry.Buffer));

			// Clean entries that are unlikely to be reused
			if (GFrameNumberRenderThread - PoolEntry.FrameFreed > 30)
			{
				DEC_DWORD_STAT(STAT_D3D11NumFreeUniformBuffers);
				DEC_MEMORY_STAT_BY(STAT_D3D11FreeUniformBufferMemory, PoolEntry.CreatedSize);
				NumCleaned++;
				UpdateBufferStats(PoolEntry.Buffer, false);
				UniformBufferPool[BucketIndex].RemoveAtSwap(EntryIndex);
			}
		}
	}

	// Index of the bucket that is now old enough to be reused
	const int32 SafeFrameIndex = GFrameNumberRenderThread % NumSafeFrames;

	// Merge the bucket into the free pool array
	for (int32 BucketIndex = 0; BucketIndex < NumPoolBuckets; BucketIndex++)
	{
		int32 LastNum = UniformBufferPool[BucketIndex].Num();
		UniformBufferPool[BucketIndex].Append(SafeUniformBufferPools[SafeFrameIndex][BucketIndex]);
#if DO_CHECK
		while (LastNum < UniformBufferPool[BucketIndex].Num())
		{
			check(IsValidRef(UniformBufferPool[BucketIndex][LastNum].Buffer));
			LastNum++;
		}
#endif
		SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Reset();
	}
}

static bool IsPoolingEnabled()
{
	if (IsRunningRHIInSeparateThread() && IsInRenderingThread() && GRHICommandList.IsRHIThreadActive())
	{
		return false; // we can't currently use pooling if the RHI thread is active. 
	}
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.UniformBufferPooling"));
	int32 CVarValue = CVar->GetValueOnRenderThread();
	return CVarValue != 0;
};

FUniformBufferRHIRef FD3D11DynamicRHI::RHICreateUniformBuffer(const void* Contents,const FRHIUniformBufferLayout& Layout,EUniformBufferUsage Usage)
{
	check(IsInRenderingThread());

	FD3D11UniformBuffer* NewUniformBuffer = nullptr;
	const uint32 NumBytes = Layout.ConstantBufferSize;
	if (NumBytes > 0)
	{
		// Constant buffers must also be 16-byte aligned.
		check(Align(NumBytes,16) == NumBytes);
		check(Align(Contents,16) == Contents);
		check(NumBytes <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);
		check(NumBytes < (1 << NumPoolBuckets));

		SCOPE_CYCLE_COUNTER(STAT_D3D11UpdateUniformBufferTime);
		if (IsPoolingEnabled())
		{
			TRefCountPtr<ID3D11Buffer> UniformBufferResource;
			FRingAllocation RingAllocation;

			if (!RingAllocation.IsValid())
			{
				// Find the appropriate bucket based on size
				const uint32 BucketIndex = GetPoolBucketIndex(NumBytes);
				TArray<FPooledUniformBuffer>& PoolBucket = UniformBufferPool[BucketIndex];

				if (PoolBucket.Num() > 0)
				{
					// Reuse the last entry in this size bucket
					FPooledUniformBuffer FreeBufferEntry = PoolBucket.Pop();
					check(IsValidRef(FreeBufferEntry.Buffer));
					UniformBufferResource = FreeBufferEntry.Buffer;
					checkf(FreeBufferEntry.CreatedSize >= NumBytes, TEXT("%u %u %u %u"), NumBytes, BucketIndex, FreeBufferEntry.CreatedSize, GetPoolBucketSize(NumBytes));
					DEC_DWORD_STAT(STAT_D3D11NumFreeUniformBuffers);
					DEC_MEMORY_STAT_BY(STAT_D3D11FreeUniformBufferMemory, FreeBufferEntry.CreatedSize);
				}

				// Nothing usable was found in the free pool, create a new uniform buffer
				if (!IsValidRef(UniformBufferResource))
				{
					D3D11_BUFFER_DESC Desc;
					// Allocate based on the bucket size, since this uniform buffer will be reused later
					Desc.ByteWidth = GetPoolBucketSize(NumBytes);
					// Use D3D11_USAGE_DYNAMIC, which allows multiple CPU writes for pool reuses
					// This is method of updating is vastly superior to creating a new constant buffer each time with D3D11_USAGE_IMMUTABLE, 
					// Since that inserts the data into the command buffer which causes GPU flushes
					Desc.Usage = D3D11_USAGE_DYNAMIC;
					Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
					Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
					Desc.MiscFlags = 0;
					Desc.StructureByteStride = 0;

					VERIFYD3D11RESULT_EX(Direct3DDevice->CreateBuffer(&Desc, NULL, UniformBufferResource.GetInitReference()), Direct3DDevice);

					UpdateBufferStats(UniformBufferResource, true);
				}

				check(IsValidRef(UniformBufferResource));

				D3D11_MAPPED_SUBRESOURCE MappedSubresource;
				// Discard previous results since we always do a full update
				VERIFYD3D11RESULT_EX(Direct3DDeviceIMContext->Map(UniformBufferResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource), Direct3DDevice);
				check(MappedSubresource.RowPitch >= NumBytes);
				FMemory::Memcpy(MappedSubresource.pData, Contents, NumBytes);
				Direct3DDeviceIMContext->Unmap(UniformBufferResource, 0);
			}

			NewUniformBuffer = new FD3D11UniformBuffer(this, Layout, UniformBufferResource, RingAllocation);
		}
		else
		{
			// No pooling
			D3D11_BUFFER_DESC Desc;
			Desc.ByteWidth = NumBytes;
			Desc.Usage = D3D11_USAGE_IMMUTABLE;
			Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			Desc.CPUAccessFlags = 0;
			Desc.MiscFlags = 0;
			Desc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA ImmutableData;
			ImmutableData.pSysMem = Contents;
			ImmutableData.SysMemPitch = ImmutableData.SysMemSlicePitch = 0;

			TRefCountPtr<ID3D11Buffer> UniformBufferResource;
			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateBuffer(&Desc,&ImmutableData,UniformBufferResource.GetInitReference()), Direct3DDevice);

			NewUniformBuffer = new FD3D11UniformBuffer(this, Layout, UniformBufferResource, FRingAllocation());
		}
	}
	else
	{
		// This uniform buffer contains no constants, only a resource table.
		NewUniformBuffer = new FD3D11UniformBuffer(this, Layout, nullptr, FRingAllocation());
	}

	if (Layout.Resources.Num())
	{
		int32 NumResources = Layout.Resources.Num();
		FRHIResource** InResources = (FRHIResource**)((uint8*)Contents + Layout.ResourceOffset);
		NewUniformBuffer->ResourceTable.Empty(NumResources);
		NewUniformBuffer->ResourceTable.AddZeroed(NumResources);

		checkf(InResources, TEXT("Invalid resources creating uniform buffer for %s [0x%x + %u]."), *Layout.GetDebugName().ToString(), Contents, Layout.ResourceOffset);

		for (int32 i = 0; i < NumResources; ++i)
		{
			checkf(InResources[i], TEXT("Invalid resource entry creating uniform buffer, %s.Resources[%u], ResourceType 0x%x."), *Layout.GetDebugName().ToString(), i, Layout.Resources[i]);
			NewUniformBuffer->ResourceTable[i] = InResources[i];
		}
	}

	return NewUniformBuffer;
}

FD3D11UniformBuffer::~FD3D11UniformBuffer()
{
	// Do not return the allocation to the pool if it is in the dynamic constant buffer!
	if (!RingAllocation.IsValid() && Resource != nullptr)
	{
		check(IsInRenderingThread());
		D3D11_BUFFER_DESC Desc;
		Resource->GetDesc(&Desc);

		// Return this uniform buffer to the free pool
		if (Desc.CPUAccessFlags == D3D11_CPU_ACCESS_WRITE && Desc.Usage == D3D11_USAGE_DYNAMIC)
		{
			check(IsValidRef(Resource));
			FPooledUniformBuffer NewEntry;
			NewEntry.Buffer = Resource;
			NewEntry.FrameFreed = GFrameNumberRenderThread;
			NewEntry.CreatedSize = Desc.ByteWidth;

			// Add to this frame's array of free uniform buffers
			const int32 SafeFrameIndex = (GFrameNumberRenderThread - 1) % NumSafeFrames;
			const uint32 BucketIndex = GetPoolBucketIndex(Desc.ByteWidth);
			int32 LastNum = SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Num();
			check(Desc.ByteWidth <= GetPoolBucketSize(Desc.ByteWidth));
			SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Add(NewEntry);
			INC_DWORD_STAT(STAT_D3D11NumFreeUniformBuffers);
			INC_MEMORY_STAT_BY(STAT_D3D11FreeUniformBufferMemory, Desc.ByteWidth);

			FPlatformMisc::MemoryBarrier(); // check for unwanted concurrency
			check(SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Num() == LastNum + 1);
		}
	}
}


void FD3D11DynamicRHI::ReleasePooledUniformBuffers()
{
	// Free D3D resources in the pool
	// Don't bother updating pool stats, this is only done on exit
	for (int32 BucketIndex = 0; BucketIndex < NumPoolBuckets; BucketIndex++)
	{
		UniformBufferPool[BucketIndex].Empty();
	}
	
	for (int32 SafeFrameIndex = 0; SafeFrameIndex < NumSafeFrames; SafeFrameIndex++)
	{
		for (int32 BucketIndex = 0; BucketIndex < NumPoolBuckets; BucketIndex++)
		{
			SafeUniformBufferPools[SafeFrameIndex][BucketIndex].Empty();
		}
	}
}