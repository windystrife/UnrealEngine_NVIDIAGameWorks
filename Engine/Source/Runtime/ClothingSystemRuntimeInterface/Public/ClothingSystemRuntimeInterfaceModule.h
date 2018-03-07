// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleInterface.h"

class FClothingSystemRuntimeInterfaceModule : public IModuleInterface
{

public:

	FClothingSystemRuntimeInterfaceModule();

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
};
