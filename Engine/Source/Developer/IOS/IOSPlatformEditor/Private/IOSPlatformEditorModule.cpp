// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "PropertyEditorModule.h"
#include "IOSRuntimeSettings.h"
#include "IOSTargetSettingsCustomization.h"
#include "ISettingsModule.h"
#include "MaterialShaderQualitySettings.h"
#include "MaterialShaderQualitySettingsCustomization.h"
#include "ComponentRecreateRenderStateContext.h"
#include "ShaderPlatformQualitySettings.h"

#define LOCTEXT_NAMESPACE "FIOSPlatformEditorModule"


/**
 * Module for iOS as a target platform
 */
class FIOSPlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// register settings detail panel customization
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"IOSRuntimeSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FIOSTargetSettingsCustomization::MakeInstance)
		);

		FOnUpdateMaterialShaderQuality UpdateMaterials = FOnUpdateMaterialShaderQuality::CreateLambda([]()
		{
			FGlobalComponentRecreateRenderStateContext Recreate;
			FlushRenderingCommands();
			UMaterial::AllMaterialsCacheResourceShadersForRendering();
			UMaterialInstance::AllMaterialsCacheResourceShadersForRendering();
		});

		PropertyModule.RegisterCustomClassLayout(
			UShaderPlatformQualitySettings::StaticClass()->GetFName(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FMaterialShaderQualitySettingsCustomization::MakeInstance, UpdateMaterials)
			);

		PropertyModule.NotifyCustomizationModuleChanged();

		// register settings
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "iOS",
				LOCTEXT("RuntimeSettingsName", "iOS"),
				LOCTEXT("RuntimeSettingsDescription", "Settings and resources for the iOS platform"),
				GetMutableDefault<UIOSRuntimeSettings>()
			);

			{
				static FName NAME_SF_METAL(TEXT("SF_METAL"));
				const UShaderPlatformQualitySettings* IOSMaterialQualitySettings = UMaterialShaderQualitySettings::Get()->GetShaderPlatformQualitySettings(NAME_SF_METAL);
				SettingsModule->RegisterSettings("Project", "Platforms", "iOSMetalQuality",
					LOCTEXT("IOSMetalQualitySettingsName", "iOS Material Quality"),
					LOCTEXT("IOSMetalQualitySettingsDescription", "Settings for iOS material quality"),
					IOSMaterialQualitySettings
				);
			}
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "iOS");
			SettingsModule->UnregisterSettings("Project", "Platforms", "iOSMetalQuality");
		}
	}
};


IMPLEMENT_MODULE(FIOSPlatformEditorModule, IOSPlatformEditor);

#undef LOCTEXT_NAMESPACE
