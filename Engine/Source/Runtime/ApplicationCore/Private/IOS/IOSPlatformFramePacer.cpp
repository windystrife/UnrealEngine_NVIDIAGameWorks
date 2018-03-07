// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformFramePacer.h"
#include "Containers/Array.h"
#include "ThreadingBase.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"

// Collection of events listening for this trigger.
static TArray<FEvent*> ListeningEvents;
static FCriticalSection HandlersMutex;
static NSMutableSet<FIOSFramePacerHandler>* Handlers = [NSMutableSet new];

/*******************************************************************
 * FIOSFramePacer implementation
 *******************************************************************/

@interface FIOSFramePacer : NSObject
{
    @public
	FEvent *FramePacerEvent;
}

-(void)run:(id)param;
-(void)signal:(id)param;

@end

// @todo ios: Move these up into some shared header
// __TV_OS_VERSION_MAX_ALLOWED is only defined when building for tvos, so we can use that to determine
#if PLATFORM_TVOS

#define UE4_HAS_IOS10 (defined(__TVOS_10_0) && __TV_OS_VERSION_MAX_ALLOWED >= __TVOS_10_0)
#define UE4_HAS_IOS9 (defined(__TVOS_9_0) && __TV_OS_VERSION_MAX_ALLOWED >= __TVOS_9_0)
#define UE4_TARGET_PRE_IOS10 (!defined(__TVOS_10_0_) || __TV_OS_VERSION_MIN_REQUIRED < __TVOS_10_0)
#define UE4_TARGET_PRE_IOS9 (!defined(__TVOS_9_0_) || __TV_OS_VERSION_MIN_REQUIRED < __TVOS_9_0)

#else

#define UE4_HAS_IOS10 (defined(__IPHONE_10_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0)
#define UE4_HAS_IOS9 (defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0)
#define UE4_TARGET_PRE_IOS10 (!defined(__IPHONE_10_0_) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0)
#define UE4_TARGET_PRE_IOS9 (!defined(__IPHONE_9_0_) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0)

#endif

@implementation FIOSFramePacer

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
-(void)run:(id)param
{
	NSRunLoop *runloop = [NSRunLoop currentRunLoop];
	CADisplayLink *displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(signal:)];
#if UE4_HAS_IOS10
	if ([displayLink respondsToSelector : @selector(preferredFramesPerSecond)] == YES)
	{
		displayLink.preferredFramesPerSecond = 60 / FIOSPlatformRHIFramePacer::FrameInterval;
	}
	else
#endif
	{
#if UE4_TARGET_PRE_IOS10
		displayLink.frameInterval = FIOSPlatformRHIFramePacer::FrameInterval;
#endif
	}

	[displayLink addToRunLoop:runloop forMode:NSDefaultRunLoopMode];
	[runloop run];
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif


-(void)signal:(id)param
{
	{
		FScopeLock Lock(&HandlersMutex);
		for (FIOSFramePacerHandler Handler in Handlers)
		{
			Handler(0);
		}
	}	
    for( auto& NextEvent : ListeningEvents )
    {
        NextEvent->Trigger();
    }
}

@end



/*******************************************************************
 * FIOSPlatformRHIFramePacer implementation
 *******************************************************************/


namespace IOSDisplayConstants
{
    const uint32 MaxRefreshRate = 60;
}

uint32 FIOSPlatformRHIFramePacer::FrameInterval = 1;
FIOSFramePacer* FIOSPlatformRHIFramePacer::FramePacer = nil;


bool FIOSPlatformRHIFramePacer::IsEnabled()
{
    static bool bIsRHIFramePacerEnabled = false;
	static bool bInitialized = false;

	if (!bInitialized)
	{
		FString FrameRateLockAsEnum;
		GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("FrameRateLock"), FrameRateLockAsEnum, GEngineIni);

		uint32 FrameRateLock = 60;
		FParse::Value(*FrameRateLockAsEnum, TEXT("PUFRL_"), FrameRateLock);
		if (FrameRateLock == 0)
		{
			FrameRateLock = 60;
		}

		if (!bIsRHIFramePacerEnabled)
		{
			check((IOSDisplayConstants::MaxRefreshRate % FrameRateLock) == 0);
			FrameInterval = IOSDisplayConstants::MaxRefreshRate / FrameRateLock;

			bIsRHIFramePacerEnabled = (FrameInterval > 0);
		}
		bInitialized = true;
	}
	
	return bIsRHIFramePacerEnabled;
}

void FIOSPlatformRHIFramePacer::InitWithEvent(FEvent* TriggeredEvent)
{
    // Create display link thread
    FramePacer = [[FIOSFramePacer alloc] init];
    [NSThread detachNewThreadSelector:@selector(run:) toTarget:FramePacer withObject:nil];
        
    // Only one supported for now, we may want more eventually.
    ListeningEvents.Add( TriggeredEvent );
}

void FIOSPlatformRHIFramePacer::AddHandler(FIOSFramePacerHandler Handler)
{
	check (FramePacer);
	FScopeLock Lock(&HandlersMutex);
	FIOSFramePacerHandler Copy = Block_copy(Handler);
	[Handlers addObject:Copy];
	Block_release(Copy);
}

void FIOSPlatformRHIFramePacer::RemoveHandler(FIOSFramePacerHandler Handler)
{
	check (FramePacer);
	FScopeLock Lock(&HandlersMutex);
	[Handlers removeObject:Handler];
}

void FIOSPlatformRHIFramePacer::Suspend()
{
    // send a signal to the events if we are enabled
    if (IsEnabled())
    {
        [FramePacer signal:0];
    }
}

void FIOSPlatformRHIFramePacer::Resume()
{
    
}

void FIOSPlatformRHIFramePacer::Destroy()
{
    if( FramePacer != nil )
    {
        [FramePacer release];
        FramePacer = nil;
    }
}