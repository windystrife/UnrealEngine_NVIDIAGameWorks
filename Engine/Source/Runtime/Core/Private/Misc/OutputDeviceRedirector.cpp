// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "Stats/Stats.h"
#include "Misc/CoreStats.h"

/*-----------------------------------------------------------------------------
	FOutputDeviceRedirector.
-----------------------------------------------------------------------------*/

/** Initialization constructor. */
FOutputDeviceRedirector::FOutputDeviceRedirector()
:	MasterThreadID(FPlatformTLS::GetCurrentThreadId())
,	bEnableBacklog(false)
{
}

FOutputDeviceRedirector* FOutputDeviceRedirector::Get()
{
	static FOutputDeviceRedirector Singleton;
	return &Singleton;
}

/**
 * Adds an output device to the chain of redirections.	
 *
 * @param OutputDevice	output device to add
 */
void FOutputDeviceRedirector::AddOutputDevice( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	if( OutputDevice )
	{
		OutputDevices.AddUnique( OutputDevice );
	}
}

/**
 * Removes an output device from the chain of redirections.	
 *
 * @param OutputDevice	output device to remove
 */
void FOutputDeviceRedirector::RemoveOutputDevice( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );
	OutputDevices.Remove( OutputDevice );
}

/**
 * Returns whether an output device is currently in the list of redirectors.
 *
 * @param	OutputDevice	output device to check the list against
 * @return	true if messages are currently redirected to the the passed in output device, false otherwise
 */
bool FOutputDeviceRedirector::IsRedirectingTo( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	return OutputDevices.Find( OutputDevice ) == INDEX_NONE ? false : true;
}

/**
 * The unsynchronized version of FlushThreadedLogs.
 * Assumes that the caller holds a lock on SynchronizationObject.
 */
void FOutputDeviceRedirector::UnsynchronizedFlushThreadedLogs( bool bUseAllDevices )
{
	for(int32 LineIndex = 0;LineIndex < BufferedLines.Num();LineIndex++)
	{
		FBufferedLine BufferedLine = BufferedLines[LineIndex];

		for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			FOutputDevice* OutputDevice = OutputDevices[OutputDeviceIndex];
			if( OutputDevice->CanBeUsedOnAnyThread() || bUseAllDevices )
			{
				OutputDevice->Serialize( *BufferedLine.Data, BufferedLine.Verbosity, BufferedLine.Category, BufferedLine.Time );
			}
		}
	}

	BufferedLines.Empty();
}

/**
 * Flushes lines buffered by secondary threads.
 */
void FOutputDeviceRedirector::FlushThreadedLogs()
{
	SCOPE_CYCLE_COUNTER(STAT_FlushThreadedLogs);
	// Acquire a lock on SynchronizationObject and call the unsynchronized worker function.
	FScopeLock ScopeLock( &SynchronizationObject );
	check(IsInGameThread());
	UnsynchronizedFlushThreadedLogs( true );
}

void FOutputDeviceRedirector::PanicFlushThreadedLogs()
{
//	SCOPE_CYCLE_COUNTER(STAT_FlushThreadedLogs);
	// Acquire a lock on SynchronizationObject and call the unsynchronized worker function.
	FScopeLock ScopeLock( &SynchronizationObject );
	
	// Flush threaded logs, but use the safe version.
	UnsynchronizedFlushThreadedLogs( false );

	// Flush devices.
	for (int32 OutputDeviceIndex = 0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++)
	{
		FOutputDevice* OutputDevice = OutputDevices[OutputDeviceIndex];
		if (OutputDevice->CanBeUsedOnAnyThread())
		{
			OutputDevice->Flush();
		}
	}

	BufferedLines.Empty();
}

/**
 * Serializes the current backlog to the specified output device.
 * @param OutputDevice	- Output device that will receive the current backlog
 */
void FOutputDeviceRedirector::SerializeBacklog( FOutputDevice* OutputDevice )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	for (int32 LineIndex = 0; LineIndex < BacklogLines.Num(); LineIndex++)
	{
		const FBufferedLine& BacklogLine = BacklogLines[ LineIndex ];
		OutputDevice->Serialize( *BacklogLine.Data, BacklogLine.Verbosity, BacklogLine.Category, BacklogLine.Time );
	}
}

/**
 * Enables or disables the backlog.
 * @param bEnable	- Starts saving a backlog if true, disables and discards any backlog if false
 */
void FOutputDeviceRedirector::EnableBacklog( bool bEnable )
{
	FScopeLock ScopeLock( &SynchronizationObject );

	bEnableBacklog = bEnable;
	if ( bEnableBacklog == false )
	{
		BacklogLines.Empty();
	}
}

/**
 * Sets the current thread to be the master thread that prints directly
 * (isn't queued up)
 */
void FOutputDeviceRedirector::SetCurrentThreadAsMasterThread()
{
	FScopeLock ScopeLock( &SynchronizationObject );

	// make sure anything queued up is flushed out, this may be called from a background thread, so use the safe version.
	UnsynchronizedFlushThreadedLogs( false );

	// set the current thread as the master thread
	MasterThreadID = FPlatformTLS::GetCurrentThreadId();
}

void FOutputDeviceRedirector::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category, const double Time )
{
	const double RealTime = Time == -1.0f ? FPlatformTime::Seconds() - GStartTime : Time;

	FScopeLock ScopeLock( &SynchronizationObject );

#if PLATFORM_DESKTOP
	// this is for errors which occur after shutdown we might be able to salvage information from stdout 
	if ((OutputDevices.Num() == 0)&& GIsRequestingExit)
	{
#if PLATFORM_WINDOWS
		_tprintf(_T("%s\n"), Data);
#else
		FGenericPlatformMisc::LocalPrint(Data);
		// printf("%s\n", TCHAR_TO_ANSI(Data));
#endif
		return;
	}
#endif


	if ( bEnableBacklog )
	{
		new(BacklogLines)FBufferedLine( Data, Category, Verbosity, RealTime );
	}

	if(FPlatformTLS::GetCurrentThreadId() != MasterThreadID || OutputDevices.Num() == 0)
	{
		new(BufferedLines)FBufferedLine( Data, Category, Verbosity, RealTime );
	}
	else
	{
		// Flush previously buffered lines from secondary threads.
		// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
		UnsynchronizedFlushThreadedLogs( true );

		for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices[OutputDeviceIndex]->Serialize( Data, Verbosity, Category, RealTime );
		}
	}
}

void FOutputDeviceRedirector::Serialize( const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	Serialize( Data, Verbosity, Category, -1.0 );
}

/**
 * Passes on the flush request to all current output devices.
 */
void FOutputDeviceRedirector::Flush()
{
	if(FPlatformTLS::GetCurrentThreadId() == MasterThreadID)
	{
		FScopeLock ScopeLock( &SynchronizationObject );

		// Flush previously buffered lines from secondary threads.
		// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
		UnsynchronizedFlushThreadedLogs( true );

		for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
		{
			OutputDevices[OutputDeviceIndex]->Flush();
		}
	}
}

/**
 * Closes output device and cleans up. This can't happen in the destructor
 * as we might have to call "delete" which cannot be done for static/ global
 * objects.
 */
void FOutputDeviceRedirector::TearDown()
{
	check(FPlatformTLS::GetCurrentThreadId() == MasterThreadID);

	FScopeLock ScopeLock( &SynchronizationObject );

	// Flush previously buffered lines from secondary threads.
	// Since we already hold a lock on SynchronizationObject, call the unsynchronized version.
	UnsynchronizedFlushThreadedLogs( false );

	for( int32 OutputDeviceIndex=0; OutputDeviceIndex<OutputDevices.Num(); OutputDeviceIndex++ )
	{
		OutputDevices[OutputDeviceIndex]->TearDown();
	}
	OutputDevices.Empty();
}

CORE_API FOutputDeviceRedirector* GetGlobalLogSingleton()
{
	return FOutputDeviceRedirector::Get();
}

