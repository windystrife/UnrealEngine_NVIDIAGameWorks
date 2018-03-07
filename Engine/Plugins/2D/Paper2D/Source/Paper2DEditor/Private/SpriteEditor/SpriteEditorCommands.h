// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "PaperStyle.h"

class FSpriteEditorCommands : public TCommands<FSpriteEditorCommands>
{
public:
	FSpriteEditorCommands()
		: TCommands<FSpriteEditorCommands>(
			TEXT("SpriteEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "PaperEditor", "Sprite Editor"), // Localized context name for displaying
			NAME_None, // Parent
			FPaperStyle::Get()->GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

public:
	// Show toggles
	TSharedPtr<FUICommandInfo> SetShowGrid;
	TSharedPtr<FUICommandInfo> SetShowSourceTexture;
	TSharedPtr<FUICommandInfo> SetShowBounds;
	TSharedPtr<FUICommandInfo> SetShowCollision;
	
	TSharedPtr<FUICommandInfo> SetShowSockets;

	TSharedPtr<FUICommandInfo> SetShowPivot;
	TSharedPtr<FUICommandInfo> SetShowMeshEdges;

	// Source region edit mode
	TSharedPtr<FUICommandInfo> ExtractSprites;
	TSharedPtr<FUICommandInfo> ToggleShowRelatedSprites;
	TSharedPtr<FUICommandInfo> ToggleShowSpriteNames;

	// Editing modes
	TSharedPtr<FUICommandInfo> EnterViewMode;
	TSharedPtr<FUICommandInfo> EnterSourceRegionEditMode;
	TSharedPtr<FUICommandInfo> EnterCollisionEditMode;
	TSharedPtr<FUICommandInfo> EnterRenderingEditMode;
};
