// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRenderCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalRenderCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalFence.h"
#include "MetalPipeline.h"

NS_ASSUME_NONNULL_BEGIN

#if METAL_DEBUG_OPTIONS
static NSString* GMetalDebugVertexShader = @"#include <metal_stdlib>\n"
@"using namespace metal;\n"
@"struct VertexInput\n"
@"{\n"
@"};\n"
@"vertex void WriteCommandIndexVS(VertexInput StageIn [[stage_in]], constant uint* Input [[ buffer(0) ]], device uint* Output  [[ buffer(1) ]])\n"
@"{\n"
@"	Output[0] = Input[0];\n"
@"}\n";

static id <MTLRenderPipelineState> GetDebugVertexShaderState(id<MTLDevice> Device, MTLRenderPassDescriptor* PassDesc)
{
	static id<MTLFunction> Func = nil;
	static FCriticalSection Mutex;
	static NSMutableDictionary* Dict = [NSMutableDictionary new];
	if(!Func)
	{
		id<MTLLibrary> Lib = [Device newLibraryWithSource:GMetalDebugVertexShader options:nil error:nullptr];
		check(Lib);
		Func = [Lib newFunctionWithName:@"WriteCommandIndexVS"];
		check(Func);
		[Lib release];
	}
	
	FScopeLock Lock(&Mutex);
	id<MTLRenderPipelineState> State = [Dict objectForKey:PassDesc];
	if (!State)
	{
		MTLRenderPipelineDescriptor* Desc = [MTLRenderPipelineDescriptor new];
		
		Desc.vertexFunction = Func;
		
		if (PassDesc.depthAttachment)
		{
			Desc.depthAttachmentPixelFormat = PassDesc.depthAttachment.texture.pixelFormat;
		}
		if (PassDesc.stencilAttachment)
		{
			Desc.stencilAttachmentPixelFormat = PassDesc.stencilAttachment.texture.pixelFormat;
		}
		if (PassDesc.colorAttachments)
		{
			MTLRenderPassColorAttachmentDescriptor* CD = [PassDesc.colorAttachments objectAtIndexedSubscript:0];
			MTLRenderPipelineColorAttachmentDescriptor* CD0 = [[MTLRenderPipelineColorAttachmentDescriptor new] autorelease];
			CD0.pixelFormat = CD.texture.pixelFormat;
			[Desc.colorAttachments setObject:CD0 atIndexedSubscript:0];
		}
		Desc.rasterizationEnabled = false;
		
		State = [[Device newRenderPipelineStateWithDescriptor:Desc error:nil] autorelease];
		check(State);
		
		[Dict setObject:State forKey:PassDesc];
		
		[Desc release];
	}
	check(State);
	return State;
}
#endif

@implementation FMetalDebugRenderCommandEncoder

@synthesize Inner;
@synthesize Buffer;
@synthesize Pipeline;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugRenderCommandEncoder)

-(id)initWithEncoder:(id<MTLRenderCommandEncoder>)Encoder fromDescriptor:(MTLRenderPassDescriptor*)Desc andCommandBuffer:(FMetalDebugCommandBuffer*)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = [Encoder retain];
		Buffer = [SourceBuffer retain];
		RenderPassDesc = [Desc retain];
#if METAL_DEBUG_OPTIONS
		DebugState = Buffer->DebugLevel >= EMetalDebugLevelValidation ? [GetDebugVertexShaderState(Buffer.device, Desc) retain] : nil;
#endif
        Pipeline = nil;
	}
	return Self;
}

-(void)dealloc
{
	[Inner release];
	[Buffer release];
	[RenderPassDesc release];
#if METAL_DEBUG_OPTIONS
	[DebugState release];
#endif
	[Pipeline release];
	[super dealloc];
}

-(id <MTLDevice>) device
{
	return Inner.device;
}

-(NSString *_Nullable)label
{
	return Inner.label;
}

-(void)setLabel:(NSString *_Nullable)Text
{
	Inner.label = Text;
}

- (void)endEncoding
{
    [Buffer endCommandEncoder];
    [Inner endEncoding];
}

- (void)insertDebugSignpost:(NSString *)string
{
    [Buffer insertDebugSignpost:string];
    [Inner insertDebugSignpost:string];
}

- (void)pushDebugGroup:(NSString *)string
{
    [Buffer pushDebugGroup:string];
    [Inner pushDebugGroup:string];
}

#if METAL_DEBUG_OPTIONS
- (void)insertDebugDraw
{
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			uint32 const Index = Buffer->DebugCommands.Num();
#if PLATFORM_MAC
			[Inner textureBarrier];
#endif
			[Inner setVertexBytes:&Index length:sizeof(Index) atIndex:0];
			[Inner setVertexBuffer:Buffer->DebugInfoBuffer offset:0 atIndex:1];
			[Inner setRenderPipelineState:DebugState];
			[Inner drawPrimitives:MTLPrimitiveTypePoint vertexStart:0 vertexCount:1];
#if PLATFORM_MAC
			[Inner textureBarrier];
#endif
			if (Pipeline && Pipeline.RenderPipelineState)
			{
				[Inner setRenderPipelineState:Pipeline.RenderPipelineState];
			}
			
			if (ShaderBuffers[EMetalShaderVertex].Buffers[0])
			{
				[Inner setVertexBuffer:ShaderBuffers[EMetalShaderVertex].Buffers[0] offset:ShaderBuffers[EMetalShaderVertex].Offsets[0] atIndex:0];
			}
			else if (ShaderBuffers[EMetalShaderVertex].Bytes[0])
			{
				[Inner setVertexBytes:ShaderBuffers[EMetalShaderVertex].Bytes[0] length:ShaderBuffers[EMetalShaderVertex].Offsets[0] atIndex:0];
			}
			
			if (ShaderBuffers[EMetalShaderVertex].Buffers[1])
			{
				[Inner setVertexBuffer:ShaderBuffers[EMetalShaderVertex].Buffers[1] offset:ShaderBuffers[EMetalShaderVertex].Offsets[1] atIndex:1];
			}
			else if (ShaderBuffers[EMetalShaderVertex].Bytes[1])
			{
				[Inner setVertexBytes:ShaderBuffers[EMetalShaderVertex].Bytes[1] length:ShaderBuffers[EMetalShaderVertex].Offsets[1] atIndex:1];
			}
		}
		default:
		{
			break;
		}
	}
}
#endif

- (void)popDebugGroup
{
	[Buffer popDebugGroup];
#if METAL_DEBUG_OPTIONS
	[self insertDebugDraw];
#endif
    [Inner popDebugGroup];
}

- (void)setRenderPipelineState:(id <MTLRenderPipelineState>)pipelineState
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer setPipeline:pipelineState.label];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:pipelineState];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setRenderPipelineState:pipelineState];
}

- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers[EMetalShaderVertex].Buffers[index] = nil;
			ShaderBuffers[EMetalShaderVertex].Bytes[index] = bytes;
			ShaderBuffers[EMetalShaderVertex].Offsets[index] = length;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderVertex].BufferMask = bytes ? (ResourceMask[EMetalShaderVertex].BufferMask | (1 << index)) : (ResourceMask[EMetalShaderVertex].BufferMask & ~(1 << index));
		}
		default:
		{
			break;
		}
	}
#endif
	
    [Inner setVertexBytes:bytes length:length atIndex:index];
}

- (void)setVertexBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers[EMetalShaderVertex].Buffers[index] = buffer;
			ShaderBuffers[EMetalShaderVertex].Bytes[index] = nil;
			ShaderBuffers[EMetalShaderVertex].Offsets[index] = offset;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:buffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderVertex].BufferMask = buffer ? (ResourceMask[EMetalShaderVertex].BufferMask | (1 << index)) : (ResourceMask[EMetalShaderVertex].BufferMask & ~(1 << index));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setVertexBuffer:buffer offset:offset atIndex:index];
}

- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers[EMetalShaderVertex].Offsets[index] = offset;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			check(ResourceMask[EMetalShaderVertex].BufferMask & 1 << index);
		}
		default:
		{
			break;
		}
	}
#endif
	[Inner setVertexBufferOffset:offset atIndex:index];
}

- (void)setVertexBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range
{
#if METAL_DEBUG_OPTIONS
	for (uint32 i = 0; i < range.length; i++)
	{
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderBuffers[EMetalShaderVertex].Buffers[i + range.location] = buffers[i];
				ShaderBuffers[EMetalShaderVertex].Bytes[i + range.location] = nil;
				ShaderBuffers[EMetalShaderVertex].Offsets[i + range.location] = offsets[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackResource:buffers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderVertex].BufferMask = buffers[i] ? (ResourceMask[EMetalShaderVertex].BufferMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderVertex].BufferMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
    }
#endif
    [Inner setVertexBuffers:buffers offsets:offsets withRange:range];
}

- (void)setVertexTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderTextures[EMetalShaderVertex].Textures[index] = texture;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:texture];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderVertex].TextureMask = texture ? (ResourceMask[EMetalShaderVertex].TextureMask | (1 << (index))) : (ResourceMask[EMetalShaderVertex].TextureMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setVertexTexture:texture atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setVertexTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range
#else
- (void)setVertexTextures : (const id <MTLTexture> __nullable[__nullable])textures withRange : (NSRange)range
#endif
{
#if METAL_DEBUG_OPTIONS
    for (uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderTextures[EMetalShaderVertex].Textures[i + range.location] = textures[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackResource:textures[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderVertex].TextureMask = textures[i] ? (ResourceMask[EMetalShaderVertex].TextureMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderVertex].TextureMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
    }
#endif
    [Inner setVertexTextures:textures withRange:range];
}

- (void)setVertexSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderSamplers[EMetalShaderVertex].Samplers[index] = sampler;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:sampler];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderVertex].SamplerMask = sampler ? (ResourceMask[EMetalShaderVertex].SamplerMask | (1 << (index))) : (ResourceMask[EMetalShaderVertex].SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setVertexSamplerState:sampler atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setVertexSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range
#else
- (void)setVertexSamplerStates : (const id <MTLSamplerState> __nullable[__nullable])samplers withRange : (NSRange)range
#endif
{
#if METAL_DEBUG_OPTIONS
    for(uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderSamplers[EMetalShaderVertex].Samplers[i + range.location] = samplers[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackState:samplers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderVertex].SamplerMask = samplers[i] ? (ResourceMask[EMetalShaderVertex].SamplerMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderVertex].SamplerMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setVertexSamplerStates:samplers withRange:range];
}

- (void)setVertexSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderSamplers[EMetalShaderVertex].Samplers[index] = sampler;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:sampler];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderVertex].SamplerMask = sampler ? (ResourceMask[EMetalShaderVertex].SamplerMask | (1 << (index))) : (ResourceMask[EMetalShaderVertex].SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setVertexSamplerState:sampler lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setVertexSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range
#else
- (void)setVertexSamplerStates : (const id <MTLSamplerState> __nullable[__nullable])samplers lodMinClamps : (const float[__nullable])lodMinClamps lodMaxClamps : (const float[__nullable])lodMaxClamps withRange : (NSRange)range
#endif
{
#if METAL_DEBUG_OPTIONS
    for(uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderSamplers[EMetalShaderVertex].Samplers[i + range.location] = samplers[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackState:samplers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderVertex].SamplerMask = samplers[i] ? (ResourceMask[EMetalShaderVertex].SamplerMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderVertex].SamplerMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setVertexSamplerStates:samplers lodMinClamps:lodMinClamps lodMaxClamps:lodMaxClamps withRange:range];
}

- (void)setViewport:(MTLViewport)viewport
{
    [Inner setViewport:viewport];
}

- (void)setFrontFacingWinding:(MTLWinding)frontFacingWinding
{
    [Inner setFrontFacingWinding:frontFacingWinding];
}

- (void)setCullMode:(MTLCullMode)cullMode
{
    [Inner setCullMode:cullMode];
}

#if PLATFORM_MAC || (PLATFORM_IOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000)
- (void)setDepthClipMode:(MTLDepthClipMode)depthClipMode
{
	if (GMetalSupportsDepthClipMode)
	{
    	[Inner setDepthClipMode:depthClipMode];
    }
}
#endif

- (void)setDepthBias:(float)depthBias slopeScale:(float)slopeScale clamp:(float)clamp
{
    [Inner setDepthBias:depthBias slopeScale:slopeScale clamp:clamp];
}

- (void)setScissorRect:(MTLScissorRect)rect
{
    [Inner setScissorRect:rect];
}

- (void)setTriangleFillMode:(MTLTriangleFillMode)fillMode
{
    [Inner setTriangleFillMode:fillMode];
}

- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers[EMetalShaderFragment].Buffers[index] = nil;
			ShaderBuffers[EMetalShaderFragment].Bytes[index] = bytes;
			ShaderBuffers[EMetalShaderFragment].Offsets[index] = length;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderFragment].BufferMask = bytes ? (ResourceMask[EMetalShaderFragment].BufferMask | (1 << (index))) : (ResourceMask[EMetalShaderFragment].BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index];
}

- (void)setFragmentBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers[EMetalShaderFragment].Buffers[index] = buffer;
			ShaderBuffers[EMetalShaderFragment].Bytes[index] = nil;
			ShaderBuffers[EMetalShaderFragment].Offsets[index] = offset;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:buffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderFragment].BufferMask = buffer ? (ResourceMask[EMetalShaderFragment].BufferMask | (1 << (index))) : (ResourceMask[EMetalShaderFragment].BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setFragmentBuffer:buffer offset:offset atIndex:index];
}

- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers[EMetalShaderFragment].Offsets[index] = offset;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			check(ResourceMask[EMetalShaderFragment].BufferMask & (1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index];
}

- (void)setFragmentBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offset withRange:(NSRange)range
{
#if METAL_DEBUG_OPTIONS
    for (uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderBuffers[EMetalShaderFragment].Buffers[i + range.location] = buffers[i];
				ShaderBuffers[EMetalShaderFragment].Bytes[i + range.location] = nil;
				ShaderBuffers[EMetalShaderFragment].Offsets[i + range.location] = offset[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackResource:buffers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderFragment].BufferMask = buffers[i] ? (ResourceMask[EMetalShaderFragment].BufferMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderFragment].BufferMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
    }
#endif
    [Inner setFragmentBuffers:buffers offsets:offset withRange:range];
}

- (void)setFragmentTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderTextures[EMetalShaderFragment].Textures[index] = texture;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:texture];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderFragment].TextureMask = texture ? (ResourceMask[EMetalShaderFragment].TextureMask | (1 << (index))) : (ResourceMask[EMetalShaderFragment].TextureMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setFragmentTexture:texture atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setFragmentTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range
#else
- (void)setFragmentTextures : (const id <MTLTexture> __nullable[__nullable])textures withRange : (NSRange)range
#endif
{
#if METAL_DEBUG_OPTIONS
    for (uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderTextures[EMetalShaderFragment].Textures[i + range.location] = textures[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackResource:textures[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderFragment].TextureMask = textures[i] ? (ResourceMask[EMetalShaderFragment].TextureMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderFragment].TextureMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setFragmentTextures:textures withRange:(NSRange)range];
}

- (void)setFragmentSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderSamplers[EMetalShaderFragment].Samplers[index] = sampler;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:sampler];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderFragment].SamplerMask = sampler ? (ResourceMask[EMetalShaderFragment].SamplerMask | (1 << (index))) : (ResourceMask[EMetalShaderFragment].SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setFragmentSamplerState:sampler atIndex:(NSUInteger)index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setFragmentSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range
#else
- (void)setFragmentSamplerStates : (const id <MTLSamplerState> __nullable[__nullable])samplers withRange : (NSRange)range
#endif
{
#if METAL_DEBUG_OPTIONS
	for(uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderSamplers[EMetalShaderFragment].Samplers[i + range.location] = samplers[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackState:samplers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderFragment].SamplerMask = samplers[i] ? (ResourceMask[EMetalShaderFragment].SamplerMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderFragment].SamplerMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setFragmentSamplerStates:samplers withRange:(NSRange)range];
}

- (void)setFragmentSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderSamplers[EMetalShaderFragment].Samplers[index] = sampler;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:sampler];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask[EMetalShaderFragment].SamplerMask = sampler ? (ResourceMask[EMetalShaderFragment].SamplerMask | (1 << (index))) : (ResourceMask[EMetalShaderFragment].SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setFragmentSamplerState:sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setFragmentSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range
#else
- (void)setFragmentSamplerStates : (const id <MTLSamplerState> __nullable[__nullable])samplers lodMinClamps : (const float[__nullable])lodMinClamps lodMaxClamps : (const float[__nullable])lodMaxClamps withRange : (NSRange)range
#endif
{
#if METAL_DEBUG_OPTIONS
    for(uint32 i = 0; i < range.length; i++)
    {
		switch (Buffer->DebugLevel)
		{
			case EMetalDebugLevelConditionalSubmit:
			case EMetalDebugLevelWaitForComplete:
			case EMetalDebugLevelLogOperations:
			case EMetalDebugLevelValidation:
			{
				ShaderSamplers[EMetalShaderFragment].Samplers[i + range.location] = samplers[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackState:samplers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask[EMetalShaderFragment].SamplerMask = samplers[i] ? (ResourceMask[EMetalShaderFragment].SamplerMask | (1 << (i + range.location))) : (ResourceMask[EMetalShaderFragment].SamplerMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
    }
#endif
    [Inner setFragmentSamplerStates:samplers lodMinClamps:lodMinClamps lodMaxClamps:lodMaxClamps withRange:(NSRange)range];
}

- (void)setBlendColorRed:(float)red green:(float)green blue:(float)blue alpha:(float)alpha
{
    [Inner setBlendColorRed:red green:green blue:blue alpha:alpha];
}

- (void)setDepthStencilState:(nullable id <MTLDepthStencilState>)depthStencilState
{
#if METAL_DEBUG_OPTIONS
	if (Buffer->DebugLevel >= EMetalDebugLevelTrackResources)
	{
		[Buffer trackState:depthStencilState];
	}
#endif
    [Inner setDepthStencilState:depthStencilState];
}

- (void)setStencilReferenceValue:(uint32_t)referenceValue
{
    [Inner setStencilReferenceValue:referenceValue];
}

- (void)setStencilFrontReferenceValue:(uint32_t)frontReferenceValue backReferenceValue:(uint32_t)backReferenceValue
{
    [Inner setStencilFrontReferenceValue:frontReferenceValue backReferenceValue:backReferenceValue];
}

- (void)setVisibilityResultMode:(MTLVisibilityResultMode)mode offset:(NSUInteger)offset
{
    [Inner setVisibilityResultMode:mode offset:offset];
}

- (void)setColorStoreAction:(MTLStoreAction)storeAction atIndex:(NSUInteger)colorAttachmentIndex
{
    [Inner setColorStoreAction:storeAction atIndex:colorAttachmentIndex];
}

- (void)setDepthStoreAction:(MTLStoreAction)storeAction
{
    [Inner setDepthStoreAction:storeAction];
}

- (void)setStencilStoreAction:(MTLStoreAction)storeAction
{
    [Inner setStencilStoreAction:storeAction];
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawPrimitives:primitiveType vertexStart:vertexStart vertexCount:vertexCount instanceCount:instanceCount];
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawPrimitives:primitiveType vertexStart:vertexStart vertexCount:vertexCount];
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:indexBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount];
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:indexBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset];
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance];
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount baseVertex:(NSInteger)baseVertex baseInstance:(NSUInteger)baseInstance
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:indexBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount baseVertex:(NSInteger)baseVertex baseInstance:(NSUInteger)baseInstance];
}

- (void)drawPrimitives:(MTLPrimitiveType)primitiveType indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:indirectBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawPrimitives:(MTLPrimitiveType)primitiveType indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset];
}

- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:indexBuffer];
			[Buffer trackResource:indirectBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset];
}

#if PLATFORM_MAC
- (void)textureBarrier
{
	[Inner textureBarrier];
}

#endif

#if METAL_SUPPORTS_HEAPS
- (void)updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages
{
#if METAL_DEBUG_OPTIONS
    if (fence && Buffer->DebugLevel >= EMetalDebugLevelValidation)
    {
        [self addUpdateFence:fence];
        if (((FMetalDebugFence*)fence).Inner)
        {
            [Inner updateFence:((FMetalDebugFence*)fence).Inner afterStages:(MTLRenderStages)stages];
        }
	}
	else
#endif
	{
		[Inner updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages];
	}
}

- (void)waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages
{
#if METAL_DEBUG_OPTIONS
	if (fence && Buffer->DebugLevel >= EMetalDebugLevelValidation)
	{
		[self addWaitFence:fence];
        if (((FMetalDebugFence*)fence).Inner)
        {
            [Inner waitForFence:((FMetalDebugFence*)fence).Inner beforeStages:(MTLRenderStages)stages];
        }
	}
	else
#endif
	{
		[Inner waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages];
	}
}
#endif

-(void)setTessellationFactorBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset instanceStride:(NSUInteger)instanceStride
{
#if METAL_DEBUG_OPTIONS
	if (Buffer->DebugLevel >= EMetalDebugLevelTrackResources)
	{
		[Buffer trackResource:buffer];
	}
#endif
    [Inner setTessellationFactorBuffer:buffer offset:(NSUInteger)offset instanceStride:(NSUInteger)instanceStride];
}

-(void)setTessellationFactorScale:(float)scale
{
    [Inner setTessellationFactorScale:(float)scale];
}

-(void)drawPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:patchIndexBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance];
}

#if PLATFORM_MAC
-(void)drawPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:patchIndexBuffer];
			[Buffer trackResource:indirectBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset];
}
#endif

-(void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:patchIndexBuffer];
			[Buffer trackResource:controlPointIndexBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance];
}

#if PLATFORM_MAC
-(void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer draw:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:patchIndexBuffer];
			[Buffer trackResource:controlPointIndexBuffer];
			[Buffer trackResource:indirectBuffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			[self validate];
		}
		default:
		{
			break;
		}
	}
#endif
	
    [Inner drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset];
}
#endif

#if METAL_SUPPORTS_INDIRECT_ARGUMENT_BUFFERS
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage
{
	if (GMetalSupportsIndirectArgumentBuffers)
	{
		[Inner useResource:resource usage:usage];
	}
}

- (void)useResources:(const id <MTLResource> [])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage
{
	if (GMetalSupportsIndirectArgumentBuffers)
	{
		[Inner useResources:resources count:count usage:usage];
	}
}

- (void)useHeap:(id <MTLHeap>)heap
{
	if (GMetalSupportsIndirectArgumentBuffers)
	{
		[Inner useHeap:heap];
	}
}

- (void)useHeaps:(const id <MTLHeap> [])heaps count:(NSUInteger)count
{
	if (GMetalSupportsIndirectArgumentBuffers)
	{
		[Inner useHeaps:heaps count:count];
	}
}
#endif // #if METAL_SUPPORTS_INDIRECT_ARGUMENT_BUFFERS

- (void)setViewports:(const MTLViewport [])viewports count:(NSUInteger)count
{
	if(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports))
	{
		id<IMetalRenderCommandEncoder> Enc = (id<IMetalRenderCommandEncoder>)Inner;
		[Enc setViewports:viewports count:count];
	}
}

- (void)setScissorRects:(const MTLScissorRect [])scissorRects count:(NSUInteger)count
{
	if(FMetalCommandQueue::SupportsFeature(EMetalFeaturesMultipleViewports))
	{
		id<IMetalRenderCommandEncoder> Enc = (id<IMetalRenderCommandEncoder>)Inner;
		[Enc setScissorRects:scissorRects count:count];
	}
}

-(NSString*) description
{
	return [Inner description];
}

-(NSString*) debugDescription
{
	return [Inner debugDescription];
}

- (bool)validateFunctionBindings:(EMetalShaderFrequency)Frequency
{
	bool bOK = true;
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			check(Pipeline);

			MTLRenderPipelineReflection* Reflection = Pipeline.RenderPipelineReflection;
			check(Reflection);
			
			NSArray<MTLArgument*>* Arguments = nil;
			switch(Frequency)
			{
				case EMetalShaderVertex:
				{
					Arguments = Reflection.vertexArguments;
					break;
				}
				case EMetalShaderFragment:
				{
					Arguments = Reflection.fragmentArguments;
					break;
				}
				default:
					check(false);
					break;
			}
			
			for (uint32 i = 0; i < Arguments.count; i++)
			{
				MTLArgument* Arg = [Arguments objectAtIndex:i];
				check(Arg);
				switch(Arg.type)
				{
					case MTLArgumentTypeBuffer:
					{
						checkf(Arg.index < ML_MaxBuffers, TEXT("Metal buffer index exceeded!"));
						if ((ShaderBuffers[Frequency].Buffers[Arg.index] == nil && ShaderBuffers[Frequency].Bytes[Arg.index] == nil))
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
						}
						break;
					}
					case MTLArgumentTypeThreadgroupMemory:
					{
						break;
					}
					case MTLArgumentTypeTexture:
					{
						checkf(Arg.index < ML_MaxTextures, TEXT("Metal texture index exceeded!"));
						if (ShaderTextures[Frequency].Textures[Arg.index] == nil)
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
						}
						else if (ShaderTextures[Frequency].Textures[Arg.index].textureType != Arg.textureType)
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Incorrect texture type bound at Metal index %u which will crash the driver: %s\n%s"), (uint32)Arg.index, *FString([Arg description]), *FString([ShaderTextures[Frequency].Textures[Arg.index] description]));
						}
						break;
					}
					case MTLArgumentTypeSampler:
					{
						checkf(Arg.index < ML_MaxSamplers, TEXT("Metal sampler index exceeded!"));
						if (ShaderSamplers[Frequency].Samplers[Arg.index] == nil)
						{
							bOK = false;
							UE_LOG(LogMetal, Warning, TEXT("Unbound sampler at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
						}
						break;
					}
					default:
						check(false);
						break;
				}
			}
			break;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			check(Pipeline);
			
			FMetalTextureMask TextureMask = (ResourceMask[Frequency].TextureMask & Pipeline->ResourceMask[Frequency].TextureMask);
			if (TextureMask != Pipeline->ResourceMask[Frequency].TextureMask)
			{
				bOK = false;
				for (uint32 i = 0; i < ML_MaxTextures; i++)
				{
					if ((Pipeline->ResourceMask[Frequency].TextureMask & (1 < i)) && ((TextureMask & (1 < i)) != (Pipeline->ResourceMask[Frequency].TextureMask & (1 < i))))
					{
						UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index %u which will crash the driver"), i);
					}
				}
			}
			
			FMetalBufferMask BufferMask = (ResourceMask[Frequency].BufferMask & Pipeline->ResourceMask[Frequency].BufferMask);
			if (BufferMask != Pipeline->ResourceMask[Frequency].BufferMask)
			{
				bOK = false;
				for (uint32 i = 0; i < ML_MaxBuffers; i++)
				{
					if ((Pipeline->ResourceMask[Frequency].BufferMask & (1 < i)) && ((BufferMask & (1 < i)) != (Pipeline->ResourceMask[Frequency].BufferMask & (1 < i))))
					{
						UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index %u which will crash the driver"), i);
					}
				}
			}
			
			FMetalSamplerMask SamplerMask = (ResourceMask[Frequency].SamplerMask & Pipeline->ResourceMask[Frequency].SamplerMask);
			if (SamplerMask != Pipeline->ResourceMask[Frequency].SamplerMask)
			{
				bOK = false;
				for (uint32 i = 0; i < ML_MaxSamplers; i++)
				{
					if ((Pipeline->ResourceMask[Frequency].SamplerMask & (1 < i)) && ((SamplerMask & (1 < i)) != (Pipeline->ResourceMask[Frequency].SamplerMask & (1 < i))))
					{
						UE_LOG(LogMetal, Warning, TEXT("Unbound sampler at Metal index %u which will crash the driver"), i);
					}
				}
			}
			
			break;
		}
		default:
		{
			break;
		}
	}
#endif
    return bOK;
}

- (void)validate
{
#if METAL_DEBUG_OPTIONS
    bool bOK = [self validateFunctionBindings:EMetalShaderVertex];
    if (!bOK)
    {
        UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for vertex shader:\n%s"), Pipeline && Pipeline.VertexSource ? *FString(Pipeline.VertexSource) : TEXT("nil"));
    }
	
    bOK = [self validateFunctionBindings:EMetalShaderFragment];
    if (!bOK)
    {
        UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for fragment shader:\n%s"), Pipeline && Pipeline.FragmentSource ? *FString(Pipeline.FragmentSource) : TEXT("nil"));
    }
#endif
}

-(id<MTLCommandEncoder>)commandEncoder
{
	return self;
}

#if (METAL_NEW_NONNULL_DECL)
- (void)setColorStoreActionOptions:(MTLStoreActionOptions)storeActionOptions atIndex:(NSUInteger)colorAttachmentIndex
{
	if (GMetalSupportsStoreActionOptions)
	{
		[Inner setColorStoreActionOptions:storeActionOptions atIndex:colorAttachmentIndex];
	}
}

- (void)setDepthStoreActionOptions:(MTLStoreActionOptions)storeActionOptions
{
	if (GMetalSupportsStoreActionOptions)
	{
		[Inner setDepthStoreActionOptions:storeActionOptions];
	}
}

- (void)setStencilStoreActionOptions:(MTLStoreActionOptions)storeActionOptions
{
	if (GMetalSupportsStoreActionOptions)
	{
		[Inner setStencilStoreActionOptions:storeActionOptions];
	}
}
#endif //(METAL_NEW_NONNULL_DECL)

#if METAL_SUPPORTS_TILE_SHADERS
- (void)setTileBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileBytes:bytes length:length atIndex:index];
	}
}

- (void)setTileBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileBuffer:buffer offset:offset atIndex:index];
	}
}

- (void)setTileBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileBufferOffset:offset atIndex:index];
	}
}

- (void)setTileBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offset withRange:(NSRange)range
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileBuffers:buffers offsets:offset withRange:range];
	}
}

- (void)setTileTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileTexture:texture atIndex:index];
	}
}

- (void)setTileTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileTextures:textures withRange:range];
	}
}

- (void)setTileSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileSamplerState:sampler atIndex:index];
	}
}

- (void)setTileSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileSamplerStates:samplers withRange:range];
	}
}

- (void)setTileSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileSamplerState:sampler lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];
	}
}

- (void)setTileSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setTileSamplerStates:samplers lodMinClamps:lodMinClamps lodMaxClamps:lodMaxClamps withRange:range];
	}
}

- (void)dispatchThreadsPerTile:(MTLSize)threadsPerTile
{
	if (GMetalSupportsTileShaders)
	{
		[Inner dispatchThreadsPerTile:threadsPerTile];
	}
}

- (void)setThreadgroupMemoryLength:(NSUInteger)length offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setThreadgroupMemoryLength:length offset:offset atIndex:index];
	}
}

#endif
@end

#if !METAL_SUPPORTS_HEAPS
@implementation FMetalDebugRenderCommandEncoder (MTLRenderCommandEncoderExtensions)
-(void) updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages
{
#if METAL_DEBUG_OPTIONS
	[self addUpdateFence:fence];
#endif
}

-(void) waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages
{
#if METAL_DEBUG_OPTIONS
	[self addWaitFence:fence];
#endif
}
@end
#endif

NS_ASSUME_NONNULL_END
