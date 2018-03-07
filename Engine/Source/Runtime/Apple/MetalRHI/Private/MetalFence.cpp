// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"

#include "MetalFence.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandQueue.h"
#include "MetalContext.h"

@implementation FMetalDebugFence
@synthesize Inner;

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDebugFence)

-(instancetype)init
{
	id Self = [super init];
	if (Self)
	{
		Label = nil;
	}
	return Self;
}

-(void)dealloc
{
	[self validate];
	FMetalDebugCommandEncoder* Encoder = nil;
	while ((Encoder = UpdatingEncoders.Pop()))
	{
		[Encoder release];
	}
	Encoder = nil;
	while ((Encoder = WaitingEncoders.Pop()))
	{
		[Encoder release];
	}
	[Label release];
	[super dealloc];
}

-(id <MTLDevice>) device
{
	if (Inner)
	{
		return Inner.device;
	}
	else
	{
		return nil;
	}
}

-(NSString *_Nullable)label
{
	return Label;
}

-(void)setLabel:(NSString *_Nullable)Text
{
	[Text retain];
	[Label release];
	Label = Text;
	if(Inner)
	{
		Inner.label = Text;
	}
}

-(void)validate
{
	UE_CLOG(UpdatingEncoders.IsEmpty() != WaitingEncoders.IsEmpty(), LogMetal, Fatal, TEXT("Fence with unmatched updates/waits destructed - there's a gap in fence (%p) %s"), self, Label ? *FString(Label) : TEXT("Null"));
}

-(void)updatingEncoder:(FMetalDebugCommandEncoder*)Encoder
{
	check(Encoder);
	UpdatingEncoders.Push([Encoder retain]);
}

-(void)waitingEncoder:(FMetalDebugCommandEncoder*)Encoder
{
	check(Encoder);
	WaitingEncoders.Push([Encoder retain]);
}

-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)updatingEncoders
{
	return &UpdatingEncoders;
}

-(TLockFreePointerListLIFO<FMetalDebugCommandEncoder>*)waitingEncoders
{
	return &WaitingEncoders;
}
@end

#if METAL_DEBUG_OPTIONS
void FMetalFence::Validate(void) const
{
	if (GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation && Object)
	{
		[(FMetalDebugFence*)Object validate];
	}
}
#endif
