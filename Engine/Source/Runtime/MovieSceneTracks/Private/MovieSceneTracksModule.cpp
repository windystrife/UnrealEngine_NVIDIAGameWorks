// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IMovieSceneModule.h"
#include "IMovieSceneTracksModule.h"


/**
 * Implements the MovieSceneTracks module.
 */
class FMovieSceneTracksModule
	: public IMovieSceneTracksModule
{
	virtual void StartupModule() override
	{
		IMovieSceneModule& MovieSceneModule = IMovieSceneModule::Get();
		
		MovieSceneModule.RegisterEvaluationGroupParameters(
			IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::PreEvaluation),
			FMovieSceneEvaluationGroupParameters(0x8FFF));

		MovieSceneModule.RegisterEvaluationGroupParameters(
			IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::SpawnObjects),
			FMovieSceneEvaluationGroupParameters(0x0FFF));

		MovieSceneModule.RegisterEvaluationGroupParameters(
			IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::PostEvaluation),
			FMovieSceneEvaluationGroupParameters(0x0008));
	}
};

FName IMovieSceneTracksModule::GetEvaluationGroupName(EBuiltInEvaluationGroup::Type InEvalGroup)
{
	static FName Names[] = {
		"PreEvaluation",
		"SpawnObjects",
		"PostEvaluation",
	};
	check(InEvalGroup >= 0 && InEvalGroup < ARRAY_COUNT(Names));
	return Names[InEvalGroup];
}

IMPLEMENT_MODULE(FMovieSceneTracksModule, MovieSceneTracks);
