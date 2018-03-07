// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HAL/IPlatformFileProfilerWrapper.h"
#include "Stats/Stats.h"
#include "Containers/Ticker.h"

#if !UE_BUILD_SHIPPING

bool bSuppressProfiledFileLog = false;
DEFINE_LOG_CATEGORY(LogProfiledFile);

DECLARE_STATS_GROUP(TEXT("File Stats"), STATGROUP_FileStats, STATCAT_Advanced);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Read Speed MB/s"), STAT_ReadSpeedMBs, STATGROUP_FileStats);
DECLARE_DWORD_COUNTER_STAT(TEXT("Read Calls"), STAT_ReadIssued, STATGROUP_FileStats);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Read Size KB"), STAT_ReadSize, STATGROUP_FileStats);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Lifetime Average Read Size KB"), STAT_LTAvgReadSize, STATGROUP_FileStats);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Lifetime Average Read Speed MB/s"), STAT_LTAvgReadSpeed, STATGROUP_FileStats);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Total MBs Read"), STAT_TotalMBRead, STATGROUP_FileStats);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Total File Read Calls"), STAT_TotalReadCalls, STATGROUP_FileStats);

bool FPlatformFileReadStatsHandle::Read( uint8* Destination, int64 BytesToRead )
{
	double Timer = FPlatformTime::Seconds();
	bool Result = FileHandle->Read(Destination, BytesToRead);
	float Delta = FPlatformTime::Seconds()-Timer;
	if (Delta > SMALL_NUMBER)
	{
		float BytesPerSec = (BytesToRead/1024.0f)/Delta;
		FPlatformAtomics::InterlockedAdd(BytesPerSecCounter, (int32)(BytesPerSec));
	}
	FPlatformAtomics::InterlockedAdd(BytesReadCounter, BytesToRead);
	FPlatformAtomics::InterlockedIncrement(ReadsCounter);
	return Result;
}

bool FPlatformFileReadStats::Tick( float Delta )
{
	float RealDelta = (float)(FPlatformTime::Seconds()-Timer);
	uint32 BytesPerSec = FPlatformAtomics::InterlockedExchange(&BytePerSecThisTick, 0);
	uint32 BytesReadTick = FPlatformAtomics::InterlockedExchange(&BytesReadThisTick, 0);
	uint32 Reads = FPlatformAtomics::InterlockedExchange(&ReadsThisTick, 0);

	uint64 ReadKBs = 0;
	float  ReadSize = 0.f;
	if (Reads)
	{
		LifetimeReadCalls += Reads;
		ReadKBs = BytesPerSec / Reads;
		ReadSize = BytesReadTick / (float)Reads;
		LifetimeReadSpeed += BytesPerSec;
		LifetimeReadSize += BytesReadTick/1024.f;

		SET_FLOAT_STAT(STAT_LTAvgReadSize, (LifetimeReadSize/LifetimeReadCalls));
		SET_FLOAT_STAT(STAT_LTAvgReadSpeed, (LifetimeReadSpeed/LifetimeReadCalls)/1024.f);
	}

	SET_FLOAT_STAT(STAT_ReadSpeedMBs, ReadKBs/1024.f);
	SET_FLOAT_STAT(STAT_ReadSize, ReadSize/1024.f);
	SET_DWORD_STAT(STAT_ReadIssued, Reads);
	INC_FLOAT_STAT_BY(STAT_TotalMBRead,(BytesReadTick / (1024.f*1024.f)));
	INC_DWORD_STAT_BY(STAT_TotalReadCalls, Reads);


	Timer = FPlatformTime::Seconds();
	return true;
}

// Wrapper function prevents Android Clang ICE (v3.3)
FORCENOINLINE void ExchangeNoInline( volatile int32* Value )
{
	FPlatformAtomics::InterlockedExchange(Value, 0);
}

bool FPlatformFileReadStats::Initialize( IPlatformFile* Inner, const TCHAR* CommandLineParam )
{
	// Inner is required.
	check(Inner != NULL);
	LowerLevel = Inner;
	FTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FPlatformFileReadStats::Tick), 0.f);

	LifetimeReadSpeed = 0;
	LifetimeReadSize = 0;
	LifetimeReadCalls = 0;
	Timer = 0.f;
	ExchangeNoInline(&BytesReadThisTick);
	ExchangeNoInline(&ReadsThisTick);
	ExchangeNoInline(&BytePerSecThisTick);
	
	return !!LowerLevel;
}

#endif // !UE_BUILD_SHIPPING
