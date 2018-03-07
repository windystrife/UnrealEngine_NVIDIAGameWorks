// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace ViewportInteractionUtils
{
	/************************************************************************/
	/* 1 Euro filter smoothing algorithm									*/
	/* http://cristal.univ-lille.fr/~casiez/1euro/							*/
	/************************************************************************/
	class VIEWPORTINTERACTION_API FOneEuroFilter
	{
	private:

		class FLowpassFilter
		{
		public:

			/** Default constructor */
			FLowpassFilter();

			/** Calculate */
			FVector Filter(const FVector& InValue, const FVector& InAlpha);

			/** If the filter was not executed yet */
			bool IsFirstTime() const;

			/** Get the previous filtered value */
			FVector GetPrevious() const;

		private:

			/** The previous filtered value */
			FVector Previous;

			/** If this is the first time doing a filter */
			bool bFirstTime;
		};

	public:

		/** Default constructor */
		FOneEuroFilter();

		FOneEuroFilter(const double InMinCutoff, const double InCutoffSlope, const double InDeltaCutoff);

		/** Smooth vector */
		FVector Filter(const FVector& InRaw, const double InDeltaTime);

		/** Set the minimum cutoff */
		void SetMinCutoff(const double InMinCutoff);

		/** Set the cutoff slope */
		void SetCutoffSlope(const double InCutoffSlope);

		/** Set the delta slope */
		void SetDeltaCutoff(const double InDeltaCutoff);

	private:

		const FVector CalculateCutoff(const FVector& InValue);
		const FVector CalculateAlpha(const FVector& InCutoff, const double InDeltaTime) const;
		const float CalculateAlpha(const float InCutoff, const double InDeltaTime) const;

		double MinCutoff;
		double CutoffSlope;
		double DeltaCutoff;
		FLowpassFilter RawFilter;
		FLowpassFilter DeltaFilter;
	};
}