// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "MovieScene3DTransformSection.h"
#include "Blending/BlendableToken.h"
#include "Evaluation/Blending/MovieSceneMultiChannelBlending.h"
#include "MovieScene3DTransformTemplate.generated.h"

class UMovieScene3DTransformSection;

USTRUCT()
struct FMovieScene3DTransformTemplateData
{
	GENERATED_BODY()

	FMovieScene3DTransformTemplateData(){}
	FMovieScene3DTransformTemplateData(const UMovieScene3DTransformSection& Section);

	MovieScene::TMultiChannelValue<float, 9> Evaluate(float InTime) const;

	UPROPERTY()
	FRichCurve TranslationCurve[3];

	UPROPERTY()
	FRichCurve RotationCurve[3];

	UPROPERTY()
	FRichCurve ScaleCurve[3];

	UPROPERTY()
	FRichCurve ManualWeight;

	UPROPERTY() 
	EMovieSceneBlendType BlendType;

	UPROPERTY()
	FMovieSceneTransformMask Mask;
};

USTRUCT()
struct FMovieSceneComponentTransformSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	FMovieScene3DTransformTemplateData TemplateData;

	FMovieSceneComponentTransformSectionTemplate(){}
	FMovieSceneComponentTransformSectionTemplate(const UMovieScene3DTransformSection& Section);

protected:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
	virtual void Interrogate(const FMovieSceneContext& Context, FMovieSceneInterrogationData& Container, UObject* BindingOverride) const override;
};
