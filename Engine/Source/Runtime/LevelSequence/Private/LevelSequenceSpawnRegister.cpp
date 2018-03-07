// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceSpawnRegister.h"
#include "Engine/EngineTypes.h"
#include "MovieScene.h"
#include "MovieSceneSequence.h"
#include "LevelSequenceModule.h"
#include "IMovieSceneObjectSpawner.h"
#include "ModuleManager.h"

FLevelSequenceSpawnRegister::FLevelSequenceSpawnRegister()
{
	FLevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<FLevelSequenceModule>("LevelSequence");
	
	for (const FOnCreateMovieSceneObjectSpawner& OnCreateMovieSceneObjectSpawner : LevelSequenceModule.OnCreateMovieSceneObjectSpawnerDelegates)
	{
		check(OnCreateMovieSceneObjectSpawner.IsBound());
		MovieSceneObjectSpawners.Add(OnCreateMovieSceneObjectSpawner.Execute());
	}

	// Now sort the spawners. Editor spawners should come first so they override runtime versions of the same supported type in-editor.
	// @TODO: we could also sort by most-derived type here to allow for type specific behaviors
	MovieSceneObjectSpawners.Sort([](TSharedRef<IMovieSceneObjectSpawner> LHS, TSharedRef<IMovieSceneObjectSpawner> RHS)
	{
		return LHS->IsEditor() > RHS->IsEditor();
	});
}

UObject* FLevelSequenceSpawnRegister::SpawnObject(FMovieSceneSpawnable& Spawnable, FMovieSceneSequenceIDRef TemplateID, IMovieScenePlayer& Player)
{
	for (TSharedRef<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		if (Spawnable.GetObjectTemplate()->IsA(MovieSceneObjectSpawner->GetSupportedTemplateType()))
		{
			
			UObject* SpawnedObject = MovieSceneObjectSpawner->SpawnObject(Spawnable, TemplateID, Player);
			if (SpawnedObject)
			{
				return SpawnedObject;
			}
		}
	}

	return nullptr;
}

void FLevelSequenceSpawnRegister::DestroySpawnedObject(UObject& Object)
{
	for (TSharedRef<IMovieSceneObjectSpawner> MovieSceneObjectSpawner : MovieSceneObjectSpawners)
	{
		if (Object.IsA(MovieSceneObjectSpawner->GetSupportedTemplateType()))
		{
			MovieSceneObjectSpawner->DestroySpawnedObject(Object);
			return;
		}
	}

	checkf(false, TEXT("No valid object spawner found to destroy spawned object of type %s"), *Object.GetClass()->GetName());
}
