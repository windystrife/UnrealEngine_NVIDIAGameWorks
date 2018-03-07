// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * Implements the UnrealEdMessages module.
 */
class FUnrealEdMessagesModule
	: public IModuleInterface
{
public:

	// IModuleInterface interface

	virtual void StartupModule( ) override { }
	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FUnrealEdMessagesModule, UnrealEdMessages);
