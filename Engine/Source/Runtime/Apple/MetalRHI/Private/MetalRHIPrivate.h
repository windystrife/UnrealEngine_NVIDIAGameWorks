// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIPrivate.h: Private Metal RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"

// Requirement for vertex buffer offset field
const uint32 BufferOffsetAlignment = 256;

// The buffer page size that can be uploaded in a set*Bytes call
const uint32 MetalBufferPageSize = 4096;

#define BUFFER_CACHE_MODE MTLResourceCPUCacheModeDefaultCache

#if PLATFORM_MAC
#define BUFFER_MANAGED_MEM MTLResourceStorageModeManaged
#define BUFFER_STORAGE_MODE MTLStorageModeManaged
#define BUFFER_RESOURCE_STORAGE_MANAGED MTLResourceStorageModeManaged
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 31;
#else
#define BUFFER_MANAGED_MEM 0
#define BUFFER_STORAGE_MODE MTLStorageModeShared
#define BUFFER_RESOURCE_STORAGE_MANAGED MTLResourceStorageModeShared
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 30;
#endif

#ifndef METAL_STATISTICS
#define METAL_STATISTICS 0
#endif

// Unavailable on iOS, but dealing with this clutters the code.
enum EMTLTextureType
{
	EMTLTextureTypeCubeArray = 6
};

#define METAL_SUPPORTS_HEAPS !PLATFORM_MAC
// This is the right VERSION check, see Availability.h in the SDK
#define METAL_SUPPORTS_INDIRECT_ARGUMENT_BUFFERS (!PLATFORM_MAC && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000) && (__clang_major__ >= 9)
#define METAL_SUPPORTS_CAPTURE_MANAGER (PLATFORM_MAC && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300) || (!PLATFORM_MAC && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000) && (__clang_major__ >= 9)
#define METAL_SUPPORTS_TILE_SHADERS (!PLATFORM_MAC && !PLATFORM_TVOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000) && (__clang_major__ >= 9)

// In addition to compile-time SDK checks we also need a way to check if these are available on runtime
extern bool GMetalSupportsIndirectArgumentBuffers;
extern bool GMetalSupportsCaptureManager;
extern bool GMetalSupportsTileShaders;
extern bool GMetalSupportsStoreActionOptions;
extern bool GMetalSupportsDepthClipMode;
extern bool GMetalCommandBufferHasStartEndTimeAPI;

#define METAL_DEBUG_OPTIONS !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if METAL_DEBUG_OPTIONS
#define METAL_DEBUG_OPTION(Code) Code
#else
#define METAL_DEBUG_OPTION(Code)
#endif

#define SHOULD_TRACK_OBJECTS (UE_BUILD_DEBUG)

/** Set to 1 to enable GPU events in Xcode frame debugger */
#define ENABLE_METAL_GPUEVENTS	(UE_BUILD_DEBUG | UE_BUILD_DEVELOPMENT)
#define ENABLE_METAL_GPUPROFILE	(ENABLE_METAL_GPUEVENTS & 1)

#define UNREAL_TO_METAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)
#define METAL_TO_UNREAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)

#define METAL_NEW_NONNULL_DECL (__clang_major__ >= 9)

// Dependencies
#include "MetalRHI.h"
#include "RHI.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// Access the internal context for the device-owning DynamicRHI object
FMetalDeviceContext& GetMetalDeviceContext();

// Safely release a metal object, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalObject(id Object);

// Safely release a metal resource, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalResource(id<MTLResource> Object);

// Safely release a pooled buffer, correctly handling the case where the RHI has been destructed first
void SafeReleasePooledBuffer(id<MTLBuffer> Buffer);

// Safely release a fence, correctly handling cases where fences aren't supported or the debug implementation is used.
void SafeReleaseMetalFence(id Object);

// Access the underlying surface object from any kind of texture
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture);

#define NOT_SUPPORTED(Func) UE_LOG(LogMetal, Fatal, TEXT("'%s' is not supported"), L##Func);

#if !METAL_SUPPORTS_HEAPS
#define MTLResourceHazardTrackingModeUntracked 0
#endif

@protocol FMTLBufferExtensions <MTLBuffer>  // extend MTLBuffer.  This overrides the availability declared in MTLBuffer.
- (id <MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor*)descriptor offset:(NSUInteger)offset bytesPerRow:(NSUInteger)bytesPerRow;
@end

#if SHOULD_TRACK_OBJECTS
void TrackMetalObject(NSObject* Object);
void UntrackMetalObject(NSObject* Object);
#define TRACK_OBJECT(Stat, Obj) INC_DWORD_STAT(Stat); TrackMetalObject(Obj)
#define UNTRACK_OBJECT(Stat, Obj) DEC_DWORD_STAT(Stat); UntrackMetalObject(Obj)
#else
#define TRACK_OBJECT(Stat, Obj) INC_DWORD_STAT(Stat)
#define UNTRACK_OBJECT(Stat, Obj) DEC_DWORD_STAT(Stat)
#endif

FORCEINLINE MTLIndexType GetMetalIndexType(EMetalIndexType IndexType)
{
	switch (IndexType)
	{
		case EMetalIndexType_UInt16: return MTLIndexTypeUInt16;
		case EMetalIndexType_UInt32: return MTLIndexTypeUInt32;
		case EMetalIndexType_None:
		default:
		{
			UE_LOG(LogMetal, Fatal, TEXT("There is not equivalent MTLIndexType for EMetalIndexType_None"));
			return MTLIndexTypeUInt16;
		}
	}
}

FORCEINLINE EMetalIndexType GetRHIMetalIndexType(MTLIndexType IndexType)
{
	switch (IndexType)
	{
		case MTLIndexTypeUInt16: return EMetalIndexType_UInt16;
		case MTLIndexTypeUInt32: return EMetalIndexType_UInt32;
		default: return EMetalIndexType_None;
	}
}

FORCEINLINE int32 GetMetalCubeFace(ECubeFace Face)
{
	// According to Metal docs these should match now: https://developer.apple.com/library/prerelease/ios/documentation/Metal/Reference/MTLTexture_Ref/index.html#//apple_ref/c/tdef/MTLTextureType
	switch (Face)
	{
		case CubeFace_PosX:;
		default:			return 0;
		case CubeFace_NegX:	return 1;
		case CubeFace_PosY:	return 2;
		case CubeFace_NegY:	return 3;
		case CubeFace_PosZ:	return 4;
		case CubeFace_NegZ:	return 5;
	}
}

FORCEINLINE MTLLoadAction GetMetalRTLoadAction(ERenderTargetLoadAction LoadAction)
{
	switch(LoadAction)
	{
		case ERenderTargetLoadAction::ENoAction: return MTLLoadActionDontCare;
		case ERenderTargetLoadAction::ELoad: return MTLLoadActionLoad;
		case ERenderTargetLoadAction::EClear: return MTLLoadActionClear;
		default: return MTLLoadActionDontCare;
	}
}

uint32 TranslateElementTypeToSize(EVertexElementType Type);

MTLPrimitiveType TranslatePrimitiveType(uint32 PrimitiveType);

#if PLATFORM_MAC
MTLPrimitiveTopologyClass TranslatePrimitiveTopology(uint32 PrimitiveType);
#endif

MTLPixelFormat ToSRGBFormat(MTLPixelFormat LinMTLFormat);

uint8 GetMetalPixelFormatKey(MTLPixelFormat Format);

template<typename TRHIType>
static FORCEINLINE typename TMetalResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TMetalResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

#include "MetalStateCache.h"
#include "MetalContext.h"
