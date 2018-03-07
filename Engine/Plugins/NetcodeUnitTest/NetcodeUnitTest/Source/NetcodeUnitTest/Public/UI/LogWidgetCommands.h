// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/CoreStyle.h"
#include "Framework/Commands/Commands.h"

/**
 * Defines a set of commands used by the unit test log window
 */
class FLogWidgetCommands : public TCommands<FLogWidgetCommands>
{
public:
	FLogWidgetCommands()
		: TCommands<FLogWidgetCommands>("NUTLogWidget", FText::FromString(TEXT("Unit Test Log")), NAME_None,
											FCoreStyle::Get().GetStyleSetName())
		, CopyLogLines(NULL)
		, FindLogText(NULL)
	{
	}

	/**
	 * Register commands
	 */
	void RegisterCommands() override;


public:
	/** Command for copying the currently selected log line(s) */
	TSharedPtr<FUICommandInfo>	CopyLogLines;

	/** Command for finding text within the current log window tab */
	TSharedPtr<FUICommandInfo>	FindLogText;
};

