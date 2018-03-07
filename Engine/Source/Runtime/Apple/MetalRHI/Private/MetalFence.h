// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#import <Metal/Metal.h>

#pragma clang diagnostic ignored "-Wnullability-completeness"

#if !METAL_SUPPORTS_HEAPS

@protocol IMTLFence <NSObject>
@property (nonnull, readonly) id <MTLDevice> device;
@property (nullable, copy, atomic) NSString *label;
@end

/*!
 @abstract Point at which a fence may be waited on or signaled.
 @constant MTLRenderStageVertex All vertex work prior to rasterization has completed.
 @constant MTLRenderStageFragment All rendering work has completed.
 */
typedef NS_OPTIONS(NSUInteger, EMTLRenderStages) {
	EMTLRenderStageVertex   = (1UL << 0),
	EMTLRenderStageFragment = (1UL << 1),
};

#define MTLRenderStages EMTLRenderStages
#define MTLRenderStageVertex EMTLRenderStageVertex
#define MTLRenderStageFragment EMTLRenderStageFragment

#define MTLFence IMTLFence

#endif

@class FMetalDebugCommandEncoder;

@interface FMetalDebugFence : FApplePlatformObject<MTLFence>
{
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> UpdatingEncoders;
	TLockFreePointerListLIFO<FMetalDebugCommandEncoder> WaitingEncoders;
	NSString* Label;
}
@property (retain) id<MTLFence> Inner;
-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders;
-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders;
-(void)validate;
@end

@protocol MTLDeviceExtensions <MTLDevice>
/*!
 @method newFence
 @abstract Create a new MTLFence object
 */
- (id <MTLFence>)newFence;
@end

@protocol MTLBlitCommandEncoderExtensions <MTLBlitCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLComputeCommandEncoderExtensions <MTLComputeCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder. */
-(void) updateFence:(id <MTLFence>)fence;
/*!
 @abstract Prevent further GPU work until the event is reached. */
-(void) waitForFence:(id <MTLFence>)fence;
@end
@protocol MTLRenderCommandEncoderExtensions <MTLRenderCommandEncoder>
/*!
 @abstract Update the event to capture all GPU work so far enqueued by this encoder for the given
 stages.
 @discussion Unlike <st>updateFence:</st>, this method will update the event when the given stage(s) complete, allowing for commands to overlap in execution.
 */
-(void) updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages;
/*!
 @abstract Prevent further GPU work until the event is reached for the given stages.
 @discussion Unlike <st>waitForFence:</st>, this method will only block commands assoicated with the given stage(s), allowing for commands to overlap in execution.
 */
-(void) waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages;
@end

class FMetalFence
{
public:
	FMetalFence()
	: Object(nil)
	{
		
	}
	
	explicit FMetalFence(id<MTLFence> Obj)
	: Object(Obj)
	{
		
	}
	
	explicit FMetalFence(FMetalFence const& Other)
	: Object(nil)
	{
		operator=(Other);
	}
	
	~FMetalFence()
	{
		SafeReleaseMetalFence(Object);
	}
	
	FMetalFence& operator=(FMetalFence const& Other)
	{
		if (&Other != this)
		{
			operator=(Other.Object);
		}
		return *this;
	}
	
	FMetalFence& operator=(id<MTLFence> const& Other)
	{
		if (Other != Object)
		{
			METAL_DEBUG_OPTION(Validate());
			[Other retain];
			SafeReleaseMetalFence(Object);
			Object = Other;
		}
		return *this;
	}
	
	operator id<MTLFence>() const
	{
		return Object;
	}
	
	id<MTLFence> operator*() const
	{
		return Object;
	}
	
#if METAL_DEBUG_OPTIONS
	void Validate(void) const;
#endif
	
	void Reset(void)
	{
		[Object release];
		Object = nil;
	}
	
private:
	id<MTLFence> Object;
};
