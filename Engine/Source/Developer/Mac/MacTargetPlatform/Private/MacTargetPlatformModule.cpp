// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "GenericMacTargetPlatform.h"
#include "ModuleManager.h"
#include "ISettingsModule.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Modules/ModuleManager.h"
#include "MacTargetSettings.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#define LOCTEXT_NAMESPACE "FMacTargetPlatformModule"


/**
 * Holds the target platform singleton.
 */
static ITargetPlatform* Singleton = NULL;


/**
 * Module for Mac as a target platform
 */
class FMacTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/**
	 * Destructor.
	 */
	~FMacTargetPlatformModule()
	{
		Singleton = NULL;
	}


public:

	// Begin ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform() override
	{
		if (Singleton == NULL)
		{
			Singleton = new TGenericMacTargetPlatform<true, false, false>();
		}

		return Singleton;
	}

	// End ITargetPlatformModule interface


public:

	// Begin IModuleInterface interface

	virtual void StartupModule() override
	{
		TargetSettings = NewObject<UMacTargetSettings>(GetTransientPackage(), "MacTargetSettings", RF_Standalone);
		
		// We need to manually load the config properties here, as this module is loaded before the UObject system is setup to do this
        GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetSettings->TargetedRHIs, GEngineIni);
		
		int32 Value = 1;
        GConfig->GetInt(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("MaxShaderLanguageVersion"), Value, GEngineIni);
        TargetSettings->MaxShaderLanguageVersion = FMath::Max(Value, 2);
		
		if (!GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("UseFastIntrinsics"), TargetSettings->UseFastIntrinsics, GEngineIni))
		{
			TargetSettings->UseFastIntrinsics = false;
		}
		
		if (!GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("EnableMathOptimisations"), TargetSettings->EnableMathOptimisations, GEngineIni))
		{
			TargetSettings->EnableMathOptimisations = true;
		}
		
		TargetSettings->AddToRoot();

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Platforms", "Mac",
				LOCTEXT("TargetSettingsName", "Mac"),
				LOCTEXT("TargetSettingsDescription", "Settings and resources for Mac platform"),
				TargetSettings
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

		if (!GExitPurge)
		{
			// If we're in exit purge, this object has already been destroyed
			TargetSettings->RemoveFromRoot();
		}
		else
		{
			TargetSettings = NULL;
		}
	}

	// End IModuleInterface interface


private:

	// Holds the target settings.
	UMacTargetSettings* TargetSettings;
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FMacTargetPlatformModule, MacTargetPlatform);
