// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneTrackImplementation.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneFadeSection.h"

#include "MovieSceneFadeTemplate.generated.h"

USTRUCT()
struct FMovieSceneFadeSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneFadeSectionTemplate() {}
	FMovieSceneFadeSectionTemplate(const UMovieSceneFadeSection& Section);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FRichCurve FadeCurve;
	
	UPROPERTY()
	FLinearColor FadeColor;

	UPROPERTY()
	uint32 bFadeAudio:1;
};
