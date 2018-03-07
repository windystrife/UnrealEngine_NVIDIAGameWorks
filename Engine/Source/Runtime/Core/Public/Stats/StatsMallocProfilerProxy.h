// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/MemoryBase.h"
#include "HAL/ThreadSafeCounter.h"

#if STATS

/**
 * Malloc proxy for gathering memory messages.
 */
class FStatsMallocProfilerProxy : public FMalloc
{
private:
	/** Malloc we're based on. */
	FMalloc* UsedMalloc;
	
	/** Whether the stats malloc profiler is enabled, disabled by default */
	bool bEnabled;
	
	/** True if the stats malloc profiler was enabled, we can't enable malloc profiler second time. */
	bool bWasEnabled;

	/** Memory sequence tag, used by the processing tool to indicate the order of allocations. */
	FThreadSafeCounter MemorySequenceTag;

	/** Malloc calls made this frame. */
	FThreadSafeCounter AllocPtrCalls;

	/** Realloc calls made this frame. */
	FThreadSafeCounter ReallocPtrCalls;

	/** Free calls made this frame. */
	FThreadSafeCounter FreePtrCalls;

public:
	/**
	 * Default constructor
	 * 
	 * @param	InMalloc - FMalloc that is going to be used for actual allocations
	 */
	FStatsMallocProfilerProxy( FMalloc* InMalloc);

	static CORE_API FStatsMallocProfilerProxy* Get();

	/**
	 * @return true, if the command line has the memory profiler token
	 */
	static CORE_API bool HasMemoryProfilerToken();

	void SetState( bool bNewState );

	bool GetState() const
	{
		return bEnabled;
	}

	virtual void InitializeStatsMetadata() override;

	/**
	 * Tracks malloc operation.
	 *
	 * @param	Ptr			- Allocated pointer 
	 * @param	Size		- Size of allocated pointer
	 * @param	SequenceTag - Previous value for the memory sequence tag
	 */
	void TrackAlloc( void* Ptr, int64 Size, int32 SequenceTag );

	/**
	 * Tracks free operation
	 *
	 * @param	Ptr	- Freed pointer
	 * @param	SequenceTag - Previous value for the memory sequence tag
	 */
	void TrackFree( void* Ptr, int32 SequenceTag );


	/** 
	 *	Tracks realloc operation
	 *	
	 * @param	Old - Previous pointer
	 * @param	New - New pointer
	 * @param	NewSize - Size of allocated pointer
	 * @param	SequenceTag - Previous value for the memory sequence tag
	 */
	void TrackRealloc( void* OldPtr, void* NewPtr, int64 NewSize, int32 SequenceTag );

	virtual void* Malloc( SIZE_T Size, uint32 Alignment ) override;
	virtual void* Realloc( void* OldPtr, SIZE_T NewSize, uint32 Alignment ) override;
	virtual void Free( void* Ptr ) override;

	virtual bool IsInternallyThreadSafe() const override
	{ 
		return UsedMalloc->IsInternallyThreadSafe(); 
	}

	virtual void UpdateStats() override;

	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats ) override
	{
		UsedMalloc->GetAllocatorStats( out_Stats );
	}

	virtual void DumpAllocatorStats( class FOutputDevice& Ar ) override
	{
		UsedMalloc->DumpAllocatorStats( Ar );
	}

	virtual bool ValidateHeap() override
	{
		return UsedMalloc->ValidateHeap();
	}

	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		return UsedMalloc->Exec( InWorld, Cmd, Ar);
	}

	virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut) override
	{
		return UsedMalloc->GetAllocationSize(Original,SizeOut);
	}

	virtual const TCHAR* GetDescriptiveName() override
	{ 
		return UsedMalloc->GetDescriptiveName(); 
	}
};


#endif //STATS
