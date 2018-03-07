// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalBufferPools.h"
#include "MetalProfiler.h"

#if METAL_DEBUG_OPTIONS
extern int32 GMetalBufferZeroFill;
#endif

void FMetalQueryBufferPool::Allocate(FMetalQueryResult& NewQuery)
{
    FMetalQueryBuffer* QB = IsValidRef(CurrentBuffer) ? CurrentBuffer.GetReference() : GetCurrentQueryBuffer();
    if(Align(QB->WriteOffset, EQueryBufferAlignment) + EQueryResultMaxSize <= EQueryBufferMaxSize)
    {
		NewQuery.SourceBuffer = QB;
        NewQuery.Offset = Align(QB->WriteOffset, EQueryBufferAlignment);
        FMemory::Memzero((((uint8*)[QB->Buffer contents]) + NewQuery.Offset), EQueryResultMaxSize);
        QB->WriteOffset = Align(QB->WriteOffset, EQueryBufferAlignment) + EQueryResultMaxSize;
	}
	else
	{
		UE_LOG(LogRHI, Warning, TEXT("Performance: Resetting render command encoder as query buffer offset: %d exceeds the maximum allowed: %d."), QB->WriteOffset, EQueryBufferMaxSize);
		Context->ResetRenderCommandEncoder();
		Allocate(NewQuery);
	}
}

FMetalQueryBuffer* FMetalQueryBufferPool::GetCurrentQueryBuffer()
{
    if(!IsValidRef(CurrentBuffer) || CurrentBuffer->WriteOffset > 0)
    {
        id<MTLBuffer> Buffer;
        if(Buffers.Num())
        {
            Buffer = Buffers.Pop();
        }
        else
        {
            Buffer = [Context->GetDevice() newBufferWithLength:EQueryBufferMaxSize options: GetMetalDeviceContext().GetCommandQueue().GetCompatibleResourceOptions(BUFFER_CACHE_MODE | MTLResourceHazardTrackingModeUntracked | MTLResourceStorageModeShared)];
			TRACK_OBJECT(STAT_MetalBufferCount, Buffer);
        }
        CurrentBuffer = new FMetalQueryBuffer(Context, Buffer);
    }

	return CurrentBuffer.GetReference();
}

void FMetalQueryBufferPool::ReleaseQueryBuffer(id<MTLBuffer> Buffer)
{
    Buffers.Add(Buffer);
}

FRingBuffer::FRingBuffer(id<MTLDevice> Device, MTLResourceOptions InOptions, uint32 Size, uint32 InDefaultAlignment)
{
	DefaultAlignment = InDefaultAlignment;
	InitialSize = Size;
	Options = InOptions;
	
	TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe> NewBuffer = MakeShared<FMetalRingBuffer, ESPMode::ThreadSafe>();
	NewBuffer->Buffer = [Device newBufferWithLength:Size options:((MTLResourceOptions)BUFFER_CACHE_MODE|Options)];
	TRACK_OBJECT(STAT_MetalBufferCount, NewBuffer->Buffer);
	NewBuffer->LastRead = 0;
	Buffer = NewBuffer;
	
	Offset = 0;
	LastWritten = 0;
	FMemory::Memzero(FrameSize);
	LastFrameChange = 0;
}

void FRingBuffer::Shrink()
{
	uint32 FrameMax = 0;
	for (uint32 i = 0; i < ARRAY_COUNT(FrameSize); i++)
	{
		FrameMax = FMath::Max(FrameMax, FrameSize[i]);
	}
	
	uint32 NecessarySize = FMath::Max(FrameMax, InitialSize);
	uint32 ThreeQuarterSize = Align((Buffer->Buffer.length / 4) * 3, DefaultAlignment);
	
	if ((GFrameNumberRenderThread - LastFrameChange) >= 120 && NecessarySize < ThreeQuarterSize && NecessarySize < Buffer->Buffer.length)
	{
		UE_LOG(LogMetal, Display, TEXT("Shrinking RingBuffer from %u to %u as max. usage is %u at frame %lld]"), (uint32)Buffer->Buffer.length, ThreeQuarterSize, FrameMax, GFrameNumberRenderThread);
		
		TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe> NewBuffer = MakeShared<FMetalRingBuffer, ESPMode::ThreadSafe>();
		NewBuffer->Buffer = [GetMetalDeviceContext().GetDevice() newBufferWithLength:ThreeQuarterSize options:((MTLResourceOptions)BUFFER_CACHE_MODE|Options)];
		TRACK_OBJECT(STAT_MetalBufferCount, NewBuffer->Buffer);
		NewBuffer->LastRead = ThreeQuarterSize;
		Buffer = NewBuffer;
		
		Offset = 0;
		LastWritten = 0;
		LastFrameChange = GFrameNumberRenderThread;
	}
	
	FrameSize[GFrameNumberRenderThread % ARRAY_COUNT(FrameSize)] = 0;
}

uint32 FRingBuffer::Allocate(uint32 Size, uint32 Alignment)
{
	if (Alignment == 0)
	{
		Alignment = DefaultAlignment;
	}
	
	if(Buffer->LastRead <= Offset)
	{
		// align the offset
		Offset = Align(Offset, Alignment);
		
		// wrap if needed
		if (Offset + Size <= Buffer->Buffer.length)
		{
			// get current location
			uint32 ReturnOffset = Offset;
			Offset += Size;
			
#if METAL_DEBUG_OPTIONS
			if (GMetalBufferZeroFill)
			{
				FMemory::Memset(((uint8*)[Buffer->Buffer contents]) + ReturnOffset, 0x0, Size);
			}
#endif

			return ReturnOffset;
		}
		else // wrap
		{
			Offset = 0;
		}
	}
	
	// align the offset
	Offset = Align(Offset, Alignment);
	if(Offset + Size < Buffer->LastRead)
	{
		// get current location
		uint32 ReturnOffset = Offset;
		Offset += Size;

#if METAL_DEBUG_OPTIONS
		if (GMetalBufferZeroFill)
		{
			FMemory::Memset(((uint8*)[Buffer->Buffer contents]) + ReturnOffset, 0x0, Size);
		}
#endif
		
		return ReturnOffset;
	}
	else
	{
		const uint32 BufferSize = Buffer->Buffer.length;
		uint32 NewBufferSize = AlignArbitrary(BufferSize + Size, Buffer->Buffer.length / 4);
		
		UE_LOG(LogMetal, Verbose, TEXT("Reallocating ring-buffer from %d to %d to avoid wrapping write at offset %d into outstanding buffer region %d at frame %lld]"), BufferSize, NewBufferSize, Offset, Buffer->LastRead, (uint64)GFrameCounter);

		SafeReleaseMetalResource(Buffer->Buffer);
		TSharedPtr<FMetalRingBuffer, ESPMode::ThreadSafe> NewBuffer = MakeShared<FMetalRingBuffer, ESPMode::ThreadSafe>();
		NewBuffer->Buffer = [GetMetalDeviceContext().GetDevice() newBufferWithLength:NewBufferSize options:((MTLResourceOptions)BUFFER_CACHE_MODE|Options)];
		TRACK_OBJECT(STAT_MetalBufferCount, NewBuffer->Buffer);
		NewBuffer->LastRead = NewBufferSize;
		Buffer = NewBuffer;
		
		Offset = 0;

		// get current location
		uint32 ReturnOffset = Offset;
		Offset += Size;

#if METAL_DEBUG_OPTIONS
		if (GMetalBufferZeroFill)
		{
			FMemory::Memset(((uint8*)[Buffer->Buffer contents]) + ReturnOffset, 0x0, Size);
		}
#endif

		return ReturnOffset;
	}
}
