// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5OutputDevices.h: KickStart platform OutputDevices functions
==============================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformOutputDevices.h"

struct CORE_API FHTML5PlatformOutputDevices : FGenericPlatformOutputDevices 
{
	static FOutputDevice*				GetLog();
};

// generic version
typedef FHTML5PlatformOutputDevices FPlatformOutputDevices;
