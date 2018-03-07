// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMovieSceneObjectSpawner.h"

class LEVELSEQUENCE_API FLevelSequenceActorSpawner : public IMovieSceneObjectSpawner
{
public:

	static TSharedRef<IMovieSceneObjectSpawner> CreateObjectSpawner();

	// IMovieSceneObjectSpawner interface
	virtual UClass* GetSupportedTemplateType() const override;
	virtual UObject* SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player) override;
	virtual void DestroySpawnedObject(UObject& Object) override;
};