// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/StatsFile.h"


/*-----------------------------------------------------------------------------
	Basic structures
-----------------------------------------------------------------------------*/

/** Simple allocation info. Assumes the sequence tag are unique and doesn't wrap. */
struct FAllocationInfo
{
	/** Old pointer, for Realloc. */
	uint64 OldPtr;

	/** Pointer, for Alloc, Realloc and Free. */
	uint64 Ptr;

	/** Size of the allocation. */
	int64 Size;
	
	/**
	 * Encoded callstack as FNames indices separated with +
	 * 123+45+56+76
	 */
	FName EncodedCallstack;
	
	/**
	 * Index of the memory operation. 
	 * Stats are handled with packets sent from different threads.
	 * This is used to sort the memory operations to be in the sequence order.
	 */
	uint32 SequenceTag;
	
	/** Memory operation, can be Alloc, Free or Realloc. */
	EMemoryOperation Op;
	
	/** If true, the callstack is broken. Missing Start/End scope. */
	bool bHasBrokenCallstack;

	/** Initialization constructor. */
	FAllocationInfo(
		uint64 InOldPtr,
		uint64 InPtr,
		int64 InSize,
		const TArray<FName>& InCallstack,
		uint32 InSequenceTag,
		EMemoryOperation InOp,
		bool bInHasBrokenCallstack
		);

	/** Copy constructor. */
	FAllocationInfo( const FAllocationInfo& Other );
};

/** Contains data about combined allocations for an unique scope. */
struct FCombinedAllocationInfo
{
	/** Size as MB. */
	double SizeMB;

	/** Allocations count. */
	int64 Count;

	/** Human readable callstack like 'GameThread -> Preinit -> LoadModule */
	FString HumanReadableCallstack;

	/** Encoded callstack like '45+656+6565'. */
	FName EncodedCallstack;

	/** Callstack indices, encoded callstack parsed into an array [45,656,6565] */
	TArray<FName> DecodedCallstack;

	/** Size for this node and children. */
	int64 Size;

	/** Default constructor. */
	FCombinedAllocationInfo()
		: SizeMB( 0 )
		, Count( 0 )
		, Size( 0 )
	{}

	/** Operator for adding another combined allocation. */
	FCombinedAllocationInfo& operator+=(const FCombinedAllocationInfo& Other)
	{
		if ((UPTRINT*)this != (UPTRINT*)&Other)
		{
			AddAllocation( Other.Size, Other.Count );
		}
		return *this;
	}

	/** Operator for subtracting another combined allocation. */
	FCombinedAllocationInfo& operator-=(const FCombinedAllocationInfo& Other)
	{
		if ((UPTRINT*)this != (UPTRINT*)&Other)
		{
			AddAllocation( -Other.Size, -Other.Count );
		}
		return *this;
	}

	/** Operator for adding a new allocation. */
	FCombinedAllocationInfo& operator+=(const FAllocationInfo& Other)
	{
		if ((UPTRINT*)this != (UPTRINT*)&Other)
		{
			AddAllocation( Other.Size, 1 );
		}
		return *this;
	}
	
	/**
	 * @return true, if the combined allocation is alive, the allocated data is larger than 0
	 */
	bool IsAlive()
	{
		return Size > 0;
	}

protected:
	/** Adds a new allocation. */
	void AddAllocation( int64 SizeToAdd, int64 CountToAdd = 1 )
	{
		Size += SizeToAdd;
		Count += CountToAdd;

		const double InvMB = 1.0 / 1024.0 / 1024.0;
		SizeMB = Size * InvMB;
	}
};

/** Contains data about node allocations, parent and children allocations. */
struct FNodeAllocationInfo
{
	/** Size as MBs for this node and children. */
	double SizeMB;

	/** Allocations count for this node and children. */
	int64 Count;

	/** Human readable callstack like 'GameThread -> Preinit -> LoadModule */
	FString HumanReadableCallstack;

	/** Encoded callstack like '45+656+6565'. */
	FName EncodedCallstack;

	/** Callstack indices, encoded callstack parsed into an array [45,656,6565] */
	TArray<FName> DecodedCallstack;

	/** Parent node. */
	FNodeAllocationInfo* Parent;

	/** Child nodes. */
	TMap<FName, FNodeAllocationInfo*> ChildNodes;

	/** Size for this node and children. */
	int64 Size;

	/** Node's depth. */
	int32 Depth;

	/** Default constructor. */
	FNodeAllocationInfo()
		: SizeMB( 0.0 )
		, Count( 0 )
		, Parent( nullptr )
		, Size( 0 )
		, Depth( 0 )
	{}

	/** Destructor. */
	~FNodeAllocationInfo()
	{
		DeleteAllChildrenNodes();
	}

	/** Accumulates this node with the other combined allocation. */
	void Accumulate( const FCombinedAllocationInfo& Other )
	{
		Size += Other.Size;
		Count += Other.Count;

		const double InvMB = 1.0 / 1024.0 / 1024.0;
		SizeMB = Size * InvMB;
	}

	/** Recursively sorts all children by size. */
	void SortBySize();

	/** Prepares all callstack data for this node. */
	void PrepareCallstackData( const TArray<FName>& InDecodedCallstack );

protected:
	/** Recursively deletes all children. */
	void DeleteAllChildrenNodes()
	{
		for (auto& It : ChildNodes)
		{
			delete It.Value;
		}
		ChildNodes.Empty();
	}
};

/*-----------------------------------------------------------------------------
	FRawStatsMemoryProfiler
-----------------------------------------------------------------------------*/

/** Class for managing the raw stats in the context of memory profiling. */
class FRawStatsMemoryProfiler : public FStatsReadFile
{
	friend struct FStatsReader<FRawStatsMemoryProfiler>;
	typedef FStatsReadFile Super;

protected:

	/** Initialization constructor. */
	FRawStatsMemoryProfiler( const TCHAR* InFilename );

	/** Called before started processing combined history. */
	virtual void PreProcessStats() override;

	/** Called after finished processing combined history. */
	virtual void PostProcessStats() override;

// 	/** Processes special message for advancing the stats frame from the game thread. */
// 	virtual void ProcessAdvanceFrameEventGameThreadOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
// 	/** Processes special message for advancing the stats frame from the render thread. */
// 	virtual void ProcessAdvanceFrameEventRenderThreadOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
// 	/** ProcessesIndicates begin of the cycle scope. */
// 	virtual void ProcessCycleScopeStartOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
// 	/** Indicates end of the cycle scope. */
// 	virtual void ProcessCycleScopeEndOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
	/** Processes special message marker used determine that we encountered a special data in the stat file. */
	virtual void ProcessSpecialMessageMarkerOperation( const FStatMessage& Message, const FStackState& StackState ) override;
// 
// 	/** Processes set operation. */
// 	virtual void ProcessSetOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
// 	/** Processes clear operation. */
// 	virtual void ProcessClearOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
// 	/** Processes add operation. */
// 	virtual void ProcessAddOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}
// 
// 	/** Processes subtract operation. */
// 	virtual void ProcessSubtractOperation( const FStatMessage& Message, const FStackState& Stack ) override
// 	{}

	/** Processes memory operation. @see EMemoryOperation. */
	virtual void ProcessMemoryOperation( EMemoryOperation MemOp, uint64 Ptr, uint64 NewPtr, int64 Size, uint32 SequenceTag, const FStackState& StackState ) override;

	/**
	 * Updates process stage progress periodically, does debug logging if enabled.
	 */
	void UpdateGenerateMemoryMapProgress( const int32 AllocationIndex );

	/**
	 * Sorts sequence allocations.
	 */
	void SortSequenceAllocations();

	/**
	 *	Generates allocation map and processes all memory snapshots .
	 */
	void GenerateAllocationMap();

	/** Processes an alloc memory operation. */
	void ProcessAlloc( const FAllocationInfo& AllocInfo, TMap<uint64, FAllocationInfo>& AllocationMap );

	/** Processes a free memory operation. */
	void ProcessFree( const FAllocationInfo& FreeInfo, TMap<uint64, FAllocationInfo>& AllocationMap, const bool bReallocFree );

	/** Dumps debug allocations. */
	void DumpDebugAllocations();

	/** Frees memory used by the debug information. */
	void FreeDebugInformation();

public:
	/** Generates UObject allocation statistics. */
	void ProcessAndDumpUObjectAllocations( const FName SnapshotName );

	void DumpScopedAllocations( const TCHAR* Name, const TMap<FString, FCombinedAllocationInfo>& CombinedAllocations );

	/** Generates callstack based allocation map. */
	void GenerateScopedAllocations( const TMap<uint64, FAllocationInfo>& InAllocationMap, TMap<FName, FCombinedAllocationInfo>& out_CombinedAllocations, uint64& TotalAllocatedMemory, uint64& NumAllocations );

	void GenerateScopedTreeAllocations( const TMap<FName, FCombinedAllocationInfo>& ScopedAllocations, FNodeAllocationInfo& out_Root );

	/** Prepares data for a snapshot. */
	void PrepareSnapshot( const FName SnapshotName, const TMap<uint64, FAllocationInfo>& InAllocationMap );

	/** Compare two snapshots and saves the result for further processing. */
	void CompareSnapshots( const FName BeginSnaphotName, const FName EndSnaphotName, TMap<FName, FCombinedAllocationInfo>& out_Result );

	/** Compare two snapshots and saves the result in human readable format, results can be saved to a file. */
	void CompareSnapshotsHumanReadable( const FName BeginSnaphotName, const FName EndSnaphotName, TMap<FString, FCombinedAllocationInfo>& out_Result );

	/**
	 * @return a set of all snapshot names.
	 */
	const TArray<FName> GetSnapshotNames() const
	{
		return SnapshotNamesArray;
	}

	/**
	 * @return Platform's name based on the loaded ue4statsraw file.
	 */
	const FString& GetPlatformName() const
	{
		return Header.PlatformName;
	}

	/**
	 * @return true, if the file contains any usable data.
	 */
	bool HasValidData() const
	{
		return SnapshotNamesArray.Num() >= 2;
	}

protected:
	/**
	 *	 All allocations ordered by the sequence tag.
	 *	 There is an assumption that the sequence tag will not turn-around.
	 */
	TArray<FAllocationInfo> SequenceAllocationArray;

	/** The sequence tag mapping to the named markers. */
	TArray<TPair<uint32, FName>> Snapshots;

	/** Unique snapshot names. */
	TSet<FName> SnapshotNamesSet;

	/** Unique snapshot names, the same as SnapshotNamesSet. */
	TArray<FName> SnapshotNamesArray;

	/** The sequence tag mapping to the named markers that need to processed. */
	TArray<TPair<uint32, FName>> SnapshotsToBeProcessed;

	/** Snapshots with allocation map. */
	TMap<FName, TMap<uint64, FAllocationInfo> > SnapshotsWithAllocationMap;

	/** Snapshots with callstack based allocation map. */
	TMap<FName, TMap<FName, FCombinedAllocationInfo> > SnapshotsWithScopedAllocations;

	TMap<FName, TMap<FString, FCombinedAllocationInfo> > SnapshotsWithDecodedScopedAllocations;

	/** Duplicated allocation map. Debug only. */
	TMultiMap<FString, FAllocationInfo> DuplicatedAllocMap;

	/** Zero allocation map. Debug only. */
	TMultiMap<FString, FAllocationInfo> ZeroAllocMap;

	/** Number of duplicated memory operations. */
	int32 NumDuplicatedMemoryOperations;

	/** Number of Malloc(0). */
	int32 NumZeroAllocs = 0;

	/** Number of memory operations. */
	int32 NumMemoryOperations;

	/** Last sequence tag for named marker. */
	int32 LastSequenceTagForNamedMarker;
};

