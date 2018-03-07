// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Curves/RichCurve.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Sections/MovieScene3DPathSection.h"
#include "MovieScene3DPathTemplate.generated.h"

USTRUCT()
struct FMovieScene3DPathSectionTemplate : public FMovieSceneEvalTemplate
{
	GENERATED_BODY()
	
	FMovieScene3DPathSectionTemplate() {}
	FMovieScene3DPathSectionTemplate(const UMovieScene3DPathSection& Section);

	/** GUID of the path we should attach to */
	UPROPERTY()
	FGuid PathGuid;

	/** The timing curve */
	UPROPERTY()
	FRichCurve TimingCurve;

	/** Front Axis */
	UPROPERTY()
	MovieScene3DPathSection_Axis FrontAxisEnum;

	/** Up Axis */
	UPROPERTY()
	MovieScene3DPathSection_Axis UpAxisEnum;

	/** Follow Curve */
	UPROPERTY()
	uint32 bFollow:1;

	/** Reverse Timing */
	UPROPERTY()
	uint32 bReverse:1;

	/** Force Upright */
	UPROPERTY()
	uint32 bForceUpright:1;

private:

	virtual UScriptStruct& GetScriptStructImpl() const override { return *StaticStruct(); }
	virtual void Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const override;
};
