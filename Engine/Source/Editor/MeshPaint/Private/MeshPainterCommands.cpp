// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPainterCommands.h"

#define LOCTEXT_NAMESPACE "MeshPainterCommands"

void FMeshPainterCommands::RegisterCommands()
{
	UI_COMMAND(IncreaseBrushSize, "IncreaseBrush", "Increases brush size", EUserInterfaceActionType::Button, FInputChord(EKeys::RightBracket, EModifierKey::Control));
	Commands.Add(IncreaseBrushSize);

	UI_COMMAND(DecreaseBrushSize, "DecreaseBrush", "Decreases brush size", EUserInterfaceActionType::Button, FInputChord(EKeys::LeftBracket, EModifierKey::Control));
	Commands.Add(DecreaseBrushSize);
}

#undef LOCTEXT_NAMESPACE

