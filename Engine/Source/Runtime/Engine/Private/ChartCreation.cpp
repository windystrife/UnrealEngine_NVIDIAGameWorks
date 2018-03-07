// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * ChartCreation
 *
 */
#include "ChartCreation.h"
#include "ProfilingDebugging/ProfilingHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "AnalyticsEventAttribute.h"
#include "GameFramework/GameUserSettings.h"
#include "Performance/EnginePerformanceTargets.h"
#include "HAL/LowLevelMemTracker.h"

DEFINE_LOG_CATEGORY_STATIC(LogChartCreation, Log, All);

// Should we round raw FPS values before thresholding them into bins?
static TAutoConsoleVariable<int32> GRoundChartingFPSBeforeBinning(
	TEXT("t.FPSChart.RoundFPSBeforeBinning"),
	0,
	TEXT("Should we round raw FPS values before thresholding them into bins when doing a FPS chart?\n")
	TEXT(" default: 0"));

// Should we subtract off idle time spent waiting (due to running above target framerate) before thresholding into bins?
static TAutoConsoleVariable<int32> GFPSChartExcludeIdleTime(
	TEXT("t.FPSChart.ExcludeIdleTime"),
	0,
	TEXT("Should we exclude idle time (i.e. one which we spent sleeping) when doing a FPS chart?\n")
	TEXT(" default: 0"));

// Should we explore to the folder that contains the .log / etc... when a dump is finished?  This can be disabled for automated testing
static TAutoConsoleVariable<int32> GFPSChartOpenFolderOnDump(
	TEXT("t.FPSChart.OpenFolderOnDump"),
	1,
	TEXT("Should we explore to the folder that contains the .log / etc... when a dump is finished?  This can be disabled for automated testing\n")
	TEXT(" default: 1"));

float GMaximumFrameTimeToConsiderForHitchesAndBinning = 1.0f;

static FAutoConsoleVariableRef GMaximumFrameTimeToConsiderForHitchesAndBinningCVar(
	TEXT("t.FPSChart.MaxFrameDeltaSecsBeforeDiscarding"),
	GMaximumFrameTimeToConsiderForHitchesAndBinning,
	TEXT("The maximum length a frame can be (in seconds) to be considered for FPS chart binning (default 1.0s; no maximum length if <= 0.0)")
	);

/** The engine-wide performance tracking chart */
FPerformanceTrackingSystem GPerformanceTrackingSystem;

// Comma separated list of interesting frame rates
static TAutoConsoleVariable<FString> GFPSChartInterestingFramerates(
	TEXT("t.FPSChart.InterestingFramerates"),
	TEXT("30,60,120"),
	TEXT("Comma separated list of interesting frame rates\n")
	TEXT(" default: 30,60,120"));

/** Array of interesting summary thresholds (e.g., 30 Hz, 60 Hz, 120 Hz) */
TArray<int32> GTargetFrameRatesForSummary;

//////////////////////////////////////////////////////////////////////
// FDumpFPSChartToEndpoint

void FDumpFPSChartToEndpoint::FillOutMemberStats()
{
	// Get OS info
	FPlatformMisc::GetOSVersions(/*out*/ OSMajor, /*out*/ OSMinor);
	OSMajor.TrimStartAndEndInline();
	OSMinor.TrimStartAndEndInline();

	// Get CPU/GPU info
	CPUVendor = FPlatformMisc::GetCPUVendor().TrimStartAndEnd();
	CPUBrand = FPlatformMisc::GetCPUBrand().TrimStartAndEnd();
	DesktopGPUBrand = FPlatformMisc::GetPrimaryGPUBrand().TrimStartAndEnd();
	ActualGPUBrand = GRHIAdapterName.TrimStartAndEnd();

	// Get settings info
	UGameUserSettings* UserSettingsObj = GEngine->GetGameUserSettings();
	check(UserSettingsObj);
	ScalabilityQuality = UserSettingsObj->ScalabilityQuality;
}

void FDumpFPSChartToEndpoint::HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, double StartFPS, double EndFPS)
{
	// Log bucket index, time and frame Percentage.
	PrintToEndpoint(FString::Printf(TEXT("Bucket: %.1f - %.1f  Time: %5.2f  Frame: %5.2f"), StartFPS, EndFPS, BucketTimePercentage, BucketFramePercentage));
}

void FDumpFPSChartToEndpoint::HandleHitchBucket(const FHistogram& HitchHistogram, int32 BucketIndex)
{
	const double LowerBound = HitchHistogram.GetBinLowerBound(BucketIndex);
	const double UpperBound = HitchHistogram.GetBinUpperBound(BucketIndex);

	FString RangeName;
	if (UpperBound == FLT_MAX)
	{
		RangeName = FString::Printf(TEXT("%0.2fs - inf"), LowerBound);
	}
	else
	{
		RangeName = FString::Printf(TEXT("%0.2fs - %0.2fs"), LowerBound, UpperBound);
	}

	PrintToEndpoint(FString::Printf(TEXT("Bucket: %s  Count: %i  Time: %.2f s"), *RangeName, HitchHistogram.GetBinObservationsCount(BucketIndex), HitchHistogram.GetBinObservationsSum(BucketIndex)));
}

void FDumpFPSChartToEndpoint::HandleHitchSummary(int32 TotalHitchCount, double TotalTimeSpentInHitchBuckets)
{
	PrintToEndpoint(FString::Printf(TEXT("Total hitch count:  %i"), TotalHitchCount));

	const double ReciprocalNumHitches = (TotalHitchCount > 0) ? (1.0 / (double)TotalHitchCount) : 0.0;
	PrintToEndpoint(FString::Printf(TEXT("Hitch frames bound by game thread:  %i  (%0.1f percent)"), Chart.TotalGameThreadBoundHitchCount, ReciprocalNumHitches * Chart.TotalGameThreadBoundHitchCount));
	PrintToEndpoint(FString::Printf(TEXT("Hitch frames bound by render thread:  %i  (%0.1f percent)"), Chart.TotalRenderThreadBoundHitchCount, ReciprocalNumHitches * Chart.TotalRenderThreadBoundHitchCount));
	PrintToEndpoint(FString::Printf(TEXT("Hitch frames bound by GPU:  %i  (%0.1f percent)"), Chart.TotalGPUBoundHitchCount, ReciprocalNumHitches * Chart.TotalGPUBoundHitchCount));
	PrintToEndpoint(FString::Printf(TEXT("Hitches / min:  %.2f"), Chart.GetAvgHitchesPerMinute()));
	PrintToEndpoint(FString::Printf(TEXT("Time spent in hitch buckets:  %.2f s"), TotalTimeSpentInHitchBuckets));
	PrintToEndpoint(FString::Printf(TEXT("Avg. hitch frame length:  %.2f s"), Chart.GetAvgHitchFrameLength()));
}

void FDumpFPSChartToEndpoint::HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames)
{
	const float PercentFramesAbove = float(NumFrames - NumFramesBelow) / float(NumFrames)*100.0f;

	PrintToEndpoint(FString::Printf(TEXT("  Target %d FPS: %.2f %% syncs missed, %4.2f %% of time spent > %d FPS (%.2f %% of frames)"), TargetFPS, PctMissedFrames, PctTimeAbove, TargetFPS, PercentFramesAbove));
}

void FDumpFPSChartToEndpoint::HandleBasicStats()
{
	PrintToEndpoint(FString::Printf(TEXT("--- Begin : FPS chart dump for level '%s'"), *MapName));

	PrintToEndpoint(FString::Printf(TEXT("Dumping FPS chart at %s using build %s in config %s built from changelist %i"), *FDateTime::Now().ToString(), FApp::GetBuildVersion(), EBuildConfigurations::ToString(FApp::GetBuildConfiguration()), GetChangeListNumberForPerfTesting()));

	PrintToEndpoint(TEXT("Machine info:"));
	PrintToEndpoint(FString::Printf(TEXT("\tOS: %s %s"), *OSMajor, *OSMinor));
	PrintToEndpoint(FString::Printf(TEXT("\tCPU: %s %s"), *CPUVendor, *CPUBrand));

	FString CompositeGPUString = FString::Printf(TEXT("\tGPU: %s"), *ActualGPUBrand);
	if (ActualGPUBrand != DesktopGPUBrand)
	{
		CompositeGPUString += FString::Printf(TEXT(" (desktop adapter %s)"), *DesktopGPUBrand);
	}
	PrintToEndpoint(CompositeGPUString);

	PrintToEndpoint(FString::Printf(TEXT("\tResolution Quality: %.2f"), ScalabilityQuality.ResolutionQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tView Distance Quality: %d"), ScalabilityQuality.ViewDistanceQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tAnti-Aliasing Quality: %d"), ScalabilityQuality.AntiAliasingQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tShadow Quality: %d"), ScalabilityQuality.ShadowQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tPost-Process Quality: %d"), ScalabilityQuality.PostProcessQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tTexture Quality: %d"), ScalabilityQuality.TextureQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tEffects Quality: %d"), ScalabilityQuality.EffectsQuality));
	PrintToEndpoint(FString::Printf(TEXT("\tFoliage Quality: %d"), ScalabilityQuality.FoliageQuality));

	PrintToEndpoint(FString::Printf(TEXT("%i frames collected over %4.2f seconds, disregarding %4.2f seconds for a %4.2f FPS average"),
		NumFrames,
		WallClockTimeFromStartOfCharting,
		TimeDisregarded,
		AvgFPS));
	PrintToEndpoint(FString::Printf(TEXT("Average GPU frametime: %4.2f ms"), AvgGPUFrameTime));
	PrintToEndpoint(FString::Printf(TEXT("BoundGameThreadPct: %4.2f"), BoundGameThreadPct));
	PrintToEndpoint(FString::Printf(TEXT("BoundRenderThreadPct: %4.2f"), BoundRenderThreadPct));
	PrintToEndpoint(FString::Printf(TEXT("BoundGPUPct: %4.2f"), BoundGPUPct));
	PrintToEndpoint(FString::Printf(TEXT("ExcludeIdleTime: %d"), GFPSChartExcludeIdleTime.GetValueOnGameThread()));
}

void FDumpFPSChartToEndpoint::DumpChart(double InWallClockTimeFromStartOfCharting, const FString& InMapName)
{
	FillOutMemberStats();

	TotalTime = Chart.FramerateHistogram.GetSumOfAllMeasures();
	WallClockTimeFromStartOfCharting = InWallClockTimeFromStartOfCharting;
	NumFrames = Chart.FramerateHistogram.GetNumMeasurements();
	MapName = InMapName;

	if (TotalTime > WallClockTimeFromStartOfCharting)
	{
		UE_LOG(LogChartCreation, Log, TEXT("Weirdness: wall clock time (%f) is smaller than total frame time (%f)"), WallClockTimeFromStartOfCharting, TotalTime);
	}

	AvgFPS = NumFrames / TotalTime;
	TimeDisregarded = FMath::Max<float>(0, WallClockTimeFromStartOfCharting - TotalTime);
	AvgGPUFrameTime = float((Chart.TotalFrameTime_GPU / NumFrames)*1000.0);

	BoundGameThreadPct = (float(Chart.NumFramesBound_GameThread) / float(NumFrames))*100.0f;
	BoundRenderThreadPct = (float(Chart.NumFramesBound_RenderThread) / float(NumFrames))*100.0f;
	BoundGPUPct = (float(Chart.NumFramesBound_GPU) / float(NumFrames))*100.0f;

	// Let the derived class process the members we've set up
	HandleBasicStats();

	// keep track of the number of frames below X FPS, and the percentage of time above X FPS
	TArray<float, TInlineAllocator<4>> TimesSpentAboveThreshold;
	TArray<int32, TInlineAllocator<4>> FramesSpentBelowThreshold;
	TimesSpentAboveThreshold.AddZeroed(GTargetFrameRatesForSummary.Num());
	FramesSpentBelowThreshold.AddZeroed(GTargetFrameRatesForSummary.Num());

	// Iterate over all FPS chart buckets, dumping percentages.
	//@TODO: Try adding an iterator to the histogram
	for (int32 NumBins = Chart.FramerateHistogram.GetNumBins(), BinIndex = 0; BinIndex < NumBins; ++BinIndex)
	{
		const double ChartEntrySumTime = Chart.FramerateHistogram.GetBinObservationsSum(BinIndex);
		const int32 ChartEntryCount = Chart.FramerateHistogram.GetBinObservationsCount(BinIndex);
		const double StartFPS = Chart.FramerateHistogram.GetBinLowerBound(BinIndex);
		const double EndFPS = Chart.FramerateHistogram.GetBinUpperBound(BinIndex);

		// Figure out bucket time and frame percentage.
		const float BucketTimePercentage = 100.f * ChartEntrySumTime / TotalTime;
		const float BucketFramePercentage = (100.f * ChartEntryCount) / NumFrames;

		for (int32 ThresholdIndex = 0; ThresholdIndex < GTargetFrameRatesForSummary.Num(); ++ThresholdIndex)
		{
			const int32 FrameRateThreshold = GTargetFrameRatesForSummary[ThresholdIndex];

			if (StartFPS >= FrameRateThreshold)
			{
				TimesSpentAboveThreshold[ThresholdIndex] += BucketTimePercentage;
			}
			else
			{
				FramesSpentBelowThreshold[ThresholdIndex] += ChartEntryCount;
			}
		}

		HandleFPSBucket(BucketTimePercentage, BucketFramePercentage, StartFPS, EndFPS);
	}

	// Handle threhsolds
	for (int32 ThresholdIndex = 0; ThresholdIndex < GTargetFrameRatesForSummary.Num(); ++ThresholdIndex)
	{
		const int32 TargetFPS = GTargetFrameRatesForSummary[ThresholdIndex];

		const float PctTimeAbove = TimesSpentAboveThreshold[ThresholdIndex];
		const int32 NumFramesBelow = FramesSpentBelowThreshold[ThresholdIndex];
		const float PctMissedFrames = (float)Chart.GetPercentMissedVSync(TargetFPS);

		HandleFPSThreshold(TargetFPS, NumFramesBelow, PctTimeAbove, PctMissedFrames);
	}

	// Dump hitch data
	{
		PrintToEndpoint(FString::Printf(TEXT("--- Begin : Hitch chart dump for level '%s'"), *MapName));

		for (int32 NumBins = Chart.HitchTimeHistogram.GetNumBins(), BinIndex = 0; BinIndex < NumBins; ++BinIndex)
		{
			HandleHitchBucket(Chart.HitchTimeHistogram, BinIndex);
		}

		const double TotalTimeSpentInHitchBuckets = Chart.HitchTimeHistogram.GetSumOfAllMeasures();
		const int32 TotalHitchCount = Chart.HitchTimeHistogram.GetNumMeasurements();

		HandleHitchSummary(TotalHitchCount, TotalTimeSpentInHitchBuckets);

		PrintToEndpoint(TEXT("--- End"));
	}
}

//////////////////////////////////////////////////////////////////////

struct FDumpFPSChartToAnalyticsArray : public FDumpFPSChartToEndpoint
{
	FDumpFPSChartToAnalyticsArray(const FPerformanceTrackingChart& InChart, TArray<FAnalyticsEventAttribute>& InParamArray, bool bShouldIncludeClientHWInfo)
		: FDumpFPSChartToEndpoint(InChart)
		, ParamArray(InParamArray)
		, bIncludeClientHWInfo(bShouldIncludeClientHWInfo)
	{
	}

protected:
	TArray<FAnalyticsEventAttribute>& ParamArray;
	bool bIncludeClientHWInfo;
protected:
	virtual void PrintToEndpoint(const FString& Text) override
	{
	}

	virtual void HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, double StartFPS, double EndFPS) override
	{
		const int32 IntegralStartFPS = (int32)StartFPS;
		const int32 IntegralEndFPS = (EndFPS == FLT_MAX) ? 999 : (int32)EndFPS;
		check((IntegralStartFPS != IntegralEndFPS) && (IntegralStartFPS < IntegralEndFPS));
		ParamArray.Add(FAnalyticsEventAttribute(FString::Printf(TEXT("Bucket_%i_%i_TimePercentage"), IntegralStartFPS, IntegralEndFPS), BucketTimePercentage));
	}

	virtual void HandleHitchBucket(const FHistogram& HitchHistogram, int32 BucketIndex) override
	{
		const double UpperBoundSecs = HitchHistogram.GetBinUpperBound(BucketIndex);
		const int32 LowerBoundMS = (int32)(HitchHistogram.GetBinLowerBound(BucketIndex) * 1000.0);
		const int32 UpperBoundMS = (int32)(UpperBoundSecs * 1000.0);

		FString ParamNameBase;
		if (UpperBoundSecs == FLT_MAX)
		{
			ParamNameBase = FString::Printf(TEXT("Hitch_%i_Plus_Hitch"), LowerBoundMS);
		}
		else
		{
			ParamNameBase = FString::Printf(TEXT("Hitch_%i_%i_Hitch"), LowerBoundMS, UpperBoundMS);
		}

		ParamArray.Add(FAnalyticsEventAttribute(ParamNameBase + TEXT("Count"), HitchHistogram.GetBinObservationsCount(BucketIndex)));
		ParamArray.Add(FAnalyticsEventAttribute(ParamNameBase + TEXT("Time"), HitchHistogram.GetBinObservationsSum(BucketIndex)));
	}


	virtual void HandleHitchSummary(int32 TotalHitchCount, double TotalTimeSpentInHitchBuckets) override
	{
		// Add hitch totals to the param array
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalHitches"), TotalHitchCount));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalGameBoundHitches"), Chart.TotalGameThreadBoundHitchCount));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalRenderBoundHitches"), Chart.TotalRenderThreadBoundHitchCount));
		if (bIncludeClientHWInfo)
		{
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalGPUBoundHitches"), Chart.TotalGPUBoundHitchCount));
		}
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalTimeInHitchFrames"), TotalTimeSpentInHitchBuckets));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("HitchesPerMinute"), Chart.GetAvgHitchesPerMinute()));

		// Determine how much time was spent 'above and beyond' regular frame time in frames that landed in hitch buckets
		const float EngineTargetMS = FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();
		const float HitchThresholdMS = FEnginePerformanceTargets::GetHitchFrameTimeThresholdMS();

		const float AcceptableFramePortionMS = (HitchThresholdMS > EngineTargetMS) ? EngineTargetMS : 0.0f;

		const float MSToSeconds = 1.0f / 1000.0f;
		const double RegularFramePortionForHitchFrames = AcceptableFramePortionMS * MSToSeconds * TotalHitchCount;

		const double TimeSpentHitching = TotalTimeSpentInHitchBuckets - RegularFramePortionForHitchFrames;
		ensure(TimeSpentHitching >= 0.0);

		const double PercentSpentHitching = (TotalTime > 0.0) ? (100.0 * TimeSpentHitching / TotalTime) : 0.0;
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentSpentHitching"), PercentSpentHitching));
	}

	virtual void HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames) override
	{
		{
			const FString ParamName = FString::Printf(TEXT("PercentAbove%d"), TargetFPS);
			const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctTimeAbove);
			ParamArray.Add(FAnalyticsEventAttribute(ParamName, ParamValue));
		}
		{
			const FString ParamName = FString::Printf(TEXT("MVP%d"), TargetFPS);
			const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctMissedFrames);
			ParamArray.Add(FAnalyticsEventAttribute(ParamName, ParamValue));
		}
	}

	virtual void HandleBasicStats() override
	{
		// Add non-bucket params
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ChangeList"), GetChangeListNumberForPerfTesting()));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("BuildType"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration())));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("DateStamp"), FDateTime::Now().ToString()));

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Platform"), FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()))));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("OS"), FString::Printf(TEXT("%s %s"), *OSMajor, *OSMinor)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("CPU"), FString::Printf(TEXT("%s %s"), *CPUVendor, *CPUBrand)));

		if (bIncludeClientHWInfo)
		{
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("DesktopGPU"), DesktopGPUBrand)); //@TODO: Cut this one out entirely?
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUAdapter"), ActualGPUBrand));

			ParamArray.Add(FAnalyticsEventAttribute(TEXT("ResolutionQuality"), ScalabilityQuality.ResolutionQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("ViewDistanceQuality"), ScalabilityQuality.ViewDistanceQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("AntiAliasingQuality"), ScalabilityQuality.AntiAliasingQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("ShadowQuality"), ScalabilityQuality.ShadowQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("PostProcessQuality"), ScalabilityQuality.PostProcessQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("TextureQuality"), ScalabilityQuality.TextureQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("FXQuality"), ScalabilityQuality.EffectsQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("FoliageQuality"), ScalabilityQuality.FoliageQuality));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentGPUBound"), FString::Printf(TEXT("%4.2f"), BoundGPUPct)));
			ParamArray.Add(FAnalyticsEventAttribute(TEXT("AvgGPUTime"), FString::Printf(TEXT("%4.2f"), AvgGPUFrameTime)));
		}

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("AvgFPS"), FString::Printf(TEXT("%4.2f"), AvgFPS)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("TimeDisregarded"), FString::Printf(TEXT("%4.2f"), TimeDisregarded)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Time"), FString::Printf(TEXT("%4.2f"), WallClockTimeFromStartOfCharting)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("FrameCount"), FString::Printf(TEXT("%i"), NumFrames)));

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentGameThreadBound"), FString::Printf(TEXT("%4.2f"), BoundGameThreadPct)));
		ParamArray.Add(FAnalyticsEventAttribute(TEXT("PercentRenderThreadBound"), FString::Printf(TEXT("%4.2f"), BoundRenderThreadPct)));

		ParamArray.Add(FAnalyticsEventAttribute(TEXT("ExcludeIdleTime"), FString::Printf(TEXT("%d"), GFPSChartExcludeIdleTime.GetValueOnGameThread())));
	}
};

//////////////////////////////////////////////////////////////////////

struct FDumpFPSChartToLogEndpoint : public FDumpFPSChartToEndpoint
{
	FDumpFPSChartToLogEndpoint(const FPerformanceTrackingChart& InChart)
		: FDumpFPSChartToEndpoint(InChart)
	{
	}

	virtual void PrintToEndpoint(const FString& Text) override
	{
		UE_LOG(LogChartCreation, Log, TEXT("%s"), *Text);
	}
};

//////////////////////////////////////////////////////////////////////

#if ALLOW_DEBUG_FILES
struct FDumpFPSChartToFileEndpoint : public FDumpFPSChartToEndpoint
{
	FArchive* MyArchive;

	FDumpFPSChartToFileEndpoint(const FPerformanceTrackingChart& InChart, FArchive* InArchive)
		: FDumpFPSChartToEndpoint(InChart)
		, MyArchive(InArchive)
	{
	}

	virtual void PrintToEndpoint(const FString& Text) override
	{
		MyArchive->Logf(TEXT("%s"), *Text);
	}
};
#endif

//////////////////////////////////////////////////////////////////////

#if ALLOW_DEBUG_FILES
struct FDumpFPSChartToHTMLEndpoint : public FDumpFPSChartToEndpoint
{
	FString& FPSChartRow;

	FDumpFPSChartToHTMLEndpoint(const FPerformanceTrackingChart& InChart, FString& InFPSChartRow)
		: FDumpFPSChartToEndpoint(InChart)
		, FPSChartRow(InFPSChartRow)
	{
	}

protected:
	virtual void PrintToEndpoint(const FString& Text) override
	{
	}

	virtual void HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, double StartFPS, double EndFPS) override
	{
		const int32 IntegralStartFPS = (int32)StartFPS;
		const int32 IntegralEndFPS = (EndFPS == FLT_MAX) ? 999 : (int32)EndFPS;
		check((IntegralStartFPS != IntegralEndFPS) && (IntegralStartFPS < IntegralEndFPS));

		const FString SrcToken = FString::Printf(TEXT("TOKEN_%i_%i"), IntegralStartFPS, IntegralEndFPS);
		const FString DstToken = FString::Printf(TEXT("%5.2f"), BucketTimePercentage);

		// Replace token with actual values.
		FPSChartRow = FPSChartRow.Replace(*SrcToken, *DstToken, ESearchCase::CaseSensitive);
	}

	virtual void HandleHitchBucket(const FHistogram& HitchHistogram, int32 BucketIndex) override
	{
		const double UpperBoundSecs = HitchHistogram.GetBinUpperBound(BucketIndex);
		const int32 LowerBoundMS = (int32)(HitchHistogram.GetBinLowerBound(BucketIndex) * 1000.0);
		const int32 UpperBoundMS = (int32)(UpperBoundSecs * 1000.0);

		FString SrcToken;
		if (UpperBoundSecs == FLT_MAX)
		{
			SrcToken = FString::Printf(TEXT("TOKEN_HITCH_%i_PLUS"), LowerBoundMS);
		}
		else
		{
			SrcToken = FString::Printf(TEXT("TOKEN_HITCH_%i_%i"), LowerBoundMS, UpperBoundMS);
		}

		const FString DstToken = FString::Printf(TEXT("%i"), HitchHistogram.GetBinObservationsCount(BucketIndex));

		// Replace token with actual values.
		FPSChartRow = FPSChartRow.Replace(*SrcToken, *DstToken, ESearchCase::CaseSensitive);
	}

	virtual void HandleHitchSummary(int32 TotalHitchCount, double TotalTimeSpentInHitchBuckets) override
	{
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_TOTAL"), *FString::Printf(TEXT("%i"), TotalHitchCount), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_GAME_BOUND_COUNT"), *FString::Printf(TEXT("%i"), Chart.TotalGameThreadBoundHitchCount), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_RENDER_BOUND_COUNT"), *FString::Printf(TEXT("%i"), Chart.TotalRenderThreadBoundHitchCount), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCH_GPU_BOUND_COUNT"), *FString::Printf(TEXT("%i"), Chart.TotalGPUBoundHitchCount), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_HITCHES_PER_MIN"), *FString::Printf(TEXT("%.2f"), Chart.GetAvgHitchesPerMinute()), ESearchCase::CaseSensitive);
	}

	virtual void HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames) override
	{
		{
			const FString ParamName = FString::Printf(TEXT("TOKEN_PCT_ABOVE_%d"), TargetFPS);
			const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctTimeAbove);
			FPSChartRow = FPSChartRow.Replace(*ParamName, *ParamValue, ESearchCase::CaseSensitive);
		}

		{
			const FString ParamName = FString::Printf(TEXT("TOKEN_MVP_%d"), TargetFPS);
			const FString ParamValue = FString::Printf(TEXT("%4.2f"), PctMissedFrames);
			FPSChartRow = FPSChartRow.Replace(*ParamName, *ParamValue, ESearchCase::CaseSensitive);
		}
	}

	virtual void HandleBasicStats() override
	{
		// Update non- bucket stats.
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_MAPNAME"), *FString::Printf(TEXT("%s"), *MapName), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_CHANGELIST"), *FString::Printf(TEXT("%i"), GetChangeListNumberForPerfTesting()), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_DATESTAMP"), *FString::Printf(TEXT("%s"), *FDateTime::Now().ToString()), ESearchCase::CaseSensitive);

		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_OS"), *FString::Printf(TEXT("%s %s"), *OSMajor, *OSMinor), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_CPU"), *FString::Printf(TEXT("%s %s"), *CPUVendor, *CPUBrand), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_GPU"), *FString::Printf(TEXT("%s"), *ActualGPUBrand), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_RES"), *FString::Printf(TEXT("%.2f"), ScalabilityQuality.ResolutionQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_VD"), *FString::Printf(TEXT("%d"), ScalabilityQuality.ViewDistanceQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_AA"), *FString::Printf(TEXT("%d"), ScalabilityQuality.AntiAliasingQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_SHADOW"), *FString::Printf(TEXT("%d"), ScalabilityQuality.ShadowQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_PP"), *FString::Printf(TEXT("%d"), ScalabilityQuality.PostProcessQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_TEX"), *FString::Printf(TEXT("%d"), ScalabilityQuality.TextureQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_FX"), *FString::Printf(TEXT("%d"), ScalabilityQuality.EffectsQuality), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_SETTINGS_FLG"), *FString::Printf(TEXT("%d"), ScalabilityQuality.FoliageQuality), ESearchCase::CaseSensitive);

		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_FPS"), *FString::Printf(TEXT("%4.2f"), AvgFPS), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_TIME_DISREGARDED"), *FString::Printf(TEXT("%4.2f"), TimeDisregarded), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_TIME"), *FString::Printf(TEXT("%4.2f"), WallClockTimeFromStartOfCharting), ESearchCase::CaseSensitive); //@TODO: Questionable given multiple charts
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_FRAMECOUNT"), *FString::Printf(TEXT("%i"), NumFrames), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_GPUTIME"), *FString::Printf(TEXT("%4.2f ms"), AvgGPUFrameTime), ESearchCase::CaseSensitive);

		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_BOUND_GAME_THREAD_PERCENT"), *FString::Printf(TEXT("%4.2f"), BoundGameThreadPct), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_BOUND_RENDER_THREAD_PERCENT"), *FString::Printf(TEXT("%4.2f"), BoundRenderThreadPct), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_BOUND_GPU_PERCENT"), *FString::Printf(TEXT("%4.2f"), BoundGPUPct), ESearchCase::CaseSensitive);

		// Add non-bucket params
		// 		ParamArray.Add(FAnalyticsEventAttribute(TEXT("BuildType"), EBuildConfigurations::ToString(FApp::GetBuildConfiguration())));
		// 		ParamArray.Add(FAnalyticsEventAttribute(TEXT("Platform"), FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()))));

		// Sum up FrameTimes and GameTimes
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_RENDTIME"), *FString::Printf(TEXT("%4.2f ms"), float((Chart.TotalFrameTime_RenderThread / NumFrames)*1000.0)), ESearchCase::CaseSensitive);
		FPSChartRow = FPSChartRow.Replace(TEXT("TOKEN_AVG_GAMETIME"), *FString::Printf(TEXT("%4.2f ms"), float((Chart.TotalFrameTime_GameThread / NumFrames)*1000.0)), ESearchCase::CaseSensitive);
	}
};
#endif

//////////////////////////////////////////////////////////////////////
// FPerformanceTrackingChart

FPerformanceTrackingChart::FPerformanceTrackingChart(const FDateTime& InStartTime, const FString& InChartLabel)
	: ChartLabel(InChartLabel)
	, NumFramesBound_GameThread(0)
	, NumFramesBound_RenderThread(0)
	, NumFramesBound_GPU(0)
	, TotalFramesBoundTime_GameThread(0.0)
	, TotalFramesBoundTime_RenderThread(0.0)
	, TotalFramesBoundTime_GPU(0.0)
	, TotalFrameTime_GameThread(0.0)
	, TotalFrameTime_RenderThread(0.0)
	, TotalFrameTime_GPU(0.0)
	, TotalGameThreadBoundHitchCount(0)
	, TotalRenderThreadBoundHitchCount(0)
	, TotalGPUBoundHitchCount(0)
	, CaptureStartTime(InStartTime)
	, AccumulatedChartTime(0.0){
	{
		const double FPSThresholds[] = { 5.0, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0 };

		FHistogramBuilder Builder(FramerateHistogram, 0.0);
		for (double Threshold : FPSThresholds)
		{
			Builder.AddBin(Threshold);
		}
	}

	{
		const double HitchThresholdsMS[] = { 30.0, 60.0, 100.0, 150.0, 200.0, 300.0, 500.0, 750.0, 1000.0, 1500.0, 2000.0, 2500.0, 5000.0 };
		const double MSToSeconds = 1.0 / 1000.0;

		FHistogramBuilder Builder(HitchTimeHistogram, 0.0);
		for (double ThresholdMS : HitchThresholdsMS)
		{
			Builder.AddBin(ThresholdMS * MSToSeconds);
		}
	}
}

void FPerformanceTrackingChart::StartCharting()
{
}

void FPerformanceTrackingChart::StopCharting()
{
}

void FPerformanceTrackingChart::ProcessFrame(const FFrameData& FrameData)
{
	AccumulatedChartTime += FrameData.TrueDeltaSeconds;

	// Handle the frame time histogram
	if (FrameData.bBinThisFrame)
	{
		{
			const float CurrentFPS_Raw = 1.0f / FrameData.DeltaSeconds;

			const bool bShouldRound = GRoundChartingFPSBeforeBinning.GetValueOnGameThread() != 0;
			const float CurrentFPS = bShouldRound ? FMath::RoundToFloat(CurrentFPS_Raw) : CurrentFPS_Raw;

			FramerateHistogram.AddMeasurement(CurrentFPS, FrameData.DeltaSeconds);//@TODO: Should we round the actual delta seconds measurement value too?
		}

		if (FrameData.bGameThreadBound)
		{
			NumFramesBound_GameThread++;
			TotalFramesBoundTime_GameThread += FrameData.DeltaSeconds;
		}

		if (FrameData.bRenderThreadBound)
		{
			NumFramesBound_RenderThread++;
			TotalFramesBoundTime_RenderThread += FrameData.DeltaSeconds;
		}

		if (FrameData.bGPUBound)
		{
			TotalFramesBoundTime_GPU += FrameData.DeltaSeconds;
			NumFramesBound_GPU++;
		}
	}

	// Track per frame stats.
	TotalFrameTime_GameThread += FrameData.GameThreadTimeSeconds;
	TotalFrameTime_RenderThread += FrameData.RenderThreadTimeSeconds;
	TotalFrameTime_GPU += FrameData.GPUTimeSeconds;

	// Handle hitching
	if (FrameData.HitchStatus != EFrameHitchType::NoHitch)
	{
		// Track the hitch by bucketing it based on time severity
		HitchTimeHistogram.AddMeasurement(FrameData.DeltaSeconds);

		switch (FrameData.HitchStatus)
		{
		case EFrameHitchType::GameThread:
			++TotalGameThreadBoundHitchCount;
			break;
		case EFrameHitchType::RenderThread:
			++TotalRenderThreadBoundHitchCount;
			break;
		case EFrameHitchType::GPU:
			++TotalGPUBoundHitchCount;
			break;
		}
	}
}

void FPerformanceTrackingChart::DumpFPSChart(const FString& InMapName)
{
	TArray<const FPerformanceTrackingChart*> Charts;
	Charts.Add(this);

	// Print chart info to the output log
	DumpChartsToOutputLog(AccumulatedChartTime, Charts, InMapName);

	const FString OutputDir = FPerformanceTrackingSystem::CreateOutputDirectory(CaptureStartTime);
	const FString ChartType = TEXT("FPS");

#if ALLOW_DEBUG_FILES
	{
		const FString LogFilename = OutputDir / FPerformanceTrackingSystem::CreateFileNameForChart(ChartType, InMapName, TEXT(".log"));
		DumpChartsToLogFile(AccumulatedChartTime, Charts, InMapName, LogFilename);
	}

	{
		const FString MapAndChartLabel = ChartLabel.IsEmpty() ? InMapName : (ChartLabel + TEXT("-") + InMapName);
		const FString HTMLFilename = OutputDir / FPerformanceTrackingSystem::CreateFileNameForChart(ChartType, *(MapAndChartLabel + "-" + CaptureStartTime.ToString()), TEXT(".html"));
		DumpChartsToHTML(AccumulatedChartTime, Charts, MapAndChartLabel, HTMLFilename);
	}
#endif
}

void FPerformanceTrackingChart::DumpChartsToOutputLog(double WallClockElapsed, const TArray<const FPerformanceTrackingChart*>& Charts, const FString& InMapName)
{
	for (const FPerformanceTrackingChart* Chart : Charts)
	{
		FDumpFPSChartToLogEndpoint Endpoint(*Chart);
		Endpoint.DumpChart(WallClockElapsed, InMapName);
	}
}

#if ALLOW_DEBUG_FILES
void FPerformanceTrackingChart::DumpChartsToLogFile(double WallClockElapsed, const TArray<const FPerformanceTrackingChart*>& Charts, const FString& InMapName, const FString& LogFileName)
{
	// Create archive for log data (append if it already exists).
	if (FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*LogFileName, FILEWRITE_Append))
	{
		for (const FPerformanceTrackingChart* pChart : Charts)
		{
			FDumpFPSChartToFileEndpoint FileEndpoint(*pChart, OutputFile);
			FileEndpoint.DumpChart(WallClockElapsed, InMapName);
		}

		OutputFile->Logf(LINE_TERMINATOR LINE_TERMINATOR LINE_TERMINATOR);

		// Flush, close and delete.
		delete OutputFile;

		const FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*LogFileName);
		UE_LOG(LogProfilingDebugging, Warning, TEXT("FPS Chart (logfile) saved to %s"), *AbsolutePath);

#if	PLATFORM_DESKTOP
		if (GFPSChartOpenFolderOnDump.GetValueOnGameThread())
		{
			FPlatformProcess::ExploreFolder(*AbsolutePath);
		}
#endif
	}
}
#endif

void FPerformanceTrackingChart::DumpChartToAnalyticsParams(const FString& InMapName, TArray<struct FAnalyticsEventAttribute>& InParamArray, bool bIncludeClientHWInfo) const
{
	// Iterate over all buckets, gathering total frame count and cumulative time.
	const double TotalTime = FramerateHistogram.GetSumOfAllMeasures();
	const int32 NumFrames = FramerateHistogram.GetNumMeasurements();

	if ((TotalTime > 0.f) && (NumFrames > 0))
	{
		// Dump all the basic stats
		FDumpFPSChartToAnalyticsArray AnalyticsEndpoint(*this, InParamArray, bIncludeClientHWInfo);
		AnalyticsEndpoint.DumpChart(AccumulatedChartTime, InMapName);

		if (bIncludeClientHWInfo)
		{
			// Dump some extra non-chart-based stats

			// Get the system memory stats
			FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalPhysical"), static_cast<uint64>(Stats.TotalPhysical)));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("TotalVirtual"), static_cast<uint64>(Stats.TotalVirtual)));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("PeakPhysical"), static_cast<uint64>(Stats.PeakUsedPhysical)));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("PeakVirtual"), static_cast<uint64>(Stats.PeakUsedVirtual)));

			// Get the texture memory stats
			FTextureMemoryStats TexMemStats;
			RHIGetTextureMemoryStats(TexMemStats);
			const int32 DedicatedVRAM = FMath::DivideAndRoundUp(TexMemStats.DedicatedVideoMemory, (int64)(1024 * 1024));
			const int32 DedicatedSystem = FMath::DivideAndRoundUp(TexMemStats.DedicatedSystemMemory, (int64)(1024 * 1024));
			const int32 DedicatedShared = FMath::DivideAndRoundUp(TexMemStats.SharedSystemMemory, (int64)(1024 * 1024));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("VRAM"), DedicatedVRAM));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("VSYS"), DedicatedSystem));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("VSHR"), DedicatedShared));

			// Get the benchmark results and resolution/display settings to phone home
			const UGameUserSettings* UserSettingsObj = GEngine->GetGameUserSettings();
			check(UserSettingsObj);

			// Additional CPU information
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("CPU_NumCoresP"), FPlatformMisc::NumberOfCores()));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("CPU_NumCoresL"), FPlatformMisc::NumberOfCoresIncludingHyperthreads()));

			// True adapter / driver version / etc... information
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUVendorID"), GRHIVendorId));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUDeviceID"), GRHIDeviceId));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("GPURevisionID"), GRHIDeviceRevision));
			GRHIAdapterInternalDriverVersion.TrimStartAndEndInline();
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUDriverVerI"), GRHIAdapterInternalDriverVersion));
			GRHIAdapterUserDriverVersion.TrimStartAndEndInline();
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUDriverVerU"), GRHIAdapterUserDriverVersion));

			// Benchmark results
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("CPUBM"), UserSettingsObj->GetLastCPUBenchmarkResult()));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("GPUBM"), UserSettingsObj->GetLastGPUBenchmarkResult()));

			{
				int32 StepIndex = 0;
				for (float StepValue : UserSettingsObj->GetLastCPUBenchmarkSteps())
				{
					const FString StepName = FString::Printf(TEXT("CPUBM_%d"), StepIndex);
					InParamArray.Add(FAnalyticsEventAttribute(StepName, StepValue));
					++StepIndex;
				}
			}
			{
				int32 StepIndex = 0;
				for (float StepValue : UserSettingsObj->GetLastGPUBenchmarkSteps())
				{
					const FString StepName = FString::Printf(TEXT("GPUBM_%d"), StepIndex);
					InParamArray.Add(FAnalyticsEventAttribute(StepName, StepValue));
					++StepIndex;
				}
			}

			// Screen percentage (3D render resolution)
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("ScreenPct"), Scalability::GetResolutionScreenPercentage()));

			// Window mode and window/monitor resolution
			const EWindowMode::Type FullscreenMode = UserSettingsObj->GetLastConfirmedFullscreenMode();
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("WindowMode"), (int32)FullscreenMode));

			FIntPoint ViewportSize(0, 0);
			if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
			{
				ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
			}
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("SizeX"), ViewportSize.X));
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("SizeY"), ViewportSize.Y));

			const int32 VSyncValue = UserSettingsObj->IsVSyncEnabled() ? 1 : 0;
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("VSync"), (int32)VSyncValue));

			const float FrameRateLimit = UserSettingsObj->GetFrameRateLimit();
			InParamArray.Add(FAnalyticsEventAttribute(TEXT("FrameRateLimit"), FrameRateLimit));
		}
	}
}

//////////////////////////////////////////////////////////////////////
// FFineGrainedPerformanceTracker

#if ALLOW_DEBUG_FILES

FFineGrainedPerformanceTracker::FFineGrainedPerformanceTracker(const FDateTime& InStartTime)
	: CaptureStartTime(InStartTime)
	, CurrentModeContext(0)
{
	// Pre-allocate 10 minutes worth of frames at 30 Hz
	const int32 InitialNumFrames = 10 * 60 * 30;
	Presize(InitialNumFrames);
}

void FFineGrainedPerformanceTracker::Presize(int32 NumFrames)
{
	RenderThreadFrameTimes.Reset(NumFrames);
	GPUFrameTimes.Reset(NumFrames);
	GameThreadFrameTimes.Reset(NumFrames);
	FrameTimes.Reset(NumFrames);
	ActiveModes.Reset(NumFrames);
}

float FFineGrainedPerformanceTracker::GetPercentileValue(TArray<float>& Samples, int32 Percentile)
{
	int32 Left = 0;
	int32 Right = Samples.Num() - 1;

	if (Right < 0)
	{
		return -1.0f;
	}

	const int32 PercentileOrdinal = (Percentile * Right) / 100;

	// this is quickselect (see http://en.wikipedia.org/wiki/Quickselect for details).
	while (Right != Left)
	{
		// partition
		int32 MovingLeft = Left - 1;
		int32 MovingRight = Right;
		float Pivot = Samples[MovingRight];
		for (; ; )
		{
			while (Samples[++MovingLeft] < Pivot);
			while (Samples[--MovingRight] > Pivot)
			{
				if (MovingRight == Left)
				{
					break;
				}
			}

			if (MovingLeft >= MovingRight)
			{
				break;
			}

			const float Temp = Samples[MovingLeft];
			Samples[MovingLeft] = Samples[MovingRight];
			Samples[MovingRight] = Temp;
		}

		const float Temp = Samples[MovingLeft];
		Samples[MovingLeft] = Samples[Right];
		Samples[Right] = Temp;

		// now we're pivoted around MovingLeft
		// decide what part K-th largest belong to
		if (MovingLeft > PercentileOrdinal)
		{
			Right = MovingLeft - 1;
		}
		else if (MovingLeft < PercentileOrdinal)
		{
			Left = MovingLeft + 1;
		}
		else
		{
			// we hit exactly the value we need, no need to sort further
			break;
		}
	}

	return Samples[PercentileOrdinal];
}

void FFineGrainedPerformanceTracker::StartCharting()
{
}

void FFineGrainedPerformanceTracker::StopCharting()
{
}

void FFineGrainedPerformanceTracker::ProcessFrame(const FFrameData& FrameData)
{
	// Capturing FPS chart info. We only use these when we intend to write out to a stats log
	GameThreadFrameTimes.Add(FrameData.GameThreadTimeSeconds);
	RenderThreadFrameTimes.Add(FrameData.RenderThreadTimeSeconds);
	GPUFrameTimes.Add(FrameData.GPUTimeSeconds);
	FrameTimes.Add(FrameData.DeltaSeconds);
	ActiveModes.Add(CurrentModeContext);
}

void FFineGrainedPerformanceTracker::DumpFrameTimesToStatsLog(const FString& FrameTimeFilename)
{
	if (FArchive* OutputFile = IFileManager::Get().CreateDebugFileWriter(*FrameTimeFilename))
	{
		OutputFile->Logf(TEXT("Percentile,Frame (ms), GT (ms), RT (ms), GPU (ms),Context"));
		TArray<float> FrameTimesCopy = FrameTimes;
		TArray<float> GameThreadFrameTimesCopy = GameThreadFrameTimes;
		TArray<float> RenderThreadFrameTimesCopy = RenderThreadFrameTimes;
		TArray<float> GPUFrameTimesCopy = GPUFrameTimes;
		// using selection a few times should still be faster than full sort once,
		// since it's linear vs non-linear (O(n) vs O(n log n) for quickselect vs quicksort)
		for (int32 Percentile = 25; Percentile <= 75; Percentile += 25)
		{
			OutputFile->Logf(TEXT("%d,%.2f,%.2f,%.2f,%.2f,%d"), Percentile,
				GetPercentileValue(FrameTimesCopy, Percentile) * 1000,
				GetPercentileValue(GameThreadFrameTimesCopy, Percentile) * 1000,
				GetPercentileValue(RenderThreadFrameTimesCopy, Percentile) * 1000,
				GetPercentileValue(GPUFrameTimesCopy, Percentile) * 1000,
				0
				);
		}

		OutputFile->Logf(TEXT("Time (sec),Frame (ms), GT (ms), RT (ms), GPU (ms),Context"));
		double ElapsedTime = 0.0;
		for (int32 i = 0; i < FrameTimes.Num(); i++)
		{
			OutputFile->Logf(TEXT("%.2f,%.2f,%.2f,%.2f,%.2f,%d"), ElapsedTime, FrameTimes[i] * 1000, GameThreadFrameTimes[i] * 1000, RenderThreadFrameTimes[i] * 1000, GPUFrameTimes[i] * 1000, ActiveModes[i]);
			ElapsedTime += FrameTimes[i];
		}
		delete OutputFile;
	}
}

#endif

//////////////////////////////////////////////////////////////////////
// FPerformanceTrackingSystem

FPerformanceTrackingSystem::FPerformanceTrackingSystem()
	: FPSChartStartTime(0.0)
	, FPSChartStopTime(0.0)
	, LastTimeChartCreationTicked(0.0)
	, LastDeltaSeconds(0.0f)
	, LastHitchTime(0.0)
{
}

FString FPerformanceTrackingSystem::CreateFileNameForChart(const FString& ChartType, const FString& InMapName, const FString& FileExtension)
{
	// Note: Using PlatformName() instead of IniPlatformName() here intentionally so we can easily spot FPS charts that came from an uncooked build
	const FString Platform = FPlatformProperties::PlatformName();
	const FString Result = InMapName + TEXT("-FPS-") + Platform + FileExtension;
	return Result;
}

FString FPerformanceTrackingSystem::CreateOutputDirectory(const FDateTime& CaptureStartTime)
{
	// Create folder for FPS chart data.
	const FString OutputDir = FPaths::ProfilingDir() / TEXT( "FPSChartStats" ) / CaptureStartTime.ToString();
	IFileManager::Get().MakeDirectory( *OutputDir, true );
	return OutputDir;
}

bool FPerformanceTrackingSystem::ShouldExcludeIdleTimeFromCharts()
{
	return GFPSChartExcludeIdleTime.GetValueOnGameThread() != 0;
}

IPerformanceDataConsumer::FFrameData FPerformanceTrackingSystem::AnalyzeFrame(float DeltaSeconds)
{
	const float MSToSeconds = 1.0f / 1000.0f;

	IPerformanceDataConsumer::FFrameData FrameData;

	// Copy these locally since the RT may update it between reads otherwise
	const uint32 LocalRenderThreadTime = GRenderThreadTime;
	const uint32 LocalGPUFrameTime = GGPUFrameTime;

	const double CurrentTime = FPlatformTime::Seconds();
	if (LastTimeChartCreationTicked > 0)
	{
		DeltaSeconds = CurrentTime - LastTimeChartCreationTicked;
	}
	LastTimeChartCreationTicked = CurrentTime;
	const double TrueDeltaSeconds = DeltaSeconds;

	FrameData.TrueDeltaSeconds = DeltaSeconds;

	// subtract idle time (FPS chart is ticked after UpdateTimeAndHandleMaxTickRate(), so we know time we spent sleeping this frame)
	if (ShouldExcludeIdleTimeFromCharts())
	{
		double ThisFrameIdleTime = FApp::GetIdleTime();
		if (LIKELY(ThisFrameIdleTime < DeltaSeconds))
		{
			DeltaSeconds -= ThisFrameIdleTime;
		}
		else
		{
			UE_LOG(LogChartCreation, Warning, TEXT("Idle time for this frame (%f) is larger than delta between FPSChart ticks (%f)"), ThisFrameIdleTime, DeltaSeconds);
		}
	}
	FrameData.DeltaSeconds = DeltaSeconds;

	// now gather some stats on what this frame was bound by (game, render, gpu)

	// determine which pipeline time is the greatest (between game thread, render thread, and GPU)
	const float EpsilonCycles = 0.250f;
	uint32 MaxThreadTimeValue = FMath::Max3<uint32>( LocalRenderThreadTime, GGameThreadTime, LocalGPUFrameTime );
	const float FrameTime = FPlatformTime::ToSeconds(MaxThreadTimeValue);

	const float EngineTargetMS = FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();

	// Try to estimate a GPU time even if the current platform does not support GPU timing
	uint32 PossibleGPUTime = LocalGPUFrameTime;
	if (PossibleGPUTime == 0)
	{
		// if we are over
		PossibleGPUTime = static_cast<uint32>(FMath::Max( FrameTime, DeltaSeconds ) / FPlatformTime::GetSecondsPerCycle() );
		MaxThreadTimeValue = FMath::Max3<uint32>( GGameThreadTime, LocalRenderThreadTime, PossibleGPUTime );
	}

	FrameData.GameThreadTimeSeconds = FPlatformTime::ToSeconds(GGameThreadTime);
	FrameData.RenderThreadTimeSeconds = FPlatformTime::ToSeconds(LocalRenderThreadTime);
	FrameData.GPUTimeSeconds = FPlatformTime::ToSeconds(LocalGPUFrameTime);

	// Optionally disregard frames that took longer than one second when accumulating data.
	const bool bBinThisFrame = (DeltaSeconds < GMaximumFrameTimeToConsiderForHitchesAndBinning) || (GMaximumFrameTimeToConsiderForHitchesAndBinning <= 0.0f);
	if (bBinThisFrame)
	{
		FrameData.bBinThisFrame = true;

		// if frame time is greater than our target then we are bounded by something
		const float TargetThreadTimeSeconds = EngineTargetMS * MSToSeconds;
		if (DeltaSeconds > TargetThreadTimeSeconds)
		{
			// If GPU time is inferred we can only determine GPU > threshold if we are GPU bound.
			bool bAreWeGPUBoundIfInferred = true;

			if (FrameData.GameThreadTimeSeconds >= TargetThreadTimeSeconds)
			{
				FrameData.bGameThreadBound = true;
				bAreWeGPUBoundIfInferred = false;
			}

			if (FrameData.RenderThreadTimeSeconds >= TargetThreadTimeSeconds)
			{
				FrameData.bRenderThreadBound = true;
				bAreWeGPUBoundIfInferred = false;
			}

			// Consider this frame GPU bound if we have an actual measurement which is over the limit,
			if (((LocalGPUFrameTime != 0) && (FrameData.GPUTimeSeconds >= TargetThreadTimeSeconds)) ||
				// Or if we don't have a measurement but neither of the other threads were the slowest
				((LocalGPUFrameTime == 0) && bAreWeGPUBoundIfInferred && (PossibleGPUTime == MaxThreadTimeValue)))
			{
				FrameData.bGPUBound = true;
			}
		}
	}

	// Check for hitches
	{
		// Minimum time quantum before we'll even consider this a hitch
		const float MinFrameTimeToConsiderAsHitch = FEnginePerformanceTargets::GetHitchFrameTimeThresholdMS() * MSToSeconds;

		// Ignore frames faster than our threshold
		if (DeltaSeconds >= MinFrameTimeToConsiderAsHitch)
		{
			// How long has it been since the last hitch we detected?
			const float TimeSinceLastHitch = (float)(CurrentTime - LastHitchTime);

			// Minimum time passed before we'll record a new hitch
			const float MinTimeBetweenHitches = FEnginePerformanceTargets::GetMinTimeBetweenHitchesMS() * MSToSeconds;

			// Make sure at least a little time has passed since the last hitch we reported
			if (TimeSinceLastHitch >= MinTimeBetweenHitches)
			{
				// For the current frame to be considered a hitch, it must have run at least this many times slower than
				// the previous frame
				const float HitchMultiplierAmount = FEnginePerformanceTargets::GetHitchToNonHitchRatio();

				// If our frame time is much larger than our last frame time, we'll count this as a hitch!
				if (DeltaSeconds > (LastDeltaSeconds * HitchMultiplierAmount))
				{
			
					// Check to see what we were limited by this frame
					if (GGameThreadTime >= (MaxThreadTimeValue - EpsilonCycles))
					{
						// Bound by game thread
						FrameData.HitchStatus = EFrameHitchType::GameThread;
					}
					else if (LocalRenderThreadTime >= (MaxThreadTimeValue - EpsilonCycles))
					{
						// Bound by render thread
						FrameData.HitchStatus = EFrameHitchType::RenderThread;
					}
					else if (PossibleGPUTime == MaxThreadTimeValue)
					{
						// Bound by GPU
						FrameData.HitchStatus = EFrameHitchType::GPU;
					}
					else
					{
						// Not sure what bound us, but we still hitched
						FrameData.HitchStatus = EFrameHitchType::UnknownUnit;
					}

					// We have a hitch!
					GEngine->OnHitchDetectedDelegate.Broadcast(FrameData.HitchStatus, DeltaSeconds);

					LastHitchTime = CurrentTime;
				}
			}
		}

		// Store stats for the next frame to look at (used in hitch rejection)
		LastDeltaSeconds = DeltaSeconds;
	}

	return FrameData;
}

void FPerformanceTrackingSystem::StartCharting()
{
	FPSChartStartTime = FPlatformTime::Seconds();

	// Signal that we haven't ticked before
	LastTimeChartCreationTicked = 0.0;

	// Determine which frame rates we care about
	GTargetFrameRatesForSummary.Reset();
	TArray<FString> InterestingFramerateStrings;
	GFPSChartInterestingFramerates.GetValueOnGameThread().ParseIntoArray(InterestingFramerateStrings, TEXT(","));
	for (FString FramerateString : InterestingFramerateStrings)
	{
		FramerateString.TrimStartAndEndInline();
		GTargetFrameRatesForSummary.Add(FCString::Atoi(*FramerateString));
	}

	GGPUFrameTime = 0;

	UE_LOG(LogChartCreation, Log, TEXT("Started creating FPS charts at %f seconds"), FPSChartStartTime);
}

void FPerformanceTrackingSystem::StopCharting()
{
	FPSChartStopTime = FPlatformTime::Seconds();

	UE_LOG(LogChartCreation, Log, TEXT("Stopped creating FPS charts at %f seconds"), FPSChartStopTime);
}

//////////////////////////////////////////////////////////////////////
// UEngine (partial)

void UEngine::TickPerformanceMonitoring(float DeltaSeconds)
{
	LLM_SCOPE(ELLMTag::Stats);

	if (ActivePerformanceDataConsumers.Num() > 0)
	{
		const IPerformanceDataConsumer::FFrameData FrameData = GPerformanceTrackingSystem.AnalyzeFrame(DeltaSeconds);

		// Route the frame data to all consumers
		for (TSharedPtr<IPerformanceDataConsumer> Consumer : GEngine->ActivePerformanceDataConsumers)
		{
			Consumer->ProcessFrame(FrameData);
		}
	}
}

void UEngine::AddPerformanceDataConsumer(TSharedPtr<IPerformanceDataConsumer> Consumer)
{
	ActivePerformanceDataConsumers.Add(Consumer);

	if (ActivePerformanceDataConsumers.Num() == 1)
	{
		GPerformanceTrackingSystem = FPerformanceTrackingSystem();
		GPerformanceTrackingSystem.StartCharting();
	}

	Consumer->StartCharting();
}

void UEngine::RemovePerformanceDataConsumer(TSharedPtr<IPerformanceDataConsumer> Consumer)
{
	Consumer->StopCharting();

	ActivePerformanceDataConsumers.Remove(Consumer);

	if (ActivePerformanceDataConsumers.Num() == 0)
	{
		GPerformanceTrackingSystem.StopCharting();
	}
}







void UEngine::StartFPSChart(const FString& Label, bool bRecordPerFrameTimes)
{
	const FDateTime CaptureStartTime = FDateTime::Now();

	if (ActivePerformanceChart.IsValid())
	{
		ActivePerformanceChart->ChangeLabel(Label);
	}
	else
	{
		ActivePerformanceChart = MakeShareable(new FPerformanceTrackingChart(CaptureStartTime, Label));
		AddPerformanceDataConsumer(ActivePerformanceChart);
	}

#if ALLOW_DEBUG_FILES
	if (bRecordPerFrameTimes)
	{
		if (!ActiveFrameTimesChart.IsValid())
		{
			ActiveFrameTimesChart = MakeShareable(new FFineGrainedPerformanceTracker(CaptureStartTime));
			AddPerformanceDataConsumer(ActiveFrameTimesChart);
		}
	}
#endif
}

void UEngine::StopFPSChart(const FString& InMapName)
{
	if (ActivePerformanceChart.IsValid())
	{
		RemovePerformanceDataConsumer(ActivePerformanceChart);
		ActivePerformanceChart->DumpFPSChart(InMapName);
		ActivePerformanceChart.Reset();
	}

#if ALLOW_DEBUG_FILES
	if (ActiveFrameTimesChart.IsValid())
	{
		RemovePerformanceDataConsumer(ActiveFrameTimesChart);

		const FString OutputDir = FPerformanceTrackingSystem::CreateOutputDirectory(ActiveFrameTimesChart->CaptureStartTime);
		const FString FrameTimeFilename = OutputDir / FPerformanceTrackingSystem::CreateFileNameForChart(TEXT("FPS"), InMapName, TEXT(".csv"));
		ActiveFrameTimesChart->DumpFrameTimesToStatsLog(FrameTimeFilename);

		ActiveFrameTimesChart.Reset();
	}
#endif
}

//////////////////////////////////////////////////////////////////////


#if ALLOW_DEBUG_FILES

const TCHAR* GFPSChartPreamble =
L"<HTML>\n"
L"   <HEAD>\n"
L"    <TITLE>FPS Chart</TITLE>\n"
L"\n"
L"    <META HTTP-EQUIV=\"CONTENT-TYPE\" CONTENT=\"TEXT/HTML; CHARSET=UTF-8\">\n"
L"    <LINK TITLE=\"default style\" REL=\"STYLESHEET\" HREF=\"../../Engine/Stats/ChartStyle.css\" TYPE=\"text/css\">\n"
L"    <LINK TITLE=\"default style\" REL=\"STYLESHEET\" HREF=\"../../Engine/Stats/FPSStyle.css\" TYPE=\"text/css\">\n"
L"\n"
L"  </HEAD>\n"
L"</HEAD>\n"
L"<BODY>\n"
L"\n"
L"<DIV CLASS=\"ChartStyle\">\n"
L"\n"
L"<TABLE BORDER=\"0\" CELLSPACING=\"0\" CELLPADDING=\"0\" BGCOLOR=\"#808080\">\n"
L"<TR><TD>\n"
L"<TABLE WIDTH=\"4000\" HEIGHT=\"100%\" BORDER=\"0\" CELLSPACING=\"1\" CELLPADDING=\"3\" BGCOLOR=\"#808080\">\n"
L"\n"
L"<TR CLASS=\"rowHeader\">\n"
L"<TD CLASS=\"rowHeadermapname\"><DIV CLASS=\"rowHeaderValue\">mapname</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderChangelist\"><DIV CLASS=\"rowHeaderValue\">changelist</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderDateStamp\"><DIV CLASS=\"rowHeaderValue\">datestamp</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderOS\"><DIV CLASS=\"rowHeaderValue\">OS</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderCPU\"><DIV CLASS=\"rowHeaderValue\">CPU</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderGPU\"><DIV CLASS=\"rowHeaderValue\">GPU</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsRes\"><DIV CLASS=\"rowHeaderValue\">Res Qual</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsVD\"><DIV CLASS=\"rowHeaderValue\">View Dist Qual</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsAA\"><DIV CLASS=\"rowHeaderValue\">AA Qual</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsShadow\"><DIV CLASS=\"rowHeaderValue\">Shadow Qual</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsPP\"><DIV CLASS=\"rowHeaderValue\">PP Qual</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsTex\"><DIV CLASS=\"rowHeaderValue\">Tex Qual</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSettingsFX\"><DIV CLASS=\"rowHeaderValue\">FX Qual</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>avg FPS</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>% over 30 FPS</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>% over 60 FPS</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>% over 120 FPS</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>Hitches/Min</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>% Missed VSync 30</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>% Missed VSync 60</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>% Missed VSync 120</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>avg GPU time</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>avg RT time</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderSummary\"><DIV>avg GT time</DIV></TD>\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>Game Thread Bound By Percent</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>Render Thread Bound By Percent</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>GPU Bound By Percent</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0 - 5</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">5 - 10</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">10 - 15</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">15 - 20</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">20 - 25</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">25 - 30</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">30 - 40</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">40 - 50</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">50 - 60</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">60 - 70</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">70 - 80</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">80 - 90</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">90 - 100</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">100 - 110</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">110 - 120</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">120 - INF</DIV></TD>\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowHeaderTimes\"><DIV>time</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderTimes\"><DIV>frame count</DIV></TD>\n"
L"<TD CLASS=\"rowHeaderTimes\"<DIV>time disregarded</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderTimes\">Total Hitches</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderTimes\">Game Thread Bound Hitch Frames</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderTimes\">Render Thread Bound Hitch Frames</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderTimes\">GPU Bound Hitch Frames</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">5.0 - INF</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">2.5 - 5.0</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">2.0 - 2.5</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">1.5 - 2.0</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">1.0 - 1.5</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.75 - 1.00</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.50 - 0.75</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.30 - 0.50</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.20 - 0.30</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.15 - 0.20</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.10 - 0.15</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.06 - 0.10</DIV></TD>\n"
L"<TD><DIV CLASS=\"rowHeaderValue\">0.03 - 0.06</DIV></TD>\n"
L"\n"
L"</TR>\n"
L"\n"
L"<UE4></UE4>";

const TCHAR* GFPSChartPostamble =
L"</TABLE>\n"
L"</TD></TR></TABLE>\n"
L"\n"
L"</DIV> <!-- <DIV CLASS=\"ChartStyle\"> -->\n"
L"\n"
L"</BODY>\n"
L"</HTML>\n"
L"";

const TCHAR* GFPSChartRow =
L"<TR CLASS=\"dataRow\">\n"
L"<TD CLASS=\"rowEntryMapName\"><DIV>TOKEN_MAPNAME</DIV></TD>\n"
L"<TD CLASS=\"rowEntryChangelist\"><DIV>TOKEN_CHANGELIST</DIV></TD>\n"
L"<TD CLASS=\"rowEntryDateStamp\"><DIV>TOKEN_DATESTAMP</DIV></TD>\n"
L"<TD CLASS=\"rowEntryOS\"><DIV>TOKEN_OS</DIV></TD>\n"
L"<TD CLASS=\"rowEntryCPU\"><DIV>TOKEN_CPU</DIV></TD>\n"
L"<TD CLASS=\"rowEntryGPU\"><DIV>TOKEN_GPU</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsRes\"><DIV>TOKEN_SETTINGS_RES</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsVD\"><DIV>TOKEN_SETTINGS_VD</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsAA\"><DIV>TOKEN_SETTINGS_AA</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsShadow\"><DIV>TOKEN_SETTINGS_SHADOW</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsPP\"><DIV>TOKEN_SETTINGS_PP</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsTex\"><DIV>TOKEN_SETTINGS_TEX</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySettingsFX\"><DIV>TOKEN_SETTINGS_FX</DIV></TD>\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_AVG_FPS</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_PCT_ABOVE_30</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_PCT_ABOVE_60</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_PCT_ABOVE_120</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_HITCHES_PER_MIN</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_MVP_30</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_MVP_60</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_MVP_120</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_AVG_GPUTIME</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_AVG_RENDTIME</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_AVG_GAMETIME</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_BOUND_GAME_THREAD_PERCENT</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_BOUND_RENDER_THREAD_PERCENT</DIV></TD>\n"
L"<TD CLASS=\"rowEntrySummary\"><DIV>TOKEN_BOUND_GPU_PERCENT</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"<TD><DIV CLASS=\"value\">TOKEN_0_5</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_5_10</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_10_15</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_15_20</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_20_25</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_25_30</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_30_40</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_40_50</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_50_60</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_60_70</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_70_80</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_80_90</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_90_100</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_100_110</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_110_120</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_120_999</DIV></TD>\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"\n"
L"\n"
L"<TD CLASS=\"rowEntryTimes\"><DIV>TOKEN_TIME</DIV></TD>\n"
L"<TD CLASS=\"rowEntryTimes\"><DIV>TOKEN_FRAMECOUNT</DIV></TD>\n"
L"<TD CLASS=\"rowEntryTimes\"><DIV>TOKEN_TIME_DISREGARDED</DIV></TD>\n"
L"\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_TOTAL</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_GAME_BOUND_COUNT</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_RENDER_BOUND_COUNT</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_GPU_BOUND_COUNT</DIV></TD>\n"
L"\n"
L"<TD CLASS=\"columnSeparator\"><DIV>&nbsp;</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_5000_PLUS</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_2500_5000</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_2000_2500</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_1500_2000</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_1000_1500</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_750_1000</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_500_750</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_300_500</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_200_300</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_150_200</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_100_150</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_60_100</DIV></TD>\n"
L"<TD><DIV CLASS=\"value\">TOKEN_HITCH_30_60</DIV></TD>\n"
L"\n"
L"</TR>";

void FPerformanceTrackingChart::DumpChartsToHTML(double WallClockElapsed, const TArray<const FPerformanceTrackingChart*>& Charts, const FString& InMapName, const FString& HTMLFilename)
{
	// Load the HTML building blocks
	static const FString FPSChartPreamble(GFPSChartPreamble);
	static const FString FPSChartPostamble(GFPSChartPostamble);
	static const FString FPSChartRowStructure(GFPSChartRow);
//@TODO: Was going to put a % this line occupied in
// 	// Determine how much all of the charts together occupied
// 	double TotalTimeTracked = 0.0;
// 	for (const FPerformanceTrackingChart* pChart : Charts)
// 	{
// 		TotalTimeTracked += pChart->FramerateHistogram.GetSumOfAllMeasures();
// 	}

	FString NewRows;
	for (const FPerformanceTrackingChart* Chart : Charts)
	{
		FString NewRow = FPSChartRowStructure;
		FDumpFPSChartToHTMLEndpoint HTMLEndpoint(*Chart, /*inout*/ NewRow);
		HTMLEndpoint.DumpChart(WallClockElapsed, InMapName);

		NewRows += NewRow;
	}

	// See whether file already exists and load it into string if it does.
	FString FPSChart;
	if (FFileHelper::LoadFileToString(FPSChart, *HTMLFilename))
	{
		// Split string where we want to insert current row.
		const FString HeaderSeparator = TEXT("<UE4></UE4>");
		FString FPSChartBeforeCurrentRow, FPSChartAfterCurrentRow;
		FPSChart.Split(*HeaderSeparator, &FPSChartBeforeCurrentRow, &FPSChartAfterCurrentRow);

		// Assemble FPS chart by inserting current row at the top.
		FPSChart = FPSChartPreamble + NewRows + FPSChartAfterCurrentRow;
	}
	else
	{
		// Assemble from scratch.
		FPSChart = FPSChartPreamble + NewRows + FPSChartPostamble;
	}

	// Save the resulting file back to disk.
	FFileHelper::SaveStringToFile(FPSChart, *HTMLFilename);

	UE_LOG(LogProfilingDebugging, Warning, TEXT( "FPS Chart (HTML) saved to %s" ), *IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*HTMLFilename));
}
#endif // ALLOW_DEBUG_FILES
