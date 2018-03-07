// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "ILauncherProfileManager.h"
#include "ILauncher.h"

/**
 * Declares a delegate to be invoked when a launcher profile manager has been initialized.
 *
 * The first parameter is the profile manager.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherProfileManagerInitialized, ILauncherProfileManager&);


/**
 * Interface for launcher tools modules.
 */
class ILauncherServicesModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a new device group.
	 *
	 * @return A new device group.
	 */
	virtual ILauncherDeviceGroupRef CreateDeviceGroup( ) = 0;

	/**
	 * Creates a new device group.
	 *
	 * @return A new device group.
	 */
	virtual ILauncherDeviceGroupRef CreateDeviceGroup(const FGuid& Guid, const FString& Name  ) = 0;

	/**
	 * Creates a game launcher.
	 *
	 * @return A new game launcher.
	 */
	virtual ILauncherRef CreateLauncher( ) = 0;

	/**
	 * Creates a launcher profile.
	 *
	 * @param ProfileName - The name of the profile to create.
	 */
	virtual ILauncherProfileRef CreateProfile( const FString& ProfileName ) = 0;

	/**
	 * Gets the launcher profile manager.
	 *
	 * @return The launcher profile manager.
	 */
	virtual ILauncherProfileManagerRef GetProfileManager( ) = 0;
	
	/**
	 * Delegate for when a platform SDK isn't installed corrected (takes the platform name and the documentation link to show)
	 */
	DECLARE_EVENT_TwoParams(ILauncherServicesModule, FLauncherServicesSDKNotInstalled, const FString&, const FString&);
	virtual FLauncherServicesSDKNotInstalled& OnLauncherServicesSDKNotInstalled( ) = 0;
	virtual void BroadcastLauncherServicesSDKNotInstalled(const FString& PlatformName, const FString& DocLink) = 0;

public:

	/** Virtual destructor. */
	virtual ~ILauncherServicesModule( ) { }

public:
	/**
	 * Delegate that is invoked when a profile manager is initialized.
	 */
	LAUNCHERSERVICES_API static FOnLauncherProfileManagerInitialized ProfileManagerInitializedDelegate;
};
