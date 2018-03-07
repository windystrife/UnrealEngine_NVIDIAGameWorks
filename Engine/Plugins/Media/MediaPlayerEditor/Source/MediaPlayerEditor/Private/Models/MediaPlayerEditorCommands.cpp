// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Models/MediaPlayerEditorCommands.h"


#define LOCTEXT_NAMESPACE "FMediaPlayerEditorCommands"


void FMediaPlayerEditorCommands::RegisterCommands()
{
	UI_COMMAND(CloseMedia, "Close", "Close the currently opened media", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ForwardMedia, "Forward", "Fast forward media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(NextMedia, "Next", "Jump to next item in the play list", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PauseMedia, "Pause", "Pause media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PlayMedia, "Play", "Start media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(PreviousMedia, "Prev", "Jump to previous item in the play list", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ReverseMedia, "Reverse", "Reverse media playback", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RewindMedia, "Rewind", "Rewind the media to the beginning", EUserInterfaceActionType::Button, FInputChord());
}


#undef LOCTEXT_NAMESPACE
