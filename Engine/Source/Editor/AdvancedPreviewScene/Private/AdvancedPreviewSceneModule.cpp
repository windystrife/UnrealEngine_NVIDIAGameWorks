// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AdvancedPreviewSceneModule.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "AdvancedPreviewSceneCommands.h"
#include "ModuleManager.h"

void FAdvancedPreviewSceneModule::StartupModule()
{
	FAdvancedPreviewSceneCommands::Register();
}

void FAdvancedPreviewSceneModule::ShutdownModule()
{
}

TSharedRef<SWidget> FAdvancedPreviewSceneModule::CreateAdvancedPreviewSceneSettingsWidget(const TSharedRef<FAdvancedPreviewScene>& InPreviewScene, UObject* InAdditionalSettings, const TArray<FDetailCustomizationInfo>& InDetailCustomizations, const TArray<FPropertyTypeCustomizationInfo>& InPropertyTypeCustomizations)
{
	return SNew(SAdvancedPreviewDetailsTab, InPreviewScene)
		.AdditionalSettings(InAdditionalSettings)
		.DetailCustomizations(InDetailCustomizations)
		.PropertyTypeCustomizations(InPropertyTypeCustomizations);
}

IMPLEMENT_MODULE(FAdvancedPreviewSceneModule, AdvancedPreviewScene);