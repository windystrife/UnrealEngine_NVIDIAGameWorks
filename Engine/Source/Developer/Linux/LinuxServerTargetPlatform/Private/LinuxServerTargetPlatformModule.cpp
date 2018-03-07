// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxTargetPlatformModule.cpp: Implements the FAndroidTargetPlatformModule class.
=============================================================================*/

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "Interfaces/ITargetPlatformModule.h"

#include "LinuxTargetDevice.h"
#include "LinuxTargetPlatform.h"

/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for the Android target platform.
 */
class FLinuxServerTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FLinuxServerTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == NULL)
		{
			Singleton = new TLinuxTargetPlatform<false, true, false>();
		}
		
		return Singleton;
	}
};


IMPLEMENT_MODULE( FLinuxServerTargetPlatformModule, LinuxServerTargetPlatform);
