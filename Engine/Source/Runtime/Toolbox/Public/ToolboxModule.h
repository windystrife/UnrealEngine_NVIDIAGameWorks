// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FWorkspaceItem;

/**
 * The toolbox module is a lightweight UI for quickly summoning
 * development tools: profilers, widget inspector, etc.
 */
class IToolboxModule : public IModuleInterface
{
public:

	/**
	 * Register spawners for tabs provided by IToolboxModule. Only the first call to this method has any effect.
	 * Subsequent calls will be ignored until the module is reloaded.
	 *
	 * @param  DebugToolsTabCategory       Optional menu category for the debug tools tab.
	 * @param  ModulesTabCategory          Optional menu category for the modules tab.
	 */
	virtual void RegisterSpawners(const TSharedPtr<FWorkspaceItem>& DebugToolsTabCategory, const TSharedPtr<FWorkspaceItem>& ModulesTabCategory) = 0;

	/** Open the toolbox tab,  */
	virtual void SummonToolbox() = 0;
};
