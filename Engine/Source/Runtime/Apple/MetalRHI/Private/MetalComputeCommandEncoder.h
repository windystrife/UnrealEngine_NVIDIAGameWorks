// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalDebugCommandEncoder.h"

NS_ASSUME_NONNULL_BEGIN

@class FMetalDebugCommandBuffer;
@class FMetalShaderPipeline;

@interface FMetalDebugComputeCommandEncoder : FMetalDebugCommandEncoder<MTLComputeCommandEncoder>
{
@private
#pragma mark - Private Member Variables -
#if METAL_DEBUG_OPTIONS
	FMetalDebugShaderResourceMask ResourceMask;
    FMetalDebugBufferBindings ShaderBuffers;
    FMetalDebugTextureBindings ShaderTextures;
    FMetalDebugSamplerBindings ShaderSamplers;
#endif
}

/** The wrapped native command-encoder for which we collect debug information. */
@property (readonly, retain) id<MTLComputeCommandEncoder> Inner;
@property (readonly, retain) FMetalDebugCommandBuffer* Buffer;
@property (nonatomic, retain) FMetalShaderPipeline* Pipeline;

/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithEncoder:(id<MTLComputeCommandEncoder>)Encoder andCommandBuffer:(FMetalDebugCommandBuffer*)Buffer;

/** Validates the pipeline/binding state */
-(void)validate;

@end
NS_ASSUME_NONNULL_END

#if METAL_DEBUG_OPTIONS
#define METAL_SET_COMPUTE_REFLECTION(Encoder, InPipeline)														\
    if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelFastValidation) \
    {																											\
        ((FMetalDebugComputeCommandEncoder*)Encoder).Pipeline = InPipeline;										\
    }
#else
#define METAL_SET_COMPUTE_REFLECTION(Encoder, InPipeline)
#endif
