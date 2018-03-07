// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FLevelSequenceEditorCommands
	: public TCommands<FLevelSequenceEditorCommands>
{
public:

	/** Default constructor. */
	FLevelSequenceEditorCommands();

	/** Initialize commands */
	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> CreateNewLevelSequenceInLevel;
	TSharedPtr<FUICommandInfo> CreateNewMasterSequenceInLevel;
	TSharedPtr<FUICommandInfo> ToggleCinematicViewportCommand;
};
