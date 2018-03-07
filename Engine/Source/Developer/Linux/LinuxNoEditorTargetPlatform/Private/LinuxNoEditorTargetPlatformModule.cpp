// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LinuxNoEditorTargetPlatformModule.cpp: Implements the FLinuxNoEditorTargetPlatformModule class.
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
class FLinuxNoEditorTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FLinuxNoEditorTargetPlatformModule( )
	{
		Singleton = NULL;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == NULL)
		{
			Singleton = new TLinuxTargetPlatform<false, false, false>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FLinuxNoEditorTargetPlatformModule, LinuxNoEditorTargetPlatform);
