// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#include "INUTUnrealEngine4.h"

#include "UnrealEngine4Environment.h"

/**
 * Module implementation
 */
class FNUTUnrealEngine4 : public INUTUnrealEngine4
{
public:
	/**
	 * Called upon loading of the NUTFortnite library
	 */
	virtual void StartupModule() override
	{
		FShooterGameEnvironment::Register();
		FQAGameEnvironment::Register();
		FUTEnvironment::Register();
	}

	/**
	 * Called immediately prior to unloading of the NUTFortnite library
	 */
	virtual void ShutdownModule() override
	{
	}
};


// Essential for getting the .dll to compile, and for the package to be loadable
IMPLEMENT_MODULE(FNUTUnrealEngine4, NUTUnrealEngine4);

