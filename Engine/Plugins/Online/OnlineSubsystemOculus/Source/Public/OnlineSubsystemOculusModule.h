// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModuleInterface.h"

/**
 * Online subsystem module class  (Oculus Implementation)
 * Code related to the loading of the Oculus module
 */
class FOnlineSubsystemOculusModule : public IModuleInterface
{
private:

	/** Class responsible for creating instance(s) of the subsystem */
	class FOnlineFactoryOculus* OculusFactory;

public:

	FOnlineSubsystemOculusModule() :
		OculusFactory(nullptr)
	{}

	virtual ~FOnlineSubsystemOculusModule() {}

	// IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	virtual bool SupportsAutomaticShutdown() override
	{
		return false;
	}
};
