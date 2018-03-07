// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	IOSPlatformFramePacer.h: Apple iOS platform frame pacer classes.
==============================================================================================*/


#pragma once
#include "GenericPlatform/GenericPlatformFramePacer.h"

// Forward declare the ios frame pacer class we will be using.
@class FIOSFramePacer;

typedef void (^FIOSFramePacerHandler)(uint32 IgnoredId);

/**
 * iOS implementation of FGenericPlatformRHIFramePacer
 **/
struct FIOSPlatformRHIFramePacer : public FGenericPlatformRHIFramePacer
{
    // FGenericPlatformRHIFramePacer interface
    static bool IsEnabled();
	static void InitWithEvent(class FEvent* TriggeredEvent);
	static void AddHandler(FIOSFramePacerHandler Handler);
	static void RemoveHandler(FIOSFramePacerHandler Handler);
    static void Destroy();
    
    /** Access to the IOS Frame Pacer: CADisplayLink */
    static FIOSFramePacer* FramePacer;
    
    /** Number of frames before the CADisplayLink triggers it's readied callback */
    static uint32 FrameInterval;
    
    /** Suspend the frame pacer so we can enter the background state */
    static void Suspend();
    
    /** Resume the frame pacer so we can enter the foreground state */
    static void Resume();
};


typedef FIOSPlatformRHIFramePacer FPlatformRHIFramePacer;
typedef FIOSFramePacerHandler FPlatformRHIFramePacerHandler;