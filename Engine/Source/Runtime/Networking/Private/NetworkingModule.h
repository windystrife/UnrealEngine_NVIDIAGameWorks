// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/INetworkingModule.h"

/**
 * Implements the Networking module.
 */
class FNetworkingModule
	: public INetworkingModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;
};
