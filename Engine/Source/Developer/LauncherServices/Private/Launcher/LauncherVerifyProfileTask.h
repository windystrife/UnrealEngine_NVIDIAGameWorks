// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Launcher/LauncherTask.h"

/**
 * Implements a launcher task for verifying the profile settings.
 */
class FLauncherVerifyProfileTask
	: public FLauncherTask
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InProfile - The launcher profile containing the cook settings.
	 * @param InSessionId - The session identifier.
	 * @param InPlatformName - The name of the platform to cook.
	 */
	FLauncherVerifyProfileTask( )
		: FLauncherTask(NSLOCTEXT("FLauncherTask", "LauncherVerifyProfileName", "Verify").ToString(), NSLOCTEXT("FLauncherTask", "LauncherVerifyProfileDesc", "Verify profile settings").ToString(), NULL, NULL)
	{ }

protected:

	virtual bool PerformTask( FLauncherTaskChainState& ChainState ) override
	{
		if (ChainState.Profile.IsValid())
		{
			return ChainState.Profile->IsValidForLaunch();
		}
		
		return false;
	}
};
