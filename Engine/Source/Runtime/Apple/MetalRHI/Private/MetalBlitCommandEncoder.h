// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Metal/Metal.h>
#include "MetalDebugCommandEncoder.h"

NS_ASSUME_NONNULL_BEGIN

@class FMetalDebugCommandBuffer;

@interface FMetalDebugBlitCommandEncoder : FMetalDebugCommandEncoder<MTLBlitCommandEncoder>

/** The wrapped native command-encoder for which we collect debug information. */
@property (readonly, retain) id<MTLBlitCommandEncoder> Inner;
@property (readonly, retain) FMetalDebugCommandBuffer* Buffer;

/** Initialise the wrapper with the provided command-buffer. */
-(id)initWithEncoder:(id<MTLBlitCommandEncoder>)Encoder andCommandBuffer:(FMetalDebugCommandBuffer*)Buffer;

@end
NS_ASSUME_NONNULL_END
