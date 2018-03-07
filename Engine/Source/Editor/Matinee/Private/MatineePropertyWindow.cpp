// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Matinee.h"
#include "Matinee/MatineeActorCameraAnim.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Camera/CameraAnim.h"

void FMatinee::BuildPropertyWindow()
{
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyWindow = PropertyModule.CreateDetailView( Args );

	if (IsCameraAnim())
	{
		// if editing a CameraAnim, show the CameraAnim object properties by default (when no track or group selected)
		AMatineeActorCameraAnim* const CamAnimMatineeActor = Cast<AMatineeActorCameraAnim>(MatineeActor);
		if ((CamAnimMatineeActor != nullptr) && (CamAnimMatineeActor->CameraAnim != nullptr))
		{
			TArray<UObject*> Objects;
			Objects.Add(CamAnimMatineeActor->CameraAnim);
			PropertyWindow->SetObjects(Objects);
		}
	}
}


/**
 * Updates the contents of the property window based on which groups or tracks are selected if any. 
 */
void FMatinee::UpdatePropertyWindow()
{
	TArray<UObject*> Objects;

	if( HasATrackSelected() )
	{
		TArray<UInterpTrack*> SelectedTracks;
		GetSelectedTracks(SelectedTracks);

		// We are guaranteed at least one selected track.
		check( SelectedTracks.Num() );

		// Send tracks to the property window
		for (int32 Index=0; Index < SelectedTracks.Num(); Index++)
		{
			Objects.Add(SelectedTracks[Index]);
		}
	}
	else if( HasAGroupSelected() )
	{
		TArray<UInterpGroup*> SelectedGroups;
		GetSelectedGroups(SelectedGroups);

		// We are guaranteed at least one selected group.
		check( SelectedGroups.Num() );

		// Send groups to the property window
		for (int32 Index=0; Index < SelectedGroups.Num(); Index++)
		{
			Objects.Add(SelectedGroups[Index]);
		}
	}
	else if (IsCameraAnim())
	{
		// if editing a CameraAnim, show the CameraAnim properties when nothing else selected
		AMatineeActorCameraAnim* const CamAnimMatineeActor = Cast<AMatineeActorCameraAnim>(MatineeActor);
		if ((CamAnimMatineeActor != nullptr) && (CamAnimMatineeActor->CameraAnim != nullptr))
		{
			Objects.Add(CamAnimMatineeActor->CameraAnim);
		}
	}
	// else send nothing
	
	PropertyWindow->SetObjects(Objects);
}
