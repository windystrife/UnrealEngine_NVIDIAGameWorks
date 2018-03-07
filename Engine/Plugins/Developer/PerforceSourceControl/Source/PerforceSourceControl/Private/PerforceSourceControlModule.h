// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PerforceSourceControlProvider.h"
#include "PerforceSourceControlSettings.h"

class FPerforceSourceControlModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Access the Perforce source control settings */
	FPerforceSourceControlSettings& AccessSettings();

	/** Save the Perforce source control settings */
	void SaveSettings();

	/** Access the one and only Perforce provider */
	FPerforceSourceControlProvider& GetProvider()
	{
		return PerforceSourceControlProvider;
	}
	
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FPerforceSourceControlModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FPerforceSourceControlModule >("PerforceSourceControl");
	}

private:
	/** The one and only Perforce source control provider */
	FPerforceSourceControlProvider PerforceSourceControlProvider;

	/** The settings for Perforce source control */
	FPerforceSourceControlSettings PerforceSourceControlSettings;
};
