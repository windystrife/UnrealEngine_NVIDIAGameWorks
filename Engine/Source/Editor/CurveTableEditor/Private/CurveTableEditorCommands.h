// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FCurveTableEditorCommands : public TCommands<FCurveTableEditorCommands>
{
public:
	FCurveTableEditorCommands()
		: TCommands<FCurveTableEditorCommands>(TEXT("CurveTableEditor"), NSLOCTEXT("Contexts", "CurveTableEditor", "Curve Table Editor"), NAME_None, FEditorStyle::GetStyleSetName())
	{
	}

	virtual void RegisterCommands() override;

public:

	// Command to allow users to toggle the view mode
	TSharedPtr<FUICommandInfo> CurveViewToggle;
};
