// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "HTML5TargetSettings.h"
#include "HTML5TargetSettingsCustomization.h"
#include "ISettingsModule.h"
#include "HTML5SDKSettings.h"


#define LOCTEXT_NAMESPACE "FHTML5PlatformEditorModule"


/**
 * Module for Android platform editor utilities
 */
class FHTML5PlatformEditorModule
	: public IModuleInterface
{
	// IModuleInterface interface

	virtual void StartupModule() override
	{
		FString SDKPath = FPaths::EngineDir() / TEXT("Extras/ThirdPartyNotUE/emsdk") /
#if PLATFORM_WINDOWS
		TEXT("Win64");
#elif PLATFORM_MAC
		TEXT("Mac");
#elif PLATFORM_LINUX
		TEXT("Linux");
#else 
		return; 
#endif 
		// we don't have the SDK, don't bother setting this up. 
		if (!IFileManager::Get().DirectoryExists(*SDKPath))
		{
			return; 
		}

		// register settings
		static FName PropertyEditor("PropertyEditor");
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
		PropertyModule.RegisterCustomClassLayout(
			"HTML5TargetSettings",
			FOnGetDetailCustomizationInstance::CreateStatic(&FHTML5TargetSettingsCustomization::MakeInstance)
			);

		PropertyModule.NotifyCustomizationModuleChanged();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "HTML5",
				LOCTEXT("TargetSettingsName", "HTML5"),
				LOCTEXT("TargetSettingsDescription", "Settings for HTML5"),
				GetMutableDefault<UHTML5TargetSettings>()
			);

// NOTE: HTML5SDKSettings has become the "list of browsers" (that Editor->Launch button will be populated with)
//       now, the list of browsers are configurable in Engine.ini
// NOTE: the "SDK" (i.e. emscripten) is "known" to be at: Engine/Extras/ThirdPartyNotUE/emsdk/...
//       see HTML5ToolChain.cs for details

// 			SettingsModule->RegisterSettings("Project", "Platforms", "HTML5SDK",
// 				LOCTEXT("SDKSettingsName", "HTML5 SDK"),
// 				LOCTEXT("SDKSettingsDescription", "Settings for HTML5 SDK (system wide)"),
// 				GetMutableDefault<UHTML5SDKSettings>()
//			);
		}
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Platforms", "HTML5");
//			SettingsModule->UnregisterSettings("Project", "Platforms", "HTML5SDK");
		}
	}
};


IMPLEMENT_MODULE(FHTML5PlatformEditorModule, HTML5PlatformEditor);

#undef LOCTEXT_NAMESPACE
