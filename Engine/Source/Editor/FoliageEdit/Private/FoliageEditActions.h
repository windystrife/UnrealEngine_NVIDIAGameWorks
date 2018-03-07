// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

/**
 * Unreal Foliage Edit mode actions
 */
class FFoliageEditCommands : public TCommands<FFoliageEditCommands>
{

public:
	FFoliageEditCommands() : TCommands<FFoliageEditCommands>
	(
		"FoliageEditMode", // Context name for fast lookup
		NSLOCTEXT("Contexts", "FoliageEditMode", "Foliage Edit Mode"), // Localized context name for displaying
		NAME_None, // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	
	/**
	 * Foliage Edit Commands
	 */
	
	/** Commands for the foliage brush settings. */
	TSharedPtr< FUICommandInfo > IncreaseBrushSize;
	TSharedPtr< FUICommandInfo > DecreaseBrushSize;

	/** Commands for the tools toolbar. */
	TSharedPtr< FUICommandInfo > SetPaint;
	TSharedPtr< FUICommandInfo > SetReapplySettings;
	TSharedPtr< FUICommandInfo > SetSelect;
	TSharedPtr< FUICommandInfo > SetLassoSelect;
	TSharedPtr< FUICommandInfo > SetPaintBucket;

	/** Commands for the foliage item toolbar. */
	TSharedPtr< FUICommandInfo > SetNoSettings;
	TSharedPtr< FUICommandInfo > SetPaintSettings;
	TSharedPtr< FUICommandInfo > SetClusterSettings;

	/** FoliageType commands */
	TSharedPtr< FUICommandInfo > RemoveFoliageType;
	TSharedPtr< FUICommandInfo > ShowFoliageTypeInCB;
	TSharedPtr< FUICommandInfo > SelectAllInstances;
	TSharedPtr< FUICommandInfo > DeselectAllInstances;
	TSharedPtr< FUICommandInfo > SelectInvalidInstances;
	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
};
