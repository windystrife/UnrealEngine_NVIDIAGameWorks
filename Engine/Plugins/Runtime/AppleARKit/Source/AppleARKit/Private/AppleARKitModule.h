// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "IHeadMountedDisplayModule.h"
#include "Features/IModularFeature.h"
#include "AppleARKitSystem.h"
#include "AppleARKitModule.h"
#include "ARHitTestingSupport.h"

class APPLEARKIT_API FAppleARKitModule : public IHeadMountedDisplayModule
{
public:
	virtual TSharedPtr<class IXRTrackingSystem, ESPMode::ThreadSafe> CreateTrackingSystem() override;
	
	static TSharedPtr<class FAppleARKitSystem, ESPMode::ThreadSafe> GetARKitSystem();
	
	FString GetModuleKeyName() const override;

	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

};

DECLARE_LOG_CATEGORY_EXTERN(LogAppleARKit, Log, All);

