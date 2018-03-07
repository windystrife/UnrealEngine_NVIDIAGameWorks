// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalComputeCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalComputeCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalFence.h"
#include "MetalPipeline.h"

NS_ASSUME_NONNULL_BEGIN

#if METAL_DEBUG_OPTIONS
static NSString* GMetalDebugComputeShader = @"#include <metal_stdlib>\n"
@"using namespace metal;\n"
@"kernel void WriteCommandIndexCS(constant uint* Input [[ buffer(0) ]], device atomic_uint* Output [[ buffer(1) ]])\n"
@"{\n"
@"	atomic_store_explicit(Output, Input[0], memory_order_relaxed);\n"
@"}\n";

static id <MTLComputePipelineState> GetDebugComputeShaderState(id<MTLDevice> Device)
{
	static id<MTLComputePipelineState> State = nil;
	if (!State)
	{
		id<MTLLibrary> Lib = [Device newLibraryWithSource:GMetalDebugComputeShader options:nil error:nullptr];
		check(Lib);
		id<MTLFunction> Func = [Lib newFunctionWithName:@"WriteCommandIndexCS"];
		check(Func);
		State = [Device newComputePipelineStateWithFunction:Func error:nil];
		[Func release];
		[Lib release];
	}
	check(State);
	return State;
}
#endif

@implementation FMetalDebugComputeCommandEncoder

@synthesize Inner;
@synthesize Buffer;
@synthesize Pipeline;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugComputeCommandEncoder)

-(id)initWithEncoder:(id<MTLComputeCommandEncoder>)Encoder andCommandBuffer:(FMetalDebugCommandBuffer*)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = [Encoder retain];
		Buffer = [SourceBuffer retain];
        Pipeline = nil;
	}
	return Self;
}

-(void)dealloc
{
	[Inner release];
	[Buffer release];
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
- (void)insertDebugDispatch
{
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			uint32 const Index = Buffer->DebugCommands.Num();
			[Inner setBytes:&Index length:sizeof(Index) atIndex:0];
			[Inner setBuffer:Buffer->DebugInfoBuffer offset:0 atIndex:1];
			[Inner setComputePipelineState:GetDebugComputeShaderState(Inner.device)];
			
			[Inner dispatchThreadgroups:MTLSizeMake(1, 1, 1) threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];

			if (Pipeline && Pipeline.ComputePipelineState)
			{
				[Inner setComputePipelineState:Pipeline.ComputePipelineState];
			}
			
			if (ShaderBuffers.Buffers[0])
			{
				[Inner setBuffer:ShaderBuffers.Buffers[0] offset:ShaderBuffers.Offsets[0] atIndex:0];
			}
			else if (ShaderBuffers.Bytes[0])
			{
				[Inner setBytes:ShaderBuffers.Bytes[0] length:ShaderBuffers.Offsets[0] atIndex:0];
			}
			
			if (ShaderBuffers.Buffers[1])
			{
				[Inner setBuffer:ShaderBuffers.Buffers[1] offset:ShaderBuffers.Offsets[1] atIndex:1];
			}
			else if (ShaderBuffers.Bytes[1])
			{
				[Inner setBytes:ShaderBuffers.Bytes[1] length:ShaderBuffers.Offsets[1] atIndex:1];
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
	[self insertDebugDispatch];
#endif
    [Inner popDebugGroup];
}

- (void)setComputePipelineState:(id <MTLComputePipelineState>)state
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			// [Buffer pipeline:state.label];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:state];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setComputePipelineState:(id <MTLComputePipelineState>)state];
}

- (void)setBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers.Buffers[index] = nil;
			ShaderBuffers.Bytes[index] = bytes;
			ShaderBuffers.Offsets[index] = length;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask.BufferMask = bytes ? (ResourceMask.BufferMask | (1 << (index))) : (ResourceMask.BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
	
    [Inner setBytes:bytes length:length atIndex:index];
}

- (void)setBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers.Buffers[index] = buffer;
			ShaderBuffers.Bytes[index] = nil;
			ShaderBuffers.Offsets[index] = offset;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:buffer];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask.BufferMask = buffer ? (ResourceMask.BufferMask & (1 << (index))) : (ResourceMask.BufferMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
	
    [Inner setBuffer:buffer offset:offset atIndex:index];
}

- (void)setBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderBuffers.Offsets[index] = offset;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		case EMetalDebugLevelFastValidation:
		{
			check(ResourceMask.BufferMask | (1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
	
    [Inner setBufferOffset:offset atIndex:index];
}

- (void)setBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range
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
				ShaderBuffers.Buffers[i + range.location] = buffers[i];
				ShaderBuffers.Bytes[i + range.location] = nil;
				ShaderBuffers.Offsets[i + range.location] = offsets[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackResource:buffers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask.BufferMask = buffers[i] ? (ResourceMask.BufferMask | (1 << (i + range.location))) : (ResourceMask.BufferMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
    }
#endif
    [Inner setBuffers:buffers offsets:offsets withRange:range];
}

- (void)setTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderTextures.Textures[index] = texture;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:texture];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask.TextureMask = texture ? (ResourceMask.TextureMask | (1 << (index))) : (ResourceMask.TextureMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
	
    [Inner setTexture:texture atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range
#else
- (void)setTextures : (const id <MTLTexture> __nullable[__nullable])textures withRange : (NSRange)range
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
				ShaderTextures.Textures[i + range.location] = textures[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackResource:textures[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask.TextureMask = textures[i] ? (ResourceMask.TextureMask | (1 << (i + range.location))) : (ResourceMask.TextureMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setTextures:textures withRange:range];
}

- (void)setSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderSamplers.Samplers[index] = sampler;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:sampler];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask.SamplerMask = sampler ? (ResourceMask.SamplerMask | (1 << (index))) : (ResourceMask.SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setSamplerState:sampler atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range
#else
- (void)setSamplerStates : (const id <MTLSamplerState> __nullable[__nullable])samplers withRange : (NSRange)range
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
				ShaderSamplers.Samplers[i + range.location] = samplers[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackState:samplers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask.SamplerMask = samplers[i] ? (ResourceMask.SamplerMask | (1 << (i + range.location))) : (ResourceMask.SamplerMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setSamplerStates:samplers withRange:range];
}

- (void)setSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index
{
#if METAL_DEBUG_OPTIONS
	switch (Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		case EMetalDebugLevelValidation:
		{
			ShaderSamplers.Samplers[index] = sampler;
		}
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackState:sampler];
		}
		case EMetalDebugLevelFastValidation:
		{
			ResourceMask.SamplerMask = sampler ? (ResourceMask.SamplerMask | (1 << (index))) : (ResourceMask.SamplerMask & ~(1 << (index)));
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner setSamplerState:sampler lodMinClamp:lodMinClamp lodMaxClamp:lodMaxClamp atIndex:index];
}

#if METAL_NEW_NONNULL_DECL
- (void)setSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range
#else
- (void)setSamplerStates : (const id <MTLSamplerState> __nullable[__nullable])samplers lodMinClamps : (const float[__nullable])lodMinClamps lodMaxClamps : (const float[__nullable])lodMaxClamps withRange : (NSRange)range
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
				ShaderSamplers.Samplers[i + range.location] = samplers[i];
			}
			case EMetalDebugLevelResetOnBind:
			case EMetalDebugLevelTrackResources:
			{
				[Buffer trackState:samplers[i]];
			}
			case EMetalDebugLevelFastValidation:
			{
				ResourceMask.SamplerMask = samplers[i] ? (ResourceMask.SamplerMask | (1 << (i + range.location))) : (ResourceMask.SamplerMask & ~(1 << (i + range.location)));
			}
			default:
			{
				break;
			}
		}
		
    }
#endif
    [Inner setSamplerStates:samplers lodMinClamps:lodMinClamps lodMaxClamps:lodMaxClamps withRange:range];
}

- (void)setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index
{
    [Inner setThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index];
}

- (void)setStageInRegion:(MTLRegion)region
{
    [Inner setStageInRegion:(MTLRegion)region];
}

- (void)dispatchThreadgroups:(MTLSize)threadgroupsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer dispatch:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
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
    [Inner dispatchThreadgroups:(MTLSize)threadgroupsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup];
}

- (void)dispatchThreadgroupsWithIndirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer dispatch:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
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
	
    [Inner dispatchThreadgroupsWithIndirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup];
}

#if METAL_SUPPORTS_HEAPS
- (void)updateFence:(id <MTLFence>)fence
{
#if METAL_DEBUG_OPTIONS
	if (fence && Buffer->DebugLevel >= EMetalDebugLevelValidation)
    {
        [self addUpdateFence:fence];
        if (((FMetalDebugFence*)fence).Inner)
        {
            [Inner updateFence:((FMetalDebugFence*)fence).Inner];
        }
	}
	else
#endif
	{
		[Inner updateFence:(id <MTLFence>)fence];
	}
}

- (void)waitForFence:(id <MTLFence>)fence
{
#if METAL_DEBUG_OPTIONS
	if (fence && Buffer->DebugLevel >= EMetalDebugLevelValidation)
	{
		[self addWaitFence:fence];
        if (((FMetalDebugFence*)fence).Inner)
        {
            [Inner waitForFence:((FMetalDebugFence*)fence).Inner];
        }
	}
	else
#endif
	{
		[Inner waitForFence:(id <MTLFence>)fence];
	}
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

-(NSString*) description
{
	return [Inner description];
}

-(NSString*) debugDescription
{
	return [Inner debugDescription];
}

- (void)validate
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
			
			MTLComputePipelineReflection* Reflection = Pipeline.ComputePipelineReflection;
			check(Reflection);
	
			NSArray<MTLArgument*>* Arguments = Reflection.arguments;
			for (uint32 i = 0; i < Arguments.count; i++)
			{
				MTLArgument* Arg = [Arguments objectAtIndex:i];
				check(Arg);
				switch(Arg.type)
				{
					case MTLArgumentTypeBuffer:
					{
						checkf(Arg.index < ML_MaxBuffers, TEXT("Metal buffer index exceeded!"));
						if ((ShaderBuffers.Buffers[Arg.index] == nil && ShaderBuffers.Bytes[Arg.index] == nil))
						{
							UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
							bOK = false;
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
						if (ShaderTextures.Textures[Arg.index] == nil)
						{
							UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index %u  which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
							bOK = false;
						}
						else if (ShaderTextures.Textures[Arg.index].textureType != Arg.textureType)
						{
							UE_LOG(LogMetal, Warning, TEXT("Incorrect texture type bound at Metal index %u which will crash the driver: %s\n%s"), (uint32)Arg.index, *FString([Arg description]), *FString([ShaderTextures.Textures[Arg.index] description]));
							bOK = false;
						}
						break;
					}
					case MTLArgumentTypeSampler:
					{
						checkf(Arg.index < ML_MaxSamplers, TEXT("Metal sampler index exceeded!"));
						if (ShaderSamplers.Samplers[Arg.index] == nil)
						{
							UE_LOG(LogMetal, Warning, TEXT("Unbound sampler at Metal index %u which will crash the driver: %s"), (uint32)Arg.index, *FString([Arg description]));
							bOK = false;
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
			
			FMetalTextureMask TextureMask = (ResourceMask.TextureMask & Pipeline->ResourceMask[EMetalShaderCompute].TextureMask);
			if (TextureMask != Pipeline->ResourceMask[EMetalShaderCompute].TextureMask)
			{
				bOK = false;
				for (uint32 i = 0; i < ML_MaxTextures; i++)
				{
					if ((Pipeline->ResourceMask[EMetalShaderCompute].TextureMask & (1 < i)) && ((TextureMask & (1 < i)) != (Pipeline->ResourceMask[EMetalShaderCompute].TextureMask & (1 < i))))
					{
						UE_LOG(LogMetal, Warning, TEXT("Unbound texture at Metal index %u which will crash the driver"), i);
					}
				}
			}
			
			FMetalBufferMask BufferMask = (ResourceMask.BufferMask & Pipeline->ResourceMask[EMetalShaderCompute].BufferMask);
			if (BufferMask != Pipeline->ResourceMask[EMetalShaderCompute].BufferMask)
			{
				bOK = false;
				for (uint32 i = 0; i < ML_MaxBuffers; i++)
				{
					if ((Pipeline->ResourceMask[EMetalShaderCompute].BufferMask & (1 < i)) && ((BufferMask & (1 < i)) != (Pipeline->ResourceMask[EMetalShaderCompute].BufferMask & (1 < i))))
					{
						UE_LOG(LogMetal, Warning, TEXT("Unbound buffer at Metal index %u which will crash the driver"), i);
					}
				}
			}
			
			FMetalSamplerMask SamplerMask = (ResourceMask.SamplerMask & Pipeline->ResourceMask[EMetalShaderCompute].SamplerMask);
			if (SamplerMask != Pipeline->ResourceMask[EMetalShaderCompute].SamplerMask)
			{
				bOK = false;
				for (uint32 i = 0; i < ML_MaxSamplers; i++)
				{
					if ((Pipeline->ResourceMask[EMetalShaderCompute].SamplerMask & (1 < i)) && ((SamplerMask & (1 < i)) != (Pipeline->ResourceMask[EMetalShaderCompute].SamplerMask & (1 < i))))
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
	
    if (!bOK)
    {
        UE_LOG(LogMetal, Error, TEXT("Metal Validation failures for compute shader:\n%s"), (Pipeline && Pipeline.ComputeSource) ? *FString(Pipeline.ComputeSource) : TEXT("nil"));
    }
#endif
}

-(id<MTLCommandEncoder>)commandEncoder
{
	return self;
}

#if METAL_SUPPORTS_TILE_SHADERS
- (void)setImageblockWidth:(NSUInteger)width height:(NSUInteger)height
{
	if (GMetalSupportsTileShaders)
	{
		[Inner setImageblockWidth:width height:height];
	}
}
#endif

#if METAL_SUPPORTS_CAPTURE_MANAGER && !PLATFORM_TVOS
- (void)dispatchThreads:(MTLSize)threadsPerGrid threadsPerThreadgroup:(MTLSize)threadsPerThreadgroup
{
	if (GMetalSupportsCaptureManager)
	{
		[Inner dispatchThreads:threadsPerGrid threadsPerThreadgroup:threadsPerThreadgroup];
	}
}
#endif

@end

#if !METAL_SUPPORTS_HEAPS
@implementation FMetalDebugComputeCommandEncoder (MTLComputeCommandEncoderExtensions)
-(void) updateFence:(id <MTLFence>)fence
{
#if METAL_DEBUG_OPTIONS
	[self addUpdateFence:fence];
#endif
}
-(void) waitForFence:(id <MTLFence>)fence
{
#if METAL_DEBUG_OPTIONS
	[self addWaitFence:fence];
#endif
}
@end
#endif

NS_ASSUME_NONNULL_END
