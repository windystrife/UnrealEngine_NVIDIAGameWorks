// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FBTCommonCommands : public TCommands<FBTCommonCommands>
{
public:
	FBTCommonCommands();

	TSharedPtr<FUICommandInfo> SearchBT;
	TSharedPtr<FUICommandInfo> NewBlackboard;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

class FBTDebuggerCommands : public TCommands<FBTDebuggerCommands>
{
public:
	FBTDebuggerCommands();

	TSharedPtr<FUICommandInfo> BackInto;
	TSharedPtr<FUICommandInfo> BackOver;
	TSharedPtr<FUICommandInfo> ForwardInto;
	TSharedPtr<FUICommandInfo> ForwardOver;
	TSharedPtr<FUICommandInfo> StepOut;

	TSharedPtr<FUICommandInfo> PausePlaySession;
	TSharedPtr<FUICommandInfo> ResumePlaySession;
	TSharedPtr<FUICommandInfo> StopPlaySession;

	TSharedPtr<FUICommandInfo> CurrentValues;
	TSharedPtr<FUICommandInfo> SavedValues;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};

class FBTBlackboardCommands : public TCommands<FBTBlackboardCommands>
{
public:
	FBTBlackboardCommands();

	TSharedPtr<FUICommandInfo> DeleteEntry;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
