// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WindowsDeviceProfileSelectorModule.h"
#include "Modules/ModuleManager.h"


IMPLEMENT_MODULE(FWindowsDeviceProfileSelectorModule, WindowsDeviceProfileSelector);


void FWindowsDeviceProfileSelectorModule::StartupModule()
{
}


void FWindowsDeviceProfileSelectorModule::ShutdownModule()
{
}

const FString FWindowsDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	FString ProfileName = FPlatformProperties::PlatformName();
	UE_LOG(LogInit, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	return ProfileName;
}
