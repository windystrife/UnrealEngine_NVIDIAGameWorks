// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IMovieSceneModule.h"
#include "MovieScene.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"


DEFINE_LOG_CATEGORY(LogMovieScene);


/**
 * MovieScene module implementation.
 */
class FMovieSceneModule
	: public IMovieSceneModule
{
public:

	// IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual void RegisterEvaluationGroupParameters(FName GroupName, const FMovieSceneEvaluationGroupParameters& GroupParameters) override
	{
		check(!GroupName.IsNone() && GroupParameters.EvaluationPriority != 0);

		for (auto& Pair : EvaluationGroupParameters)
		{
			checkf(Pair.Key != GroupName, TEXT("Cannot add 2 groups of the same name"));
			checkf(Pair.Value.EvaluationPriority != GroupParameters.EvaluationPriority, TEXT("Cannot add 2 groups of the same priority"));
		}

		EvaluationGroupParameters.Add(GroupName, GroupParameters);
	}

	virtual FMovieSceneEvaluationGroupParameters GetEvaluationGroupParameters(FName GroupName) const override
	{
		return EvaluationGroupParameters.FindRef(GroupName);
	}

private:
	TMap<FName, FMovieSceneEvaluationGroupParameters> EvaluationGroupParameters;
};


IMPLEMENT_MODULE(FMovieSceneModule, MovieScene);
