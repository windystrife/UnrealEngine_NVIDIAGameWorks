// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/IntegralCurve.h"
#include "Evaluation/MovieSceneAnimTypeID.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieSceneSpawnTemplate.generated.h"

class UMovieSceneSpawnSection;

/** Spawn track eval template that evaluates a curve */
USTRUCT()
struct MOVIESCENETRACKS_API FMovieSceneSpawnSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneSpawnSectionTemplate() {}
	FMovieSceneSpawnSectionTemplate(const UMovieSceneSpawnSection& SpawnSection);

	static FMovieSceneAnimTypeID GetAnimTypeID();
	
private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

protected:
	UPROPERTY()
	FIntegralCurve Curve;
};
