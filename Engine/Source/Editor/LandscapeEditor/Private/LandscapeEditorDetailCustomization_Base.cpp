// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditorDetailCustomization_Base.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "LandscapeEdMode.h"



FEdModeLandscape* FLandscapeEditorDetailCustomization_Base::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}

bool FLandscapeEditorDetailCustomization_Base::IsToolActive(FName ToolName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode != NULL && LandscapeEdMode->CurrentTool != NULL)
	{
		const FName CurrentToolName = LandscapeEdMode->CurrentTool->GetToolName();
		return CurrentToolName == ToolName;
	}

	return false;
}

bool FLandscapeEditorDetailCustomization_Base::IsBrushSetActive(FName BrushSetName)
{
	FEdModeLandscape* LandscapeEdMode = GetEditorMode();
	if (LandscapeEdMode && LandscapeEdMode->CurrentBrushSetIndex >= 0)
	{
		const FName CurrentBrushSetName = LandscapeEdMode->LandscapeBrushSets[LandscapeEdMode->CurrentBrushSetIndex].BrushSetName;
		return CurrentBrushSetName == BrushSetName;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

FEdModeLandscape* FLandscapeEditorStructCustomization_Base::GetEditorMode()
{
	return (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
}
