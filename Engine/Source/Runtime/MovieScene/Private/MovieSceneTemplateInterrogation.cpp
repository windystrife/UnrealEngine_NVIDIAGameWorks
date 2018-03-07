// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTemplateInterrogation.h"
#include "MovieSceneBlendingAccumulator.h"

void FMovieSceneInterrogationData::Finalize(const FMovieSceneContext& Context, UObject* BindingOverride)
{
	if (Accumulator.IsValid())
	{
		Accumulator->Interrogate(Context, *this, BindingOverride);
	}
}

FMovieSceneBlendingAccumulator& FMovieSceneInterrogationData::GetAccumulator()
{
	if (!Accumulator.IsValid())
	{
		Accumulator = MakeShared<FMovieSceneBlendingAccumulator>();
	}

	return *Accumulator;
}