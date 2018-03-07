// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/MovieSceneMarginTemplate.h"

#include "Animation/MovieSceneMarginSection.h"
#include "Tracks/MovieScenePropertyTrack.h"
#include "MovieSceneEvaluation.h"

template<> FMovieSceneAnimTypeID GetBlendingDataType<FMargin>()
{
	static FMovieSceneAnimTypeID TypeId = FMovieSceneAnimTypeID::Unique();
	return TypeId;
}

FMovieSceneMarginSectionTemplate::FMovieSceneMarginSectionTemplate(const UMovieSceneMarginSection& Section, const UMovieScenePropertyTrack& Track)
	: FMovieScenePropertySectionTemplate(Track.GetPropertyName(), Track.GetPropertyPath())
	, TopCurve(Section.GetTopCurve())
	, LeftCurve(Section.GetLeftCurve())
	, RightCurve(Section.GetRightCurve())
	, BottomCurve(Section.GetBottomCurve())
	, BlendType(Section.GetBlendType().Get())
{
}

void FMovieSceneMarginSectionTemplate::Evaluate(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const float Time = Context.GetTime();
	MovieScene::TMultiChannelValue<float, 4> AnimatedData;

	if (LeftCurve.HasAnyData())
	{
		AnimatedData.Set(0, LeftCurve.Eval(Time));
	}

	if (TopCurve.HasAnyData())
	{
		AnimatedData.Set(1, TopCurve.Eval(Time));
	}

	if (RightCurve.HasAnyData())
	{
		AnimatedData.Set(2, RightCurve.Eval(Time));
	}

	if (BottomCurve.HasAnyData())
	{
		AnimatedData.Set(3, BottomCurve.Eval(Time));
	}

	if (!AnimatedData.IsEmpty())
	{
		FMovieSceneBlendingActuatorID ActuatorTypeID = EnsureActuator<FMargin>(ExecutionTokens.GetBlendingAccumulator());

		// Add the blendable to the accumulator
		float Weight = EvaluateEasing(Context.GetTime());
		ExecutionTokens.BlendToken(ActuatorTypeID, TBlendableToken<FMargin>(AnimatedData, BlendType, Weight));
	}
}
