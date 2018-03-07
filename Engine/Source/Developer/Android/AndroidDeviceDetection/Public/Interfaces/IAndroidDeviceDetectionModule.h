// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IAndroidDeviceDetectionModule.h: Declares the IAndroidDeviceDetectionModule interface.
=============================================================================*/

#pragma once

#include "Modules/ModuleInterface.h"

class IAndroidDeviceDetection;

/**
 * Interface for AndroidDeviceDetection module.
 */
class IAndroidDeviceDetectionModule
	: public IModuleInterface
{
public:
	/**
	 * Returns the android device detection singleton.
	 */
	virtual IAndroidDeviceDetection* GetAndroidDeviceDetection() = 0;

protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IAndroidDeviceDetectionModule( ) { }
};
