// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"

struct FForceFeedbackValues;
enum class FForceFeedbackChannelType;

/**
 * Input device interface.
 * Useful for plugins/modules to support custom external input devices.
 */
class IInputDevice
{
public:
	virtual ~IInputDevice() {}

	/** Tick the interface (e.g. check for new controllers) */
	virtual void Tick( float DeltaTime ) = 0;

	/** Poll for controller state and send events if needed */
	virtual void SendControllerEvents() = 0;

	/** Set which MessageHandler will get the events from SendControllerEvents. */
	virtual void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) = 0;

	/** Exec handler to allow console commands to be passed through for debugging */
    virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) = 0;

	/**
	 * IForceFeedbackSystem pass through functions
	 */
	virtual void SetChannelValue (int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) = 0;
	virtual void SetChannelValues (int32 ControllerId, const FForceFeedbackValues &values) = 0;

	/** If this device supports a haptic interface, implement this, and inherit the IHapticDevice interface */
	virtual class IHapticDevice* GetHapticDevice()
	{
		return nullptr;
	}

	virtual bool IsGamepadAttached() const
	{
		return false;
	}
};

