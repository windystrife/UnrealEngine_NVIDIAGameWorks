// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Evaluation/MovieSceneParameterTemplate.h"
#include "MovieSceneWidgetMaterialTemplate.generated.h"

class UMovieSceneWidgetMaterialTrack;

USTRUCT()
struct FMovieSceneWidgetMaterialSectionTemplate : public FMovieSceneParameterSectionTemplate
{
	GENERATED_BODY()

	FMovieSceneWidgetMaterialSectionTemplate() {}
	FMovieSceneWidgetMaterialSectionTemplate(const UMovieSceneParameterSection& Section, const UMovieSceneWidgetMaterialTrack& Track);

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;

	UPROPERTY()
	TArray<FName> BrushPropertyNamePath;
};
