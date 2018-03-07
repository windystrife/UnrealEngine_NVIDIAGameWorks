// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IMixedRealityFrameworkModule.h"
#include "ModuleManager.h" // for IMPLEMENT_MODULE()

class FMixedRealityFrameworkModule : public IMixedRealityFrameworkModule
{
public:
	//~ IModuleInterface interface
};

IMPLEMENT_MODULE(FMixedRealityFrameworkModule, MixedRealityFramework);
