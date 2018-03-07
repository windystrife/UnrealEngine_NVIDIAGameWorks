// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneSpawnSection.h"


UMovieSceneSpawnSection::UMovieSceneSpawnSection(const FObjectInitializer& Init)
	: Super(Init)
{
	EvalOptions.EnableAndSetCompletionMode(EMovieSceneCompletionMode::RestoreState);
}
