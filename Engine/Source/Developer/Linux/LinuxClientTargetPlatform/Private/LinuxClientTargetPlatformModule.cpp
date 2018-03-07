// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxClientTargetPlatformModule.cpp: Implements the FLinuxClientTargetPlatformModule class.
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
 * Module for the Linux target platform (without editor).
 */
class FLinuxClientTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FLinuxClientTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == NULL)
		{
 			Singleton = new TLinuxTargetPlatform<false, false, true>();
		}

		return Singleton;
	}
};

// 
IMPLEMENT_MODULE(FLinuxClientTargetPlatformModule, LinuxClientTargetPlatform);
