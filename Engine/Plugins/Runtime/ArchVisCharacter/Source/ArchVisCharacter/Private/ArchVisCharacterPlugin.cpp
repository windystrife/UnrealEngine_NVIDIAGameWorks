// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"


class FArchVisCharacterPlugin : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FArchVisCharacterPlugin, ArchVisCharacter)


void FArchVisCharacterPlugin::StartupModule()
{
	
}

void FArchVisCharacterPlugin::ShutdownModule()
{
	
}



