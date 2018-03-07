// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditModeSettings.h"
#include "EditorModeManager.h"
#include "ControlRigEditMode.h"
#include "ControlRigSequence.h"
#include "AssetEditorManager.h"
#include "Components/SkeletalMeshComponent.h"

void UControlRigEditModeSettings::PreEditChange(UProperty* PropertyAboutToChange)
{
	if (PropertyAboutToChange)
	{
		if (PropertyAboutToChange->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEditModeSettings, Actor))
		{
			PrevActor = Actor.Get();
		}
	}
}

void UControlRigEditModeSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (UProperty* Property = PropertyChangedEvent.Property)
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEditModeSettings, Actor))
		{
			if (Actor.IsValid())
			{
				if (Actor->FindComponentByClass<USkeletalMeshComponent>())
				{
					// user set the actor we are targeting, so bind our (standalone) sequence to it
					if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
					{
						ControlRigEditMode->HandleBindToActor(Actor.Get(), true);
					}
				}
				else
				{
					Actor = PrevActor;
				}
			}
			else
			{
				// user cleared the actor we are targeting, so (un)bind our sequence to it
				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->HandleBindToActor(nullptr, true);
				}
			}
		}
		else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEditModeSettings, Sequence))
		{
			if (Sequence)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(Sequence);
			}
		}
		else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEditModeSettings, bDisplayTrajectories))
		{
			if (Sequence)
			{
				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->RefreshTrajectoryCache();
				}
			}
		}
	}
}