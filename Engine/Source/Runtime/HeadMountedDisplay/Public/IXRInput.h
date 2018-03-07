// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HeadMountedDisplayTypes.h"
#include "InputCoreTypes.h"

/** 
 * Optional interface returned from IXRTrackingSystem if the plugin requires being able to grab touch or keyboard input events.
 */
class HEADMOUNTEDDISPLAY_API IXRInput
{
public:
	/**
	* Passing key events to HMD.
	* If returns 'false' then key will be handled by PlayerController;
	* otherwise, key won't be handled by the PlayerController.
	*/
	virtual bool HandleInputKey(class UPlayerInput*, const struct FKey& Key, enum EInputEvent EventType, float AmountDepressed, bool bGamepad) { return false; }

	/**
	* Passing touch events to HMD.
	* If returns 'false' then touch will be handled by PlayerController;
	* otherwise, touch won't be handled by the PlayerController.
	*/
	virtual bool HandleInputTouch(uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, FDateTime DeviceTimestamp, uint32 TouchpadIndex) { return false; }
};

