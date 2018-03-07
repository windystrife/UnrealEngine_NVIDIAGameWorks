// Copyright 2015 Kite & Lightning.  All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterface.h"

class FStereoPanoramaManager;

/**
 * Implements the StereoPanorama module.
 */
class FStereoPanoramaModule
	: public IModuleInterface
{
public:

	//~ IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:

	static TSharedPtr<FStereoPanoramaManager> Get();
};
