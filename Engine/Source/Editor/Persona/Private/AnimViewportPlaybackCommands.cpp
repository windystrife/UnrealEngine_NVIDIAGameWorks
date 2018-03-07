// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimViewportPlaybackCommands.h"
#include "EditorStyleSet.h"
#include "AnimationEditorViewportClient.h"

#define LOCTEXT_NAMESPACE "AnimViewportPlaybackCommands"

FAnimViewportPlaybackCommands::FAnimViewportPlaybackCommands() : TCommands<FAnimViewportPlaybackCommands>
	(
	TEXT("AnimViewportPlayback"), // Context name for fast lookup
	NSLOCTEXT("Contexts", "AnimViewportPlayback", "Animation Viewport Playback"), // Localized context name for displaying
	NAME_None, // Parent context name.  
	FEditorStyle::GetStyleSetName() // Icon Style Set
	)
{
	PlaybackSpeedCommands.AddZeroed(EAnimationPlaybackSpeeds::NumPlaybackSpeeds);
	TurnTableSpeeds.AddZeroed(EAnimationPlaybackSpeeds::NumPlaybackSpeeds);
}

void FAnimViewportPlaybackCommands::RegisterCommands()
{
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::OneTenth],	"x0.1", "Set the animation playback speed to a tenth of normal", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::Quarter],		"x0.25", "Set the animation playback speed to a quarter of normal", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::Half],		"x0.5", "Set the animation playback speed to a half of normal", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::Normal],		"x1.0", "Set the animation playback speed to normal", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::Double],		"x2.0", "Set the animation playback speed to double the speed of normal", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::FiveTimes],	"x5.0", "Set the animation playback speed to five times the normal speed", EUserInterfaceActionType::RadioButton, FInputChord() );
	UI_COMMAND( PlaybackSpeedCommands[EAnimationPlaybackSpeeds::TenTimes],	"x10.0", "Set the animation playback speed to ten times the normal speed", EUserInterfaceActionType::RadioButton, FInputChord() );

	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::OneTenth], "x0.1", "Set the turn table rotation speed to a tenth of normal", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::Quarter], "x0.25", "Set the turn table rotation speed to a quarter of normal", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::Half], "x0.5", "Set the turn table rotation speed to a half of normal", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::Normal], "x1.0", "Set the turn table rotation speed to normal", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::Double], "x2.0", "Set the turn table rotation speed to double the speed of normal", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::FiveTimes], "x5.0", "Set the turn table rotation speed to five times the normal speed", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(TurnTableSpeeds[EAnimationPlaybackSpeeds::TenTimes], "x10.0", "Set the turn table rotation speed to ten times the normal speed", EUserInterfaceActionType::RadioButton, FInputChord());

	UI_COMMAND(PersonaTurnTablePlay, "Play", "Turn table rotates", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(PersonaTurnTablePause, "Pause", "Freeze with current rotation", EUserInterfaceActionType::RadioButton, FInputChord());
	UI_COMMAND(PersonaTurnTableStop, "Stop", "Stop and Reset orientation", EUserInterfaceActionType::RadioButton, FInputChord());

}

#undef LOCTEXT_NAMESPACE
