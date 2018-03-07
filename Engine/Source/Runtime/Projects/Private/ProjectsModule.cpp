// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ProjectsModule.cpp: Implements the FProjectsModule class.
=============================================================================*/

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


/**
 * Implements the Projects module.
 */
class FProjectsModule
	: public IModuleInterface
{
public:

	virtual void StartupModule( ) override { }

	virtual void ShutdownModule( ) override { }

	virtual bool SupportsDynamicReloading( ) override
	{
		return true;
	}
};


IMPLEMENT_MODULE(FProjectsModule, Projects);
