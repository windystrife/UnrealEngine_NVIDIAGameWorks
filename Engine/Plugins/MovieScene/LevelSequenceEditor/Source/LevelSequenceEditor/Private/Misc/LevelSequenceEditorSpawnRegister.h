// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/ValueOrError.h"
#include "MovieSceneSequenceID.h"
#include "MovieSceneSpawnRegister.h"
#include "LevelSequenceSpawnRegister.h"
#include "UObject/ObjectKey.h"

class IMovieScenePlayer;
class ISequencer;
class UMovieScene;

/**
 * Spawn register used in the editor to add some usability features like maintaining selection states, and projecting spawned state onto spawnable defaults
 */
class FLevelSequenceEditorSpawnRegister
	: public FLevelSequenceSpawnRegister
{
public:

	/** Constructor */
	FLevelSequenceEditorSpawnRegister();

	/** Destructor. */
	~FLevelSequenceEditorSpawnRegister();

public:

	void SetSequencer(const TSharedPtr<ISequencer>& Sequencer);

public:

	// FLevelSequenceSpawnRegister interface

	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
	virtual void PreDestroyObject(UObject& Object, const FGuid& BindingId, FMovieSceneSequenceIDRef TemplateID) override;
	virtual void SaveDefaultSpawnableState(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
#if WITH_EDITOR
	virtual TValueOrError<FNewSpawnable, FText> CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene) override;
	virtual void SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const FTransformData& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings) override;
	virtual void HandleConvertPossessableToSpawnable(UObject* OldObject, IMovieScenePlayer& Player, FTransformData& OutTransformData) override;
	virtual bool CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const override;
#endif

private:

	/** Called when the editor selection has changed. */
	void HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

	/** Called before sequencer attempts to save the movie scene(s) it's editing */
	void OnPreSaveMovieScene(ISequencer& InSequencer);

	/** Called when a new movie scene sequence instance has been activated */
	void OnSequenceInstanceActivated(FMovieSceneSequenceIDRef ActiveInstance);

	/** Saves the default state for the specified spawnable, if an instance for it currently exists */
	void SaveDefaultSpawnableState(const FGuid& BindingId);

	/** Check whether the specified objects are editable on the details panel. Called from the level editor */
	bool AreObjectsEditable(const TArray<TWeakObjectPtr<UObject>>& InObjects) const;
	
	/** Called from the editor when a blueprint object replacement has occurred */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

private:

	/** Handles for delegates that we've bound to. */
	FDelegateHandle OnActorSelectionChangedHandle, OnAreObjectsEditableHandle;

	/** Set of spawn register keys for objects that should be selected if they are spawned. */
	TSet<FMovieSceneSpawnRegisterKey> SelectedSpawnedObjects;

	/** Set of currently spawned objects */
	TMap<FMovieSceneSequenceID, TSet<FObjectKey>> SpawnedObjects;

	/** True if we should clear the above selection cache when the editor selection has been changed. */
	bool bShouldClearSelectionCache;

	/** Weak pointer to the active sequencer. */
	TWeakPtr<ISequencer> WeakSequencer;

	/** Identifier for the current active level sequence */
	FMovieSceneSequenceID ActiveSequence;
};
