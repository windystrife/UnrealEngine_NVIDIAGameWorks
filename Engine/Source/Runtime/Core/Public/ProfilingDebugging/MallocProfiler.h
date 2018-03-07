// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocProfiler.h: Memory profiling support.
=============================================================================*/
#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "HAL/MemoryBase.h"
#include "Serialization/Archive.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Misc/CompressedGrowableBuffer.h"

#if USE_MALLOC_PROFILER

#include "HAL/PlatformTLS.h"
#include "Misc/ScopeLock.h"

/*=============================================================================
	Malloc profiler enumerations
=============================================================================*/

/**
 * The lower 2 bits of a pointer are piggy-bagged to store what kind of data follows it. This enum lists
 * the possible types.
 */
enum EProfilingPayloadType
{
	TYPE_Malloc  = 0,
	TYPE_Free	 = 1,
	TYPE_Realloc = 2,
	TYPE_Other   = 3,
	// Don't add more than 4 values - we only have 2 bits to store this.
};

/**
 *  The the case of TYPE_Other, this enum determines the subtype of the token.
 */
enum EProfilingPayloadSubType
{
	// Core marker types

	/** Marker used to determine when malloc profiler stream has ended. */
	SUBTYPE_EndOfStreamMarker					= 0,

	/** Marker used to determine when we need to read data from the next file. */
	SUBTYPE_EndOfFileMarker						= 1,

	/** Marker used to determine when a snapshot has been added. */
	SUBTYPE_SnapshotMarker						= 2,

	/** Marker used to determine when a new frame has started. */
	SUBTYPE_FrameTimeMarker						= 3,

	// Markers from 4 to 20 are deprecated and have been removed.

	/// Version 3
	// Marker types for automatic snapshots.

	/** Marker used to determine when engine has started the cleaning process before loading a new level. */
	SUBTYPE_SnapshotMarker_LoadMap_Start		= 21,

	/** Marker used to determine when a new level has started loading. */
	SUBTYPE_SnapshotMarker_LoadMap_Mid			= 22,

	/** Marker used to determine when a new level has been loaded. */
	SUBTYPE_SnapshotMarker_LoadMap_End			= 23,

	/** Marker used to determine when garbage collection has started. */
    SUBTYPE_SnapshotMarker_GC_Start				= 24,

	/** Marker used to determine when garbage collection has finished. */
    SUBTYPE_SnapshotMarker_GC_End		        = 25,

	/** Marker used to determine when a new streaming level has been requested to load. */
	SUBTYPE_SnapshotMarker_LevelStream_Start	= 26,

	/** Marker used to determine when a previously streamed level has been made visible. */
	SUBTYPE_SnapshotMarker_LevelStream_End		= 27,

	/** Marker used to store a generic malloc statistics. */
	SUBTYPE_MemoryAllocationStats				= 31,

	/** Start licensee-specific subtypes from here. */
	SUBTYPE_LicenseeBase						= 50,

	/** Unknown the subtype of the token. */
	SUBTYPE_Unknown,
};

/** Whether we are performing symbol lookup at runtime or not.					*/
#define SERIALIZE_SYMBOL_INFO PLATFORM_SUPPORTS_STACK_SYMBOLS

/*=============================================================================
	CallStack address information.
=============================================================================*/

/**
 * Helper structure encapsulating a single address/ point in a callstack
 */
struct FCallStackAddressInfo
{
	/** Program counter address of entry.			*/
	uint64	ProgramCounter;
#if SERIALIZE_SYMBOL_INFO
	/** Index into name table for filename.			*/
	int32		FilenameNameTableIndex;
	/** Index into name table for function name.	*/
	int32		FunctionNameTableIndex;
	/** Line number in file.						*/
	int32		LineNumber;
#endif

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	AddressInfo	AddressInfo to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FCallStackAddressInfo AddressInfo )
	{
		Ar	<< AddressInfo.ProgramCounter
#if SERIALIZE_SYMBOL_INFO
			<< AddressInfo.FilenameNameTableIndex
			<< AddressInfo.FunctionNameTableIndex
			<< AddressInfo.LineNumber
#endif
			;
		return Ar;
	}
};

/*=============================================================================
	FMallocProfilerBufferedFileWriter
=============================================================================*/

/**
 * Special buffered file writer, used to serialize data before GFileManager is initialized.
 */
class FMallocProfilerBufferedFileWriter : public FArchive
{
public:
	/** Internal file writer used to serialize to HDD. */
	FArchive*		FileWriter;
	/** Buffered data being serialized before GameName has been set up. */
	TArray<uint8>	BufferedData;
	/** Timestamped filename with path.	*/
	FString		FullFilepath;
	/** Timestamped file path for the memory captures, just add extension. */
	FString			BaseFilePath;

	/**
	 * Constructor. Called before GMalloc is initialized!!!
	 */
	FMallocProfilerBufferedFileWriter();

	/**
	 * Destructor, cleaning up FileWriter.
	 */
	virtual ~FMallocProfilerBufferedFileWriter();

	//~ Begin FArchive Interface.
	virtual void Serialize( void* V, int64 Length );
	virtual void Seek( int64 InPos );
	virtual bool Close();
	virtual int64 Tell();

	/** Returns the allocated size for use in untracked memory calculations. */
	uint32 GetAllocatedSize();
};

/*=============================================================================
	FMallocProfiler
=============================================================================*/

/** This is an utility class that handles specific malloc profiler mutex locking. */
class FScopedMallocProfilerLock
{
	/* Non-copyable */
	FScopedMallocProfilerLock(const FScopedMallocProfilerLock&) = delete;
	FScopedMallocProfilerLock& operator=(const FScopedMallocProfilerLock&) = delete;

	FMallocProfiler& Profiler;

public:
	/** Constructor that performs a lock on the malloc profiler tracking methods. */
	explicit FScopedMallocProfilerLock(FMallocProfiler& InProfiler);

	/** Destructor that performs a release on the malloc profiler tracking methods. */
	~FScopedMallocProfilerLock();
};

/**
 * Memory profiling malloc, routing allocations to real malloc and writing information on all 
 * operations to a file for analysis by a standalone tool.
 */
class CORE_API FMallocProfiler : public FMalloc
{
	friend class	FMallocGcmProfiler;
	friend class	FScopedMallocProfilerLock;
	friend class	FMallocProfilerBufferedFileWriter;

protected:
	/** Malloc we're based on, aka using under the hood												*/
	FMalloc*								UsedMalloc;
	/** Whether or not EndProfiling()  has been Ended.  Once it has been ended it has ended most operations are no longer valid **/
	bool									bEndProfilingHasBeenCalled;
	/** Time malloc profiler was created. Used to convert arbitrary double time to relative float.	*/
	double									StartTime;
	/** File writer used to serialize data.															*/
	FMallocProfilerBufferedFileWriter		BufferedFileWriter;

	/** Critical section to sequence tracking.														*/
	FCriticalSection						CriticalSection;

	/** Mapping from program counter to index in program counter array.								*/
	TMap<uint64,int32>						ProgramCounterToIndexMap;
	/** Array of unique call stack address infos.													*/
	TArray<struct FCallStackAddressInfo>	CallStackAddressInfoArray;

	/** Mapping from callstack CRC to offset in call stack info buffer.								*/
	TMap<uint32,int32>							CRCToCallStackIndexMap;
	/** Buffer of unique call stack infos.															*/
	FCompressedGrowableBuffer				CallStackInfoBuffer;

	/** Mapping from a hash to an index in the tags array.											*/
	TMap<uint32, int32>						HashToTagTableIndexMap;
	/** Array of unique tags.																		*/
	TArray<FString>							TagsArray;

	/** Mapping from name to index in name array.													*/
	TMap<FString,int32>						NameToNameTableIndexMap;
	/** Array of unique names.																		*/
	TArray<FString>							NameArray;

	/** Whether the output file has been closed. */
	bool									bOutputFileClosed;

	/** Guards against recursive tracking of allocations caused by tracking */
	int32									TrackingDepth;

	/** Simple count of memory operations															*/
	uint64									MemoryOperationCount;

	/** Returns true if malloc profiler is outside one of the tracking methods, returns false otherwise. */
	bool IsOutsideTrackingFunction() const
	{
		return TrackingDepth == 0;
	}

	/** 
	 * Returns index of callstack, captured by this function into array of callstack infos. If not found, adds it.
	 *
	 * @return index into callstack array
	 */
	int32 GetCallStackIndex();

	/** 
	 * Returns index of the tags active on the thread making the allocation.
	 *
	 * @return index into tags array
	 */
	int32 GetTagsIndex();

	/**
	 * Returns index of passed in program counter into program counter array. If not found, adds it.
	 *
	 * @param	ProgramCounter	Program counter to find index for
	 * @return	Index of passed in program counter if it's not NULL, INDEX_NONE otherwise
	 */
	int32 GetProgramCounterIndex( uint64 ProgramCounter );

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	int32 GetNameTableIndex( const FString& Name );

	/**
	 * Returns index of passed in name into name array. If not found, adds it.
	 *
	 * @param	Name	Name to find index for
	 * @return	Index of passed in name
	 */
	int32 GetNameTableIndex( const FName& Name )
	{
		return GetNameTableIndex(Name.ToString());
	}

	/**
	 * Tracks malloc operation.
	 *
	 * @param	Ptr		Allocated pointer 
	 * @param	Size	Size of allocated pointer
	 */
	void TrackMalloc( void* Ptr, uint32 Size );
	
	/**
	 * Tracks free operation
	 *
	 * @param	Ptr		Freed pointer
	 */
	void TrackFree( void* Ptr );
	
	/**
	 * Tracks realloc operation
	 *
	 * @param	OldPtr	Previous pointer
	 * @param	NewPtr	New pointer
	 * @param	NewSize	New size of allocation
	 */
	void TrackRealloc( void* OldPtr, void* NewPtr, uint32 NewSize );

	/**
	 * Tracks memory allocations stats every 1024 memory operations. Used for time line view in memory profiler app.
	 * Expects to be inside of the critical section
	 */
	void TrackSpecialMemory();

	/**
	 * Ends profiling operation and closes file.
	 */
	void EndProfiling();

	/**
	 * Embeds token into stream to snapshot memory at this point.
	 */
	void SnapshotMemory(EProfilingPayloadSubType SubType, const FString& MarkerName);

	/**
	 * Embeds float token into stream (e.g. delta time).
	 */
	void EmbedFloatMarker(EProfilingPayloadSubType SubType, float DeltaTime);

	/**
	 * Embeds token into stream to snapshot memory at this point.
	 */
	void EmbedDwordMarker(EProfilingPayloadSubType SubType, uint32 Info);

	/** 
	 * Writes additional memory stats for a snapshot like memory allocations stats, list of all loaded levels and platform dependent memory metrics.
	 */
	void WriteAdditionalSnapshotMemoryStats();

	/** 
	 * Writes memory allocations stats. 
	 */
	void WriteMemoryAllocationStats();

	/** 
	 * Writes names of currently loaded levels. 
	 * Only to be called from within the mutex / scope lock of the FMallocProifler.
	 *
	 * @param	InWorld		World Context.
	 */
	virtual void WriteLoadedLevels( UWorld* InWorld );

	/** 
	 * Gather texture memory stats. 
	 */
	virtual void GetTexturePoolSize( FGenericMemoryStats& out_Stats );

	/** 
		Added for untracked memory calculation
		Note that this includes all the memory used by dependent malloc profilers, such as FMallocGcmProfiler,
		so they don't need their own version of this function. 
	*/
	int32 CalculateMemoryProfilingOverhead();

public:
	/** Snapshot taken when engine has started the cleaning process before loading a new level. */
	static void SnapshotMemoryLoadMapStart(const FString& MapName);

	/** Snapshot taken when a new level has started loading. */
	static void SnapshotMemoryLoadMapMid(const FString& MapName);

	/** Snapshot taken when a new level has been loaded. */
	static void SnapshotMemoryLoadMapEnd(const FString& MapName);

	/** Snapshot taken when garbage collection has started. */
	static void SnapshotMemoryGCStart();

	/** Snapshot taken when garbage collection has finished. */
	static void SnapshotMemoryGCEnd();

	/** Snapshot taken when a new streaming level has been requested to load. */
	static void SnapshotMemoryLevelStreamStart(const FString& LevelName);

	/** Snapshot taken when a previously  streamed level has been made visible. */
	static void SnapshotMemoryLevelStreamEnd(const FString& LevelName);

	/** Returns malloc we're based on. */
	virtual FMalloc* GetInnermostMalloc()
	{ 
		return UsedMalloc;
	}

	/** 
	 * This function is intended to be called when a fatal error has occurred inside the allocator and
	 * you want to dump the current mprof before crashing, so that it can be used to help debug the error.
	 * IMPORTANT: This function assumes that this thread already has the allocator mutex.. 
	 */
	void PanicDump(EProfilingPayloadType FailedOperation, void* Ptr1, void* Ptr2);

	/**
	 * Constructor, initializing all member variables and potentially loading symbols.
	 *
	 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
	 */
	FMallocProfiler(FMalloc* InMalloc);

	/**
	 * Begins profiling operation and opens file.
	 */
	void BeginProfiling();

	/** 
	 * Adds a tag to the list of tags associated with the calling thread.
	 * @note Tags are ref-counted so calls to Add and Remove must be paired correctly.
	 */
	void AddTag(const FName InTag);

	/** 
	 * Removes a tag from the list of tags associated with the calling thread.
	 * @note Tags are ref-counted so calls to Add and Remove must be paired correctly.
	 */
	void RemoveTag(const FName InTag);

	/** 
	 * Malloc
	 */
	virtual void* Malloc( SIZE_T Size, uint32 Alignment ) override
	{
		if (Size == 0)
		{
			return nullptr;
		}

		FScopeLock Lock( &CriticalSection );
		void* Ptr = UsedMalloc->Malloc( Size, Alignment );

		if (IsOutsideTrackingFunction())
		{
			FScopedMallocProfilerLock MallocProfilerLock(*this);
			TrackMalloc( Ptr, (uint32)Size );
			TrackSpecialMemory();
		}

		return Ptr;
	}

	/** 
	 * Realloc
	 */
	virtual void* Realloc( void* OldPtr, SIZE_T NewSize, uint32 Alignment ) override
	{
		FScopeLock Lock( &CriticalSection );
		void* NewPtr = UsedMalloc->Realloc( OldPtr, NewSize, Alignment );

		if (IsOutsideTrackingFunction())
		{
			FScopedMallocProfilerLock MallocProfilerLock(*this);
			TrackRealloc( OldPtr, NewPtr, (uint32)NewSize );
			TrackSpecialMemory();
		}

		return NewPtr;
	}

	/** 
	 * Free
	 */
	virtual void Free( void* Ptr ) override
	{
		if (Ptr == nullptr)
		{
			return;
		}
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->Free( Ptr );

		if (IsOutsideTrackingFunction())
		{
			FScopedMallocProfilerLock MallocProfilerLock(*this);
			TrackFree( Ptr );
			TrackSpecialMemory();
		}
	}

	/**
	 * Returns if the allocator is guaranteed to be thread-safe and therefore
	 * doesn't need a unnecessary thread-safety wrapper around it.
	 */
	virtual bool IsInternallyThreadSafe() const override
	{ 
		return true; 
	}

	/** Called once per frame, gathers and sets all memory allocator statistics into the corresponding stats. MUST BE THREAD SAFE. */
	virtual void UpdateStats() override
	{
		UsedMalloc->UpdateStats();
	}

	/** Writes allocator stats from the last update into the specified destination. */
	virtual void GetAllocatorStats( FGenericMemoryStats& out_Stats ) override
	{
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->GetAllocatorStats( out_Stats );
	}

	/** Dumps allocator stats to an output device. */
	virtual void DumpAllocatorStats( class FOutputDevice& Ar ) override
	{
		FScopeLock Lock( &CriticalSection );
		UsedMalloc->DumpAllocatorStats( Ar );
	}

	/**
	 * Validates the allocator's heap
	 */
	virtual bool ValidateHeap() override
	{
		FScopeLock Lock( &CriticalSection );
		return( UsedMalloc->ValidateHeap() );
	}

	/**
	* If possible determine the size of the memory allocated at the given address
	*
	* @param Original - Pointer to memory we are checking the size of
	* @param SizeOut - If possible, this value is set to the size of the passed in pointer
	* @return true if succeeded
	*/
	virtual bool GetAllocationSize(void *Original, SIZE_T &SizeOut) override
	{
		FScopeLock Lock( &CriticalSection );
		return UsedMalloc->GetAllocationSize(Original,SizeOut);
	}

	
	//~ Begin Exec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar ) override;
	//~ End Exec Interface

	/** 
	 * Exec command handlers
	 */
	bool HandleMProfCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleDumpAllocsToFileCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleSnapshotMemoryCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool HandleSnapshotMemoryFrameCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	virtual const TCHAR* GetDescriptiveName() override
	{ 
		FScopeLock ScopeLock( &CriticalSection );
		check(UsedMalloc);
		return UsedMalloc->GetDescriptiveName(); 
	}
};

#endif //USE_MALLOC_PROFILER

