// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSPlatformOutputDevices.h"
#include "Misc/OutputDeviceFile.h"


class FOutputDevice*	FIOSPlatformOutputDevices::GetLog()
{
    static FOutputDeviceFile Singleton(nullptr, true);
    return &Singleton;
}

//class FFeedbackContext*				FIOSPlatformOutputDevices::GetWarn()
//{
//	static FOutputDeviceIOSDebug Singleton;
//	return &Singleton;
//}
