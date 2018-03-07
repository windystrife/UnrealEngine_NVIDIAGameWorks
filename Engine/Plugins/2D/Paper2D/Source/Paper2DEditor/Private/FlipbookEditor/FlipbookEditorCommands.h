// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "PaperStyle.h"

class FFlipbookEditorCommands : public TCommands<FFlipbookEditorCommands>
{
public:
	FFlipbookEditorCommands()
		: TCommands<FFlipbookEditorCommands>(
			TEXT("FlipbookEditor"), // Context name for fast lookup
			NSLOCTEXT("Contexts", "FlipbookEditor", "Flipbook Editor"), // Localized context name for displaying
			NAME_None, // Parent
			FPaperStyle::Get()->GetStyleSetName() // Icon Style Set
			)
	{
	}

	// TCommand<> interface
	virtual void RegisterCommands() override;
	// End of TCommand<> interface

public:
	TSharedPtr<FUICommandInfo> AddKeyFrame;

	TSharedPtr<FUICommandInfo> SetShowGrid;
	TSharedPtr<FUICommandInfo> SetShowBounds;
	TSharedPtr<FUICommandInfo> SetShowCollision;

	// View Menu Commands
	TSharedPtr<FUICommandInfo> SetShowPivot;
	TSharedPtr<FUICommandInfo> SetShowSockets;

	// Timeline commands
	TSharedPtr<FUICommandInfo> AddNewFrame;
	TSharedPtr<FUICommandInfo> AddNewFrameBefore;
	TSharedPtr<FUICommandInfo> AddNewFrameAfter;

	// Asset commands
	TSharedPtr<FUICommandInfo> PickNewSpriteFrame;
	TSharedPtr<FUICommandInfo> EditSpriteFrame;
	TSharedPtr<FUICommandInfo> ShowInContentBrowser;
};
