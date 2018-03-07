// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ITargetDeviceProxyManager.h"
#include "ITargetDeviceServiceManager.h"
#include "Modules/ModuleInterface.h"


/**
 * Interface for target device services modules.
 */
class ITargetDeviceServicesModule
	: public IModuleInterface
{
public:

	/**
	 * Gets the target device proxy manager.
	 *
	 * @return The device proxy manager.
	 * @see GetDeviceServiceManager
	 */
	virtual TSharedRef<ITargetDeviceProxyManager> GetDeviceProxyManager() = 0;

	/**
	 * Gets the target device service manager.
	 *
	 * @return The device service manager.
	 * @see GetDeviceProxyManager
	 */
	virtual TSharedRef<ITargetDeviceServiceManager> GetDeviceServiceManager() = 0;

public:

	/** Virtual destructor. */
	virtual ~ITargetDeviceServicesModule() { }
};
