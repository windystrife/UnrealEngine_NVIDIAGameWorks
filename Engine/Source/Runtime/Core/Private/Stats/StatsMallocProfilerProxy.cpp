// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Stats/StatsMallocProfilerProxy.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"

#if STATS


/** Fake stat group and memory stats. */
DECLARE_STATS_GROUP( TEXT( "Memory Profiler" ), STATGROUP_MemoryProfiler, STATCAT_Advanced );

DECLARE_PTR_STAT( TEXT( "Memory Free Ptr" ), STAT_Memory_FreePtr, STATGROUP_MemoryProfiler );
DECLARE_PTR_STAT( TEXT( "Memory Alloc Ptr" ), STAT_Memory_AllocPtr, STATGROUP_MemoryProfiler );
DECLARE_PTR_STAT( TEXT( "Memory Realloc Ptr" ), STAT_Memory_ReallocPtr, STATGROUP_MemoryProfiler );
DECLARE_MEMORY_STAT( TEXT( "Memory Alloc Size" ), STAT_Memory_AllocSize, STATGROUP_MemoryProfiler );
DECLARE_MEMORY_STAT( TEXT( "Memory Operation Sequence Tag" ), STAT_Memory_OperationSequenceTag, STATGROUP_MemoryProfiler );
DECLARE_FNAME_STAT( TEXT( "Memory Snapshot" ), STAT_Memory_Snapshot, STATGROUP_MemoryProfiler );

/** Stats for memory usage by the profiler. */
DECLARE_DWORD_COUNTER_STAT( TEXT( "Profiler AllocPtr Calls" ), STAT_Memory_AllocPtr_Calls, STATGROUP_StatSystem );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Profiler ReallocPtr Calls" ), STAT_Memory_ReallocPtr_Calls, STATGROUP_StatSystem );
DECLARE_DWORD_COUNTER_STAT( TEXT( "Profiler FreePtr Calls" ), STAT_Memory_FreePtr_Calls, STATGROUP_StatSystem );
DECLARE_MEMORY_STAT( TEXT( "Profiler AllocPtr" ), STAT_Memory_AllocPtr_Mem, STATGROUP_StatSystem );
DECLARE_MEMORY_STAT( TEXT( "Profiler FreePtr" ), STAT_Memory_FreePtr_Mem, STATGROUP_StatSystem );

//#define DEBUG_MALLOC_PROXY

/** Debugging only. */
CORE_API FThreadStats* GThreadStatsToDumpMemory = nullptr;

FStatsMallocProfilerProxy::FStatsMallocProfilerProxy( FMalloc* InMalloc ) 
	: UsedMalloc( InMalloc )
	, bEnabled(false)
	, bWasEnabled(false)
{
}

FStatsMallocProfilerProxy* FStatsMallocProfilerProxy::Get()
{
	static FStatsMallocProfilerProxy* Instance = nullptr;
	if( Instance == nullptr )
	{
		Instance = new FStatsMallocProfilerProxy( GMalloc );
		// Initialize stats metadata.
		// We need to do here, after all hardcoded names have been initialized.
		Instance->InitializeStatsMetadata();
	}
	return Instance;
}

bool FStatsMallocProfilerProxy::HasMemoryProfilerToken()
{
	return FParse::Param( FCommandLine::Get(), TEXT( "MemoryProfiler" ) );
}

void FStatsMallocProfilerProxy::SetState( bool bNewState )
{
	if( !bWasEnabled && bNewState )
	{
		bEnabled = true;
		bWasEnabled = true;
		
		UE_LOG( LogStats, Warning, TEXT( "Malloc profiler is enabled" ) );
	}
	else if( bWasEnabled && !bNewState )
	{
		bEnabled = false;
		UE_LOG( LogStats, Warning, TEXT( "Malloc profiler has been disabled, all data should be ready" ) );
	}
	else if( bWasEnabled )
	{
		UE_LOG( LogStats, Warning, TEXT( "Malloc profiler has already been stopped and cannot be restarted." ) );
	}
	FPlatformMisc::MemoryBarrier();
}

void FStatsMallocProfilerProxy::InitializeStatsMetadata()
{
	UsedMalloc->InitializeStatsMetadata();

	// Initialize the memory messages metadata.
	// Malloc profiler proxy needs to be disabled otherwise it will hit infinite recursion in DoSetup.
	// Needs to be changed if we want to support boot time memory profiling.
	const FName NameAllocPtr = GET_STATFNAME(STAT_Memory_AllocPtr);
	const FName NameReallocPtr = GET_STATFNAME(STAT_Memory_ReallocPtr);
	const FName NameFreePtr = GET_STATFNAME(STAT_Memory_FreePtr);
	const FName NameAllocSize = GET_STATFNAME(STAT_Memory_AllocSize);
	const FName NameOperationSequenceTag = GET_STATFNAME(STAT_Memory_OperationSequenceTag);

	GET_STATFNAME(STAT_Memory_AllocPtr_Calls);
	GET_STATFNAME(STAT_Memory_ReallocPtr_Calls);
	GET_STATFNAME(STAT_Memory_FreePtr_Calls);

	GET_STATFNAME(STAT_Memory_AllocPtr_Mem);
	GET_STATFNAME(STAT_Memory_FreePtr_Mem);
}

void FStatsMallocProfilerProxy::TrackAlloc( void* Ptr, int64 Size, int32 SequenceTag )
{
	if( bEnabled )
	{
		FThreadStats* ThreadStats = FThreadStats::GetThreadStats();

#ifdef DEBUG_MALLOC_PROXY
		if (GThreadStatsToDumpMemory == ThreadStats && ThreadStats->MemoryMessageScope == 0)
		{
			ThreadStats->MemoryMessageScope++;
			UE_LOG( LogStats, Warning, TEXT( "TrackAlloc, %llu, %lli, %i, %i" ), (uint64)(UPTRINT)Ptr, Size, SequenceTag, ThreadStats->MemoryMessageScope );
			ThreadStats->MemoryMessageScope--;
		}
#endif // DEBUG_MALLOC_PROXY

		if (ThreadStats->MemoryMessageScope == 0)
		{
#if	UE_BUILD_DEBUG
			if (ThreadStats->Packet.StatMessages.Num() > 0 && ThreadStats->Packet.StatMessages.Num() % 32767 == 0)
			{
				ThreadStats->MemoryMessageScope++;
				const double InvMB = 1.0f / 1024.0f / 1024.0f;
				UE_LOG( LogStats, Verbose, TEXT( "ThreadID: %i, Current: %.1f" ), FPlatformTLS::GetCurrentThreadId(), InvMB*(int64)ThreadStats->Packet.StatMessages.Num()*sizeof( FStatMessage ) );
				ThreadStats->MemoryMessageScope--;
			}
#endif // UE_BUILD_DEBUG

			// 48 bytes per allocation.
			ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_AllocPtr ), (uint64)(UPTRINT)Ptr | (uint64)EMemoryOperation::Alloc );
			ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_AllocSize ), Size );
			ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_OperationSequenceTag ), (int64)SequenceTag );
			AllocPtrCalls.Increment();
		}
	}
}

void FStatsMallocProfilerProxy::TrackFree( void* Ptr, int32 SequenceTag )
{
	if( bEnabled )
	{
		if( Ptr != nullptr )
		{
			FThreadStats* ThreadStats = FThreadStats::GetThreadStats();

#ifdef DEBUG_MALLOC_PROXY
			if( GThreadStatsToDumpMemory == ThreadStats && ThreadStats->MemoryMessageScope == 0 )
			{
				ThreadStats->MemoryMessageScope++;
				UE_LOG(LogStats, Warning, TEXT("TrackFree, %llu, 0, %i, %i"), (uint64)(UPTRINT)Ptr, SequenceTag, ThreadStats->MemoryMessageScope);
				ThreadStats->MemoryMessageScope--;
			}
#endif // DEBUG_MALLOC_PROXY

			if( ThreadStats->MemoryMessageScope == 0 )
			{
				// 32 bytes per free.
				ThreadStats->AddMemoryMessage( GET_STATFNAME(STAT_Memory_FreePtr), (uint64)(UPTRINT)Ptr | (uint64)EMemoryOperation::Free );	// 16 bytes total				
				ThreadStats->AddMemoryMessage( GET_STATFNAME(STAT_Memory_OperationSequenceTag), (int64)SequenceTag );	
				FreePtrCalls.Increment();
			}
		}	
	}
}

void FStatsMallocProfilerProxy::TrackRealloc( void* OldPtr, void* NewPtr, int64 NewSize, int32 SequenceTag )
{
	if (bEnabled)
	{
		if (OldPtr != nullptr && NewSize > 0)
		{
			FThreadStats* ThreadStats = FThreadStats::GetThreadStats();
			if (ThreadStats->MemoryMessageScope == 0)
			{
				// 64 bytes per reallocation. 80 for Free/Alloc
				ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_FreePtr ), (uint64)(UPTRINT)OldPtr | (uint64)EMemoryOperation::Realloc );
				ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_AllocPtr ), (uint64)(UPTRINT)NewPtr | (uint64)EMemoryOperation::Realloc );
				ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_AllocSize ), NewSize );
				ThreadStats->AddMemoryMessage( GET_STATFNAME( STAT_Memory_OperationSequenceTag ), (int64)SequenceTag );
				ReallocPtrCalls.Increment();
			}
		}
		else if (OldPtr == nullptr)
		{
#if !PLATFORM_XBOXONE
			TrackAlloc( NewPtr, NewSize, SequenceTag );
#endif
		}
		else
		{
			TrackFree( OldPtr, SequenceTag );
		}
	}
}

void* FStatsMallocProfilerProxy::Malloc( SIZE_T Size, uint32 Alignment )
{
	void* Ptr = UsedMalloc->Malloc( Size, Alignment );
	const int32 SequenceTag = MemorySequenceTag.Increment();
	// We lose the Size's precision, but don't worry about it.
	TrackAlloc( Ptr, (int64)Size, SequenceTag );
	return Ptr;
}

void* FStatsMallocProfilerProxy::Realloc( void* OldPtr, SIZE_T NewSize, uint32 Alignment )
{
	void* NewPtr = UsedMalloc->Realloc( OldPtr, NewSize, Alignment );
	const int32 SequenceTag = MemorySequenceTag.Increment();
	TrackRealloc( OldPtr, NewPtr, (int64)NewSize, SequenceTag );
	return NewPtr;
}

void FStatsMallocProfilerProxy::Free( void* Ptr )
{
	const int32 SequenceTag = MemorySequenceTag.Increment();
	TrackFree( Ptr, SequenceTag );
	UsedMalloc->Free( Ptr );
}

void FStatsMallocProfilerProxy::UpdateStats()
{
	UsedMalloc->UpdateStats();

	if( bEnabled )
	{
		const int32 NumAllocPtrCalls = AllocPtrCalls.GetValue();
		const int32 NumReallocPtrCalls = ReallocPtrCalls.GetValue();
		const int32 NumFreePtrCalls = FreePtrCalls.GetValue();

		SET_DWORD_STAT( STAT_Memory_AllocPtr_Calls, NumAllocPtrCalls );
		SET_DWORD_STAT( STAT_Memory_ReallocPtr_Calls, NumReallocPtrCalls );
		SET_DWORD_STAT( STAT_Memory_FreePtr_Calls, NumFreePtrCalls );

		// AllocPtr, AllocSize
		SET_MEMORY_STAT( STAT_Memory_AllocPtr_Mem, NumAllocPtrCalls*(sizeof(FStatMessage)*2) );
		// FreePtr
		SET_MEMORY_STAT( STAT_Memory_FreePtr_Mem, NumFreePtrCalls*(sizeof(FStatMessage)*1) );

		AllocPtrCalls.Reset();
		ReallocPtrCalls.Reset();
		FreePtrCalls.Reset();
	}
}

#endif //STATS
