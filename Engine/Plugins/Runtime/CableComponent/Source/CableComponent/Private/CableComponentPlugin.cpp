// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"




class FCableComponentPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FCableComponentPlugin, CableComponent )



void FCableComponentPlugin::StartupModule()
{
	
}


void FCableComponentPlugin::ShutdownModule()
{
	
}



