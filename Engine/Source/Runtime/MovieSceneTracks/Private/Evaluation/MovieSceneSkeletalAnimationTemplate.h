// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#include "MovieSceneSkeletalAnimationTemplate.generated.h"

USTRUCT()
struct FMovieSceneSkeletalAnimationSectionTemplateParameters : public FMovieSceneSkeletalAnimationParams
{
	GENERATED_BODY()

	FMovieSceneSkeletalAnimationSectionTemplateParameters() {}
	FMovieSceneSkeletalAnimationSectionTemplateParameters(const FMovieSceneSkeletalAnimationParams& BaseParams, float InSectionStartTime, float InSectionEndTime)
		: FMovieSceneSkeletalAnimationParams(BaseParams)
		, SectionStartTime(InSectionStartTime)
		, SectionEndTime(InSectionEndTime)
	{}

	float MapTimeToAnimation(float InPosition) const;

	UPROPERTY()
	float SectionStartTime;

	UPROPERTY()
	float SectionEndTime;
};

USTRUCT()
struct FMovieSceneSkeletalAnimationSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieSceneSkeletalAnimationSectionTemplate() {}
	FMovieSceneSkeletalAnimationSectionTemplate(const UMovieSceneSkeletalAnimationSection& Section);

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	FMovieSceneSkeletalAnimationSectionTemplateParameters Params;
};
