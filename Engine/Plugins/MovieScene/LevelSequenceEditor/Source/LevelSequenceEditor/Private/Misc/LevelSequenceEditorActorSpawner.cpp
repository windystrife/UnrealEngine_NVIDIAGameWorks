// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceEditorActorSpawner.h"
#include "MovieScene.h"
#include "ActorFactories/ActorFactory.h"
#include "AssetSelection.h"
#include "LevelEditorViewport.h"
#include "SnappingUtils.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "SequencerSettings.h"
#include "ISequencer.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "MovieSceneSequence.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorActorSpawner"

TSharedRef<IMovieSceneObjectSpawner> FLevelSequenceEditorActorSpawner::CreateObjectSpawner()
{
	return MakeShareable(new FLevelSequenceEditorActorSpawner);
}

#if WITH_EDITOR

TValueOrError<FNewSpawnable, FText> FLevelSequenceEditorActorSpawner::CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene)
{
	FNewSpawnable NewSpawnable(nullptr, FName::NameToDisplayString(SourceObject.GetName(), false));

	const FName TemplateName = MakeUniqueObjectName(&OwnerMovieScene, UObject::StaticClass(), SourceObject.GetFName());

	// First off, deal with creating a spawnable from a class
	if (UClass* InClass = Cast<UClass>(&SourceObject))
	{
		if (!InClass->IsChildOf(AActor::StaticClass()))
		{
			FText ErrorText = FText::Format(LOCTEXT("NotAnActorClass", "Unable to add spawnable for class of type '{0}' since it is not a valid actor class."), FText::FromString(InClass->GetName()));
			return MakeError(ErrorText);
		}

		NewSpawnable.ObjectTemplate = NewObject<UObject>(&OwnerMovieScene, InClass, TemplateName);
	}

	// Deal with creating a spawnable from an instance of an actor
	else if (AActor* Actor = Cast<AActor>(&SourceObject))
	{
		AActor* SpawnedActor = Cast<AActor>(StaticDuplicateObject(Actor, &OwnerMovieScene, TemplateName, RF_AllFlags & ~RF_Transactional));
		SpawnedActor->bIsEditorPreviewActor = false;
		NewSpawnable.ObjectTemplate = SpawnedActor;
		NewSpawnable.Name = Actor->GetActorLabel();		
	}

	// If it's a blueprint, we need some special handling
	else if (UBlueprint* SourceBlueprint = Cast<UBlueprint>(&SourceObject))
	{
		NewSpawnable.ObjectTemplate = NewObject<UObject>(&OwnerMovieScene, SourceBlueprint->GeneratedClass, TemplateName);
	}

	// At this point we have to assume it's an asset
	else
	{
		// @todo sequencer: Add support for forcing specific factories for an asset?
		UActorFactory* FactoryToUse = FActorFactoryAssetProxy::GetFactoryForAssetObject(&SourceObject);
		if (!FactoryToUse)
		{
			FText ErrorText = FText::Format(LOCTEXT("CouldNotFindFactory", "Unable to create spawnable from  asset '{0}' - no valid factory could be found."), FText::FromString(SourceObject.GetName()));
			return MakeError(ErrorText);
		}

		FText ErrorText;
		if (!FactoryToUse->CanCreateActorFrom(FAssetData(&SourceObject), ErrorText))
		{
			if (!ErrorText.IsEmpty())
			{
				return MakeError(FText::Format(LOCTEXT("CannotCreateActorFromAsset_Ex", "Unable to create spawnable from  asset '{0}'. {1}."), FText::FromString(SourceObject.GetName()), ErrorText));
			}
			else
			{
				return MakeError(FText::Format(LOCTEXT("CannotCreateActorFromAsset", "Unable to create spawnable from  asset '{0}'."), FText::FromString(SourceObject.GetName())));
			}
		}

		AActor* Instance = FactoryToUse->CreateActor(&SourceObject, GWorld->PersistentLevel, FTransform(), RF_Transient, TemplateName );
		Instance->bIsEditorPreviewActor = false;
		NewSpawnable.ObjectTemplate = StaticDuplicateObject(Instance, &OwnerMovieScene, TemplateName, RF_AllFlags & ~RF_Transient);

		const bool bNetForce = false;
		const bool bShouldModifyLevel = false;
		GWorld->DestroyActor(Instance, bNetForce, bShouldModifyLevel);
	}

	if (!NewSpawnable.ObjectTemplate || !NewSpawnable.ObjectTemplate->IsA<AActor>())
	{
		FText ErrorText = FText::Format(LOCTEXT("UnknownClassError", "Unable to create a new spawnable object from {0}."), FText::FromString(SourceObject.GetName()));
		return MakeError(ErrorText);
	}

	return MakeValue(NewSpawnable);
}

static void PlaceActorInFrontOfCamera(AActor* ActorCDO)
{
	// Place the actor in front of the active perspective camera if we have one
	if ((GCurrentLevelEditingViewportClient != nullptr) && GCurrentLevelEditingViewportClient->IsPerspective())
	{
		// Don't allow this when the active viewport is showing a simulation/PIE level
		const bool bIsViewportShowingPIEWorld = GCurrentLevelEditingViewportClient->GetWorld()->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor);
		if (!bIsViewportShowingPIEWorld)
		{
			// @todo sequencer actors: Ideally we could use the actor's collision to figure out how far to push out
			// the object (like when placing in viewports), but we can't really do that because we're only dealing with a CDO
			const float DistanceFromCamera = 50.0f;

			// Find a place to put the object
			// @todo sequencer cleanup: This code should be reconciled with the GEditor->MoveActorInFrontOfCamera() stuff
			const FVector& CameraLocation = GCurrentLevelEditingViewportClient->GetViewLocation();
			FRotator CameraRotation = GCurrentLevelEditingViewportClient->GetViewRotation();
			const FVector CameraDirection = CameraRotation.Vector();

			FVector NewLocation = CameraLocation + CameraDirection * (DistanceFromCamera + GetDefault<ULevelEditorViewportSettings>()->BackgroundDropDistance);
			FSnappingUtils::SnapPointToGrid(NewLocation, FVector::ZeroVector);

			CameraRotation.Roll = 0.f;
			CameraRotation.Pitch = 0.f;

			ActorCDO->SetActorRelativeLocation(NewLocation);
			ActorCDO->SetActorRelativeRotation(CameraRotation);
		}
	}
}

bool FLevelSequenceEditorActorSpawner::CanSetupDefaultsForSpawnable(UObject* SpawnedObject) const
{
	if (SpawnedObject == nullptr)
	{
		return true;
	}

	return FLevelSequenceActorSpawner::CanSetupDefaultsForSpawnable(SpawnedObject);
}

void FLevelSequenceEditorActorSpawner::SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const FTransformData& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings)
{
	FTransformData DefaultTransform = TransformData;

	AActor* SpawnedActor = Cast<AActor>(SpawnedObject);
	if (SpawnedActor)
	{
		// Place the new spawnable in front of the camera (unless we were automatically created from a PIE actor)
		if (Settings->GetSpawnPosition() == SSP_PlaceInFrontOfCamera)
		{
			PlaceActorInFrontOfCamera(SpawnedActor);
		}
		DefaultTransform.Translation = SpawnedActor->GetActorLocation();
		DefaultTransform.Rotation = SpawnedActor->GetActorRotation();
		DefaultTransform.Scale = FVector(1.0f, 1.0f, 1.0f);
		DefaultTransform.bValid = true;

		Sequencer->OnActorAddedToSequencer().Broadcast(SpawnedActor, Guid);

		const bool bNotifySelectionChanged = true;
		const bool bDeselectBSP = true;
		const bool bWarnAboutTooManyActors = false;
		const bool bSelectEvenIfHidden = false;

		GEditor->SelectNone(bNotifySelectionChanged, bDeselectBSP, bWarnAboutTooManyActors);
		GEditor->SelectActor(SpawnedActor, true, bNotifySelectionChanged, bSelectEvenIfHidden);
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	UMovieScene* OwnerMovieScene = Sequence->GetMovieScene();

	// Ensure it has a spawn track
	UMovieSceneSpawnTrack* SpawnTrack = Cast<UMovieSceneSpawnTrack>(OwnerMovieScene->FindTrack(UMovieSceneSpawnTrack::StaticClass(), Guid, NAME_None));
	if (!SpawnTrack)
	{
		SpawnTrack = Cast<UMovieSceneSpawnTrack>(OwnerMovieScene->AddTrack(UMovieSceneSpawnTrack::StaticClass(), Guid));
	}

	if (SpawnTrack)
	{
		UMovieSceneBoolSection* SpawnSection = Cast<UMovieSceneBoolSection>(SpawnTrack->CreateNewSection());
		SpawnSection->SetDefault(true);
		SpawnSection->SetIsInfinite(Sequencer->GetInfiniteKeyAreas());
		SpawnTrack->AddSection(*SpawnSection);
		SpawnTrack->SetObjectId(Guid);
	}

	// Ensure it will spawn in the right place
	if (DefaultTransform.bValid)
	{
		UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(OwnerMovieScene->FindTrack(UMovieScene3DTransformTrack::StaticClass(), Guid, "Transform"));
		if (!TransformTrack)
		{
			TransformTrack = Cast<UMovieScene3DTransformTrack>(OwnerMovieScene->AddTrack(UMovieScene3DTransformTrack::StaticClass(), Guid));
		}

		if (TransformTrack)
		{
			const bool bUnwindRotation = false;

			EMovieSceneKeyInterpolation Interpolation = Sequencer->GetKeyInterpolation();

			const TArray<UMovieSceneSection*>& Sections = TransformTrack->GetAllSections();
			if (!Sections.Num())
			{
				TransformTrack->AddSection(*TransformTrack->CreateNewSection());
			}

			for (UMovieSceneSection* Section : Sections)
			{
				UMovieScene3DTransformSection* TransformSection = CastChecked<UMovieScene3DTransformSection>(Section);

				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::X, DefaultTransform.Translation.X, bUnwindRotation));
				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::Y, DefaultTransform.Translation.Y, bUnwindRotation));
				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Translation, EAxis::Z, DefaultTransform.Translation.Z, bUnwindRotation));

				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::X, DefaultTransform.Rotation.Euler().X, bUnwindRotation));
				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Y, DefaultTransform.Rotation.Euler().Y, bUnwindRotation));
				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Rotation, EAxis::Z, DefaultTransform.Rotation.Euler().Z, bUnwindRotation));

				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::X, DefaultTransform.Scale.X, bUnwindRotation));
				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::Y, DefaultTransform.Scale.Y, bUnwindRotation));
				TransformSection->SetDefault(FTransformKey(EKey3DTransformChannel::Scale, EAxis::Z, DefaultTransform.Scale.Z, bUnwindRotation));

				TransformSection->SetIsInfinite(Sequencer->GetInfiniteKeyAreas());
			}
		}
	}
}

#endif	// #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE