// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalIndexBuffer.cpp: Metal Index buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "Containers/ResourceArray.h"
#include "RenderUtils.h"

/** Constructor */
FMetalIndexBuffer::FMetalIndexBuffer(uint32 InStride, uint32 InSize, uint32 InUsage)
	: FRHIIndexBuffer(InStride, InSize, InUsage)
	, Buffer(nil)
	, LinearTexture(nil)
	, LockOffset(0)
	, LockSize(0)
	, IndexType((InStride == 2) ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (InUsage & (BUF_UnorderedAccess|BUF_ShaderResource)))
	{
		InSize = Align(InSize, 1024);
	}
	
	Alloc(InSize);
}

FMetalIndexBuffer::~FMetalIndexBuffer()
{
	if (LinearTexture)
	{
		SafeReleaseMetalObject(LinearTexture);
		LinearTexture = nil;
	}

	INC_DWORD_STAT_BY(STAT_MetalIndexMemFreed, GetSize());
	SafeReleasePooledBuffer(Buffer);
}

void FMetalIndexBuffer::Alloc(uint32 InSize)
{
	check(!Buffer);

	MTLStorageMode Mode = BUFFER_STORAGE_MODE;
	Buffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), InSize, Mode));
	INC_DWORD_STAT_BY(STAT_MetalIndexMemAlloc, InSize);
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (GetUsage() & (BUF_UnorderedAccess|BUF_ShaderResource)))
	{
		check(!LinearTexture);

		MTLPixelFormat MTLFormat = IndexType == MTLIndexTypeUInt32 ? MTLPixelFormatR32Uint : MTLPixelFormatR16Uint;
		uint32 NumElements = (Buffer.length / GetStride());
		uint32 SizeX = NumElements;
		uint32 SizeY = 1;
		if (NumElements > GMaxTextureDimensions)
		{
			uint32 Dimension = GMaxTextureDimensions;
			while((NumElements % Dimension) != 0)
			{
				check(Dimension >= 1);
				Dimension = (Dimension >> 1);
			}
			SizeX = Dimension;
			SizeY = NumElements / Dimension;
			check(SizeX <= GMaxTextureDimensions);
			check(SizeY <= GMaxTextureDimensions);
		}
		
		MTLTextureDescriptor* Desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLFormat width:SizeX height:SizeY mipmapped:NO];
		
		id<FMTLBufferExtensions> BufferWithExtraAPI = (id<FMTLBufferExtensions>)Buffer;
		Desc.resourceOptions = (BufferWithExtraAPI.storageMode << MTLResourceStorageModeShift) | (BufferWithExtraAPI.cpuCacheMode << MTLResourceCPUCacheModeShift);
		Desc.storageMode = BufferWithExtraAPI.storageMode;
		Desc.cpuCacheMode = BufferWithExtraAPI.cpuCacheMode;
		if (GetUsage() & BUF_ShaderResource)
		{
			Desc.usage |= MTLTextureUsageShaderRead;
		}
		if (GetUsage() & BUF_UnorderedAccess)
		{
			Desc.usage |= MTLTextureUsageShaderWrite;
		}
		
		check(((SizeX*GetStride()) % 1024) == 0);
		
		LinearTexture = [BufferWithExtraAPI newTextureWithDescriptor:Desc offset: 0 bytesPerRow: SizeX*GetStride()];
		check(LinearTexture);
	}
}

void* FMetalIndexBuffer::Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size)
{
	check(LockOffset == 0 && LockSize == 0);
	
	// In order to properly synchronise the buffer access, when a dynamic buffer is locked for writing, discard the old buffer & create a new one. This prevents writing to a buffer while it is being read by the GPU & thus causing corruption. This matches the logic of other RHIs.
	if ((GetUsage() & BUFFER_DYNAMIC_REALLOC) && LockMode == RLM_WriteOnly)
	{
		uint32 InSize = Buffer.length;
		INC_MEMORY_STAT_BY(STAT_MetalIndexMemFreed, InSize);		
		GetMetalDeviceContext().ReleasePooledBuffer(Buffer);
		Buffer = nil;
		if (LinearTexture)
		{
			SafeReleaseMetalObject(LinearTexture);
			LinearTexture = nil;
		}
		Alloc(InSize);
	}
	
	if(LockMode != RLM_ReadOnly)
	{
		LockOffset = Offset;
		LockSize = Size;
	}
#if PLATFORM_MAC
	else if(Buffer.storageMode == MTLStorageModeManaged)
	{
		SCOPE_CYCLE_COUNTER(STAT_MetalBufferPageOffTime);
		
		// Synchronise the buffer with the CPU
		GetMetalDeviceContext().SynchroniseResource(Buffer);
		
		//kick the current command buffer.
		GetMetalDeviceContext().SubmitCommandBufferAndWait();
	}
#endif
	
	return ((uint8*)[Buffer contents]) + Offset;
}

void FMetalIndexBuffer::Unlock()
{
#if PLATFORM_MAC
	if(LockSize && Buffer.storageMode == MTLStorageModeManaged)
	{
		[Buffer didModifyRange:NSMakeRange(LockOffset, LockSize)];
	}
#endif
	LockOffset = 0;
	LockSize = 0;
}

FIndexBufferRHIRef FMetalDynamicRHI::RHICreateIndexBuffer(uint32 Stride,uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
	// make the RHI object, which will allocate memory
	FMetalIndexBuffer* IndexBuffer = new FMetalIndexBuffer(Stride, Size, InUsage);
	
	if (CreateInfo.ResourceArray)
	{
		check(Size == CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockIndexBuffer(IndexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return IndexBuffer;
	}
}

void* FMetalDynamicRHI::RHILockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	@autoreleasepool {
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	return (uint8*)IndexBuffer->Lock(LockMode, Offset, Size);
	}
}

void FMetalDynamicRHI::RHIUnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	@autoreleasepool {
	FMetalIndexBuffer* IndexBuffer = ResourceCast(IndexBufferRHI);

	IndexBuffer->Unlock();
	}
}

FIndexBufferRHIRef FMetalDynamicRHI::CreateIndexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Stride, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
	return GDynamicRHI->RHICreateIndexBuffer(Stride, Size, InUsage, CreateInfo);
	}
}

