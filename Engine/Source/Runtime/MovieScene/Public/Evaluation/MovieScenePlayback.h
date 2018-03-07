// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieSceneFwd.h"
#include "Evaluation/MovieSceneSequenceTransform.h"


/** Enumeration specifying whether we're playing forwards or backwards */
enum class EPlayDirection
{
	Forwards, Backwards
};


/** MovieScene evaluation context. Should remain bitwise copyable, and contain no external state since this has the potential to be used on a thread */
struct MOVIESCENE_API FMovieSceneEvaluationRange
{
	/**
	 * Construct this range from a single fixed time
	 */
	FMovieSceneEvaluationRange(float InTime);

	/**
	 * Construct this range from a raw range and a direction
	 */
	FMovieSceneEvaluationRange(TRange<float> InRange, EPlayDirection InDirection);

	/**
	 * Construct this range from 2 times, and whether the range should include the previous time or not
	 */
	FMovieSceneEvaluationRange(float InCurrentTime, float InPreviousTime, bool bInclusivePreviousTime = false);

	/**
	 * Get the range that we should be evaluating
	 */
	FORCEINLINE TRange<float> GetRange() const
	{
		return EvaluationRange;
	}

	/**
	 * Get the direction to evaluate our range
	 */
	FORCEINLINE EPlayDirection GetDirection() const 
	{
		return Direction;
	}

	/**
	 * Get the current time of evaluation.
	 */
	FORCEINLINE float GetTime() const
	{
		if (TimeOverride != TNumericLimits<float>::Lowest())
		{
			return TimeOverride;
		}

		return Direction == EPlayDirection::Forwards ? EvaluationRange.GetUpperBoundValue() : EvaluationRange.GetLowerBoundValue();
	}

	/**
	 * Get the absolute amount of time that has passed since the last update (will always be >= 0)
	 */
	FORCEINLINE float GetDelta() const
	{
		return EvaluationRange.Size<float>();
	}

	/**
	 * Get the previous time of evaluation. Should not generally be used. Prefer GetRange instead.
	 */
	FORCEINLINE float GetPreviousTime() const
	{
		return Direction == EPlayDirection::Forwards ? EvaluationRange.GetLowerBoundValue() : EvaluationRange.GetUpperBoundValue();
	}
	
	/**
	 * Override the time that we're actually evaluating at
	 */
	FORCEINLINE void OverrideTime(float InTimeOverride)
	{
		TimeOverride = InTimeOverride;
	}

protected:

	/** The range to evaluate */
	TRange<float> EvaluationRange;

	/** Whether to evaluate the range forwards, or backwards */
	EPlayDirection Direction;

	/** Overridden current time (doesn't manipulate the actual evaluated range) */
	float TimeOverride;
};

/** MovieScene evaluation context. Should remain bitwise copyable, and contain no external state since this has the potential to be used on a thread */
struct FMovieSceneContext : FMovieSceneEvaluationRange
{
	/**
	 * Construction from an evaluation range, and a current status
	 */
	FMovieSceneContext(FMovieSceneEvaluationRange InRange)
		: FMovieSceneEvaluationRange(InRange)
		, Status(EMovieScenePlayerStatus::Stopped)
		, PrePostRollStartEndTime(TNumericLimits<float>::Lowest())
		, HierarchicalBias(0)
		, bHasJumped(false)
		, bSilent(false)
		, bSectionPreRoll(false)
		, bSectionPostRoll(false)
		, bHasPreRollEndTime(false)
		, bHasPostRollStartTime(false)
	{}

	/**
	 * Construction from an evaluation range, and a current status
	 */
	FMovieSceneContext(FMovieSceneEvaluationRange InRange, EMovieScenePlayerStatus::Type InStatus)
		: FMovieSceneEvaluationRange(InRange)
		, Status(InStatus)
		, PrePostRollStartEndTime(TNumericLimits<float>::Lowest())
		, HierarchicalBias(0)
		, bHasJumped(false)
		, bSilent(false)
		, bSectionPreRoll(false)
		, bSectionPostRoll(false)
		, bHasPreRollEndTime(false)
		, bHasPostRollStartTime(false)
	{}

	/**
	 * Get the playback status
	 */
	FORCEINLINE EMovieScenePlayerStatus::Type GetStatus() const
	{
		return Status;
	}

	/**
	 * Check whether we've just jumped to a different time
	 */
	FORCEINLINE bool HasJumped() const
	{
		return bHasJumped;
	}

	/**
	 * Check whether we're evaluating in silent mode (no audio or mutating eval)
	 */
	FORCEINLINE bool IsSilent() const
	{
		return bSilent;
	}

	/**
	 * Get the current root to sequence transform for the current sub sequence
	 */
	FORCEINLINE const FMovieSceneSequenceTransform& GetRootToSequenceTransform() const
	{
		return RootToSequenceTransform;
	}

	/**
	 * Apply section pre and post roll based on whether we're in the leading (preroll), or trailing (postroll) region for the section, and the current play direction
	 *
	 * @param bInLeadingRegion			Whether we are considered to be in the section's leading (aka preroll) region
	 * @param bInTrailingRegion			Whether we are considered to be in the section's trailing (aka postroll) region
	 */
	FORCEINLINE void ApplySectionPrePostRoll(bool bInLeadingRegion, bool bInTrailingRegion)
	{
		if (Direction == EPlayDirection::Forwards)
		{
			bSectionPreRoll = bInLeadingRegion;
			bSectionPostRoll = bInTrailingRegion;
		}
		else
		{
			bSectionPreRoll = bInTrailingRegion;
			bSectionPostRoll = bInLeadingRegion;
		}
	}

public:

	/**
	 * Indicate that we've just jumped to a different time
	 */
	FMovieSceneContext& SetHasJumped(bool bInHasJumped)
	{
		bHasJumped = bInHasJumped;
		return *this;
	}

	/**
	 * Set the context to silent mode
	 */
	FMovieSceneContext& SetIsSilent(bool bInIsSilent)
	{
		bSilent = bInIsSilent;
		return *this;
	}

	/**
	 * Clamp the current evaluation range to the specified range (in the current transform space)
	 */
	FMovieSceneContext Clamp(TRange<float> NewRange) const
	{
		FMovieSceneContext NewContext = *this;
		NewContext.EvaluationRange = NewRange;
		return NewContext;
	}

	/**
	 * Transform this context to a different sub sequence space
	 */
	FMovieSceneContext Transform(const FMovieSceneSequenceTransform& InTransform) const
	{
		FMovieSceneContext NewContext = *this;
		NewContext.EvaluationRange = EvaluationRange * InTransform;
		NewContext.RootToSequenceTransform = NewContext.RootToSequenceTransform * InTransform;
		return NewContext;
	}

	/**
	 * Get the hierarchical bias for the current context
	 * @param InHierarchicalBias		The current hierarchical bias
	 */
	void SetHierarchicalBias(int32 InHierarchicalBias)
	{
		HierarchicalBias = InHierarchicalBias;
	}

	/**
	 * Get the hierarchical bias for the current context
	 * @return The current hierarchical bias
	 */
	int32 GetHierarchicalBias() const
	{
		return HierarchicalBias;
	}

public:

	/**
	 * Check if we're in any kind of preroll (either prerolling section specifically, or as part of a sub-section)
	 * @note Play direction has already been considered in the calculation of this function, so needs no extra consideration.
	 */
	bool IsPreRoll() const
	{
		return bHasPreRollEndTime || bSectionPreRoll;
	}

	/**
	 * Check if we're in any kind of postroll (either postrolling section specifically, or as part of a sub-section)
	 * @note Play direction has already been considered in the calculation of this function, so needs no extra consideration.
	 */
	bool IsPostRoll() const
	{
		return bHasPostRollStartTime || bSectionPostRoll;
	}

	/**
	 * Check whether we have an externally supplied time at which preroll will end.
	 * @note When not set (and IsPreRoll() is true), preroll ends at either the start or end of section bounds, depending on play direction.
	 */
	bool HasPreRollEndTime() const
	{
		return bHasPreRollEndTime;
	}

	/**
	 * Check whether we have an externally supplied time at which postroll started.
	 * @note When not set (and IsPostRoll() is true), preroll ends at either the start or end of section bounds, depending on play direction.
	 */
	bool HasPostRollStartTime() const
	{
		return bHasPostRollStartTime;
	}

	/**
	 * Access the time at which preroll will stop, and evaluation will commence
	 * @note: Only valid to call when HasPreRollEndTime() is true
	 */
	float GetPreRollEndTime() const
	{
		checkf(bHasPreRollEndTime, TEXT("It's invalid to call GetPreRollEndTime() without first checking HasPreRollEndTime()"));
		return PrePostRollStartEndTime;
	}

	/**
	 * Access the time at which post roll started (or in other terms: when evaluation stopped)
	 * @note: Only valid to call when HasPostRollStartTime() is true
	 */
	float GetPostRollStartTime() const
	{
		checkf(bHasPostRollStartTime, TEXT("It's invalid to call GetPostRollStartTime() without first checking HasPostRollStartTime()"));
		return PrePostRollStartEndTime;
	}

	/**
	 * Report the outer section pre and post roll ranges for the current context
	 *
	 * @param InLeadingRange			The leading (preroll) range in front of the outer section, in the current transformation's time space
	 * @param InTrailingRange			The trailing (postroll) range at the end of the outer section, in the current transformation's time space
	 */
	void ReportOuterSectionRanges(TRange<float> InLeadingRange, TRange<float> InTrailingRange)
	{
		const float Now = GetTime();
		if (InLeadingRange.Contains(Now) && InLeadingRange.HasUpperBound())
		{
			PrePostRollStartEndTime = InLeadingRange.GetUpperBoundValue();

			bHasPreRollEndTime = Direction == EPlayDirection::Forwards;
			bHasPostRollStartTime = !bHasPreRollEndTime;
		}
		else if (InTrailingRange.Contains(Now) && InTrailingRange.HasLowerBound())
		{
			PrePostRollStartEndTime = InTrailingRange.GetLowerBoundValue();

			bHasPreRollEndTime = Direction == EPlayDirection::Backwards;
			bHasPostRollStartTime = !bHasPreRollEndTime;
		}
		else
		{
			bHasPreRollEndTime = bHasPostRollStartTime = false;
			PrePostRollStartEndTime = TNumericLimits<float>::Lowest();
		}
	}

protected:

	/** The transform from the root sequence to the current sequence space */
	FMovieSceneSequenceTransform RootToSequenceTransform;

	/** The current playback status */
	EMovieScenePlayerStatus::Type Status;

	/** When bHasPreRollEndTime or bHasPostRollStartTime is true, this defines either the time at which 'real' evaluation commences, or finished */
	float PrePostRollStartEndTime;

	/** Hierachical bias. Higher bias should take precedence. */
	int32 HierarchicalBias;

protected:

	/** Whether this evaluation frame is happening as part of a large jump */
	bool bHasJumped : 1;

	/** Whether this evaluation should happen silently */
	bool bSilent : 1;

	/** True if we should explicitly preroll the section. Already reconciled with play direction. */
	bool bSectionPreRoll : 1;

	/** True if we should explicitly postroll the section. Already reconciled with play direction. */
	bool bSectionPostRoll : 1;

	/** True if the value of PrePostRollStartEndTime has been set, and refers to the time at which preroll will end. Already reconciled with play direction. */
	bool bHasPreRollEndTime : 1;

	/** True if the value of PrePostRollStartEndTime has been set, and refers to the time at which postroll started. Already reconciled with play direction. */
	bool bHasPostRollStartTime : 1;
};

/** Helper class designed to abstract the complexity of calculating evaluation ranges for previous times and fixed time intervals */
struct MOVIESCENE_API FMovieScenePlaybackPosition
{
	/**
	 * Reset this position to the specified time.
	 * @note Future calls to 'PlayTo' will include this time in its resulting evaluation range
	 */
	void Reset(float StartPos);

	/**
	 * Jump to the specified time.
	 * @note Will reset previous play position. Any subsequent call to 'PlayTo' will include NewPosition.
	 * @return A range encompassing only the specified time.
	 */
	FMovieSceneEvaluationRange JumpTo(float NewPosition, TOptional<float> FixedInterval);

	/**
	 * Play from the previously evaluated play time, to the specified time
	 * @return An evaluation range from the previously evaluated time, to the specified time
	 */
	FMovieSceneEvaluationRange PlayTo(float NewPosition, TOptional<float> FixedInterval);

	/**
	 * Get a range that encompasses the last evaluated range
	 * @return An optional evaluation range
	 */
	TOptional<FMovieSceneEvaluationRange> GetLastRange() const;

	/**
	 * Get the last position that was set
	 */
	TOptional<float> GetPreviousPosition() const { return PreviousPosition; }

	/**
	 * Get the last actual time that was evaluated during playback
	 */
	TOptional<float> GetLastPlayEvalPostition() const { return PreviousPlayEvalPosition; }


private:
	/** The previous *actual* time position set. Never rounded to a fixed interval. */
	TOptional<float> PreviousPosition;

	/** The previous evaluated position when playing, potentially rounded to a frame interval. */
	TOptional<float> PreviousPlayEvalPosition;

	/** The previously evaluated range, if available */
	TOptional<FMovieSceneEvaluationRange> LastRange;
};
