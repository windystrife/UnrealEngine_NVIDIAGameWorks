// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAccumulatedBlendState.h"
#include "IMovieScenePlayer.h"

void FMovieSceneAccumulatedBlendState::Consolidate(TMap<UObject*, TMap<FMovieSceneBlendingActuatorID, FActuatorTokenStackPtr>>& InOutBlendState, FMovieSceneEvaluationOperand InOperand, IMovieScenePlayer& Player)
{
	if (InOperand.ObjectBindingID.IsValid())
	{
		for (TWeakObjectPtr<> WeakObj : Player.FindBoundObjects(InOperand))
		{
			if (UObject* Obj = WeakObj.Get())
			{
				Consolidate(InOutBlendState.FindOrAdd(Obj));
			}
		}
	}
	else
	{
		// Explicit nullptr means master tracks
		Consolidate(InOutBlendState.FindOrAdd(nullptr));
	}
}

void FMovieSceneAccumulatedBlendState::Consolidate(TMap<FMovieSceneBlendingActuatorID, FActuatorTokenStackPtr>& InOutBlendState)
{
	for (auto& Token : TokensToBlend)
	{
		Token->Consolidate(InOutBlendState);
	}
}