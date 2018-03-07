// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSequenceTransform.generated.h"

/**
 * Movie scene sequence transform class that transforms from one time-space to another.
 *
 * @note The transform can be thought of as the top row of a 2x2 matrix, where the bottom row is the identity:
 * 			| TimeScale	Offset	|
 *			| 0			1		|
 *
 * As such, traditional matrix mathematics can be applied to transform between different sequence's time-spaces.
 * Transforms apply offset first, then time scale.
 */
USTRUCT()
struct FMovieSceneSequenceTransform
{
	GENERATED_BODY()

	/**
	 * Default construction to the identity transform
	 */
	FMovieSceneSequenceTransform()
		: TimeScale(1.f)
		, Offset(0.f)
	{}

	/**
	 * Construction from an offset, and a scale
	 *
	 * @param InOffset 			The offset to translate by
	 * @param InTimeScale 		The timescale. For instance, if a sequence is playing twice as fast, pass 2.f
	 */
	FMovieSceneSequenceTransform(float InOffset, float InTimeScale = 1.f)
		: TimeScale(InTimeScale)
		, Offset(InOffset)
	{}

	/**
	 * Retrieve the inverse of this transform
	 */
	FMovieSceneSequenceTransform Inverse() const
	{
		return FMovieSceneSequenceTransform(-Offset/TimeScale, 1.f/TimeScale);
	}

	/** The sequence's time scale (or play rate) */
	UPROPERTY()
	float TimeScale;

	/** Scalar time offset applied before the scale */
	UPROPERTY()
	float Offset;
};

/**
 * Transform a time by a sequence transform
 *
 * @param InTime 			The time to transform
 * @param RHS 				The transform
 */
inline float operator*(float InTime, const FMovieSceneSequenceTransform& RHS)
{
	return RHS.Offset + InTime * RHS.TimeScale;
}

/**
 * Transform a time by a sequence transform
 *
 * @param InTime 			The time to transform
 * @param RHS 				The transform
 */
inline float& operator*=(float& InTime, const FMovieSceneSequenceTransform& RHS)
{
	InTime = InTime * RHS;
	return InTime;
}

/**
 * Transform a time range by a sequence transform
 *
 * @param LHS 				The time range to transform
 * @param RHS 				The transform
 */
template<typename T>
TRange<T> operator*(const TRange<T>& LHS, const FMovieSceneSequenceTransform& RHS)
{
	TRangeBound<float> SourceLower = LHS.GetLowerBound();
	TRangeBound<float> TransformedLower =
		SourceLower.IsOpen() ? 
			TRangeBound<float>() : 
			SourceLower.IsInclusive() ?
				TRangeBound<float>::Inclusive(SourceLower.GetValue() * RHS) :
				TRangeBound<float>::Exclusive(SourceLower.GetValue() * RHS);

	TRangeBound<float> SourceUpper = LHS.GetUpperBound();
	TRangeBound<float> TransformedUpper =
		SourceUpper.IsOpen() ? 
			TRangeBound<float>() : 
			SourceUpper.IsInclusive() ?
				TRangeBound<float>::Inclusive(SourceUpper.GetValue() * RHS) :
				TRangeBound<float>::Exclusive(SourceUpper.GetValue() * RHS);

	return TRange<float>(TransformedLower, TransformedUpper);
}

/**
 * Transform a time range by a sequence transform
 *
 * @param InTime 			The time range to transform
 * @param RHS 				The transform
 */
template<typename T>
TRange<T>& operator*=(TRange<T>& LHS, const FMovieSceneSequenceTransform& RHS)
{
	LHS = LHS * RHS;
	return LHS;
}

/**
 * Multiply 2 transforms together, resulting in a single transform that gets from RHS parent to LHS space
 * @note Transforms apply from right to left
 */
inline FMovieSceneSequenceTransform operator*(const FMovieSceneSequenceTransform& LHS, const FMovieSceneSequenceTransform& RHS)
{
	// The matrix multiplication occurs as follows:
	//
	// | TimeScaleA	, OffsetA	|	.	| TimeScaleB, OffsetB	|
	// | 0			, 1			|		| 0			, 1			|

	return FMovieSceneSequenceTransform(
		RHS.Offset*LHS.TimeScale + LHS.Offset,		// New Offset
		LHS.TimeScale * RHS.TimeScale				// New TimeScale
		);
}
