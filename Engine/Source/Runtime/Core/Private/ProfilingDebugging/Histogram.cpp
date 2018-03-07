// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilingDebugging/Histogram.h"

DEFINE_LOG_CATEGORY(LogHistograms);

//////////////////////////////////////////////////////////////////////
// FHistogram

void FHistogram::InitLinear(double MinTime, double MaxTime, double BinSize)
{
	SumOfAllMeasures = 0.0;
	CountOfAllMeasures = 0;

	Bins.Empty();

	double CurrentBinMin = MinTime;
	while (CurrentBinMin < MaxTime)
	{
		Bins.Add(FBin(CurrentBinMin, CurrentBinMin + BinSize));
		CurrentBinMin += BinSize;
	}
	Bins.Add(FBin(CurrentBinMin));	// catch-all-above bin
}

void FHistogram::InitHitchTracking()
{
	SumOfAllMeasures = 0.0;
	CountOfAllMeasures = 0;

	Bins.Empty();

	Bins.Add(FBin(0.0, 9.0));		// >= 120 fps
	Bins.Add(FBin(9.0, 17.0));		// 60 - 120 fps
	Bins.Add(FBin(17.0, 34.0));		// 30 - 60 fps
	Bins.Add(FBin(34.0, 50.0));		// 20 - 30 fps
	Bins.Add(FBin(50.0, 67.0));		// 15 - 20 fps
	Bins.Add(FBin(67.0, 100.0));	// 10 - 15 fps
	Bins.Add(FBin(100.0, 200.0));	// 5 - 10 fps
	Bins.Add(FBin(200.0, 300.0));	// < 5 fps
	Bins.Add(FBin(300.0, 500.0));
	Bins.Add(FBin(500.0, 750.0));
	Bins.Add(FBin(750.0, 1000.0));
	Bins.Add(FBin(1000.0, 1500.0));
	Bins.Add(FBin(1500.0, 2000.0));
	Bins.Add(FBin(2000.0, 2500.0));
	Bins.Add(FBin(2500.0, 5000.0));
	Bins.Add(FBin(5000.0));
}

void FHistogram::InitFromArray(TArrayView<double> Thresholds)
{
	SumOfAllMeasures = 0.0;
	CountOfAllMeasures = 0;
	Bins.Empty();

	for (int32 Index = 0; Index < Thresholds.Num(); ++Index)
	{
		const int32 NextIndex = Index + 1;
		if (NextIndex == Thresholds.Num())
		{
			Bins.Emplace(Thresholds[Index]);
		}
		else
		{
			Bins.Emplace(Thresholds[Index], Thresholds[NextIndex]);
		}
	}
}

void FHistogram::Reset()
{
	for (FBin& Bin : Bins)
	{
		Bin.Count = 0;
		Bin.Sum = 0.0;
	}
	SumOfAllMeasures = 0.0;
	CountOfAllMeasures = 0;
}

void FHistogram::AddMeasurement(double ValueForBinning, double MeasurementValue)
{
	if (LIKELY(Bins.Num()))
	{
		FBin& FirstBin = Bins[0];
		if (UNLIKELY(ValueForBinning < FirstBin.MinValue))
		{
			return;
		}

		for (int BinIdx = 0, LastBinIdx = Bins.Num() - 1; BinIdx < LastBinIdx; ++BinIdx)
		{
			FBin& Bin = Bins[BinIdx];
			if (Bin.UpperBound > ValueForBinning)
			{
				++Bin.Count;
				++CountOfAllMeasures;
				Bin.Sum += MeasurementValue;
				SumOfAllMeasures += MeasurementValue;
				return;
			}
		}

		// if we got here, ValueForBinning did not fit in any of the previous bins
		FBin& LastBin = Bins.Last();
		++LastBin.Count;
		++CountOfAllMeasures;
		LastBin.Sum += MeasurementValue;
		SumOfAllMeasures += MeasurementValue;
	}
}

void FHistogram::DumpToAnalytics(const FString& ParamNamePrefix, TArray<TPair<FString, double>>& OutParamArray)
{
	double TotalSum = 0;
	double TotalObservations = 0;
	double AverageObservation = 0;

	if (LIKELY(Bins.Num()))
	{
		for (int BinIdx = 0, LastBinIdx = Bins.Num() - 1; BinIdx < LastBinIdx; ++BinIdx)
		{
			FBin& Bin = Bins[BinIdx];
			FString ParamName = FString::Printf(TEXT("_%.0f_%.0f"), Bin.MinValue, Bin.UpperBound);
			OutParamArray.Emplace(ParamNamePrefix + ParamName + TEXT("_Count"), Bin.Count);
			OutParamArray.Emplace(ParamNamePrefix + ParamName + TEXT("_Sum"), Bin.Sum);

			TotalObservations += Bin.Count;
			TotalSum += Bin.Sum;
		}

		FBin& LastBin = Bins.Last();
		FString ParamName = FString::Printf(TEXT("_%.0f_AndAbove"), LastBin.MinValue);
		OutParamArray.Emplace(ParamNamePrefix + ParamName + TEXT("_Count"), LastBin.Count);
		OutParamArray.Emplace(ParamNamePrefix + ParamName + TEXT("_Sum"), LastBin.Sum);

		TotalObservations += LastBin.Count;
		TotalSum += LastBin.Sum;
	}

	if (TotalObservations > 0)
	{
		AverageObservation = TotalSum / TotalObservations;
	}

	// add an average value for ease of monitoring/analyzing
	OutParamArray.Emplace(ParamNamePrefix + TEXT("_Average"), AverageObservation);
}

void FHistogram::DumpToLog(const FString& HistogramName)
{
	UE_LOG(LogHistograms, Log, TEXT("Histogram '%s': %d bins"), *HistogramName, Bins.Num());

	if (LIKELY(Bins.Num()))
	{
		double TotalSum = 0;
		double TotalObservations = 0;

		for (int BinIdx = 0, LastBinIdx = Bins.Num() - 1; BinIdx < LastBinIdx; ++BinIdx)
		{
			FBin& Bin = Bins[BinIdx];
			UE_LOG(LogHistograms, Log, TEXT("Bin %4.0f - %4.0f: %5d observation(s) which sum up to %f"), Bin.MinValue, Bin.UpperBound, Bin.Count, Bin.Sum);

			TotalObservations += Bin.Count;
			TotalSum += Bin.Sum;
		}

		FBin& LastBin = Bins.Last();
		UE_LOG(LogHistograms, Log, TEXT("Bin %4.0f +     : %5d observation(s) which sum up to %f"), LastBin.MinValue, LastBin.Count, LastBin.Sum);
		TotalObservations += LastBin.Count;
		TotalSum += LastBin.Sum;

		if (TotalObservations > 0)
		{
			UE_LOG(LogHistograms, Log, TEXT("Average value for observation: %f"), TotalSum / TotalObservations);
		}
	}
}
