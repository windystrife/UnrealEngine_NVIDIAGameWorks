// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"

class FOutputDevice;
class FOutputDeviceConsole;
class FOutputDeviceError;
class FFeedbackContext;

struct CORE_API FWindowsPlatformOutputDevices
	: public FGenericPlatformOutputDevices
{
	static FOutputDevice*			GetEventLog();
};


typedef FWindowsPlatformOutputDevices FPlatformOutputDevices;
