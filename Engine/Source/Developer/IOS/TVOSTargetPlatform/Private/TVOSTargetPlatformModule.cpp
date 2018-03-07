// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IOSTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"


/** Holds the target platform singleton. */
static ITargetPlatform* Singleton = nullptr;


/**
 * Module for TVOS as a target platform
 */
class FTVOSTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FTVOSTargetPlatformModule()
	{
		Singleton = nullptr;
	}

public:

	//~ ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform()
	{
		if (Singleton == nullptr)
		{
			Singleton = new FIOSTargetPlatform(true);
		}

		return Singleton;
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }
};


IMPLEMENT_MODULE(FTVOSTargetPlatformModule, TVOSTargetPlatform);
