// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_CameraAnim.h"
#include "Engine/World.h"
#include "Editor.h"
#include "EdMode.h"
#include "Misc/ConfigCacheIni.h"
#include "LevelEditorViewport.h"
#include "EditorModes.h"
#include "Matinee/InterpData.h"
#include "Matinee/InterpGroup.h"
#include "Matinee/InterpGroupCamera.h"
#include "Matinee/InterpGroupInst.h"
#include "Matinee/InterpTrackInst.h"
#include "Matinee/InterpTrackAnimControl.h"
#include "Matinee/MatineeActorCameraAnim.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_CameraAnim::CreateMatineeActorForCameraAnim(UCameraAnim* InCameraAnim)
{
	check(InCameraAnim);

	FActorSpawnParameters ActorSpawnParameters;
	ActorSpawnParameters.Name = InCameraAnim->GetFName();
	PreviewMatineeActor = GEditor->GetEditorWorldContext().World()->SpawnActor<AMatineeActorCameraAnim>(ActorSpawnParameters);
	check(PreviewMatineeActor.IsValid());
	UInterpData* NewData = NewObject<UInterpData>(GetTransientPackage(), NAME_None, RF_Transactional);
	PreviewMatineeActor.Get()->MatineeData = NewData;
	PreviewMatineeActor.Get()->CameraAnim = InCameraAnim;
}

void FAssetTypeActions_CameraAnim::CreateCameraActorForCameraAnim(UCameraAnim* InCameraAnim)
{
	check(InCameraAnim);

	FVector ViewportCamLocation(FVector::ZeroVector);
	FRotator ViewportCamRotation(FRotator::ZeroRotator);

	for (int32 ViewportIndex = 0; ViewportIndex < GEditor->LevelViewportClients.Num(); ViewportIndex++)
	{
		FLevelEditorViewportClient* ViewportClient = GEditor->LevelViewportClients[ViewportIndex];
		if(ViewportClient != NULL && ViewportClient->ViewportType == LVT_Perspective)
		{
			ViewportCamLocation = ViewportClient->ViewTransformPerspective.GetLocation();
			ViewportCamRotation = ViewportClient->ViewTransformPerspective.GetRotation();
			break;
		}
	}

	PreviewCamera = GEditor->GetEditorWorldContext().World()->SpawnActor<ACameraActor>(ViewportCamLocation, ViewportCamRotation);
	check(PreviewCamera.IsValid());
	PreviewCamera.Get()->SetFlags(RF_Transient);
	PreviewCamera.Get()->SetActorLabel(FText::Format(LOCTEXT("CamerAnimPreviewCameraName", "Preview Camera - {0}"), FText::FromName(InCameraAnim->GetFName())).ToString());

	// copy data from the CamAnim to the CameraActor
	check(PreviewCamera.Get()->GetCameraComponent());
	PreviewCamera.Get()->PreviewedCameraAnim = InCameraAnim;
	PreviewCamera.Get()->GetCameraComponent()->FieldOfView = InCameraAnim->BaseFOV;
	PreviewCamera.Get()->GetCameraComponent()->PostProcessSettings = InCameraAnim->BasePostProcessSettings;
}

void FAssetTypeActions_CameraAnim::CreatePreviewPawnForCameraAnim(UCameraAnim* InCameraAnim)
{
	check(InCameraAnim);

	UInterpGroupCamera* CamInterpGroup = Cast<UInterpGroupCamera>(InCameraAnim->CameraInterpGroup);
	if(CamInterpGroup != NULL)
	{
		// link back to camera, so it can update back after it's done
		CamInterpGroup->CameraAnimInst = InCameraAnim;

		// if no preview pawn class is set, get default one
		if (CamInterpGroup->Target.PawnClass == NULL)
		{
			UClass* DefaultPreviewPawnClass = NULL;
			FString PreviewPawnName = GConfig->GetStr(TEXT("CameraPreview"), TEXT("DefaultPreviewPawnClassName"), GEditorIni);
			if ( PreviewPawnName.Len() > 0 )
			{
				DefaultPreviewPawnClass = LoadObject<UClass>(NULL, *PreviewPawnName, NULL, LOAD_None, NULL);
				CamInterpGroup->Target.PawnClass = DefaultPreviewPawnClass;
			}
			else
			{
				UE_LOG(LogCameraAnim, Display, TEXT("Matinee preview default pawn class is missing."));
				return;
			}
		}

		// create preview pawn in the location
		InCameraAnim->PreviewInterpGroup = CreateInterpGroup(InCameraAnim, CamInterpGroup->Target);
	}
}

void FAssetTypeActions_CameraAnim::CreatePreviewPawn(UCameraAnim* InCameraAnim, UClass* InPreviewPawnClass, const FVector& InLocation, const FRotator& InRotation)
{
	check(InCameraAnim);
	check(InPreviewPawnClass);

	PreviewPawn = GEditor->GetEditorWorldContext().World()->SpawnActor<APawn>(InPreviewPawnClass, InLocation, InRotation);
	check(PreviewPawn.IsValid());
	PreviewPawn.Get()->SetFlags(RF_Transient);
	PreviewPawn.Get()->SetActorLabel(FText::Format(LOCTEXT("CamerAnimPreviewPawnName", "Preview Pawn - {0}"), FText::FromName(InCameraAnim->GetFName())).ToString());
}

UInterpGroup* FAssetTypeActions_CameraAnim::CreateInterpGroup(UCameraAnim* InCameraAnim, FCameraPreviewInfo& PreviewInfo)
{
	check(InCameraAnim);

	CreatePreviewPawn(InCameraAnim, PreviewInfo.PawnClass, PreviewInfo.Location, PreviewInfo.Rotation);

	PreviewInfo.PawnInst = PreviewPawn.Get();

	if (PreviewInfo.PawnInst)
	{
		// create InterpGroup so that we can play animation to this pawn
		check(PreviewMatineeActor.Get()->MatineeData);
		UInterpGroup* NewGroup = NewObject<UInterpGroup>(PreviewMatineeActor.Get()->MatineeData, NAME_None, RF_Transient);
		NewGroup->GroupName = FName(TEXT("Preview Pawn"));
		NewGroup->EnsureUniqueName();

		PreviewMatineeActor.Get()->MatineeData->InterpGroups.Add(NewGroup);

		// now add group inst
		UInterpGroupInst* NewGroupInst = NewObject<UInterpGroupInst>(PreviewMatineeActor.Get(), NAME_None, RF_Transient);
		// Initialise group instance, saving ref to actor it works on.
		NewGroupInst->InitGroupInst(NewGroup, PreviewInfo.PawnInst);
		const int32 NewGroupInstIndex = PreviewMatineeActor.Get()->GroupInst.Add(NewGroupInst);

		//Link group with actor
		PreviewMatineeActor.Get()->InitGroupActorForGroup(NewGroup, PreviewInfo.PawnInst);

		// Now time to add AnimTrack so that we can play animation
		int32 AnimTrackIndex = INDEX_NONE;
		// add anim track but do not use addtotrack function that does too many things 
		// Construct track and track instance objects.
		UInterpTrackAnimControl* AnimTrack = NewObject<UInterpTrackAnimControl>(NewGroup, NAME_None, RF_Transient);
		check(AnimTrack);

		NewGroup->InterpTracks.Add(AnimTrack);
		
		// use config anim slot
		AnimTrack->SlotName = FName(*GConfig->GetStr(TEXT("MatineePreview"), TEXT("DefaultAnimSlotName"), GEditorIni));
		UInterpTrackInst* NewTrackInst = NewObject<UInterpTrackInst>(NewGroupInst, AnimTrack->TrackInstClass, NAME_None, RF_Transient);
		check(NewTrackInst);

		NewGroupInst->TrackInst.Add(NewTrackInst);

		// Initialize track, giving selected object.
		NewTrackInst->InitTrackInst(AnimTrack);

		// Save state into new track before doing anything else (because we didn't do it on ed mode change).
		NewTrackInst->SaveActorState(AnimTrack);
		check(NewGroupInst->TrackInst.Num() > 0);

		// add default anim curve weights to be 1
		int32 KeyIndex = AnimTrack->CreateNewKey(0.0f);
		AnimTrack->SetKeyOut(0, KeyIndex, 1.0f);

		if(PreviewInfo.AnimSeq != NULL)
		{
			KeyIndex = AnimTrack->AddKeyframe(0.0f, NewGroupInst->TrackInst[0], CIM_Linear);
			FAnimControlTrackKey& NewSeqKey = AnimTrack->AnimSeqs[KeyIndex];
			NewSeqKey.AnimSeq = PreviewInfo.AnimSeq;	
		}

		PreviewPawn = PreviewInfo.PawnInst;

		return NewGroup;
	}
	return NULL;
}


void FAssetTypeActions_CameraAnim::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	if(InObjects.Num() > 0)
	{
		UCameraAnim* CameraAnim = Cast<UCameraAnim>(InObjects[0]);
		if (CameraAnim != NULL)
		{
			// construct a temporary matinee actor
			CreateMatineeActorForCameraAnim(CameraAnim);

			if (GEditor->ShouldOpenMatinee(PreviewMatineeActor.Get()))
			{
				// changed the actor type, but don't want to lose any properties from previous
				// so duplicate from old, but with new class
				check(CameraAnim->CameraInterpGroup);
				if (!CameraAnim->CameraInterpGroup->IsA(UInterpGroupCamera::StaticClass()))
				{
					CameraAnim->CameraInterpGroup = CastChecked<UInterpGroupCamera>(StaticDuplicateObject(CameraAnim->CameraInterpGroup, CameraAnim, TEXT("CameraAnimation"), RF_NoFlags, UInterpGroupCamera::StaticClass()));
				}

				UInterpGroupCamera* NewInterpGroup = CastChecked<UInterpGroupCamera>(CameraAnim->CameraInterpGroup);
				check(NewInterpGroup);

				if (PreviewMatineeActor.Get()->MatineeData)
				{
					PreviewMatineeActor.Get()->MatineeData->SetFlags(RF_Transient);
					PreviewMatineeActor.Get()->MatineeData->InterpLength = CameraAnim->AnimLength;

					if (NewInterpGroup)
					{
						PreviewMatineeActor.Get()->MatineeData->InterpGroups.Add(NewInterpGroup);
					}
				}

				// create a CameraActor and connect it to the Interp. will create this at the perspective viewport's location and rotation
				CreateCameraActorForCameraAnim(CameraAnim);

				// set up the group actor
				PreviewMatineeActor.Get()->InitGroupActorForGroup(NewInterpGroup, PreviewCamera.Get());

				// Create preview pawn
				CreatePreviewPawnForCameraAnim(CameraAnim);

				// this will create the instances for everything
				PreviewMatineeActor.Get()->InitInterp();

				// open matinee for this actor
				GEditor->OpenMatinee(PreviewMatineeActor.Get());

				// install our delegate so we can clean up when finished
				OnMatineeEditorClosedDelegateHandle = FEditorDelegates::EditorModeExit.AddSP(this, &FAssetTypeActions_CameraAnim::OnMatineeEditorClosed);
			}
			else
			{
				GEditor->GetEditorWorldContext().World()->DestroyActor(PreviewMatineeActor.Get());
			}
		}
	}
}

void FAssetTypeActions_CameraAnim::OnMatineeEditorClosed(FEdMode* InEditorMode)
{
	check(InEditorMode);
	if(InEditorMode->GetID() == FBuiltinEditorModes::EM_InterpEdit)
	{
		// clean up our preview actors if they are still present
		if(PreviewCamera.IsValid())
		{
			GEditor->GetEditorWorldContext().World()->DestroyActor(PreviewCamera.Get(), false, false);
			PreviewCamera.Reset();
		}

		if (PreviewMatineeActor.IsValid())
		{
			GEditor->GetEditorWorldContext().World()->DestroyActor(PreviewMatineeActor.Get(), false, false);
			PreviewMatineeActor.Reset();
		}

		if(PreviewPawn.IsValid())
		{
			GEditor->GetEditorWorldContext().World()->DestroyActor(PreviewPawn.Get(), false, false);
			PreviewPawn.Reset();
		}

		// remove our delegate
		FEditorDelegates::EditorModeExit.Remove(OnMatineeEditorClosedDelegateHandle);
	}
}

#undef LOCTEXT_NAMESPACE
