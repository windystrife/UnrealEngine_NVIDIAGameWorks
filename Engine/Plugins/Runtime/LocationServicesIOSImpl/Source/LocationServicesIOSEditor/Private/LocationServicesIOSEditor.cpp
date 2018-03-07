// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LocationServicesIOSEditor.h"
#include "LocationServicesIOSSettings.h"
#include "ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#endif


IMPLEMENT_MODULE( FLocationServicesIOSEditorModule, LocationServicesIOSEditorModule );

#define LOCTEXT_NAMESPACE "FLocationServicesIOSEditorModule"

void FLocationServicesIOSEditorModule::StartupModule()
{
#if WITH_EDITOR
	// register settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "Location Services IOS",
			LOCTEXT("LocationServicesIOSSettingsName", "Location Services - IOS"),
			LOCTEXT("LocationServicesIOSSettingsDescription", "Configure the Location Services settings for IOS"),
			GetMutableDefault<ULocationServicesIOSSettings>()
		);
	}
#endif // WITH_EDITOR
}

void FLocationServicesIOSEditorModule::ShutdownModule()
{
#if WITH_EDITOR
	// unregister settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Location Services IOS");
	}
#endif
}

ULocationServicesIOSSettings::ULocationServicesIOSSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#undef LOCTEXT_NAMESPACE
