// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "RenderDocPluginCommands.h"

#define LOCTEXT_NAMESPACE "RenderDocPlugin"
 
void FRenderDocPluginCommands::RegisterCommands()
{
	UI_COMMAND(CaptureFrameCommand
		, "Capture Frame", "Captures the next frame and launches the renderdoc UI (Alt+F12)",
		EUserInterfaceActionType::Button, FInputGesture(EKeys::F12, EModifierKey::Alt));
}

#undef LOCTEXT_NAMESPACE

#endif //WITH_EDITOR
