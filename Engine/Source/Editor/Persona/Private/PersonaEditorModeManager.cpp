// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PersonaEditorModeManager.h"
#include "IPersonaEditMode.h"

bool FPersonaEditorModeManager::GetCameraTarget(FSphere& OutTarget) const
{
	// Note: assumes all of our modes are IPersonaEditMode!
	for(int32 ModeIndex = 0; ModeIndex < Modes.Num(); ++ModeIndex)
	{
		TSharedPtr<IPersonaEditMode> EditMode = StaticCastSharedPtr<IPersonaEditMode>(Modes[ModeIndex]);

		FSphere Target;
		if (EditMode->GetCameraTarget(Target))
		{
			OutTarget = Target;
			return true;
		}
	}

	return false;
}

void FPersonaEditorModeManager::GetOnScreenDebugInfo(TArray<FText>& OutDebugText) const
{
	// Note: assumes all of our modes are IPersonaEditMode!
	for (int32 ModeIndex = 0; ModeIndex < Modes.Num(); ++ModeIndex)
	{
		TSharedPtr<IPersonaEditMode> EditMode = StaticCastSharedPtr<IPersonaEditMode>(Modes[ModeIndex]);
		EditMode->GetOnScreenDebugInfo(OutDebugText);
	}
}
