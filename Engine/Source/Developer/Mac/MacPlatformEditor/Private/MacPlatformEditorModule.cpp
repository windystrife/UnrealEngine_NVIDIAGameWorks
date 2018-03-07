// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ModuleInterface.h"
#include "ISettingsModule.h"
#include "ModuleManager.h"
#include "MacTargetSettings.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Class.h"

#define LOCTEXT_NAMESPACE "MacPlatformEditorModule"


/**
 * Module for Mac project settings
 */
class FMacPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Mac",
				LOCTEXT("TargetSettingsName", "Mac"),
				LOCTEXT("TargetSettingsDescription", "Settings for Mac target platform"),
				GetMutableDefault<UMacTargetSettings>()
			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "Mac");
		}
	}
};


IMPLEMENT_MODULE(FMacPlatformEditorModule, MacPlatformEditor);

#undef LOCTEXT_NAMESPACE
