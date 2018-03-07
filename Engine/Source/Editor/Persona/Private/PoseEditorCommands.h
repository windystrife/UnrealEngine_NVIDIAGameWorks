// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

/**
* Class containing commands for pose editor
*/
class FPoseEditorCommands : public TCommands<FPoseEditorCommands>
{
public:
	FPoseEditorCommands()
	: TCommands<FPoseEditorCommands>
		(
			TEXT("PoseEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "PoseEditor", "Pose Editor"), // Localized context name for displaying
			NAME_None, // Parent context name.  
			FEditorStyle::GetStyleSetName() // Icon Style Set
		)
	{}

	/** Initialize commands */
	virtual void RegisterCommands() override;

	/** Paste all names */
	TSharedPtr< FUICommandInfo > PasteAllNames;
};


