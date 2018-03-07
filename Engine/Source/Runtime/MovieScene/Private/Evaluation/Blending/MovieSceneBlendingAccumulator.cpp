// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBlendingAccumulator.h"
#include "MovieSceneTemplateInterrogation.h"

FMovieSceneAnimTypeID GetInitialValueTypeID()
{
	static FMovieSceneAnimTypeID ID = FMovieSceneAnimTypeID::Unique();
	return ID;
}

void FMovieSceneBlendingAccumulator::Apply(const FMovieSceneContext& Context, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player)
{
	TMap<UObject*, TMap<FMovieSceneBlendingActuatorID, FActuatorTokenStackPtr>> BlendState;

	UnboundBlendState.Consolidate(BlendState, FMovieSceneEvaluationOperand(), Player);
	for (auto& Pair : OperandToBlendState)
	{
		Pair.Value.Consolidate(BlendState, Pair.Key, Player);
	}

	// Evaluate the token stacks
	for (auto& Pair : BlendState)
	{
		UObject* Object = Pair.Key;
		
		for (auto& InnerPair : Pair.Value)
		{
			InnerPair.Value->ComputeAndActuate(Object, *this, InnerPair.Key, Context, PersistentData, Player);
		}
	}

	UnboundBlendState.Reset();
	for (auto& Pair : OperandToBlendState)
	{
		Pair.Value.Reset();
	}
}

void FMovieSceneBlendingAccumulator::Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& InterrogationData, UObject* AnimatedObject)
{
	//@todo: interrogation currently does not work on entire sequences, so we just accumulate into a single map.
	TMap<FMovieSceneBlendingActuatorID, FActuatorTokenStackPtr> BlendState;

	UnboundBlendState.Consolidate(BlendState);
	for (auto& Pair : OperandToBlendState)
	{
		Pair.Value.Consolidate(BlendState);
	}

	// Evaluate the token stacks
	for (auto& Pair : BlendState)
	{
		Pair.Value->Interrogate(AnimatedObject, InterrogationData, *this, Pair.Key, Context);
	}

	UnboundBlendState.Reset();
	for (auto& Pair : OperandToBlendState)
	{
		Pair.Value.Reset();
	}
}