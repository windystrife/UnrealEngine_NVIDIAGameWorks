// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandBuffere.cpp: Metal command buffer wrapper.
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandBuffer.h"
#include "MetalRenderCommandEncoder.h"
#include "MetalParallelRenderCommandEncoder.h"
#include "MetalBlitCommandEncoder.h"
#include "MetalComputeCommandEncoder.h"
#include <objc/runtime.h>

NSString* GMetalDebugCommandTypeNames[EMetalDebugCommandTypeInvalid] = {
	@"RenderEncoder",
	@"ComputeEncoder",
	@"BlitEncoder",
    @"EndEncoder",
    @"Pipeline",
	@"Draw",
	@"Dispatch",
	@"Blit",
	@"Signpost",
	@"PushGroup",
	@"PopGroup"
};

extern int32 GMetalRuntimeDebugLevel;

@implementation NSObject (IMetalDebugGroupAssociation)
@dynamic debugGroups;
- (void)setDebugGroups:(NSMutableArray<NSString*>*)Data
{
	objc_setAssociatedObject(self, @selector(debugGroups), Data, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (NSMutableArray<NSString*>*)debugGroups
{
	return (NSMutableArray<NSString*>*)objc_getAssociatedObject(self, @selector(debugGroups));
}
@end

@implementation FMetalDebugCommandBuffer

@synthesize InnerBuffer;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugCommandBuffer)

-(id)initWithCommandBuffer:(id<MTLCommandBuffer>)Buffer
{
	id Self = [super init];
	if (Self)
	{
        DebugLevel = (EMetalDebugLevel)GMetalRuntimeDebugLevel;
		InnerBuffer = Buffer;
		DebugGroup = [NSMutableArray new];
		ActiveEncoder = nil;
		DebugInfoBuffer = nil;
		if (DebugLevel >= EMetalDebugLevelValidation)
		{
			DebugInfoBuffer = [Buffer.device newBufferWithLength:BufferOffsetAlignment options:0];
		}
	}
	return Self;
}

-(void)dealloc
{
	for (FMetalDebugCommand* Command : DebugCommands)
	{
		[Command->Label release];
		[Command->PassDesc release];
		delete Command;
	}
	
	[InnerBuffer release];
	[DebugGroup release];
	[DebugInfoBuffer release];
	DebugInfoBuffer = nil;
	
	[super dealloc];
}

-(id <MTLDevice>) device
{
	return InnerBuffer.device;
}

-(id <MTLCommandQueue>) commandQueue
{
	return InnerBuffer.commandQueue;
}

-(BOOL) retainedReferences
{
	return InnerBuffer.retainedReferences;
}

-(NSString *)label
{
	return InnerBuffer.label;
}

-(void)setLabel:(NSString *)Text
{
	InnerBuffer.label = Text;
}

-(MTLCommandBufferStatus) status
{
	return InnerBuffer.status;
}

-(NSError *)error
{
	return InnerBuffer.error;
}

- (void)enqueue
{
	[InnerBuffer enqueue];
}

- (void)commit
{
	[InnerBuffer commit];
}

- (void)addScheduledHandler:(MTLCommandBufferHandler)block
{
	[InnerBuffer addScheduledHandler:^(id <MTLCommandBuffer>){
		block(self);
	 }];
}

- (void)presentDrawable:(id <MTLDrawable>)drawable
{
    [InnerBuffer presentDrawable:drawable];
}

#if !PLATFORM_MAC
- (void)presentDrawable:(id <MTLDrawable>)drawable afterMinimumDuration:(CFTimeInterval)duration
{
#if (__clang_major__ > 8) || (__clang_major__ == 8 && __clang_minor__ >= 1)
    [InnerBuffer presentDrawable:drawable afterMinimumDuration:duration];
#endif
}
#endif

- (void)presentDrawable:(id <MTLDrawable>)drawable atTime:(CFTimeInterval)presentationTime
{
    [InnerBuffer presentDrawable:drawable atTime:presentationTime];
}

- (void)waitUntilScheduled
{
	[InnerBuffer waitUntilScheduled];
}

- (void)addCompletedHandler:(MTLCommandBufferHandler)block
{
	[InnerBuffer addCompletedHandler:^(id <MTLCommandBuffer>){
		block(self);
	}];
}

- (void)waitUntilCompleted
{
	[InnerBuffer waitUntilCompleted];
}

- (id <MTLBlitCommandEncoder>)blitCommandEncoder
{
	[self beginBlitCommandEncoder:DebugGroup.lastObject ? DebugGroup.lastObject : @"Blit"];
	return [[[FMetalDebugBlitCommandEncoder alloc] initWithEncoder:[InnerBuffer blitCommandEncoder] andCommandBuffer:self] autorelease];
}

- (id <MTLRenderCommandEncoder>)renderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor
{
    [self beginRenderCommandEncoder:DebugGroup.lastObject ? DebugGroup.lastObject : @"Render" withDescriptor:renderPassDescriptor];
	return [[[FMetalDebugRenderCommandEncoder alloc] initWithEncoder:[InnerBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor] fromDescriptor:renderPassDescriptor andCommandBuffer:self] autorelease];
}

- (id <MTLComputeCommandEncoder>)computeCommandEncoder
{
    [self beginComputeCommandEncoder:DebugGroup.lastObject ? DebugGroup.lastObject : @"Compute"];
    return [[[FMetalDebugComputeCommandEncoder alloc] initWithEncoder:[InnerBuffer computeCommandEncoder] andCommandBuffer:self] autorelease];
}

- (id <MTLParallelRenderCommandEncoder>)parallelRenderCommandEncoderWithDescriptor:(MTLRenderPassDescriptor *)renderPassDescriptor
{
    [self beginRenderCommandEncoder:DebugGroup.lastObject ? DebugGroup.lastObject : @"Parallel Render" withDescriptor:renderPassDescriptor];
    return [[[FMetalDebugParallelRenderCommandEncoder alloc] initWithEncoder:[InnerBuffer parallelRenderCommandEncoderWithDescriptor:renderPassDescriptor] andCommandBuffer:self withDescriptor:renderPassDescriptor] autorelease];
}

-(NSString*) description
{
	NSMutableString* String = [NSMutableString new];
	NSString* Label = self.label ? self.label : @"Unknown";
	[String appendFormat:@"Command Buffer %p %@:", self, Label];
	return String;
}

-(NSString*) debugDescription
{
	NSMutableString* String = [NSMutableString new];
	NSString* Label = self.label ? self.label : @"Unknown";
	[String appendFormat:@"Command Buffer %p %@:", self, Label];

	uint32 Index = 0;
	if (DebugInfoBuffer)
	{
		Index = *((uint32*)DebugInfoBuffer.contents);
	}
	
	uint32 Count = 1;
	for (FMetalDebugCommand* Command : DebugCommands)
	{
		if (Index == Count++)
		{
			[String appendFormat:@"\n\t--> %@: %@", GMetalDebugCommandTypeNames[Command->Type], Command->Label];
		}
		else
		{
			[String appendFormat:@"\n\t%@: %@", GMetalDebugCommandTypeNames[Command->Type], Command->Label];
		}
	}
	
	[String appendFormat:@"\nResources:"];
	
	for (id<MTLResource> Resource : Resources)
	{
		[String appendFormat:@"\n\t%@ (%d): %@", Resource.label, (uint32)[Resource retainCount], [Resource description]];
	}
	
	[String appendFormat:@"\nStates:"];
	
	for (id State : States)
	{
		[String appendFormat:@"\n\t%@ (%d): %@", ([State respondsToSelector:@selector(label)] ? [State label] : @"(null)"), (uint32)[State retainCount], [State description]];
	}
	
	return String;
}

-(void) trackResource:(id<MTLResource>)Resource
{
    if (DebugLevel >= EMetalDebugLevelValidation)
    {
        Resources.Add(Resource);
    }
}

-(void) trackState:(id)State
{
    if (DebugLevel >= EMetalDebugLevelValidation)
    {
        States.Add(State);
    }
}

-(void) beginRenderCommandEncoder:(NSString*)Label withDescriptor:(MTLRenderPassDescriptor*)RenderDesc
{
    if (DebugLevel >= EMetalDebugLevelValidation)
    {
        if (DebugLevel >= EMetalDebugLevelLogOperations)
        {
            check(!ActiveEncoder);
            ActiveEncoder = [Label retain];
            FMetalDebugCommand* Command = new FMetalDebugCommand;
            Command->Type = EMetalDebugCommandTypeRenderEncoder;
            Command->Label = [ActiveEncoder retain];
            Command->PassDesc = [RenderDesc retain];
            DebugCommands.Add(Command);
        }
        
        if(RenderDesc.colorAttachments)
        {
            for(uint i = 0; i< 8; i++)
            {
                MTLRenderPassColorAttachmentDescriptor* Desc = [RenderDesc.colorAttachments objectAtIndexedSubscript:i];
                if(Desc)
                {
                    [self trackResource:Desc.texture];
                }
            }
        }
        if(RenderDesc.depthAttachment)
        {
            [self trackResource:RenderDesc.depthAttachment.texture];
        }
        if(RenderDesc.stencilAttachment)
        {
            [self trackResource:RenderDesc.stencilAttachment.texture];
        }
        if(RenderDesc.visibilityResultBuffer)
        {
            [self trackResource:RenderDesc.visibilityResultBuffer];
        }
    }
}

-(void) beginComputeCommandEncoder:(NSString*)Label
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        check(!ActiveEncoder);
        ActiveEncoder = [Label retain];
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeComputeEncoder;
        Command->Label = [ActiveEncoder retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) beginBlitCommandEncoder:(NSString*)Label
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        check(!ActiveEncoder);
        ActiveEncoder = [Label retain];
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeBlitEncoder;
        Command->Label = [ActiveEncoder retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) endCommandEncoder
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        check(ActiveEncoder);
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeEndEncoder;
        Command->Label = ActiveEncoder;
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
        ActiveEncoder = nil;
    }
}

-(void) setPipeline:(NSString*)Desc
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypePipeline;
        Command->Label = [Desc retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) draw:(NSString*)Desc
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeDraw;
        Command->Label = [Desc retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) dispatch:(NSString*)Desc
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeDispatch;
        Command->Label = [Desc retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) blit:(NSString*)Desc
{
    if (DebugLevel >= EMetalDebugLevelLogOperations)
    {
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeBlit;
        Command->Label = [Desc retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) insertDebugSignpost:(NSString*)Label
{
    if (DebugLevel >= EMetalDebugLevelLogDebugGroups)
    {
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypeSignpost;
        Command->Label = [Label retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) pushDebugGroup:(NSString*)Group
{
    if (DebugLevel >= EMetalDebugLevelLogDebugGroups)
    {
        [DebugGroup addObject:Group];
        FMetalDebugCommand* Command = new FMetalDebugCommand;
        Command->Type = EMetalDebugCommandTypePushGroup;
        Command->Label = [Group retain];
        Command->PassDesc = nil;
        DebugCommands.Add(Command);
    }
}

-(void) popDebugGroup
{
    if (DebugLevel >= EMetalDebugLevelLogDebugGroups)
    {
        if (DebugGroup.lastObject)
        {
            FMetalDebugCommand* Command = new FMetalDebugCommand;
            Command->Type = EMetalDebugCommandTypePopGroup;
            Command->Label = [DebugGroup.lastObject retain];
            Command->PassDesc = nil;
            DebugCommands.Add(Command);
        }
    }
}

-(CFTimeInterval) kernelStartTime
{
#if METAL_NEW_NONNULL_DECL
	if (GMetalCommandBufferHasStartEndTimeAPI)
#endif
	{
		return ((id<IMetalCommandBufferExtensions>)InnerBuffer).kernelStartTime;
	}
#if METAL_NEW_NONNULL_DECL
	else
	{
		return 0;
	}
#endif
}

-(CFTimeInterval) kernelEndTime
{
#if METAL_NEW_NONNULL_DECL
	if (GMetalCommandBufferHasStartEndTimeAPI)
#endif
	{
		return ((id<IMetalCommandBufferExtensions>)InnerBuffer).kernelEndTime;
	}
#if METAL_NEW_NONNULL_DECL
	else
	{
		return 0;
	}
#endif
}

-(CFTimeInterval) GPUStartTime
{
#if METAL_NEW_NONNULL_DECL
	if (GMetalCommandBufferHasStartEndTimeAPI)
#endif
	{
		return ((id<IMetalCommandBufferExtensions>)InnerBuffer).GPUStartTime;
	}
#if METAL_NEW_NONNULL_DECL
	else
	{
		return 0;
	}
#endif
}

-(CFTimeInterval) GPUEndTime
{
#if METAL_NEW_NONNULL_DECL
	if (GMetalCommandBufferHasStartEndTimeAPI)
#endif
	{
		return ((id<IMetalCommandBufferExtensions>)InnerBuffer).GPUEndTime;
	}
#if METAL_NEW_NONNULL_DECL
	else
	{
		return 0;
	}
#endif
}

@end

