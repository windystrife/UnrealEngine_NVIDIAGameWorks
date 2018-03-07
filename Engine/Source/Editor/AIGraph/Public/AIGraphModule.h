// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

AIGRAPH_API DECLARE_LOG_CATEGORY_EXTERN(LogAIGraph, Display, All);

class FAIGraphModule : public IModuleInterface
{
public:
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
