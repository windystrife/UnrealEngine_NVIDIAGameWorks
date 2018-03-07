// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalParallelRenderCommandEncoder.cpp: Metal command encoder wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalParallelRenderCommandEncoder.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalCommandBuffer.h"

NS_ASSUME_NONNULL_BEGIN

@implementation FMetalDebugParallelRenderCommandEncoder

@synthesize Inner;
@synthesize Buffer;
@synthesize RenderPassDescriptor;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugParallelRenderCommandEncoder)

-(id)initWithEncoder:(id<MTLParallelRenderCommandEncoder>)Encoder andCommandBuffer:(FMetalDebugCommandBuffer*)SourceBuffer withDescriptor:(MTLRenderPassDescriptor *)Desc
{
	id Self = [super init];
	if (Self)
	{
        Inner = [Encoder retain];
		Buffer = [SourceBuffer retain];
		RenderPassDescriptor = [Desc retain];
	}
	return Self;
}

-(void)dealloc
{
	[Inner release];
	[Buffer release];
	[RenderPassDescriptor release];
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

- (void)popDebugGroup
{
    [Buffer popDebugGroup];
    [Inner popDebugGroup];
}

#if METAL_NEW_NONNULL_DECL
- (nullable id <MTLRenderCommandEncoder>)renderCommandEncoder
#else
- (id <MTLRenderCommandEncoder>)renderCommandEncoder
#endif
{
	return [[[FMetalDebugRenderCommandEncoder alloc] initWithEncoder:[Inner renderCommandEncoder] fromDescriptor:RenderPassDescriptor andCommandBuffer:Buffer] autorelease];
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

@end

NS_ASSUME_NONNULL_END
