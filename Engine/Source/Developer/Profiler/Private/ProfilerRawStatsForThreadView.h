// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/StatsFile.h"
#include "ProfilerStream.h"
#include "ProfilerSession.h"

class FRawProfilerSession
	: public FProfilerSession
{
	friend class FProfilerManager;
	friend class FProfilerActionManager;

	/** Profiler stream that contains all read raw profiler frames. */
	FProfilerStream ProfilerStream;

	/** Stats thread state, mostly used to manage the stats metadata. */
	FStatsLoadedState StatsThreadStats;
	FStatsReadStream Stream;

	/** Index of the last processed data for the mini-view. */
	int32 CurrentMiniViewFrame;

public:

	/**
	* Default constructor, creates a profiler session from a capture file.
	*/
	FRawProfilerSession(const FString& InRawStatsFileFileath);

	/** Destructor. */
	~FRawProfilerSession();

	/** Updates this profiler session. */
	bool HandleTicker(float DeltaTime);

	/** Starts a process of loading the raw stats file. */
	void PrepareLoading();

	const FProfilerStream& GetStream() const
	{
		return ProfilerStream;
	}

	/**
	 *	Process all stats packets and convert them to data accessible by the profiler.
	 *	Temporary version, will be optimized later.
	 */
	void ProcessStatPacketArray(const FStatPacketArray& PacketArray, FProfilerFrame& out_ProfilerFrame, int32 FrameIndex);
};
