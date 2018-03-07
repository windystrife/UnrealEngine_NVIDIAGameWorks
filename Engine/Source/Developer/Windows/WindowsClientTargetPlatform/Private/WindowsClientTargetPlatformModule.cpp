// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "GenericWindowsTargetPlatform.h"
#include "Interfaces/ITargetPlatformModule.h"


/** Holds the target platform singleton. */
static ITargetPlatform* Singleton = nullptr;


/**
 * Module for the Windows target platform as a server.
 */
class FWindowsClientTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	virtual ~FWindowsClientTargetPlatformModule( )
	{
		Singleton = nullptr;
	}

	virtual ITargetPlatform* GetTargetPlatform( )
	{
		if (Singleton == nullptr)
		{
			Singleton = new TGenericWindowsTargetPlatform<false, false, true>();
		}

		return Singleton;
	}
};


IMPLEMENT_MODULE(FWindowsClientTargetPlatformModule, WindowsClientTargetPlatform);
