// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FSequenceRecorderCommands : public TCommands<FSequenceRecorderCommands>
{
public:
	FSequenceRecorderCommands();

	TSharedPtr<FUICommandInfo> RecordAll;
	TSharedPtr<FUICommandInfo> StopAll;
	TSharedPtr<FUICommandInfo> AddRecording;
	TSharedPtr<FUICommandInfo> RemoveRecording;
	TSharedPtr<FUICommandInfo> RemoveAllRecordings;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
