// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/Exec.h"

enum
{
	// Default allocator alignment. If the default is specified, the allocator applies to engine rules.
	// Blocks >= 16 bytes will be 16-byte-aligned, Blocks < 16 will be 8-byte aligned. If the allocator does
	// not support allocation alignment, the alignment will be ignored.
	DEFAULT_ALIGNMENT = 0,

	// Minimum allocator alignment
	MIN_ALIGNMENT = 8,
};


/** The global memory allocator. */
CORE_API extern class FMalloc* GMalloc;
CORE_API extern class FMalloc** GFixedMallocLocationPtr;

/** Global FMallocProfiler variable to allow multiple malloc profilers to communicate. */
MALLOC_PROFILER( CORE_API extern class FMallocProfiler* GMallocProfiler; )

/** Holds generic memory stats, internally implemented as a map. */
struct FGenericMemoryStats;


/**
 * Inherit from FUseSystemMallocForNew if you want your objects to be placed in memory
 * alloced by the system malloc routines, bypassing GMalloc. This is e.g. used by FMalloc
 * itself.
 */
class CORE_API FUseSystemMallocForNew
{
public:
	/**
	 * Overloaded new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or nullptr
	 */
	void* operator new(size_t Size);

	/**
	 * Overloaded delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	void operator delete(void* Ptr);

	/**
	 * Overloaded array new operator using the system allocator.
	 *
	 * @param	Size	Amount of memory to allocate (in bytes)
	 * @return			A pointer to a block of memory with size Size or nullptr
	 */
	void* operator new[](size_t Size);

	/**
	 * Overloaded array delete operator using the system allocator
	 *
	 * @param	Ptr		Pointer to delete
	 */
	void operator delete[](void* Ptr);
};

/** The global memory allocator's interface. */
class CORE_API FMalloc  : 
	public FUseSystemMallocForNew,
	public FExec
{
public:
	/**
	 * Malloc
	 */
	virtual void* Malloc( SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT ) = 0;

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* Original, SIZE_T Count, uint32 Alignment=DEFAULT_ALIGNMENT ) = 0;

	/** 
	 * Free
	 */
	virtual void Free( void* Original ) = 0;
		
	/** 
	* For some allocators this will return the actual size that should be requested to eliminate
	* internal fragmentation. The return value will always be >= Count. This can be used to grow
	* and shrink containers to optimal sizes.
	* This call is always fast and threadsafe with no locking.
	*/
	virtual SIZE_T QuantizeSize(SIZE_T Count, uint32 Alignment)
	{
		return Count; // Default implementation has no way of determining this
	}

	/**
	* If possible determine the size of the memory allocated at the given address
	*
	* @param Original - Pointer to memory we are checking the size of
	* @param SizeOut - If possible, this value is set to the size of the passed in pointer
	* @return true if succeeded
	*/
	virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut)
	{
		return false; // Default implementation has no way of determining this
	}

	/**
	* Releases as much memory as possible. Must be called from the main thread.
	*/
	virtual void Trim()
	{
	}

	/**
	* Set up TLS caches on the current thread. These are the threads that we can trim.
	*/
	virtual void SetupTLSCachesOnCurrentThread()
	{
	}

	/**
	* Clears the TLS caches on the current thread and disables any future caching.
	*/
	virtual void ClearAndDisableTLSCachesOnCurrentThread()
	{
	}

	/**
	*	Initializes stats metadata. We need to do this as soon as possible, but cannot be done in the constructor
	*	due to the FName::StaticInit
	*/
	virtual void InitializeStatsMetadata();

	/**
	 * Handles any commands passed in on the command line
	 */
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{ 
		return false; 
	}

	/** Called once per frame, gathers and sets all memory allocator statistics into the corresponding stats. MUST BE THREAD SAFE. */
	virtual void UpdateStats();

	/** Writes allocator stats from the last update into the specified destination. */
	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats );

	/** Dumps current allocator stats to the log. */
	virtual void DumpAllocatorStats( class FOutputDevice& Ar )
	{
		Ar.Logf( TEXT("Allocator Stats for %s: (not implemented)" ), GetDescriptiveName() );
	}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual bool IsInternallyThreadSafe() const 
	{ 
		return false; 
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual bool ValidateHeap()
	{
		return( true );
	}

	/**
	 * Gets descriptive name for logging purposes.
	 *
	 * @return pointer to human-readable malloc name
	 */
	virtual const TCHAR* GetDescriptiveName()
	{
		return TEXT("Unspecified allocator");
	}

protected:
	friend struct FCurrentFrameCalls;

	/** Atomically increment total malloc calls. */
	FORCEINLINE void IncrementTotalMallocCalls()
	{
		FPlatformAtomics::InterlockedIncrement( (volatile int32*)&FMalloc::TotalMallocCalls );
	}

	/** Atomically increment total free calls. */
	FORCEINLINE void IncrementTotalFreeCalls()
	{
		FPlatformAtomics::InterlockedIncrement( (volatile int32*)&FMalloc::TotalFreeCalls );
	}

	/** Atomically increment total realloc calls. */
	FORCEINLINE void IncrementTotalReallocCalls()
	{
		FPlatformAtomics::InterlockedIncrement( (volatile int32*)&FMalloc::TotalReallocCalls );
	}

	/** Total number of calls Malloc, if implemented by derived class. */
	static uint32 TotalMallocCalls;
	/** Total number of calls Malloc, if implemented by derived class. */
	static uint32 TotalFreeCalls;
	/** Total number of calls Malloc, if implemented by derived class. */
	static uint32 TotalReallocCalls;
};


