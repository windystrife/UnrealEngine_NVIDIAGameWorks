// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PersonaCommonCommands.h"

#define LOCTEXT_NAMESPACE "PersonaCommonCommands"

void FPersonaCommonCommands::RegisterCommands()
{
	UI_COMMAND(TogglePlay, "Play/Pause", "Play or pause the current animation", EUserInterfaceActionType::ToggleButton, FInputChord(EKeys::SpaceBar));
}

const FPersonaCommonCommands& FPersonaCommonCommands::Get()
{
	return TCommands<FPersonaCommonCommands>::Get();
}

#undef LOCTEXT_NAMESPACE
