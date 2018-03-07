// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreFwd.h"
#include "Modules/ModuleInterface.h"

/**
 * Platform File Module Interface
 */
class IPlatformFileModule
	: public IModuleInterface
{
public:

	/**
	 * Creates a platform file instance.
	 *
	 * @return Platform file instance.
	 */
	virtual IPlatformFile* GetPlatformFile() = 0;
};
