// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeviceProfileManager.h: Declares the FDeviceProfileManager class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "DeviceProfileManager.generated.h"

class UDeviceProfile;

// Delegate used to refresh the UI when the profiles change
DECLARE_MULTICAST_DELEGATE( FOnDeviceProfileManagerUpdated );

/**
 * Implements a helper class that manages all profiles in the Device
 */
 UCLASS( config=DeviceProfiles, transient )
class ENGINE_API UDeviceProfileManager : public UObject
{
public:

	GENERATED_BODY()

	/**
	 * Startup and select the active device profile
	 * Then Init the CVars from this profile and it's Device profile parent tree.
	 */
	static void InitializeCVarsForActiveDeviceProfile(bool bPushSettings=false);

	/**
	 * Create a copy of a device profile from a copy.
	 *
	 * @param ProfileName - The profile name.
 	 * @param ProfileToCopy - The profile to copy name.
	 *
	 * @return the created profile.
	 */
	UDeviceProfile* CreateProfile(const FString& ProfileName, const FString& ProfileType, const FString& ParentName=TEXT(""), const TCHAR* ConfigPlatform=nullptr);

	/**
	 * Delete a profile.
	 *
	 * @param Profile - The profile to delete.
	 */
	void DeleteProfile( UDeviceProfile* Profile );

	/**
	 * Find a profile based on the name.
	 *
	 * @param ProfileName - The profile name to find.
	 * @return The found profile.
	 */
	UDeviceProfile* FindProfile( const FString& ProfileName );

	/**
	 * Get the device profile .ini name.
	 *
	 * @return the device profile .ini name.
	 */
	const FString GetDeviceProfileIniName() const;

	/**
	 * Load the device profiles from the config file.
	 */
	void LoadProfiles();

	/**
	 * Returns a delegate that is invoked when manager is updated.
	 *
	 * @return The delegate.
	 */
	FOnDeviceProfileManagerUpdated& OnManagerUpdated();

	/**
	 * Save the device profiles.
	 */
	void SaveProfiles(bool bSaveToDefaults = false);

	/**
	 * Get the selected device profile
	 *
	 * @return The selected profile.
	 */
	UDeviceProfile* GetActiveProfile() const;

	/**
	* Get a list of all possible parent profiles for a given device profile
	*
	* @param ChildProfile				- The profile we are looking for potential parents
	* @param PossibleParentProfiles	- The list of profiles which would be suitable as a parent for the given profile
	*/
	void GetAllPossibleParentProfiles(const UDeviceProfile* ChildProfile, OUT TArray<UDeviceProfile*>& PossibleParentProfiles) const;

	/**
	* Get the selected device profile name, either the platform name, or the name
	* provided by a Device Profile Selector Module.
	*
	* @return The selected profile.
	*/
	static const FString GetActiveProfileName();

private:
	/**
	 * Set the active device profile - set via the device profile blueprint.
	 *
	 * @param DeviceProfileName - The profile name.
	 */
	void SetActiveDeviceProfile( UDeviceProfile* DeviceProfile );

	/**
	* Override CVar value change callback
	*/
	void HandleDeviceProfileOverrideChange();

	/** Handle restoing CVars set in HandleDeviceProfileOverrideChange */
	void HandleDeviceProfileOverridePop();

public:

	static class UDeviceProfileManager* DeviceProfileManagerSingleton;
	static UDeviceProfileManager& Get(bool bFromPostCDOContruct = false);

	virtual void PostCDOContruct() override
	{
		Get(true); // get this taken care of now
	}


public:

	// Holds the collection of managed profiles.
	UPROPERTY( EditAnywhere, Category=Properties )
	TArray< UObject* > Profiles;

private:

	// Holds a delegate to be invoked profiles are updated.
	FOnDeviceProfileManagerUpdated ManagerUpdatedDelegate;

	// Holds the selected device profile
	UDeviceProfile* ActiveDeviceProfile;

	// Holds the device profile .ini location
	static FString DeviceProfileFileName;

	// values of CVars set in HandleDeviceProfileOverrideChange, to be popped later
	TMap<FString, FString> PushedSettings;
};
