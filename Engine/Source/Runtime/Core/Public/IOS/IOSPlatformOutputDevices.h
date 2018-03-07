// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================================
	IOSPlatformOutputDevices.h: iOS platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FIOSPlatformOutputDevices : public FGenericPlatformOutputDevices
{
    static FOutputDevice*		GetLog();
};

typedef FIOSPlatformOutputDevices FPlatformOutputDevices;
