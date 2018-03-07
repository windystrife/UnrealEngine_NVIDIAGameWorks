// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "SubversionSourceControlSettings.h"
#include "SubversionSourceControlProvider.h"

class FSubversionSourceControlModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Access the Subversion source control settings */
	FSubversionSourceControlSettings& AccessSettings();

	/** Save the Subversion source control settings */
	void SaveSettings();

	/** Access the one and only Subversion provider */
	FSubversionSourceControlProvider& GetProvider()
	{
		return SubversionSourceControlProvider;
	}

private:
	/** The one and only Subversion source control provider */
	FSubversionSourceControlProvider SubversionSourceControlProvider;

	/** The settings for Subversion source control */
	FSubversionSourceControlSettings SubversionSourceControlSettings;
};
