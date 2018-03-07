// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HTML5SDKSettings.h"
#include "IHTML5TargetPlatformModule.h"
#include "Modules/ModuleManager.h"

//#include "EngineTypes.h"

#define LOCTEXT_NAMESPACE "FHTML5PlatformEditorModule"

DEFINE_LOG_CATEGORY_STATIC(HTML5SDKSettings, Log, All);

UHTML5SDKSettings::UHTML5SDKSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetPlatformModule(nullptr)
{
}

#if WITH_EDITOR
void UHTML5SDKSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (!TargetPlatformModule)
	{
		TargetPlatformModule = FModuleManager::LoadModulePtr<IHTML5TargetPlatformModule>("HTML5TargetPlatform");
	}
	if (TargetPlatformModule)
	{
		UpdateGlobalUserConfigFile();
		TargetPlatformModule->RefreshAvailableDevices();
	}
}

#endif //WITH_EDITOR
#undef LOCTEXT_NAMESPACE
