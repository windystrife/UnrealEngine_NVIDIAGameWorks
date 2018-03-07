// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

/**
 * Thread heartbeat check class.
 * Used by crash handling code to check for hangs.
 */
class CORE_API FThreadHeartBeat : public FRunnable
{
	static FThreadHeartBeat* Singleton;

	/** Holds per-thread info about the heartbeat */
	struct FHeartBeatInfo
	{
		FHeartBeatInfo()
			: LastHeartBeatTime(0.0)
			, SuspendedCount(0)
		{}

		/** Time we last received a heartbeat for the current thread */
		double LastHeartBeatTime;
		/** Suspended counter */
		int32 SuspendedCount;
	};
	/** Thread to run the worker FRunnable on */
	FRunnableThread* Thread;
	/** Stops this thread */
	FThreadSafeCounter StopTaskCounter;
	/** Synch object for the heartbeat */
	FCriticalSection HeartBeatCritical;
	/** Keeps track of the last heartbeat time for threads */
	TMap<uint32, FHeartBeatInfo> ThreadHeartBeat;
	/** True if heartbeat should be measured */
	FThreadSafeBool bReadyToCheckHeartbeat;
	/** Max time the thread is allowed to not send the heartbeat*/
	double HangDuration;

	/** CRC of the last hang's callstack */
	uint32 LastHangCallstackCRC;
	/** Id of the last thread that hung */
	uint32 LastHungThreadId;

	FThreadHeartBeat();
	virtual ~FThreadHeartBeat();

public:

	enum EConstants
	{
		/** Invalid thread Id used by CheckHeartBeat */
		InvalidThreadId = (uint32)-1
	};

	/** Gets the heartbeat singleton */
	static FThreadHeartBeat& Get();
	static FThreadHeartBeat* GetNoInit();

	/** Begin measuring heartbeat */
	void Start();
	/** Called from a thread once per frame to update the heartbeat time */
	void HeartBeat();
	/** Called by a supervising thread to check the threads' health */
	uint32 CheckHeartBeat();
	/** Called by a thread when it's no longer expecting to be ticked */
	void KillHeartBeat();
	/** 
	 * Suspend heartbeat measuring for the current thread if the thread has already had a heartbeat 
	 */
	void SuspendHeartBeat();
	/** 
	 * Resume heartbeat measuring for the current thread 
	 */
	void ResumeHeartBeat();

	/**
	* Returns true/false if this thread is currently performing heartbeat monitoring
	*/
	bool IsBeating();

	//~ Begin FRunnable Interface.
	virtual bool Init();
	virtual uint32 Run();
	virtual void Stop();
	//~ End FRunnable Interface
};

/** Suspends heartbeat measuring for the current thread in the current scope */
struct FSlowHeartBeatScope
{
	FORCEINLINE FSlowHeartBeatScope()
	{
		FThreadHeartBeat::Get().SuspendHeartBeat();
	}
	FORCEINLINE ~FSlowHeartBeatScope()
	{
		FThreadHeartBeat::Get().ResumeHeartBeat();
	}
};
