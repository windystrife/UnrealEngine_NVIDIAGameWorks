// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Evaluation/MovieScenePropertyTemplate.h"

#include "MovieSceneColorTemplate.generated.h"

class UMovieSceneColorSection;
class UMovieSceneColorTrack;

USTRUCT()
struct FMovieSceneColorSectionTemplate : public FMovieScenePropertySectionTemplate
{
	GENERATED_BODY()
	
	FMovieSceneColorSectionTemplate() {}
	FMovieSceneColorSectionTemplate(const UMovieSceneColorSection& Section, const UMovieSceneColorTrack& Track);

	/** Curve data as RGBA */
	UPROPERTY()
	FRichCurve Curves[4];

	UPROPERTY()
	EMovieSceneBlendType BlendType;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
