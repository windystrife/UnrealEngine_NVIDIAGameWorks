// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>

// For some reason when including this file while building the editor Clang 9 ignores this pragma from MacPlatformCompilerPreSetup.h,
// resulting in errors in FMetalDebugBufferBindings, FMetalDebugTextureBindings and FMetalDebugSamplerBindings. Readding it here works around this problem.
#if (__clang_major__ >= 9)
#pragma clang diagnostic ignored "-Wnullability-inferred-on-nested-type"
#endif

NS_ASSUME_NONNULL_BEGIN

/**
 * The sampler, buffer and texture resource limits as defined here:
 * https://developer.apple.com/library/ios/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Render-Ctx/Render-Ctx.html
 */
#if PLATFORM_IOS
#define METAL_MAX_BUFFERS 31
#define METAL_MAX_TEXTURES 31
typedef uint32 FMetalTextureMask;
#elif PLATFORM_MAC
#define METAL_MAX_BUFFERS 31
#define METAL_MAX_TEXTURES 128
typedef __uint128_t FMetalTextureMask;
#else
#error "Unsupported Platform!"
#endif
typedef uint32 FMetalBufferMask;
typedef uint16 FMetalSamplerMask;

enum EMetalLimits
{
	ML_MaxSamplers = 16, /** Maximum number of samplers */
	ML_MaxBuffers = METAL_MAX_BUFFERS, /** Maximum number of buffers */
	ML_MaxTextures = METAL_MAX_TEXTURES, /** Maximum number of textures - there are more textures available on Mac than iOS */
	ML_MaxViewports = 16 /** Technically this may be different at runtime, but this is the likely absolute upper-bound */
};

enum EMetalShaderFrequency
{
    EMetalShaderVertex = 0,
    EMetalShaderFragment = 1,
    EMetalShaderCompute = 2,
    EMetalShaderRenderNum = 2,
	EMetalShaderStagesNum = 3
};

/** A structure for quick mask-testing of shader-stage resource bindings */
struct FMetalDebugShaderResourceMask
{
	FMetalTextureMask TextureMask;
	FMetalBufferMask BufferMask;
	FMetalSamplerMask SamplerMask;
};

/** A structure of arrays for the current buffer binding settings. */
struct FMetalDebugBufferBindings
{
    /** The bound buffers or nil. */
    id<MTLBuffer> _Nullable Buffers[ML_MaxBuffers];
    /** Optional bytes buffer used instead of an id<MTLBuffer> */
    void const* _Nullable Bytes[ML_MaxBuffers];
    /** The bound buffer offsets or 0. */
    NSUInteger Offsets[ML_MaxBuffers];
};

/** A structure of arrays for the current texture binding settings. */
struct FMetalDebugTextureBindings
{
    /** The bound textures or nil. */
    id<MTLTexture> _Nullable Textures[ML_MaxTextures];
};

/** A structure of arrays for the current sampler binding settings. */
struct FMetalDebugSamplerBindings
{
    /** The bound sampler states or nil. */
    id<MTLSamplerState> _Nullable Samplers[ML_MaxSamplers];
};

@class FMetalDebugCommandBuffer;

@class FMetalDebugFence;

@interface FMetalDebugCommandEncoder : FApplePlatformObject
{
@public
	NSHashTable<FMetalDebugFence*>* UpdatedFences;
	NSHashTable<FMetalDebugFence*>* WaitingFences;
}
-(instancetype)init;
-(id<MTLCommandEncoder>)commandEncoder;
-(void)addUpdateFence:(id)Fence;
-(void)addWaitFence:(id)Fence;
@end

NS_ASSUME_NONNULL_END

