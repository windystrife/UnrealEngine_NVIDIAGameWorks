// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FCodeProjectEditorCommands : public TCommands<FCodeProjectEditorCommands>
{
public:
	FCodeProjectEditorCommands();

	TSharedPtr<FUICommandInfo> Save;
	TSharedPtr<FUICommandInfo> SaveAll;

	/** Initialize commands */
	virtual void RegisterCommands() override;
};
