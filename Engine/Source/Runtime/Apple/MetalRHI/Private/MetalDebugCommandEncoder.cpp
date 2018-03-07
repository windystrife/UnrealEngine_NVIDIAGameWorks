// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalDebugCommandEncoder.h"
#include "MetalCommandBuffer.h"
#include "MetalFence.h"

extern int32 GMetalRuntimeDebugLevel;

@implementation FMetalDebugCommandEncoder

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugCommandEncoder)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		UpdatedFences = [[NSHashTable weakObjectsHashTable] retain];
		WaitingFences = [[NSHashTable weakObjectsHashTable] retain];
	}
	return Self;
}

-(void)dealloc
{
	[UpdatedFences release];
	[WaitingFences release];
	[super dealloc];
}

-(id<MTLCommandEncoder>)commandEncoder
{
	check(false);
	return nil;
}
-(void)addUpdateFence:(id)Fence
{
	if ((EMetalDebugLevel)GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation && Fence)
	{
		[UpdatedFences addObject:(FMetalDebugFence*)Fence];
		[(FMetalDebugFence*)Fence updatingEncoder:self];
	}
}
-(void)addWaitFence:(id)Fence
{
	if ((EMetalDebugLevel)GMetalRuntimeDebugLevel >= EMetalDebugLevelValidation && Fence)
	{
		[WaitingFences addObject:(FMetalDebugFence*)Fence];
		[(FMetalDebugFence*)Fence waitingEncoder:self];
	}
}
@end
