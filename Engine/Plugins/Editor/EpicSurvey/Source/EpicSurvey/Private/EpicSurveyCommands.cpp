// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EpicSurveyCommands.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

FEpicSurveyCommands::FEpicSurveyCommands()
	: TCommands<FEpicSurveyCommands>(
		TEXT("EpicSurvey"), // Context name for fast lookup
		LOCTEXT( "EpicSurvey", "Epic Survey" ), // Localized context name for displaying
		NAME_None,	 // No parent context
		FEditorStyle::GetStyleSetName() // Icon Style Set
		)
{
}

void FEpicSurveyCommands::RegisterCommands()
{
	UI_COMMAND( OpenEpicSurvey, "Open Epic Survey...", "Allows you to send feedback to Epic to help improve UE4.", EUserInterfaceActionType::Button, FInputChord() );
}

#undef LOCTEXT_NAMESPACE
