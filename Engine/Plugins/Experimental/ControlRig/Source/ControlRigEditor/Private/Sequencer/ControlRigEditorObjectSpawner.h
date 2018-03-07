// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ControlRigObjectSpawner.h"

class FControlRigEditorObjectSpawner : public FControlRigObjectSpawner
{
public:

	static TSharedRef<IMovieSceneObjectSpawner> CreateObjectSpawner();

	// IMovieSceneObjectSpawner interface
	virtual bool IsEditor() const override { return true; }
	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
#if WITH_EDITOR
	virtual TValueOrError<FNewSpawnable, FText> CreateNewSpawnableType(UObject& SourceObject, UMovieScene& OwnerMovieScene) override;
	virtual void SetupDefaultsForSpawnable(UObject* SpawnedObject, const FGuid& Guid, const FTransformData& TransformData, TSharedRef<ISequencer> Sequencer, USequencerSettings* Settings) override;
	virtual bool CanConvertSpawnableToPossessable(FMovieSceneSpawnable& Spawnable) const override { return false; }
#endif
};