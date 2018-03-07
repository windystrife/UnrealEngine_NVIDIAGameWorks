// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/PersistentEvaluationData.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneLevelVisibilitySection.h"
#include "MovieSceneLevelVisibilityTemplate.generated.h"

USTRUCT()
struct FMovieSceneLevelVisibilitySectionTemplate
	: public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneLevelVisibilitySectionTemplate(){}
	FMovieSceneLevelVisibilitySectionTemplate(const UMovieSceneLevelVisibilitySection& Section);

	static FSharedPersistentDataKey GetSharedDataKey();

private:

	virtual UScriptStruct& GetScriptStructImpl() const override
	{
		return *StaticStruct();
	}
	virtual void SetupOverrides() override
	{
		EnableOverrides(RequiresSetupFlag | RequiresTearDownFlag);
	}

	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;
	virtual void TearDown(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const override;

	UPROPERTY()
	ELevelVisibility Visibility;

	UPROPERTY()
	TArray<FName> LevelNames;
};
