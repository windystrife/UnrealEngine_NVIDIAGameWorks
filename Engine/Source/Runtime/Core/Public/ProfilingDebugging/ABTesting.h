// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
* Declarations for ABTesting framework.
*/

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/BitArray.h"
#include "Math/RandomStream.h"

#if !UE_BUILD_SHIPPING
#define ENABLE_ABTEST 1
#else
#define ENABLE_ABTEST 0
#endif

#include "ProfilingDebugging/ScopedTimers.h"

template<typename Allocator > class TBitArray;

#if ENABLE_ABTEST
class CORE_API FABTest
{
public:

	FABTest();
	
	//returns a command to execute, if any.
	const TCHAR* TickAndGetCommand();

	bool IsActive()
	{
		return bABTestActive;
	}

	void ReportScopeTime(double ScopeTime)
	{
		TotalScopeTimeInFrame += ScopeTime;
	}

	bool GetDoFirstScopeTest() const
	{
		return CurrentTest == 0;
	}

	static FABTest& Get();

	static void ABTestCmdFunc(const TArray<FString>& Args);

	static bool StaticIsActive()
	{
		return Get().bABTestActive;
	}

private:

	void StartFrameLog();
	void FrameLogTick(double Delta);
	
	void Start(FString* InABTestCmds, bool bScopeTest);
	void Stop();

	const TCHAR* SwitchTest(int32 Index);
	
	

	FRandomStream Stream;
	bool bABTestActive;
	bool bABScopeTestActive; //whether we are doing abtesting on the scope macro rather than a CVAR.	
	bool bFrameLog;
	FString ABTestCmds[2];
	int32 ABTestNumSamples;
	int32 RemainingCoolDown;
	int32 CurrentTest;
	int32 RemainingTrial;
	int32 RemainingPrint;
	int32 SampleIndex;

	int32 HistoryNum;
	int32 ReportNum;
	int32 CoolDown;
	int32 MinFramesPerTrial;
	int32 NumResamples;

	struct FSample
	{
		uint32 Micros;
		int32 TestIndex;
		TBitArray<> InResamples;
	};

	TArray<FSample> Samples;
	TArray<uint32> ResampleAccumulators;
	TArray<uint32> ResampleCount;
	uint32 Totals[2];
	uint32 Counts[2];

	double TotalTime;
	uint32 TotalFrames;
	uint32 Spikes;

	double TotalScopeTimeInFrame;
	uint64 LastGCFrame;

};

class CORE_API FScopedABTimer : public FDurationTimer
{
public:
	explicit FScopedABTimer() 
		: FDurationTimer(TimerData)
		, TimerData(0)
	{
		Start();
	}

	/** Dtor, updating seconds with time delta. */
	~FScopedABTimer()
	{
		Stop();
		FABTest::Get().ReportScopeTime(GetAccumulatedTime());
	}
private:
	double TimerData;
};

#define SCOPED_ABTEST() FScopedABTimer ABSCOPETIMER;
#define SCOPED_ABTEST_DOFIRSTTEST() FABTest::Get().GetDoFirstScopeTest()
#else

class CORE_API FABTest
{
public:
	static bool StaticIsActive()
	{
		return false;
	}
};

#define SCOPED_ABTEST(TimerName)
#define SCOPED_ABTEST_DOFIRSTTEST() true
#endif
