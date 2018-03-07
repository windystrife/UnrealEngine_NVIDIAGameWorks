// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "LinuxTargetSettings.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "LinuxPlatformEditorModule"


/**
 * Module for Linux project settings
 */
class FLinuxPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Linux",
				LOCTEXT("TargetSettingsName", "Linux"),
				LOCTEXT("TargetSettingsDescription", "Settings for Linux target platform"),
				GetMutableDefault<ULinuxTargetSettings>()
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Linux");
		}
	}
};


IMPLEMENT_MODULE(FLinuxPlatformEditorModule, LinuxPlatformEditor);

#undef LOCTEXT_NAMESPACE
