// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalBlitCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalBlitCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalFence.h"

extern int32 GMetalRuntimeDebugLevel;

@implementation FMetalDebugBlitCommandEncoder

@synthesize Inner;
@synthesize Buffer;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugBlitCommandEncoder)

-(id)initWithEncoder:(id<MTLBlitCommandEncoder>)Encoder andCommandBuffer:(FMetalDebugCommandBuffer*)SourceBuffer
{
	id Self = [super init];
	if (Self)
	{
        Inner = [Encoder retain];
		Buffer = [SourceBuffer retain];
	}
	return Self;
}

-(void)dealloc
{
	[Inner release];
	[Buffer release];
	[super dealloc];
}

-(id <MTLDevice>) device
{
	return Inner.device;
}

-(NSString *)label
{
	return Inner.label;
}

-(void)setLabel:(NSString *)Text
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

- (void)popDebugGroup
{
    [Buffer popDebugGroup];
    [Inner popDebugGroup];
}

#if PLATFORM_MAC
- (void)synchronizeResource:(id<MTLResource>)resource
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:resource];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner synchronizeResource:(id<MTLResource>)resource];
}

- (void)synchronizeTexture:(id<MTLTexture>)texture slice:(NSUInteger)slice level:(NSUInteger)level
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:texture];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner synchronizeTexture:(id<MTLTexture>)texture slice:(NSUInteger)slice level:(NSUInteger)level];
}
#endif

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:sourceTexture];
			[Buffer trackResource:destinationTexture];
		}
		default:
		{
			break;
		}
	}
#endif
    
    [Inner copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin];
}

- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset sourceBytesPerRow:(NSUInteger)sourceBytesPerRow sourceBytesPerImage:(NSUInteger)sourceBytesPerImage sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:sourceBuffer];
			[Buffer trackResource:destinationTexture];
		}
		default:
		{
			break;
		}
	}
#endif
    
    [Inner copyFromBuffer:(id<MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset sourceBytesPerRow:(NSUInteger)sourceBytesPerRow sourceBytesPerImage:(NSUInteger)sourceBytesPerImage sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin];
}

- (void)copyFromBuffer:(id<MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset sourceBytesPerRow:(NSUInteger)sourceBytesPerRow sourceBytesPerImage:(NSUInteger)sourceBytesPerImage sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin options:(MTLBlitOption)options
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:sourceBuffer];
			[Buffer trackResource:destinationTexture];
		}
		default:
		{
			break;
		}
	}
#endif
    
    [Inner copyFromBuffer:(id<MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset sourceBytesPerRow:(NSUInteger)sourceBytesPerRow sourceBytesPerImage:(NSUInteger)sourceBytesPerImage sourceSize:(MTLSize)sourceSize toTexture:(id<MTLTexture>)destinationTexture destinationSlice:(NSUInteger)destinationSlice destinationLevel:(NSUInteger)destinationLevel destinationOrigin:(MTLOrigin)destinationOrigin options:(MTLBlitOption)options];
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toBuffer:(id<MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset destinationBytesPerRow:(NSUInteger)destinationBytesPerRow destinationBytesPerImage:(NSUInteger)destinationBytesPerImage
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:sourceTexture];
			[Buffer trackResource:destinationBuffer];
		}
		default:
		{
			break;
		}
	}
#endif
    
    [Inner copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toBuffer:(id<MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset destinationBytesPerRow:(NSUInteger)destinationBytesPerRow destinationBytesPerImage:(NSUInteger)destinationBytesPerImage];
}

- (void)copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toBuffer:(id<MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset destinationBytesPerRow:(NSUInteger)destinationBytesPerRow destinationBytesPerImage:(NSUInteger)destinationBytesPerImage options:(MTLBlitOption)options
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:sourceTexture];
			[Buffer trackResource:destinationBuffer];
		}
		default:
		{
			break;
		}
	}
#endif
    
    [Inner copyFromTexture:(id<MTLTexture>)sourceTexture sourceSlice:(NSUInteger)sourceSlice sourceLevel:(NSUInteger)sourceLevel sourceOrigin:(MTLOrigin)sourceOrigin sourceSize:(MTLSize)sourceSize toBuffer:(id<MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset destinationBytesPerRow:(NSUInteger)destinationBytesPerRow destinationBytesPerImage:(NSUInteger)destinationBytesPerImage options:(MTLBlitOption)options];
}

- (void)generateMipmapsForTexture:(id<MTLTexture>)texture
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:texture];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner generateMipmapsForTexture:(id<MTLTexture>)texture];
}

- (void)fillBuffer:(id <MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:buffer];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner fillBuffer:(id <MTLBuffer>)buffer range:(NSRange)range value:(uint8_t)value];
}

- (void)copyFromBuffer:(id <MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset toBuffer:(id <MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset size:(NSUInteger)size
{
#if METAL_DEBUG_OPTIONS
	switch(Buffer->DebugLevel)
	{
		case EMetalDebugLevelConditionalSubmit:
		case EMetalDebugLevelWaitForComplete:
		case EMetalDebugLevelLogOperations:
		{
			[Buffer blit:[NSString stringWithFormat:@"%s", __PRETTY_FUNCTION__]];
		}
		case EMetalDebugLevelValidation:
		case EMetalDebugLevelResetOnBind:
		case EMetalDebugLevelTrackResources:
		{
			[Buffer trackResource:sourceBuffer];
			[Buffer trackResource:destinationBuffer];
		}
		default:
		{
			break;
		}
	}
#endif
    [Inner copyFromBuffer:(id <MTLBuffer>)sourceBuffer sourceOffset:(NSUInteger)sourceOffset toBuffer:(id <MTLBuffer>)destinationBuffer destinationOffset:(NSUInteger)destinationOffset size:(NSUInteger)size];
}

#if METAL_SUPPORTS_HEAPS
- (void)updateFence:(id <MTLFence>)fence
{
#if METAL_DEBUG_OPTIONS
	if (fence && (EMetalDebugLevel)GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
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
	if (fence && (EMetalDebugLevel)GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation)
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

-(NSString*) description
{
	return [Inner description];
}

-(NSString*) debugDescription
{
	return [Inner debugDescription];
}

-(id<MTLCommandEncoder>)commandEncoder
{
	return self;
}

@end

#if !METAL_SUPPORTS_HEAPS
@implementation FMetalDebugBlitCommandEncoder (MTLBlitCommandEncoderExtensions)
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
