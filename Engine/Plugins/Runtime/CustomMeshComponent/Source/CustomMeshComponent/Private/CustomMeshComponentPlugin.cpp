// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "ICustomMeshComponentPlugin.h"




class FCustomMeshComponentPlugin : public ICustomMeshComponentPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE( FCustomMeshComponentPlugin, CustomMeshComponent )



void FCustomMeshComponentPlugin::StartupModule()
{
	
}


void FCustomMeshComponentPlugin::ShutdownModule()
{
	
}



