// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePlayback.h"

#include "MovieScene.h"

namespace 
{
	TRange<float> CalculateEvaluationRange(float CurrentTime, float PreviousTime, bool bInclusivePreviousTime)
	{
		if (CurrentTime == PreviousTime)
		{
			return TRange<float>(CurrentTime);
		}
		else if (CurrentTime < PreviousTime)
		{
			return TRange<float>(
				TRangeBound<float>::Inclusive(CurrentTime),
				bInclusivePreviousTime
					? TRangeBound<float>::Inclusive(PreviousTime)
					: TRangeBound<float>::Exclusive(PreviousTime)
				);
		}

		return TRange<float>(
			bInclusivePreviousTime
				? TRangeBound<float>::Inclusive(PreviousTime)
				: TRangeBound<float>::Exclusive(PreviousTime)
			, TRangeBound<float>::Inclusive(CurrentTime)
			);
	}
}

FMovieSceneEvaluationRange::FMovieSceneEvaluationRange(float InTime)
	: EvaluationRange(InTime)
	, Direction(EPlayDirection::Forwards)
	, TimeOverride(TNumericLimits<float>::Lowest())
{
}

FMovieSceneEvaluationRange::FMovieSceneEvaluationRange(TRange<float> InRange, EPlayDirection InDirection)
	: EvaluationRange(InRange)
	, Direction(InDirection)
	, TimeOverride(TNumericLimits<float>::Lowest())
{
}

FMovieSceneEvaluationRange::FMovieSceneEvaluationRange(float InCurrentTime, float InPreviousTime, bool bInclusivePreviousTime)
	: EvaluationRange(CalculateEvaluationRange(InCurrentTime, InPreviousTime, bInclusivePreviousTime))
	, Direction((InCurrentTime - InPreviousTime >= 0) ? EPlayDirection::Forwards : EPlayDirection::Backwards)
	, TimeOverride(TNumericLimits<float>::Lowest())
{
}

void FMovieScenePlaybackPosition::Reset(float StartPos)
{
	PreviousPosition = StartPos;
	PreviousPlayEvalPosition.Reset();
	LastRange.Reset();
}

FMovieSceneEvaluationRange FMovieScenePlaybackPosition::JumpTo(float NewPosition, TOptional<float> FixedInterval)
{
	Reset(NewPosition);

	PreviousPosition = NewPosition;
	float EvalPosition = FixedInterval.IsSet() ? UMovieScene::CalculateFixedFrameTime(NewPosition, FixedInterval.GetValue()) : NewPosition;

	FMovieSceneEvaluationRange Range(TRange<float>(EvalPosition), EPlayDirection::Forwards);
	LastRange = Range;
	return Range;
}

FMovieSceneEvaluationRange FMovieScenePlaybackPosition::PlayTo(float NewPosition, TOptional<float> FixedInterval)
{
	float EvalPositionTo = FixedInterval.IsSet() ? UMovieScene::CalculateFixedFrameTime(NewPosition, FixedInterval.GetValue()) : NewPosition;
	float EvalPositionFrom = PreviousPlayEvalPosition.Get(PreviousPosition.Get(NewPosition));

	FMovieSceneEvaluationRange Range(EvalPositionTo, EvalPositionFrom, !PreviousPlayEvalPosition.IsSet());

	PreviousPosition = NewPosition;
	PreviousPlayEvalPosition = EvalPositionTo;

	LastRange = Range;

	return Range;
}

TOptional<FMovieSceneEvaluationRange> FMovieScenePlaybackPosition::GetLastRange() const
{
	return LastRange;
}
