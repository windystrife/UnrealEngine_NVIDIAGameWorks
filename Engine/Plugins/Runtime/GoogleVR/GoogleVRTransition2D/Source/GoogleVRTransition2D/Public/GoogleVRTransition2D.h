// Copyright 2017 Google Inc.

#pragma once

#include "ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGoogleVRTransition2D, Log, All);

class FGoogleVRTransition2DModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};