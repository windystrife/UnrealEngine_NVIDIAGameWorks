// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Here are a number of profiling helper functions so we do not have to duplicate a lot of the glue
 * code everywhere.  And we can have consistent naming for all our files.
 *
 */

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Logging/LogMacros.h"
#include "Containers/ArrayView.h"

struct FHistogramBuilder;

DECLARE_LOG_CATEGORY_EXTERN(LogHistograms, Log, All);

 struct FHistogramBuilder;

 /** Fairly generic histogram for values that have natural lower bound and possibly no upper bound, e.g., frame time */
struct CORE_API FHistogram
{
	/** Inits histogram with linear, equally sized bins */
	void InitLinear(double MinTime, double MaxTime, double BinSize);

	/** Inits histogram to mimic our existing hitch buckets */
	void InitHitchTracking();

	/** Inits histogram with the specified bin boundaries, with the final bucket extending to infinity (e.g., passing in 0,5 creates a [0..5) bucket and a [5..infinity) bucket) */
	void InitFromArray(TArrayView<double> Thresholds);

	/** Resets measurements, without resetting the configured bins. */
	void Reset();

	/** Adds an observed measurement. */
	inline void AddMeasurement(double Value)
	{
		AddMeasurement(Value, Value);
	}

	/** Adds an observed measurement (with a different thresholding key than the measurement, e.g., when accumulating time spent in a chart keyed on framerate). */
	void AddMeasurement(double ValueForBinning, double MeasurementValue);

	/** Prints histogram contents to the log. */
	void DumpToLog(const FString& HistogramName);

	/** Populates array commonly used in analytics events, adding two pairs per bin (count and sum). */
	void DumpToAnalytics(const FString& ParamNamePrefix, TArray<TPair<FString, double>>& OutParamArray);

	/** Gets number of bins. */
	inline int32 GetNumBins() const
	{
		return Bins.Num();
	}

	/** Gets lower bound of the bin, i.e. minimum value that goes into it. */
	inline double GetBinLowerBound(int IdxBin) const
	{
		return Bins[IdxBin].MinValue;
	}

	/** Gets upper bound of the bin, i.e. first value that does not go into it. */
	inline double GetBinUpperBound(int IdxBin) const
	{
		return Bins[IdxBin].UpperBound;
	}

	/** Gets number of observations in the bin. */
	inline int32 GetBinObservationsCount(int IdxBin) const
	{
		return Bins[IdxBin].Count;
	}

	/** Gets sum of observations in the bin. */
	inline double GetBinObservationsSum(int IdxBin) const
	{
		return Bins[IdxBin].Sum;
	}

	/** Returns the sum of all counts (the number of recorded measurements) */
	inline int64 GetNumMeasurements() const
	{
		return CountOfAllMeasures;
	}

	/** Returns the sum of all measurements */
	inline double GetSumOfAllMeasures() const
	{
		return SumOfAllMeasures;
	}

	FORCEINLINE FHistogram operator-(const FHistogram& Other) const
	{
		// Logic below expects the bins to be of equal size
		check(GetNumBins() == Other.GetNumBins());

		FHistogram Tmp;
		for (int32 BinIndex = 0, NumBins = Bins.Num(); BinIndex < NumBins; BinIndex++)
		{
			Tmp.Bins.Add(Bins[BinIndex] - Other.Bins[BinIndex]);
		}
		return Tmp;
	}

protected:

	/** Bin */
	struct FBin
	{
		/** MinValue to be stored in the bin, inclusive. */
		double MinValue;

		/** First value NOT to be stored in the bin. */
		double UpperBound;

		/** Sum of all values that were put into this bin. */
		double Sum;

		/** How many elements are in this bin. */
		int32 Count;

		FBin()
		{
		}

		/** Constructor for a pre-seeded bin */
		FBin(double MinInclusive, double MaxExclusive, int32 InSum, int32 InCount)
			: MinValue(MinInclusive)
			, UpperBound(MaxExclusive)
			, Sum(InSum)
			, Count(InCount)
		{
		}

		/** Constructor for any bin */
		FBin(double MinInclusive, double MaxExclusive)
			: MinValue(MinInclusive)
			, UpperBound(MaxExclusive)
			, Sum(0)
			, Count(0)
		{
		}

		/** Constructor for the last bin. */
		FBin(double MinInclusive)
			: MinValue(MinInclusive)
			, UpperBound(FLT_MAX)
			, Sum(0)
			, Count(0)
		{
		}

		FORCEINLINE FBin operator-(const FBin& Other) const
		{
			return FBin(MinValue, UpperBound, Sum - Other.Sum, Count - Other.Count);
		}
	};

	/** Bins themselves, should be continous in terms of [MinValue; UpperBound) and sorted ascending by MinValue. Last bin's UpperBound doesn't matter */
	TArray<FBin> Bins;

	/** Quick stats for all bins */
	double SumOfAllMeasures;
	int64 CountOfAllMeasures;

	/** This is exposed as a clean way to build bins while enforcing the condition mentioned above */
	friend FHistogramBuilder;
};

/** Used to construct a histogram that runs over a custom set of ranges, while still enforcing contiguity on the bin ranges */
struct FHistogramBuilder
{
	FHistogramBuilder(FHistogram& InHistogram, double StartingValue = 0.0)
		: MyHistogram(&InHistogram)
		, LastValue(StartingValue)
	{
		MyHistogram->SumOfAllMeasures = 0.0;
		MyHistogram->CountOfAllMeasures = 0;
		MyHistogram->Bins.Reset();
	}

	/** This will add a bin to the histogram, extending from the previous bin (or starting value) to the passed in MaxValue */
	void AddBin(double MaxValue)
	{
		// Can't add to a closed builder
		check(MyHistogram);
		MyHistogram->Bins.Emplace(LastValue, MaxValue);
		LastValue = MaxValue;
	}

	/** Call when done adding bins, this will create a final unbounded bin to catch values above the maximum value */
	void FinishBins()
	{
		MyHistogram->Bins.Emplace(LastValue);
		MyHistogram = nullptr;
	}

	~FHistogramBuilder()
	{
		// Close it out if the user hasn't already
		if (MyHistogram != nullptr)
		{
			FinishBins();
		}
	}
private:
	FHistogram* MyHistogram;
	double LastValue;
};

