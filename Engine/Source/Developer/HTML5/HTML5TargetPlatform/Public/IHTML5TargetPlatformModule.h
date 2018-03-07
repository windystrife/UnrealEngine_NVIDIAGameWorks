// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatformModule.h"
/**
 * Interface for HTML5TargetPlatformModule module.
 */
class IHTML5TargetPlatformModule
	: public ITargetPlatformModule
{
public:
	/**
	 * Refresh the list of HTML5 browsers that exist on the system
	 */
	virtual void RefreshAvailableDevices() = 0;

protected:

	/**
	 * Virtual destructor
	 */
	virtual ~IHTML5TargetPlatformModule( ) { }
};
