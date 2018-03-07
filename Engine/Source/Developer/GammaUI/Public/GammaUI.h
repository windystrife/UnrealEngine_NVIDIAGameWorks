// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Widgets/SWidget.h"

class FGammaUI : public IModuleInterface
{

public:

	// IModuleInterface implementation.
	virtual void StartupModule();
	virtual void ShutdownModule();

	/** @return a pointer to the Gamma UI panel; invalid pointer if one has already been allocated */
	virtual TSharedPtr< class SWidget > GetGammaUIPanel();
};
