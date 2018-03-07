// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

/**
 * Unreal level editor actions
 */
class LEVELEDITOR_API FLevelEditorModesCommands : public TCommands<FLevelEditorModesCommands>
{

public:
	FLevelEditorModesCommands() : TCommands<FLevelEditorModesCommands>
	(
		"LevelEditorModes", // Context name for fast lookup
		NSLOCTEXT("Contexts", "LevelEditorModes", "Level Editor Modes"), // Localized context name for displaying
		"MainFrame", // Parent
		FEditorStyle::GetStyleSetName() // Icon Style Set
	)
	{
	}
	
	/**
	 * Initialize commands
	 */
	virtual void RegisterCommands() override;

public:
	
	/** Editor mode commands */
	TArray< TSharedPtr< FUICommandInfo > > EditorModeCommands;
};


