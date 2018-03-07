// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ILevelSequenceModule.h"

/**
 * Implements the LevelSequence module.
 */
class FLevelSequenceModule : public ILevelSequenceModule
{
public:

	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// ILevelSequenceModule interface
	virtual FDelegateHandle RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner InOnCreateMovieSceneObjectSpawner) override;
	virtual void UnregisterObjectSpawner(FDelegateHandle InHandle) override;

public:
	/** List of object spawner delegates used to extend the spawn register */
	TArray< FOnCreateMovieSceneObjectSpawner > OnCreateMovieSceneObjectSpawnerDelegates;

	/** Internal delegate handle used for spawning actors */
	FDelegateHandle OnCreateMovieSceneObjectSpawnerDelegateHandle;
};
