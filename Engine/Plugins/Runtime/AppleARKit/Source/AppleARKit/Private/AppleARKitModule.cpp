// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitModule.h"
#include "AppleARKitSystem.h"
#include "ModuleManager.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"


TWeakPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> FAppleARKitARKitSystemPtr;

TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe> FAppleARKitModule::CreateTrackingSystem()
{
	auto NewARKitSystem = AppleARKitSupport::CreateAppleARKitSystem();
	FAppleARKitARKitSystemPtr = NewARKitSystem;
    return NewARKitSystem;
}

TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> FAppleARKitModule::GetARKitSystem()
{
    return FAppleARKitARKitSystemPtr.Pin();
}

FString FAppleARKitModule::GetModuleKeyName() const
{
    static const FString ModuleKeyName(TEXT("AppleARKit"));
    return ModuleKeyName;
}

void FAppleARKitModule::StartupModule()
{
	ensureMsgf(FModuleManager::Get().LoadModule("AugmentedReality"), TEXT("ARKit depends on the AugmentedReality module."));
	IHeadMountedDisplayModule::StartupModule();
}

void FAppleARKitModule::ShutdownModule()
{
	IHeadMountedDisplayModule::ShutdownModule();
}


IMPLEMENT_MODULE(FAppleARKitModule, AppleARKit);

DEFINE_LOG_CATEGORY(LogAppleARKit);

