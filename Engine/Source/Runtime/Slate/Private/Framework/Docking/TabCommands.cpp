// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Docking/TabCommands.h"

#define LOCTEXT_NAMESPACE "TabCommands"

void FTabCommands::RegisterCommands()
{
	UI_COMMAND(CloseMajorTab, "Close Major Tab", "Closes the focused major tab", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::F4))
	UI_COMMAND(CloseMinorTab, "Close Minor Tab", "Closes the current window's active minor tab", EUserInterfaceActionType::Button, FInputChord())
}

#undef LOCTEXT_NAMESPACE
