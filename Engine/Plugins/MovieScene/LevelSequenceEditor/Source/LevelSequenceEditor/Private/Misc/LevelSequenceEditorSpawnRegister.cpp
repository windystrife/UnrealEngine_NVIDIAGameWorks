// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequenceEditorSpawnRegister.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "MovieScene.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneSequence.h"
#include "AssetData.h"
#include "Engine/Selection.h"
#include "ActorFactories/ActorFactory.h"
#include "Editor.h"
#include "ISequencer.h"
#include "LevelEditor.h"
#include "AssetSelection.h"
#include "Evaluation/MovieSceneSpawnTemplate.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Sections/MovieScene3DTransformSection.h"

#define LOCTEXT_NAMESPACE "LevelSequenceEditorSpawnRegister"

/* FLevelSequenceEditorSpawnRegister structors
 *****************************************************************************/

FLevelSequenceEditorSpawnRegister::FLevelSequenceEditorSpawnRegister()
{
	bShouldClearSelectionCache = true;

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	OnActorSelectionChangedHandle = LevelEditor.OnActorSelectionChanged().AddRaw(this, &FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged);

	FAreObjectsEditable AreObjectsEditable = FAreObjectsEditable::CreateRaw(this, &FLevelSequenceEditorSpawnRegister::AreObjectsEditable);
	OnAreObjectsEditableHandle = AreObjectsEditable.GetHandle();
	LevelEditor.AddEditableObjectPredicate(AreObjectsEditable);

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnObjectsReplaced);
#endif
}


FLevelSequenceEditorSpawnRegister::~FLevelSequenceEditorSpawnRegister()
{
	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditor->OnActorSelectionChanged().Remove(OnActorSelectionChangedHandle);
		LevelEditor->RemoveEditableObjectPredicate(OnAreObjectsEditableHandle);
	}

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (Sequencer.IsValid())
	{
		Sequencer->OnPreSave().RemoveAll(this);
		Sequencer->OnActivateSequence().RemoveAll(this);
	}

#if WITH_EDITOR
	GEditor->OnObjectsReplaced().RemoveAll(this);
#endif
}


/* FLevelSequenceSpawnRegister interface
 *****************************************************************************/

UObject* FLevelSequenceEditorSpawnRegister::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);

	UObject* NewObject = FLevelSequenceSpawnRegister::SpawnObject(Spawnable, TemplateID, Player);
	
	if (AActor* NewActor = Cast<AActor>(NewObject))
	{
		// Cache the spawned object, and editable state first
		SpawnedObjects.FindOrAdd(TemplateID).Add(NewActor);

		// Select the actor if we think it should be selected
		if (SelectedSpawnedObjects.Contains(FMovieSceneSpawnRegisterKey(TemplateID, Spawnable.GetGuid())))
		{
			GEditor->SelectActor(NewActor, true /*bSelected*/, true /*bNotify*/);
		}
	}

	return NewObject;
}


void FLevelSequenceEditorSpawnRegister::PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID)
{
	TGuardValue<bool> Guard(bShouldClearSelectionCache, false);
	
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();

	// We only save default state for the currently focussed movie scene sequence instance
	if (Sequencer.IsValid() && Sequencer->GetFocusedTemplateID() == TemplateID)
	{
		SaveDefaultSpawnableState(BindingId);
	}

	// Cache its selection state
	AActor* Actor = Cast<AActor>(&Object);
	if (Actor && GEditor->GetSelectedActors()->IsSelected(Actor))
	{
		SelectedSpawnedObjects.Add(FMovieSceneSpawnRegisterKey(TemplateID, BindingId));
		GEditor->SelectActor(Actor, false /*bSelected*/, true /*bNotify*/);
	}
	
	// Remove the spawned object (and anything that's null) from our cache
	TSet<FObjectKey>* ExistingObjects = SpawnedObjects.Find(TemplateID);
	if (ExistingObjects)
	{
		ExistingObjects->Remove(FObjectKey(&Object));
		if (ExistingObjects->Num() == 0)
		{
			SpawnedObjects.Remove(TemplateID);
		}
	}

	FLevelSequenceSpawnRegister::PreDestroyObject(Object, BindingId, TemplateID);
}

void FLevelSequenceEditorSpawnRegister::SaveDefaultSpawnableState(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	const FMovieSceneEvaluationTemplateInstance* Template = Player.GetEvaluationTemplate().GetInstance(TemplateID);
	UMovieSceneSequence* Sequence = Template ? Template->Sequence.Get() : nullptr;

	UObject* Object = FindSpawnedObject(Spawnable.GetGuid(), TemplateID);
	if (!Object || !Sequence)
	{
		return;
	}

	auto RestorePredicate = [](FMovieSceneAnimTypeID TypeID){ return TypeID != FMovieSceneSpawnSectionTemplate::GetAnimTypeID(); };

	if (AActor* Actor = Cast<AActor>(Object))
	{
		// Restore state on any components
		for (UActorComponent* Component : TInlineComponentArray<UActorComponent*>(Actor))
		{
			if (Component)
			{
				Player.RestorePreAnimatedState(*Component, RestorePredicate);
			}
		}
	}

	// Restore state on the object itself
	Player.RestorePreAnimatedState(*Object, RestorePredicate);

	// Copy the template
	Spawnable.CopyObjectTemplate(*Object, *Sequence);
}

void FLevelSequenceEditorSpawnRegister::SaveDefaultSpawnableState(const FGuid& BindingId)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	FMovieSceneSequenceID TemplateID = Sequencer->GetFocusedTemplateID();

	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(BindingId);

	if (Spawnable)
	{
		SaveDefaultSpawnableState(*Spawnable, TemplateID, *Sequencer);
	}
}

void FLevelSequenceEditorSpawnRegister::OnPreSaveMovieScene(ISequencer& InSequencer)
{
	// We're about to save the movie scene(s), so we need to save default spawnable state for the currently focused movie scene sequence instance

	UMovieSceneSequence* Sequence = InSequencer.GetFocusedMovieSceneSequence();
	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return;
	}

	for (int32 SpawnableIndex = 0; SpawnableIndex < MovieScene->GetSpawnableCount(); ++SpawnableIndex)
	{
		FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(SpawnableIndex);
		SaveDefaultSpawnableState(Spawnable.GetGuid());
	}
}

void FLevelSequenceEditorSpawnRegister::OnSequenceInstanceActivated(FMovieSceneSequenceIDRef InTemplateID)
{
	ActiveSequence = InTemplateID;
}

/* FLevelSequenceEditorSpawnRegister implementation
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::SetSequencer(const TSharedPtr<ISequencer>& Sequencer)
{
	WeakSequencer = Sequencer;
	Sequencer->OnPreSave().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnPreSaveMovieScene);
	Sequencer->OnActivateSequence().AddRaw(this, &FLevelSequenceEditorSpawnRegister::OnSequenceInstanceActivated);

	ActiveSequence = Sequencer->GetFocusedTemplateID();
}


/* FLevelSequenceEditorSpawnRegister callbacks
 *****************************************************************************/

void FLevelSequenceEditorSpawnRegister::HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	if (bShouldClearSelectionCache)
	{
		// @todo: arodham: how does this merge? SpawnedObject.Key relates to the old unique sequence instance ID, not the object binding ID. Not sure what's intended here. Source Ref: CL#3117184
		// TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		// if (Sequencer.IsValid())
		// {
		// 	for (auto SpawnedObject : SpawnedObjects)
		// 	{
		// 		FMovieSceneSpawnRegisterKey SpawnRegisterKey(SpawnedObject.Key, Sequencer->GetFocusedMovieSceneSequenceInstance().Get());
		// 		if (SelectedSpawnedObjects.Contains(SpawnRegisterKey))
		// 		{
		// 			SelectedSpawnedObjects.Remove(SpawnRegisterKey);
		// 			break;
		// 		}
		// 	}
		// }

		SelectedSpawnedObjects.Reset();
	}
}

bool FLevelSequenceEditorSpawnRegister::AreObjectsEditable(const TArray<TWeakObjectPtr<UObject>>& InObjects) const
{
	// Check that none of the objects specified are (or belong to) a spawned actor
	for (const TWeakObjectPtr<UObject>& WeakObject : InObjects)
	{
		UObject* Object = WeakObject.Get();
		if (!Object)
		{
			continue;
		}

		AActor* SourceActor = Cast<AActor>(Object);
		if (!SourceActor)
		{
			UActorComponent* ActorComponent = Cast<UActorComponent>(Object);
			if (ActorComponent)
			{
				SourceActor = ActorComponent->GetOwner();
			}
		}

		if (!SourceActor)
		{
			continue;
		}

		FObjectKey ThisObject(SourceActor);
		for (auto& Pair : SpawnedObjects)
		{
			if (Pair.Key != ActiveSequence && Pair.Value.Contains(ThisObject))
			{
				return false;
			}
		}
	}
	return true;
}

void FLevelSequenceEditorSpawnRegister::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer.IsValid())
	{
		return;
	}

	for (auto& Pair : Register)
	{
		TWeakObjectPtr<>& WeakObject = Pair.Value.Object;
		UObject* SpawnedObject = WeakObject.Get();
		if (UObject* NewObject = OldToNewInstanceMap.FindRef(SpawnedObject))
		{
			// Reassign the object
			WeakObject = NewObject;

			// It's a spawnable, so ensure it's transient
			NewObject->SetFlags(RF_Transient);

			// Invalidate the binding - it will be resolved if it's ever asked for again
			Sequencer->State.Invalidate(Pair.Key.BindingId, Pair.Key.TemplateID);
		}
	}
}

#if WITH_EDITOR

TValueOrError<FNewSpawnable, FText> FLevelSequenceEditorSpawnRegister::CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene)
{
	for (TSharedPtr<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		TValueOrError<FNewSpawnable, FText> Result = MovieSceneObjectSpawner->CreateNewSpawnableType(SourceObject, OwnerMovieScene);
		if (Result.IsValid())
		{
			return Result;
		}
	}

	return MakeError(LOCTEXT("NoSpawnerFound", "No spawner found to create new spawnable type"));
}

void FLevelSequenceEditorSpawnRegister::SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const FTransformData& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings)
{
	for (TSharedPtr<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		if (MovieSceneObjectSpawner->CanSetupDefaultsForSpawnable(SpawnedObject))
		{
			MovieSceneObjectSpawner->SetupDefaultsForSpawnable(SpawnedObject, Guid, TransformData, Sequencer, Settings);
			return;
		}
	}
}

void FLevelSequenceEditorSpawnRegister::HandleConvertPossessableToSpawnable(UObject* OldObject, IMovieScenePlayer& Player, FTransformData& OutTransformData)
{
	// @TODO: this could probably be handed off to a spawner if we need anything else to be convertible between spawnable/posessable

	AActor* OldActor = Cast<AActor>(OldObject);
	if (OldActor)
	{
		OutTransformData.Translation = OldActor->GetActorLocation();
		OutTransformData.Rotation = OldActor->GetActorRotation();
		OutTransformData.Scale = OldActor->GetActorScale();
		OutTransformData.bValid = true;

		GEditor->SelectActor(OldActor, false, true);
		UWorld* World = Cast<UWorld>(Player.GetPlaybackContext());
		if (World)
		{
			World->EditorDestroyActor(OldActor, true);
		}
	}
}

bool FLevelSequenceEditorSpawnRegister::CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const
{
	for (TSharedPtr<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		if (Spawnable.GetObjectTemplate()->IsA(MovieSceneObjectSpawner->GetSupportedTemplateType()))
		{
			return MovieSceneObjectSpawner->CanConvertSpawnableToPossessable(Spawnable);
		}
	}

	return false;
}

#endif


#undef LOCTEXT_NAMESPACE
