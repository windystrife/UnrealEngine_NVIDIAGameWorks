// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/** 
 * ChartCreation
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/Histogram.h"
#include "Scalability.h"

//////////////////////////////////////////////////////////////////////

// What kind of hitch was detected (if any)
enum class EFrameHitchType : uint8
{
	// We didn't hitch
	NoHitch,

	// We hitched but couldn't isolate which unit caused it
	UnknownUnit,

	// Hitched and it was likely caused by the game thread
	GameThread,

	// Hitched and it was likely caused by the render thread
	RenderThread,

	// Hitched and it was likely caused by the GPU
	GPU
};


//////////////////////////////////////////////////////////////////////
// IPerformanceDataConsumer

// This is an interface for any consumer of per-frame performance data
// such as FPS charts, PerfCounters, analytics, etc...

class IPerformanceDataConsumer
{
public:
	struct FFrameData
	{
		// Estimate of how long the last frame was (this is either TrueDeltaSeconds or TrueDeltaSeconds - IdleSeconds, depending on the cvar t.FPSChart.ExcludeIdleTime)
		double DeltaSeconds;

		// Time elapsed since the last time the performance tracking system ran
		double TrueDeltaSeconds;

		// How long did we burn idling until this frame (e.g., when running faster than a frame rate target on a dedicated server)
		double IdleSeconds;

		// Duration of each of the major functional units (GPU time is frequently inferred rather than actual)
		double GameThreadTimeSeconds;
		double RenderThreadTimeSeconds;
		double GPUTimeSeconds;

		// Should this frame be considered for histogram generation (controlled by t.FPSChart.MaxFrameDeltaSecsBeforeDiscarding)
		bool bBinThisFrame;

		// Was this frame bound in one of the major functional units (only set if bBinThisFrame is true and the frame was longer than FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS) 
		bool bGameThreadBound;
		bool bRenderThreadBound;
		bool bGPUBound;

		// Did we hitch?
		EFrameHitchType HitchStatus;

		// If a hitch, how was it bound
		//@TODO: This uses different logic to the three bools above but probably shouldn't (though it also ignores the MaxFrameDeltaSecsBeforeDiscarding)

		FFrameData()
			: DeltaSeconds(0.0)
			, TrueDeltaSeconds(0.0)
			, IdleSeconds(0.0)
			, GameThreadTimeSeconds(0.0)
			, RenderThreadTimeSeconds(0.0)
			, GPUTimeSeconds(0.0)
			, bBinThisFrame(false)
			, bGameThreadBound(false)
			, bRenderThreadBound(false)
			, bGPUBound(false)
			, HitchStatus(EFrameHitchType::NoHitch)
		{
		}
	};

	virtual void StartCharting()=0;
	virtual void ProcessFrame(const FFrameData& FrameData)=0;
	virtual void StopCharting()=0;
	virtual ~IPerformanceDataConsumer() {}
};

//////////////////////////////////////////////////////////////////////
// FPerformanceTrackingChart

 // Chart for a single portion of gameplay (e.g., gameplay or in-game-shop or settings menu open)
class ENGINE_API FPerformanceTrackingChart : public IPerformanceDataConsumer
{
public:
	// The mode being tracked by this chart
	FString ChartLabel;

	// Frame rate histogram (thresholds in frames/second, values in seconds)
	FHistogram FramerateHistogram;

	// Hitch time histogram (in seconds)
	FHistogram HitchTimeHistogram;

	/** Number of frames for each time of <boundtype> **/
	uint32 NumFramesBound_GameThread;
	uint32 NumFramesBound_RenderThread;
	uint32 NumFramesBound_GPU;

	/** Time spent bound on each kind of thing (in seconds) */
	double TotalFramesBoundTime_GameThread;
	double TotalFramesBoundTime_RenderThread;
	double TotalFramesBoundTime_GPU;

	/** Total time spent on each thread (in seconds) */
	double TotalFrameTime_GameThread;
	double TotalFrameTime_RenderThread;
	double TotalFrameTime_GPU;

	/** Total number of hitches bound by each kind of thing */
	int32 TotalGameThreadBoundHitchCount;
	int32 TotalRenderThreadBoundHitchCount;
	int32 TotalGPUBoundHitchCount;

	/** Start time of the capture */
	const FDateTime CaptureStartTime;

	/** Total accumulated raw (including idle) time spent with the chart registered */
	double AccumulatedChartTime;

public:
	FPerformanceTrackingChart(const FDateTime& InStartTime, const FString& InChartLabel);

	double GetAverageFramerate() const
	{
		return FramerateHistogram.GetNumMeasurements() / FramerateHistogram.GetSumOfAllMeasures();
	}

	double GetPercentMissedVSync(int32 TargetFPS) const
	{
		const int64 TotalTargetFrames = TargetFPS * FramerateHistogram.GetSumOfAllMeasures();
		const int64 MissedFrames = FMath::Max<int64>(TotalTargetFrames - FramerateHistogram.GetNumMeasurements(), 0);
		return ((MissedFrames * 100.0) / (double)TotalTargetFrames);
	}

	double GetAvgHitchesPerMinute() const
	{
		const double TotalTime = FramerateHistogram.GetSumOfAllMeasures();
		const int32 TotalHitchCount = HitchTimeHistogram.GetNumMeasurements();

		return (TotalTime > 0.0) ? (TotalHitchCount / (TotalTime / 60.0f)) : 0.0;
	}

	double GetAvgHitchFrameLength() const
	{
		const double TotalTime = FramerateHistogram.GetSumOfAllMeasures();
		const int32 TotalHitchFrameTime = HitchTimeHistogram.GetSumOfAllMeasures();

		return (TotalTime > 0.0) ? (TotalHitchFrameTime / TotalTime) : 0.0;
	}

	int64 GetNumFrames() const
	{
		return FramerateHistogram.GetNumMeasurements();
	}

	void ChangeLabel(const FString& NewLabel)
	{
		ChartLabel = NewLabel;
	}

	void DumpFPSChart(const FString& InMapName);

	// Dumps the FPS chart information to an analytic event param array.
	void DumpChartToAnalyticsParams(const FString& InMapName, TArray<struct FAnalyticsEventAttribute>& InParamArray, bool bIncludeClientHWInfo) const;


	// Dumps the FPS chart information to the log.
	static void DumpChartsToOutputLog(double WallClockElapsed, const TArray<const FPerformanceTrackingChart*>& Charts, const FString& InMapName);

#if ALLOW_DEBUG_FILES
	// Dumps the FPS chart information to HTML.
	static void DumpChartsToHTML(double WallClockElapsed, const TArray<const FPerformanceTrackingChart*>& Charts, const FString& InMapName, const FString& HTMLFilename);

	// Dumps the FPS chart information to the special stats log file.
	static void DumpChartsToLogFile(double WallClockElapsed, const TArray<const FPerformanceTrackingChart*>& Charts, const FString& InMapName, const FString& LogFileName);
#endif

	// IPerformanceDataConsumer interface
	virtual void StartCharting() override;
	virtual void ProcessFrame(const FFrameData& FrameData) override;
	virtual void StopCharting() override;
	// End of IPerformanceDataConsumer interface
};

//////////////////////////////////////////////////////////////////////
// FFineGrainedPerformanceTracker

#if ALLOW_DEBUG_FILES

// Fine-grained tracking (records the frame time of each frame rather than just a histogram)
class ENGINE_API FFineGrainedPerformanceTracker : public IPerformanceDataConsumer
{
public:
	FFineGrainedPerformanceTracker(const FDateTime& InStartTime);

	/** Resets the fine-grained tracker, allocating enough memory to hold NumFrames frames (it can track more, but this avoids extra allocations when the length is short enough) */
	void Presize(int32 NumFrames);

	/**
	 * Dumps the timings for each frame to a .csv
	 */
	void DumpFrameTimesToStatsLog(const FString& FrameTimeFilename);

	/**
	 * Finds a percentile value in an array.
	 *
	 * @param Array array of values to look for (needs to be writable, will be modified)
	 * @param Percentile number between 0-99
	 * @return percentile value or -1 if no samples
	 */
	static float GetPercentileValue(TArray<float>& Samples, int32 Percentile);

	// IPerformanceDataConsumer interface
	virtual void StartCharting() override;
	virtual void ProcessFrame(const FFrameData& FrameData) override;
	virtual void StopCharting() override;
	// End of IPerformanceDataConsumer interface

public:
	/** Arrays of render/game/GPU and total frame times. Captured and written out if FPS charting is enabled and bFPSChartRecordPerFrameTimes is true */
	TArray<float> RenderThreadFrameTimes;
	TArray<float> GameThreadFrameTimes;
	TArray<float> GPUFrameTimes;
	TArray<float> FrameTimes;
	TArray<int32> ActiveModes;

	/** Start time of the capture */
	const FDateTime CaptureStartTime;

	/** Current context (user-specified integer stored per frame, could be used to signal game mode changes without doing discrete captures */
	int32 CurrentModeContext;
};

#endif

//////////////////////////////////////////////////////////////////////
// FPerformanceTrackingSystem

// Overall state of the built-in performance tracking
struct ENGINE_API FPerformanceTrackingSystem
{
public:
	FPerformanceTrackingSystem();

	IPerformanceDataConsumer::FFrameData AnalyzeFrame(float DeltaSeconds);
	void StartCharting();
	void StopCharting();

	/**
	 * This will create the file name for the file we are saving out.
	 *
	 * @param ChartType	the type of chart we are making 
	 * @param InMapName	the name of the map
	 * @param FileExtension	the filename extension to append
	 **/
	static FString CreateFileNameForChart(const FString& ChartType, const FString& InMapName, const FString& FileExtension);

	/** This will create the folder name for the output directory for FPS charts (and actually create the directory). */
	static FString CreateOutputDirectory(const FDateTime& CaptureStartTime);

	// Should we subtract off idle time spent waiting (due to running above target framerate) before thresholding into bins?
	// (controlled by t.FPSChart.ExcludeIdleTime)
	static bool ShouldExcludeIdleTimeFromCharts();

private:
	/** Start time of current FPS chart */
	double FPSChartStartTime;

	/** Stop time of current FPS chart */
	double FPSChartStopTime;

	/** We can't trust delta seconds if frame time clamping is enabled or if we're benchmarking so we simply calculate it ourselves. */
	double LastTimeChartCreationTicked;

	/** Keep track of our previous frame's statistics */
	float LastDeltaSeconds;

	/** Keep track of the last time we saw a hitch (used to suppress knock on hitches for a short period) */
	double LastHitchTime;
};

//////////////////////////////////////////////////////////////////////

// Prints the FPS chart summary to an endpoint
struct ENGINE_API FDumpFPSChartToEndpoint
{
protected:
	const FPerformanceTrackingChart& Chart;

public:
	/**
	* Dumps a chart, allowing subclasses to format the data in their own way via various protected virtuals to be overridden
	*/
	void DumpChart(double InWallClockTimeFromStartOfCharting, const FString& InMapName);

	FDumpFPSChartToEndpoint(const FPerformanceTrackingChart& InChart)
		: Chart(InChart)
	{
	}

	virtual ~FDumpFPSChartToEndpoint() { }

protected:
	double TotalTime;
	double WallClockTimeFromStartOfCharting; // This can be much larger than TotalTime if the chart was paused or long frames were omitted
	int32 NumFrames;
	FString MapName;

	float AvgFPS;
	float TimeDisregarded;
	float AvgGPUFrameTime;

	float BoundGameThreadPct;
	float BoundRenderThreadPct;
	float BoundGPUPct;

	Scalability::FQualityLevels ScalabilityQuality;
	FString OSMajor;
	FString OSMinor;

	FString CPUVendor;
	FString CPUBrand;

	// The primary GPU for the desktop (may not be the one we ended up using, e.g., in an optimus laptop)
	FString DesktopGPUBrand;

	// The actual GPU adapter we initialized
	FString ActualGPUBrand;

protected:
	virtual void PrintToEndpoint(const FString& Text) = 0;

	virtual void FillOutMemberStats();
	virtual void HandleFPSBucket(float BucketTimePercentage, float BucketFramePercentage, double StartFPS, double EndFPS);
	virtual void HandleHitchBucket(const FHistogram& HitchHistogram, int32 BucketIndex);
	virtual void HandleHitchSummary(int32 TotalHitchCount, double TotalTimeSpentInHitchBuckets);
	virtual void HandleFPSThreshold(int32 TargetFPS, int32 NumFramesBelow, float PctTimeAbove, float PctMissedFrames);
	virtual void HandleBasicStats();
};
