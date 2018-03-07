// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDeviceProfileSelectorModule.h"

/**
 * Implements the Android Device Profile Selector module.
 */
class FAndroidDeviceProfileSelectorRuntimeModule
	: public IDeviceProfileSelectorModule
{
public:

	//~ Begin IDeviceProfileSelectorModule Interface
	virtual const FString GetRuntimeDeviceProfileName() override;
	//~ End IDeviceProfileSelectorModule Interface


	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	
	/**
	 * Virtual destructor.
	 */
	virtual ~FAndroidDeviceProfileSelectorRuntimeModule()
	{
	}

private:

	/**
	 * Make sure we use the java workaround for view scaling on devices that require this
	 */
	void CheckForJavaSurfaceViewWorkaround(const FString& DeviceMake, const FString& DeviceModel) const;
};