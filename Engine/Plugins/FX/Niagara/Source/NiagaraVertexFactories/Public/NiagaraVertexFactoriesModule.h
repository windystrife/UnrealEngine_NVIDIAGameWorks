// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/**
* Niagara vertex factories module interface
*/
class INiagaraVertexFactoriesModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{}
	virtual void ShutdownModule() override
	{}
};

