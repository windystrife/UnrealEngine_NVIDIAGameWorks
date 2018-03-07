// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MediaUtilsPrivate.h"

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


DEFINE_LOG_CATEGORY(LogMediaUtils);


/**
 * Implements the Media module.
 */
class FMediaUtilsModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FMediaUtilsModule, MediaUtils);
