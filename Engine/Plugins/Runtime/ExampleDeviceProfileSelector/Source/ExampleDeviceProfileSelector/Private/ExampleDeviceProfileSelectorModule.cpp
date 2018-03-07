// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ExampleDeviceProfileSelectorModule.h"
#include "Modules/ModuleManager.h"


IMPLEMENT_MODULE(FExampleDeviceProfileSelectorModule, ExampleDeviceProfileSelector);


void FExampleDeviceProfileSelectorModule::StartupModule()
{
}


void FExampleDeviceProfileSelectorModule::ShutdownModule()
{
}


const FString FExampleDeviceProfileSelectorModule::GetRuntimeDeviceProfileName()
{
	FString ProfileName = FPlatformProperties::PlatformName();
	UE_LOG(LogInit, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	return ProfileName;
}
