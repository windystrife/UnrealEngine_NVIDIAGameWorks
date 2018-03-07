// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	MacPlatformFramePacer.h: Apple Mac platform frame pacer classes.
==============================================================================================*/


#pragma once
#include "GenericPlatform/GenericPlatformFramePacer.h"

struct FMacFramePacer;

typedef void (^FMacFramePacerHandler)(uint32 CGDirectDisplayID);

/**
 * Mac implementation of FGenericPlatformRHIFramePacer
 **/
struct FMacPlatformRHIFramePacer : public FGenericPlatformRHIFramePacer
{
    // FGenericPlatformRHIFramePacer interface
    static bool IsEnabled();
	static void InitWithEvent(class FEvent* TriggeredEvent);
	static void AddHandler(FMacFramePacerHandler Handler);
	static void AddEvent(uint32 CGDirectDisplayID, class FEvent* TriggeredEvent);
	static void RemoveHandler(FMacFramePacerHandler Handler);
    static void Destroy();
    
    /** Access to the Mac Frame Pacer: CVDisplayLink */
    static FMacFramePacer* FramePacer;
};


typedef FMacPlatformRHIFramePacer FPlatformRHIFramePacer;
typedef FMacFramePacerHandler FPlatformRHIFramePacerHandler;