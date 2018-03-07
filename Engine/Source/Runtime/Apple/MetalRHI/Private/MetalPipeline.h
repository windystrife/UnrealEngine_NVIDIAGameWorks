// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetalRHIPrivate.h"
#if METAL_DEBUG_OPTIONS
#include "MetalDebugCommandEncoder.h"
#endif

enum EMetalPipelineHashBits
{
	NumBits_RenderTargetFormat = 5, //(x8=40),
	NumBits_DepthFormat = 3, //(x1=3),
	NumBits_StencilFormat = 3, //(x1=3),
	NumBits_SampleCount = 3, //(x1=3),

	NumBits_BlendState = 5, //(x8=40),
	NumBits_PrimitiveTopology = 2, //(x1=2)
	NumBits_IndexType = 2,
};

enum EMetalPipelineHashOffsets
{
	Offset_BlendState0 = 0,
	Offset_BlendState1 = Offset_BlendState0 + NumBits_BlendState,
	Offset_BlendState2 = Offset_BlendState1 + NumBits_BlendState,
	Offset_BlendState3 = Offset_BlendState2 + NumBits_BlendState,
	Offset_BlendState4 = Offset_BlendState3 + NumBits_BlendState,
	Offset_BlendState5 = Offset_BlendState4 + NumBits_BlendState,
	Offset_BlendState6 = Offset_BlendState5 + NumBits_BlendState,
	Offset_BlendState7 = Offset_BlendState6 + NumBits_BlendState,
	Offset_PrimitiveTopology = Offset_BlendState7 + NumBits_BlendState,
	Offset_IndexType = Offset_PrimitiveTopology + NumBits_PrimitiveTopology,
	Offset_RasterEnd = Offset_IndexType + NumBits_IndexType,

	Offset_RenderTargetFormat0 = 64,
	Offset_RenderTargetFormat1 = Offset_RenderTargetFormat0 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat2 = Offset_RenderTargetFormat1 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat3 = Offset_RenderTargetFormat2 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat4 = Offset_RenderTargetFormat3 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat5 = Offset_RenderTargetFormat4 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat6 = Offset_RenderTargetFormat5 + NumBits_RenderTargetFormat,
	Offset_RenderTargetFormat7 = Offset_RenderTargetFormat6 + NumBits_RenderTargetFormat,
	Offset_DepthFormat = Offset_RenderTargetFormat7 + NumBits_RenderTargetFormat,
	Offset_StencilFormat = Offset_DepthFormat + NumBits_DepthFormat,
	Offset_SampleCount = Offset_StencilFormat + NumBits_StencilFormat,
	Offset_End = Offset_SampleCount + NumBits_SampleCount
};


@interface FMetalTessellationPipelineDesc : FApplePlatformObject
@property (nonatomic, retain) MTLVertexDescriptor* DomainVertexDescriptor;
@property (nonatomic) NSUInteger TessellationInputControlPointBufferIndex;
@property (nonatomic) NSUInteger TessellationOutputControlPointBufferIndex;
@property (nonatomic) NSUInteger TessellationPatchControlPointOutSize;
@property (nonatomic) NSUInteger TessellationPatchConstBufferIndex;
@property (nonatomic) NSUInteger TessellationInputPatchConstBufferIndex;
@property (nonatomic) NSUInteger TessellationPatchConstOutSize;
@property (nonatomic) NSUInteger TessellationTessFactorOutSize;
@property (nonatomic) NSUInteger TessellationFactorBufferIndex;
@property (nonatomic) NSUInteger TessellationPatchCountBufferIndex;
@property (nonatomic) NSUInteger TessellationControlPointIndexBufferIndex;
@property (nonatomic) NSUInteger TessellationIndexBufferIndex;
@property (nonatomic) NSUInteger DSNumUniformBuffers; // DEBUG ONLY
@end

@interface FMetalShaderPipeline : FApplePlatformObject
{
#if METAL_DEBUG_OPTIONS
@public
	FMetalDebugShaderResourceMask ResourceMask[EMetalShaderStagesNum];
#endif
}
@property (nonatomic, retain) id<MTLRenderPipelineState> RenderPipelineState;
@property (nonatomic, retain) id<MTLComputePipelineState> ComputePipelineState;
@property (nonatomic, retain) FMetalTessellationPipelineDesc* TessellationPipelineDesc;
#if METAL_DEBUG_OPTIONS
@property (nonatomic, retain) MTLRenderPipelineReflection* RenderPipelineReflection;
@property (nonatomic, retain) MTLComputePipelineReflection* ComputePipelineReflection;
@property (nonatomic, retain) NSString* VertexSource;
@property (nonatomic, retain) NSString* FragmentSource;
@property (nonatomic, retain) NSString* ComputeSource;
- (void)initResourceMask;
- (void)initResourceMask:(EMetalShaderFrequency)Frequency;
#endif
@end
