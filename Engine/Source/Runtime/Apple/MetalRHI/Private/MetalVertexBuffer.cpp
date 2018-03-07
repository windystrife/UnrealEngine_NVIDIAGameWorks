// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalVertexBuffer.cpp: Metal vertex buffer RHI implementation.
=============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"
#include "Containers/ResourceArray.h"
#include "RenderUtils.h"

@implementation FMetalBufferData

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalBufferData)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		self->Data = nullptr;
		self->Len = 0;
	}
	return Self;
}
-(instancetype)initWithSize:(uint32)InSize
{
	id Self = [super init];
	if (Self)
	{
		self->Data = (uint8*)FMemory::Malloc(InSize);
		self->Len = InSize;
		check(self->Data);
	}
	return Self;
}
-(instancetype)initWithBytes:(void const*)InData length:(uint32)InSize
{
	id Self = [super init];
	if (Self)
	{
		self->Data = (uint8*)FMemory::Malloc(InSize);
		self->Len = InSize;
		check(self->Data);
		FMemory::Memcpy(self->Data, InData, InSize);
	}
	return Self;
}
-(void)dealloc
{
	check(self->Data);
	FMemory::Free(self->Data);
	self->Data = nullptr;
	self->Len = 0;
	[super dealloc];
}
@end


FMetalVertexBuffer::FMetalVertexBuffer(uint32 InSize, uint32 InUsage)
	: FRHIVertexBuffer(InSize, InUsage)
	, Buffer(nil)
	, Data(nil)
	, LockOffset(0)
	, LockSize(0)
	, ZeroStrideElementSize((InUsage & BUF_ZeroStride) ? InSize : 0)
{
	checkf(InSize <= 256 * 1024 * 1024, TEXT("Metal doesn't support buffers > 256 MB"));
	
	INC_DWORD_STAT_BY(STAT_MetalVertexMemAlloc, InSize);

	// Anything less than the buffer page size - currently 4Kb - is better off going through the set*Bytes API if available.
	// These can't be used for shader resources or UAVs if we want to use the 'Linear Texture' code path - this is presently disabled so don't consider it
	if (!(InUsage & (BUF_UnorderedAccess /*|BUF_ShaderResource*/ )) && InSize < MetalBufferPageSize && (PLATFORM_MAC || (InSize < 512)))
	{
		Data = [[FMetalBufferData alloc] initWithSize:InSize];
	}
	else
	{
		uint32 Size = InSize;
		if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (InUsage & (BUF_UnorderedAccess|BUF_ShaderResource)))
		{
			Size = Align(InSize, 1024);
		}
		
        if ((InUsage & BUF_UnorderedAccess) && ((InSize - Size) < 512))
		{
			// Padding for write flushing when not using linear texture bindings for buffers
			Size = Align(Size + 512, 1024);
		}
		
		Alloc(Size);
	}
}

FMetalVertexBuffer::~FMetalVertexBuffer()
{
	INC_DWORD_STAT_BY(STAT_MetalVertexMemFreed, GetSize());

	for (TPair<EPixelFormat, id<MTLTexture>> Pair : LinearTextures)
	{
		SafeReleaseMetalObject(Pair.Value);
	}
	LinearTextures.Empty();
	
	if (Buffer)
	{
		DEC_MEMORY_STAT_BY(STAT_MetalWastedPooledBufferMem, Buffer.length - GetSize());
		if(!(GetUsage() & BUF_ZeroStride))
		{
			SafeReleasePooledBuffer(Buffer);
		}
		else
		{
			SafeReleaseMetalResource(Buffer);
		}
	}
	if (Data)
	{
		SafeReleaseMetalObject(Data);
	}
}
	
void FMetalVertexBuffer::Alloc(uint32 InSize)
{
	check(!Buffer);

	// Zero-stride buffers must be separate in order to wrap appropriately
	if(!(GetUsage() & BUF_ZeroStride))
	{
		FMetalPooledBufferArgs Args(GetMetalDeviceContext().GetDevice(), InSize, BUFFER_STORAGE_MODE);
		Buffer = GetMetalDeviceContext().CreatePooledBuffer(Args);
	}
	else
	{
		check(!(GetUsage() & BUF_UnorderedAccess));
		Buffer = [GetMetalDeviceContext().GetDevice() newBufferWithLength:InSize options:GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions(BUFFER_CACHE_MODE|MTLResourceHazardTrackingModeUntracked|BUFFER_MANAGED_MEM)];
		TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
	}
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (GetUsage() & (BUF_UnorderedAccess|BUF_ShaderResource)))
	{
		for (TPair<EPixelFormat, id<MTLTexture>>& Pair : LinearTextures)
		{
			SafeReleaseMetalObject(Pair.Value);
			Pair.Value = nil;
			
			Pair.Value = AllocLinearTexture(Pair.Key);
			check(Pair.Value);
		}
	}
}

id<MTLTexture> FMetalVertexBuffer::AllocLinearTexture(EPixelFormat Format)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (GetUsage() & (BUF_UnorderedAccess|BUF_ShaderResource)))
	{
		MTLPixelFormat MTLFormat = (MTLPixelFormat)GPixelFormats[Format].PlatformFormat;
		uint32 Stride = GPixelFormats[Format].BlockBytes;
		
		uint32 NumElements = (Buffer.length / Stride);
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
		
		check(((SizeX*Stride) % 1024) == 0);
		
		id<MTLTexture> Texture = [BufferWithExtraAPI newTextureWithDescriptor:Desc offset: 0 bytesPerRow: SizeX*Stride];
		check(Texture);
		
		return Texture;
	}
	else
	{
		return nil;
	}
}

id<MTLTexture> FMetalVertexBuffer::GetLinearTexture(EPixelFormat Format)
{
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesLinearTextures) && (GetUsage() & (BUF_UnorderedAccess|BUF_ShaderResource)))
	{
		id<MTLTexture> Texture = LinearTextures.FindRef(Format);
		if (!Texture)
		{
			Texture = AllocLinearTexture(Format);
			check(Texture);
			LinearTextures.Add(Format, Texture);
		}
		return Texture;
	}
	else
	{
		return nil;
	}
}

void* FMetalVertexBuffer::Lock(EResourceLockMode LockMode, uint32 Offset, uint32 Size)
{
	check(LockSize == 0 && LockOffset == 0);
	
	if (Data)
	{
		return ((uint8*)Data->Data) + Offset;
	}
	
	// In order to properly synchronise the buffer access, when a dynamic buffer is locked for writing, discard the old buffer & create a new one. This prevents writing to a buffer while it is being read by the GPU & thus causing corruption. This matches the logic of other RHIs.
	if ((GetUsage() & BUFFER_DYNAMIC_REALLOC) && LockMode == RLM_WriteOnly)
	{
		INC_MEMORY_STAT_BY(STAT_MetalVertexMemAlloc, GetSize());
		INC_MEMORY_STAT_BY(STAT_MetalVertexMemFreed, GetSize());
		
		uint32 InSize = Buffer.length;
		if(!(GetUsage() & BUF_ZeroStride))
		{
			GetMetalDeviceContext().ReleasePooledBuffer(Buffer);
		}
		else
		{
			GetMetalDeviceContext().ReleaseResource(Buffer);
		}
		Buffer = nil;
		Alloc(InSize);
	}
	
	if(LockMode != RLM_ReadOnly)
	{
		LockSize = Size;
		LockOffset = Offset;
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

void FMetalVertexBuffer::Unlock()
{
	if (!Data)
	{
#if PLATFORM_MAC
		if(LockSize && Buffer.storageMode == MTLStorageModeManaged)
		{
			[Buffer didModifyRange:NSMakeRange(LockOffset, LockSize)];
		}
#endif
	}
	LockSize = 0;
	LockOffset = 0;
}

FVertexBufferRHIRef FMetalDynamicRHI::RHICreateVertexBuffer(uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
	// make the RHI object, which will allocate memory
	FMetalVertexBuffer* VertexBuffer = new FMetalVertexBuffer(Size, InUsage);

	if (CreateInfo.ResourceArray)
	{
		check(Size >= CreateInfo.ResourceArray->GetResourceDataSize());

		// make a buffer usable by CPU
		void* Buffer = RHILockVertexBuffer(VertexBuffer, 0, Size, RLM_WriteOnly);

		// copy the contents of the given data into the buffer
		FMemory::Memcpy(Buffer, CreateInfo.ResourceArray->GetResourceData(), Size);

		RHIUnlockVertexBuffer(VertexBuffer);

		// Discard the resource array's contents.
		CreateInfo.ResourceArray->Discard();
	}

	return VertexBuffer;
	}
}

void* FMetalDynamicRHI::RHILockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI, uint32 Offset, uint32 Size, EResourceLockMode LockMode)
{
	@autoreleasepool {
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	// default to vertex buffer memory
	return (uint8*)VertexBuffer->Lock(LockMode, Offset, Size);
	}
}

void FMetalDynamicRHI::RHIUnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	@autoreleasepool {
	FMetalVertexBuffer* VertexBuffer = ResourceCast(VertexBufferRHI);

	VertexBuffer->Unlock();
	}
}

void FMetalDynamicRHI::RHICopyVertexBuffer(FVertexBufferRHIParamRef SourceBufferRHI,FVertexBufferRHIParamRef DestBufferRHI)
{
	NOT_SUPPORTED("RHICopyVertexBuffer");
}

FVertexBufferRHIRef FMetalDynamicRHI::CreateVertexBuffer_RenderThread(class FRHICommandListImmediate& RHICmdList, uint32 Size, uint32 InUsage, FRHIResourceCreateInfo& CreateInfo)
{
	@autoreleasepool {
	return GDynamicRHI->RHICreateVertexBuffer(Size, InUsage, CreateInfo);
	}
}
