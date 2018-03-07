// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "Features/IModularFeature.h"


/**
 * FExternalProfiler
 *
 * Interface to various external profiler API functions, dynamically linked
 */
class CORE_API FExternalProfiler : public IModularFeature
{

public:

	/** Constructor */
	FExternalProfiler();

	/** Empty virtual destructor. */
	virtual ~FExternalProfiler()
	{
	}

	/** Pauses profiling. */
	void PauseProfiler();

	/** Resumes profiling. */
	void ResumeProfiler();

	/**
	 * Profiler interface.
	 */

	/** Mark where the profiler should consider the frame boundary to be. */
	virtual void FrameSync() = 0;

	/** Pauses profiling. */
	virtual void ProfilerPauseFunction() = 0;

	/** Resumes profiling. */
	virtual void ProfilerResumeFunction() = 0;

	/** Gets the name of this profiler as a string.  This is used to allow the user to select this profiler in a system configuration file or on the command-line */
	virtual const TCHAR* GetProfilerName() const = 0;

	/** @return Returns the name to use for any profiler registered as a modular feature usable by this system */
	static FName GetFeatureName();


private:

	/** Number of timers currently running. Timers are always 'global inclusive'. */
	int32 TimerCount;

	/** Whether or not profiling is currently paused (as far as we know.) */
	bool bIsPaused;

	/** Friend class so we can access the private members directly. */
	friend class FScopedExternalProfilerBase;
};

class FActiveExternalProfilerBase
{
public:	

	CORE_API static FExternalProfiler* GetActiveProfiler();


private:
	/** Static: True if we've tried to initialize a profiler already */
	static bool bDidInitialize;

	/** Static: Active profiler instance that we're using */
	static FExternalProfiler* ActiveProfiler;
};

/**
 * Base class for FScoped*Timer and FScoped*Excluder
 */
class FScopedExternalProfilerBase : public FActiveExternalProfilerBase
{
protected:
	/**
	 * Pauses or resumes profiler and keeps track of the prior state so it can be restored later.
	 *
	 * @param	bWantPause	true if this timer should 'include' code, or false to 'exclude' code
	 *
	 **/
	CORE_API void StartScopedTimer( const bool bWantPause );

	/** Stops the scoped timer and restores profiler to its prior state */
	CORE_API void StopScopedTimer();

private:
	/** Stores the previous 'Paused' state of VTune before this scope started */
	bool bWasPaused;
};


/**
 * FExternalProfilerIncluder
 *
 * Use this to include a body of code in profiler's captured session using 'Resume' and 'Pause' cues.  It
 * can safely be embedded within another 'timer' or 'excluder' scope.
 */
class FExternalProfilerIncluder : public FScopedExternalProfilerBase
{
public:
	/** Constructor */
	FExternalProfilerIncluder()
	{
		// 'Timer' scopes will always 'resume' VTune
		const bool bWantPause = false;
		StartScopedTimer( bWantPause );
	}

	/** Destructor */
	~FExternalProfilerIncluder()
	{
		StopScopedTimer();
	}
};


/**
 * FExternalProfilerExcluder
 *
 * Use this to EXCLUDE a body of code from profiler's captured session.  It can safely be embedded
 * within another 'timer' or 'excluder' scope.
 */
class FExternalProfilerExcluder : public FScopedExternalProfilerBase
{
public:
	/** Constructor */
	FExternalProfilerExcluder()
	{
		// 'Excluder' scopes will always 'pause' VTune
		const bool bWantPause = true;
		StartScopedTimer( bWantPause );
	}

	/** Destructor */
	~FExternalProfilerExcluder()
	{
		StopScopedTimer();
	}

};

#define SCOPE_PROFILER_INCLUDER(X) FExternalProfilerIncluder ExternalProfilerIncluder_##X;
#define SCOPE_PROFILER_EXCLUDER(X) FExternalProfilerExcluder ExternalProfilerExcluder_##X;
