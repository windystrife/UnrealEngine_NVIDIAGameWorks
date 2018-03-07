// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorObjectSpawner.h"
#include "ControlRig.h"
#include "MovieScene.h"
#include "MovieSceneSpawnable.h"
#include "IMovieScenePlayer.h"
#include "ControlRigBindingTrack.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "ISequencer.h"
#include "ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "MovieSceneSequence.h"

#define LOCTEXT_NAMESPACE "ControlRigEditorObjectSpawner"

TSharedRef<IMovieSceneObjectSpawner> FControlRigEditorObjectSpawner::CreateObjectSpawner()
{
	return MakeShareable(new FControlRigEditorObjectSpawner);
}

UObject* FControlRigEditorObjectSpawner::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	UObject* SpawnedObject = FControlRigObjectSpawner::SpawnObject(Spawnable, TemplateID, Player);
	if (SpawnedObject)
	{
		// Let the edit mode know about a re-spawned Guid, as we may need to re-display the object
		if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
		{
			ControlRigEditMode->HandleObjectSpawned(Spawnable.GetGuid(), SpawnedObject, Player);
		}
	}
	return SpawnedObject;
}

#if WITH_EDITOR

TValueOrError<FNewSpawnable, FText> FControlRigEditorObjectSpawner::CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene)
{
	// Right now we only support creating a spawnable for classes
	if (UClass* InClass = Cast<UClass>(&SourceObject))
	{
		if (!InClass->IsChildOf(UControlRig::StaticClass()))
		{
			FText ErrorText = FText::Format(LOCTEXT("NotAnActorClass", "Unable to add spawnable for class of type '{0}' since it is not a valid animation controller class."), FText::FromString(InClass->GetName()));
			return MakeError(ErrorText);
		}

		FString ObjectName = SourceObject.GetName();
		ObjectName.RemoveFromEnd(TEXT("_C"));

		FNewSpawnable NewSpawnable(nullptr, FName::NameToDisplayString(ObjectName, false));

		const FName TemplateName = MakeUniqueObjectName(&OwnerMovieScene, UObject::StaticClass(), SourceObject.GetFName());

		NewSpawnable.ObjectTemplate = NewObject<UObject>(&OwnerMovieScene, InClass, TemplateName);

		return MakeValue(NewSpawnable);
	}

	return MakeError(FText());
}

void FControlRigEditorObjectSpawner::SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const FTransformData& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings)
{
	UMovieScene* OwnerMovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();

	// Ensure it has a binding track
	UControlRigBindingTrack* BindingTrack = Cast<UControlRigBindingTrack>(OwnerMovieScene->FindTrack(UControlRigBindingTrack::StaticClass(), Guid, NAME_None));
	if (!BindingTrack)
	{
		BindingTrack = Cast<UControlRigBindingTrack>(OwnerMovieScene->AddTrack(UControlRigBindingTrack::StaticClass(), Guid));
	}

	if (BindingTrack)
	{
		UMovieSceneSpawnSection* SpawnSection = Cast<UMovieSceneSpawnSection>(BindingTrack->CreateNewSection());
		SpawnSection->SetDefault(true);
		SpawnSection->SetIsInfinite(Sequencer->GetInfiniteKeyAreas());
		BindingTrack->AddSection(*SpawnSection);
		BindingTrack->SetObjectId(Guid);
	}
}

#endif	// #if WITH_EDITOR

#undef LOCTEXT_NAMESPACE