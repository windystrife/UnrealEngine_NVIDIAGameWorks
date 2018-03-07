// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FCompareBasepassShaders;

class FRuntimeTestsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	FCompareBasepassShaders* FCompareBasepassShadersAutomationTestInstance;
};
