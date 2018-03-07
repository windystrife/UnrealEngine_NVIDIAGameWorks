// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "AllDesktopTargetPlatform.h"
 
/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for a generic target platform for desktop platforms
 */
class FAllDesktopTargetPlatformModule : public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	FAllDesktopTargetPlatformModule()
	{
		Singleton = NULL;
	}


public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (Singleton == NULL)
		{
			Singleton = new FAllDesktopTargetPlatform();
		}

		return Singleton;
	}

	// End ITargetPlatformModule interface


public:

	// Begin IModuleInterface interface
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
	// End IModuleInterface interface
};

IMPLEMENT_MODULE( FAllDesktopTargetPlatformModule, AllDesktopTargetPlatform);
