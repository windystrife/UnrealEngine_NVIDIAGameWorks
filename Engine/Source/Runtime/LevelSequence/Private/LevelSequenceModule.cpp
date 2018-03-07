// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceModule.h"
#include "ModuleManager.h"
#include "LevelSequenceActorSpawner.h"

void FLevelSequenceModule::StartupModule()
{
	OnCreateMovieSceneObjectSpawnerDelegateHandle = RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FLevelSequenceActorSpawner::CreateObjectSpawner));
}

void FLevelSequenceModule::ShutdownModule()
{
	UnregisterObjectSpawner(OnCreateMovieSceneObjectSpawnerDelegateHandle);
}

FDelegateHandle FLevelSequenceModule::RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner InOnCreateMovieSceneObjectSpawner)
{
	OnCreateMovieSceneObjectSpawnerDelegates.Add(InOnCreateMovieSceneObjectSpawner);
	return OnCreateMovieSceneObjectSpawnerDelegates.Last().GetHandle();
}

void FLevelSequenceModule::UnregisterObjectSpawner(FDelegateHandle InHandle)
{
	OnCreateMovieSceneObjectSpawnerDelegates.RemoveAll([=](const FOnCreateMovieSceneObjectSpawner& Delegate) { return Delegate.GetHandle() == InHandle; });
}

IMPLEMENT_MODULE(FLevelSequenceModule, LevelSequence);
