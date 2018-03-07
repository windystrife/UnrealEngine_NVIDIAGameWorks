// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	LinuxPlatformOutputDevices.h: Linux platform OutputDevices functions
==============================================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FLinuxOutputDevices : public FGenericPlatformOutputDevices
{
	static void SetupOutputDevices();
	static FString GetAbsoluteLogFilename();
	static FOutputDevice* GetEventLog();
};

typedef FLinuxOutputDevices FPlatformOutputDevices;
