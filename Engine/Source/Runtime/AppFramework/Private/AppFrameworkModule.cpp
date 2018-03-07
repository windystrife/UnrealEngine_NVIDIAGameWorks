// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UObject/NameTypes.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


static const FName AppFrameworkTabName("AppFramework");


/**
 * Implements the AppFramework module.
 */
class FAppFrameworkModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface
	
	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};


IMPLEMENT_MODULE(FAppFrameworkModule, AppFramework);
