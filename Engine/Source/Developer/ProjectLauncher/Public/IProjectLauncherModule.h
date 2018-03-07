// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"


/**
 * Interface for launcher UI modules.
 */
class IProjectLauncherModule
	: public IModuleInterface
{
public:

	/** Virtual destructor. */
	virtual ~IProjectLauncherModule() { }
};
