// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "Modules/ModuleInterface.h"

class FSpawnTabArgs;

/**
 * Device Profile Editor module
 */
class FDeviceProfileEditorModule : public IModuleInterface
{

public:

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

public:
	/**
	 * Create the slate UI for the Device Profile Editor
	 */
	static TSharedRef<SDockTab> SpawnDeviceProfileEditorTab( const FSpawnTabArgs& SpawnTabArgs );
};
