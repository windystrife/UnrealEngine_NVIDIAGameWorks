// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class IVideoRecordingSystem;

/**
 * Interface for platform feature modules
 */

/** Defines the interface of a module implementing platform feature collector class. */
class ENGINE_API IPlatformFeaturesModule : public IModuleInterface
{
public:

	static IPlatformFeaturesModule& Get()
	{
		static IPlatformFeaturesModule* StaticModule = NULL;
		// first time initialization
		if (!StaticModule)
		{
			const TCHAR* PlatformModuleName = FPlatformMisc::GetPlatformFeaturesModuleName();
			if (PlatformModuleName)
			{
				StaticModule = &FModuleManager::LoadModuleChecked<IPlatformFeaturesModule>(PlatformModuleName);
			}
			else
			{
				// if the platform doesn't care about a platform features module, then use this generic almost empty implementation
				StaticModule = new IPlatformFeaturesModule;
			}
		}

		return *StaticModule;
	}

	virtual class ISaveGameSystem* GetSaveGameSystem();

	virtual class IDVRStreamingSystem* GetStreamingSystem();

	virtual TSharedPtr<const class FJsonObject> GetTitleSettings();

	virtual FString GetUniqueAppId();

	virtual IVideoRecordingSystem* GetVideoRecordingSystem();
};
