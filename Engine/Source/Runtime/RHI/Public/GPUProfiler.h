// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GPUProfiler.h: Hierarchical GPU Profiler.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "RHI.h"

/** Stats for a single perf event node. */
class FGPUProfilerEventNodeStats : public FRefCountedObject
{
public:
	FGPUProfilerEventNodeStats() :
		NumDraws(0),
		NumPrimitives(0),
		NumVertices(0),
		NumTotalDraws(0),
		NumTotalPrimitives(0),
		NumTotalVertices(0),
		TimingResult(0),
		NumEvents(0)
	{
	}

	/** Exclusive number of draw calls rendered in this event. */
	uint32 NumDraws;

	/** Exclusive number of primitives rendered in this event. */
	uint32 NumPrimitives;

	/** Exclusive number of vertices rendered in this event. */
	uint32 NumVertices;

	/** Inclusive number of draw calls rendered in this event and children. */
	uint32 NumTotalDraws;

	/** Inclusive number of primitives rendered in this event and children. */
	uint32 NumTotalPrimitives;

	/** Inclusive number of vertices rendered in this event and children. */
	uint32 NumTotalVertices;

	/** GPU time spent inside the perf event's begin and end, in ms. */
	float TimingResult;

	/** Inclusive number of other perf events that this is the parent of. */
	uint32 NumEvents;

	const FGPUProfilerEventNodeStats operator+=(const FGPUProfilerEventNodeStats& rhs)
	{
		NumDraws += rhs.NumDraws;
		NumPrimitives += rhs.NumPrimitives;
		NumVertices += rhs.NumVertices;
		NumTotalDraws += rhs.NumDraws;
		NumTotalPrimitives += rhs.NumPrimitives;
		NumTotalVertices += rhs.NumVertices;
		TimingResult += rhs.TimingResult;
		NumEvents += rhs.NumEvents;

		return *this;
	}
};

/** Stats for a single perf event node. */
class FGPUProfilerEventNode : public FGPUProfilerEventNodeStats
{
public:
	FGPUProfilerEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) :
		FGPUProfilerEventNodeStats(),
		Name(InName),
		Parent(InParent)
	{
	}

	~FGPUProfilerEventNode() {}

	FString Name;

	/** Pointer to parent node so we can walk up the tree on appEndDrawEvent. */
	FGPUProfilerEventNode* Parent;

	/** Children perf event nodes. */
	TArray<TRefCountPtr<FGPUProfilerEventNode> > Children;

	virtual float GetTiming() { return 0.0f; }
	virtual void StartTiming() {}
	virtual void StopTiming() {}
};

/** An entire frame of perf event nodes, including ancillary timers. */
struct RHI_API FGPUProfilerEventNodeFrame
{
	virtual ~FGPUProfilerEventNodeFrame() {}

	/** Root nodes of the perf event tree. */
	TArray<TRefCountPtr<FGPUProfilerEventNode> > EventTree;

	/** Start this frame of per tracking */
	virtual void StartFrame() {}

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() {}

	/** Dumps perf event information, blocking on GPU. */
	void DumpEventTree();

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() { return 0.0f; }

	/** D3D11 Hack */
	virtual void LogDisjointQuery() {}

	virtual bool PlatformDisablesVSync() const { return false; }
};

/**
 * Holds information if this platform's GPU allows timing
 */
struct RHI_API FGPUTiming
{
public:
	/**
	 * Whether GPU timing measurements are supported by the driver.
	 *
	 * @return true if GPU timing measurements are supported by the driver.
	 */
	static bool IsSupported()
	{
		return GIsSupported;
	}

	/**
	 * Returns the frequency for the timing values, in number of ticks per seconds.
	 *
	 * @return Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported.
	 */
	static uint64 GetTimingFrequency()
	{
		return GTimingFrequency;
	}

	typedef void (PlatformStaticInitialize)(void*);
	static void StaticInitialize(void* UserData, PlatformStaticInitialize* PlatformFunction)
	{
		if (!GAreGlobalsInitialized && PlatformFunction)
		{
			(*PlatformFunction)(UserData);

			if (GTimingFrequency != 0)
			{
				GIsSupported = true;
			}
			else
			{
				GIsSupported = false;
			}

			GAreGlobalsInitialized = true;
		}
	}

protected:
	/** Whether the static variables have been initialized. */
	static bool		GAreGlobalsInitialized;

	/** Whether GPU timing measurements are supported by the driver. */
	static bool		GIsSupported;

	/** Frequency for the timing values, in number of ticks per seconds, or 0 if the feature isn't supported. */
	static uint64	GTimingFrequency;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct RHI_API FGPUProfiler
{
	/** Whether we are currently tracking perf events or not. */
	bool bTrackingEvents;

	/** A latched version of GTriggerGPUProfile. This is a form of pseudo-thread safety. We read the value once a frame only. */
	bool bLatchedGProfilingGPU;

	/** A latched version of GTriggerGPUHitchProfile. This is a form of pseudo-thread safety. We read the value once a frame only. */
	bool bLatchedGProfilingGPUHitches;

	/** The previous latched version of GTriggerGPUHitchProfile.*/
	bool bPreviousLatchedGProfilingGPUHitches;

	/** Original state of GEmitDrawEvents before it was overridden for profiling. */
	bool bOriginalGEmitDrawEvents;

	/** GPU hitch profile history debounce...after a hitch, we just ignore frames for a while */
	int32 GPUHitchDebounce;

	/** Current perf event node frame. */
	FGPUProfilerEventNodeFrame* CurrentEventNodeFrame;

	/** Current perf event node. */
	FGPUProfilerEventNode* CurrentEventNode;

	int32 StackDepth;

	FGPUProfiler() :
		bTrackingEvents(false),
		bLatchedGProfilingGPU(false),
		bLatchedGProfilingGPUHitches(false),
		bPreviousLatchedGProfilingGPUHitches(false),
		bOriginalGEmitDrawEvents(false),
		GPUHitchDebounce(0),
		CurrentEventNodeFrame(NULL),
		CurrentEventNode(NULL),
		StackDepth(0)
	{
	}

	virtual ~FGPUProfiler()
	{
	}

	void RegisterGPUWork(uint32 NumPrimitives = 0, uint32 NumVertices = 0)
	{
		if (bTrackingEvents && CurrentEventNode)
		{
			check(IsInRenderingThread() || IsInRHIThread());
			CurrentEventNode->NumDraws++;
			CurrentEventNode->NumPrimitives += NumPrimitives;
			CurrentEventNode->NumVertices += NumVertices;
		}
	}

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent)
	{
		return new FGPUProfilerEventNode(InName, InParent);
	}

	virtual void PushEvent(const TCHAR* Name, FColor Color);
	virtual void PopEvent();
};



/**
 * Simple moving window averaged GPU timer; create an instance, call Begin() and End() around the block to time, 
 * then call GetElapsedAverage to get the averaged timings; BufferSize determines the number of queries in the window,
 * FramesBehind determines how long we wait to grab query results, so we don't have to block on them, so the effective
 * window size is BufferSize - FramesBehind
 * The timer keeps track of failed queries; GetElapsedAverage returns a float [0;1] that indicates fail rate: 0 means
 * no queries have failed, 1 means all queries within the window have failed. If more than 0.1, it's a good indication
 * that FramesBehind has to be increased.
 */
class FWindowedGPUTimer
{
public:
	explicit FWindowedGPUTimer(FRHICommandListImmediate& RHICmdList)
	{
		PrivateInit(10,2,RHICmdList);
	}

	FWindowedGPUTimer(uint32 BufferSize, uint32 FramesBehind, FRHICommandListImmediate& RHICmdList)
	{
		PrivateInit(BufferSize,FramesBehind,RHICmdList);
	}

	void Begin(FRHICommandListImmediate& RHICmdList)
	{
		RotateQueryBuffer(StartQueries);
		RHICmdList.EndRenderQuery(StartQueries[0]);
	}


	void End(FRHICommandListImmediate& RHICmdList)
	{
		RotateQueryBuffer(EndQueries);
		RHICmdList.EndRenderQuery(EndQueries[0]);
		QueriesFinished++;
	}

	void RotateQueryBuffer(TArray<FRenderQueryRHIRef> &QueryArray)
	{
		FRenderQueryRHIRef LastQuery = QueryArray.Last();

		for (int32 i = QueryArray.Num() - 1; i > 0; --i)
		{
			QueryArray[i] = QueryArray[i - 1];
		}
		QueryArray[0] = LastQuery;
	}


	float GetElapsedAverage(FRHICommandListImmediate& RHICmdList, float &OutAvgTimeInSeconds);

private:
	void PrivateInit(uint32 BufferSize, uint32 FramesBehind, FRHICommandListImmediate& RHICmdList)
	{
		QueriesFailed = 0;
		QueriesFinished = 0;
		StartQueries.AddZeroed(BufferSize);
		EndQueries.AddZeroed(BufferSize);
		for (uint32 i = 0; i < BufferSize; i++)
		{
			StartQueries[i] = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);;
			EndQueries[i] = RHICmdList.CreateRenderQuery(RQT_AbsoluteTime);;
		}
		WindowSize = BufferSize - FramesBehind;
	}

	int32 QueriesFailed;
	uint32 WindowSize;
	int32 QueriesFinished;
	TArray<FRenderQueryRHIRef> StartQueries;
	TArray<FRenderQueryRHIRef> EndQueries;
};
