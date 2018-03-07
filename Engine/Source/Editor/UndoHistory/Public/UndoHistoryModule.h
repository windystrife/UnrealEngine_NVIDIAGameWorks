// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IUndoHistoryModule.h"

static const FName UndoHistoryTabName("UndoHistory");

/**
* Implements the UndoHistory module.
*/
class FUndoHistoryModule : public IUndoHistoryModule
{
public:

	//~ Begin IModuleInterface Interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool SupportsDynamicReloading() override;

	//~ End IModuleInterface Interface

	static void ExecuteOpenUndoHistory()
	{
		FGlobalTabmanager::Get()->InvokeTab(UndoHistoryTabName);
	}

private:
	// Handles creating the project settings tab.
	TSharedRef<SDockTab> HandleSpawnSettingsTab(const FSpawnTabArgs& SpawnTabArgs);
};
