// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/**
 * Class containing commands for viewport playback actions
 */
class FAnimViewportPlaybackCommands : public TCommands<FAnimViewportPlaybackCommands>
{
public:
	FAnimViewportPlaybackCommands();

	/** Command list for playback speed, indexed by EPlaybackSpeeds*/
	TArray<TSharedPtr< FUICommandInfo >> PlaybackSpeedCommands;

	/** Command list for turn table speed, indexed by EPlaybackSpeeds*/
	TArray<TSharedPtr< FUICommandInfo >> TurnTableSpeeds;
	TSharedPtr<FUICommandInfo> PersonaTurnTablePlay;
	TSharedPtr<FUICommandInfo> PersonaTurnTablePause;
	TSharedPtr<FUICommandInfo> PersonaTurnTableStop;

public:
	/** Registers our commands with the binding system */
	virtual void RegisterCommands() override;
};
