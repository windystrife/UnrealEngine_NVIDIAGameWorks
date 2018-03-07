// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	GenericPlatformFramePacer.h: Generic platform frame pacer classes
==============================================================================================*/


#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"

/**
 * Generic implementation for most platforms, these tend to be unused and unimplemented
 **/
struct APPLICATIONCORE_API FGenericPlatformRHIFramePacer
{
    /**
     * Should the Frame Pacer be enabled?
     */
    bool IsEnabled() { return false; }
    
    /**
     * Init the RHI frame pacer
     *
     * @param InTriggeredEvent - The event we wish to trigger when the frame interval has been triggered by the hardware.
     * @param InFrameInterval - How often should the event be triggered, in Frames.
     */
	static void InitWithEvent(FEvent* InTriggeredEvent) {}
    
    /**
     * Teardown the Frame Pacer.
     */
    static void Destroy() {}
};
