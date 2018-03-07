// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IDeviceProfileSelectorModule.h: Declares the IDeviceProfileSelectorModule interface.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


/**
 * Device Profile Selector module
 */
class IDeviceProfileSelectorModule
	: public IModuleInterface
{
public:

	/**
	 * Run the logic to choose an appropriate device profile for this session
	 *
	 * @return The name of the device profile to use for this session
	 */
	virtual const FString GetRuntimeDeviceProfileName() = 0;

	/**
	* Run the logic to choose an appropriate device profile for this session.
	* @param DeviceParameters	A map of parameters to be used by device profile logic.
	* @return The name of the device profile to use for this session
	*/
	virtual const FString GetDeviceProfileName(const TMap<FString, FString>& DeviceParameters) { return FString(); }


	/**
	 * Virtual destructor.
	 */
	virtual ~IDeviceProfileSelectorModule()
	{
	}
};
