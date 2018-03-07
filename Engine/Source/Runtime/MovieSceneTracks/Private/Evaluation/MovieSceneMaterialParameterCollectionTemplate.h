// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneParameterTemplate.h"
#include "MovieSceneMaterialParameterCollectionTemplate.generated.h"

class UMovieSceneMaterialParameterCollection;
class UMovieSceneMaterialParameterCollectionTrack;

USTRUCT()
struct FMovieSceneMaterialParameterCollectionTemplate : public FMovieSceneParameterSectionTemplate
{
	GENERATED_BODY()

	FMovieSceneMaterialParameterCollectionTemplate() {}
	FMovieSceneMaterialParameterCollectionTemplate(const UMovieSceneParameterSection& Section, const UMovieSceneMaterialParameterCollectionTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	UMaterialParameterCollection* MPC;
};
