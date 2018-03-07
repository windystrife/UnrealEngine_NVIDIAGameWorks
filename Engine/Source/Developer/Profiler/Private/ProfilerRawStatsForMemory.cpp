// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerRawStatsForMemory.h"
#include "Stats/StatsMisc.h"
#include "ProfilingDebugging/DiagnosticTable.h"

/*-----------------------------------------------------------------------------
	Sort helpers
-----------------------------------------------------------------------------*/

/** Sorts allocations by size. */
struct FAllocationInfoSequenceTagLess
{
	FORCEINLINE bool operator()( const FAllocationInfo& A, const FAllocationInfo& B ) const
	{
		return A.SequenceTag < B.SequenceTag;
	}
};

/** Sorts allocations by size. */
struct FAllocationInfoSizeGreater
{
	FORCEINLINE bool operator()( const FAllocationInfo& A, const FAllocationInfo& B ) const
	{
		return B.Size < A.Size;
	}
};

/** Sorts combined allocations by size. */
struct FCombinedAllocationInfoSizeGreater
{
	FORCEINLINE bool operator()( const FCombinedAllocationInfo& A, const FCombinedAllocationInfo& B ) const
	{
		return B.Size < A.Size;
	}
};


/** Sorts node allocations by size. */
struct FNodeAllocationInfoSizeGreater
{
	FORCEINLINE bool operator()( const FNodeAllocationInfo& A, const FNodeAllocationInfo& B ) const
	{
		return B.Size < A.Size;
	}
};

/*-----------------------------------------------------------------------------
	Callstack decoding/encoding
-----------------------------------------------------------------------------*/

/** Helper struct used to manipulate stats based callstacks. */
struct FStatsCallstack
{
	/** Separator. */
	static const TCHAR* CallstackSeparator;

	/** Encodes decoded callstack a string, to be like '45+656+6565'. */
	static FString Encode( const TArray<FName>& Callstack )
	{
		FString Result;
		for (const auto& Name : Callstack)
		{
			Result += TTypeToString<int32>::ToString( (int32)Name.GetComparisonIndex() );
			Result += CallstackSeparator;
		}
		return Result;
	}

	/** Decodes encoded callstack to an array of FNames. */
	static void DecodeToNames( const FName& EncodedCallstack, TArray<FName>& out_DecodedCallstack )
	{
		TArray<FString> DecodedCallstack;
		DecodeToStrings( EncodedCallstack, DecodedCallstack );

		// Convert back to FNames
		for (const auto& It : DecodedCallstack)
		{
			NAME_INDEX NameIndex = 0;
			TTypeFromString<NAME_INDEX>::FromString( NameIndex, *It );
			const FName LongName = FName( NameIndex, NameIndex, 0 );

			out_DecodedCallstack.Add( LongName );
		}
	}

	/** Converts the encoded callstack into human readable callstack. */
	static FString GetHumanReadable( const FName& EncodedCallstack )
	{
		TArray<FName> DecodedCallstack;
		DecodeToNames( EncodedCallstack, DecodedCallstack );
		const FString Result = GetHumanReadable( DecodedCallstack );
		return Result;
	}

	/** Converts the encoded callstack into human readable callstack. */
	static FString GetHumanReadable( const TArray<FName>& DecodedCallstack )
	{
		FString Result;

		const int32 NumEntries = DecodedCallstack.Num();
		//for (int32 Index = DecodedCallstack.Num() - 1; Index >= 0; --Index)
		for (int32 Index = 0; Index < NumEntries; ++Index)
		{
			const FName LongName = DecodedCallstack[Index];
			const FString ShortName = FStatNameAndInfo::GetShortNameFrom( LongName ).ToString();
			//const FString Group = FStatNameAndInfo::GetGroupNameFrom( LongName ).ToString();
			FString Desc = FStatNameAndInfo::GetDescriptionFrom( LongName );
			Desc.TrimStartInline();

			if (Desc.Len() == 0)
			{
				Result += ShortName;
			}
			else
			{
				Result += Desc;
			}

			if (Index != NumEntries - 1)
			{
				Result += TEXT( " -> " );
			}
		}

		Result.ReplaceInline( TEXT( "STAT_" ), TEXT( "" ), ESearchCase::CaseSensitive );
		return Result;
	}

protected:
	/** Decodes encoded callstack to an array of strings. Where each string is the index of the FName. */
	static void DecodeToStrings( const FName& EncodedCallstack, TArray<FString>& out_DecodedCallstack )
	{
		EncodedCallstack.ToString().ParseIntoArray( out_DecodedCallstack, CallstackSeparator, true );
	}
};

const TCHAR* FStatsCallstack::CallstackSeparator = TEXT( "+" );

/*-----------------------------------------------------------------------------
	Allocation info
-----------------------------------------------------------------------------*/

FAllocationInfo::FAllocationInfo( uint64 InOldPtr, uint64 InPtr, int64 InSize, const TArray<FName>& InCallstack, uint32 InSequenceTag, EMemoryOperation InOp, bool bInHasBrokenCallstack ) 
	: OldPtr( InOldPtr )
	, Ptr( InPtr )
	, Size( InSize )
	, EncodedCallstack( *FStatsCallstack::Encode( InCallstack ) )
	, SequenceTag( InSequenceTag )
	, Op( InOp )
	, bHasBrokenCallstack( bInHasBrokenCallstack )
{

}

FAllocationInfo::FAllocationInfo( const FAllocationInfo& Other ) 
	: OldPtr( Other.OldPtr )
	, Ptr( Other.Ptr )
	, Size( Other.Size )
	, EncodedCallstack( Other.EncodedCallstack )
	, SequenceTag( Other.SequenceTag )
	, Op( Other.Op )
	, bHasBrokenCallstack( Other.bHasBrokenCallstack )
{

}

/*-----------------------------------------------------------------------------
	FNodeAllocationInfo
-----------------------------------------------------------------------------*/

void FNodeAllocationInfo::SortBySize()
{
	ChildNodes.ValueSort( FNodeAllocationInfoSizeGreater() );
	for (auto& It : ChildNodes)
	{
		It.Value->SortBySize();
	}
}


void FNodeAllocationInfo::PrepareCallstackData( const TArray<FName>& InDecodedCallstack )
{
	DecodedCallstack = InDecodedCallstack;
	EncodedCallstack = *FStatsCallstack::Encode( DecodedCallstack );
	HumanReadableCallstack = FStatsCallstack::GetHumanReadable( DecodedCallstack );
}

/*-----------------------------------------------------------------------------
	FRawStatsMemoryProfiler
-----------------------------------------------------------------------------*/

FRawStatsMemoryProfiler::FRawStatsMemoryProfiler( const TCHAR* InFilename )
	: FStatsReadFile( InFilename, true )
	, NumDuplicatedMemoryOperations( 0 )
	, NumMemoryOperations( 0 )
	, LastSequenceTagForNamedMarker( 0 )
{}

void FRawStatsMemoryProfiler::PreProcessStats()
{
	Super::PreProcessStats();

	// Begin marker.
	Snapshots.Emplace( LastSequenceTagForNamedMarker, TEXT( "BeginSnapshot" ) );
}


void FRawStatsMemoryProfiler::PostProcessStats()
{
	Super::PostProcessStats();

	const double StartTime = FPlatformTime::Seconds();

	if (!IsProcessingStopped())
	{
		SortSequenceAllocations();

		// End marker.
		Snapshots.Emplace( TNumericLimits<uint32>::Max(), TEXT( "EndSnapshot" ) );

		// Copy snapshots.
		SnapshotsToBeProcessed = Snapshots;

		UE_LOG( LogStats, Log, TEXT( "NumMemoryOperations:   %i" ), NumMemoryOperations );
		UE_LOG( LogStats, Log, TEXT( "SequenceAllocationNum: %i" ), SequenceAllocationArray.Num() );

		GenerateAllocationMap();
		DumpDebugAllocations();
	}

	if (!IsProcessingStopped())
	{
		StageProgress.Set( 100 );

		const double TotalTime = FPlatformTime::Seconds() - StartTime;
		UE_LOG( LogStats, Log, TEXT( "Post-Processing took %.2f sec(s)" ), TotalTime );
	}
	else
	{
		UE_LOG( LogStats, Warning, TEXT( "Post-Processing stopped, abandoning" ) );
	}
}

void FRawStatsMemoryProfiler::DumpDebugAllocations()
{
#if	UE_BUILD_DEBUG

	// Dump problematic allocations
	DuplicatedAllocMap.ValueSort( FAllocationInfoSizeGreater() );

	uint64 TotalDuplicatedMemory = 0;
	for (const auto& It : DuplicatedAllocMap)
	{
		const FAllocationInfo& Alloc = It.Value;
		TotalDuplicatedMemory += Alloc.Size;
	}

	UE_LOG( LogStats, Warning, TEXT( "Dumping duplicated alloc map" ) );
	UE_LOG( LogStats, Warning, TEXT( "TotalDuplicatedMemory: %llu bytes (%.2f MB)" ), TotalDuplicatedMemory, TotalDuplicatedMemory / 1024.0f / 1024.0f );
	const float MaxPctDisplayed = 0.9f;
	uint64 DisplayedSoFar = 0;
	for (const auto& It : DuplicatedAllocMap)
	{
		const FAllocationInfo& Alloc = It.Value;
		const FString& AllocCallstack = It.Key;
		UE_LOG( LogStats, Log, TEXT( "%lli (%.2f MB) %s" ), Alloc.Size, Alloc.Size / 1024.0f / 1024.0f, *AllocCallstack );

		DisplayedSoFar += Alloc.Size;

		const float CurrentPct = (float)DisplayedSoFar / (float)TotalDuplicatedMemory;
		if (CurrentPct > MaxPctDisplayed)
		{
			break;
		}
	}

#endif // UE_BUILD_DEBUG
}

void FRawStatsMemoryProfiler::FreeDebugInformation()
{
	DuplicatedAllocMap.Empty();
	ZeroAllocMap.Empty();
}

void FRawStatsMemoryProfiler::GenerateAllocationMap()
{
	/** Map of currently alive allocations. Ptr to AllocationInfo. */
	TMap<uint64, FAllocationInfo> AllocationMap;

	// Initialize the begin snapshot.
	auto BeginSnapshot = SnapshotsToBeProcessed[0];

	SnapshotsToBeProcessed.RemoveAt( 0 );
	PrepareSnapshot( BeginSnapshot.Value, AllocationMap );
	auto CurrentSnapshot = SnapshotsToBeProcessed[0];

	UE_LOG( LogStats, Log, TEXT( "Generating memory operations map" ) );

	const int32 NumSequenceAllocations = SequenceAllocationArray.Num();
	const int32 OnePercent = FMath::Max( NumSequenceAllocations / 100, 1024 );
	for (int32 AllocationIndex = 0; AllocationIndex < NumSequenceAllocations; AllocationIndex++)
	{
		if (AllocationIndex % OnePercent == 0)
		{
			UpdateGenerateMemoryMapProgress( AllocationIndex );
			if (IsProcessingStopped())
			{
				break;
			}
		}

		const FAllocationInfo& Alloc = SequenceAllocationArray[AllocationIndex];

		// Check named marker/snapshots
		if (Alloc.SequenceTag > CurrentSnapshot.Key)
		{
			SnapshotsToBeProcessed.RemoveAt( 0 );
			PrepareSnapshot( CurrentSnapshot.Value, AllocationMap );
			CurrentSnapshot = SnapshotsToBeProcessed[0];
		}

		if (Alloc.Op == EMemoryOperation::Alloc)
		{
			ProcessAlloc( Alloc, AllocationMap );
		}
		else if (Alloc.Op == EMemoryOperation::Realloc)
		{
			// Previous Alloc or Realloc
			if (Alloc.OldPtr != 0)
			{
				ProcessFree( Alloc, AllocationMap, true );
			}

#if	UE_BUILD_DEBUG
			if (Alloc.OldPtr == 0 && Alloc.Size == 0)
			{
				const FString ReallocCallstack = FStatsCallstack::GetHumanReadable( Alloc.EncodedCallstack );
				UE_LOG( LogStats, VeryVerbose, TEXT( "ReallocZero: %s %i %i/%i [%i]" ), *ReallocCallstack, Alloc.Size, Alloc.OldPtr, Alloc.Ptr, Alloc.SequenceTag );
			}
#endif // UE_BUILD_DEBUG

			if (Alloc.Ptr != 0)
			{
				ProcessAlloc( Alloc, AllocationMap );
			}			
		}
		else if (Alloc.Op == EMemoryOperation::Free)
		{
			ProcessFree( Alloc, AllocationMap, false );
		}
	}

	auto EndSnapshot = SnapshotsToBeProcessed[0];
	SnapshotsToBeProcessed.RemoveAt( 0 );
	PrepareSnapshot( EndSnapshot.Value, AllocationMap );

	// We don't need the allocation map. Each snapshot has its own copy.
	AllocationMap.Empty();

	SnapshotNamesArray = SnapshotNamesSet.Array();

	UE_LOG( LogStats, Verbose, TEXT( "NumDuplicatedMemoryOperations: %i" ), NumDuplicatedMemoryOperations );
	UE_LOG( LogStats, Verbose, TEXT( "NumZeroAllocs:                 %i" ), NumZeroAllocs );
}

void FRawStatsMemoryProfiler::ProcessAlloc( const FAllocationInfo& AllocInfo, TMap<uint64, FAllocationInfo>& AllocationMap )
{
	if( AllocInfo.Size == 0 )
	{
		NumZeroAllocs++;
		ZeroAllocMap.Add( FStatsCallstack::GetHumanReadable( AllocInfo.EncodedCallstack ), AllocInfo );
	}

	const FAllocationInfo* Found = AllocationMap.Find( AllocInfo.Ptr );
	if (!Found)
	{
		AllocationMap.Add( AllocInfo.Ptr, AllocInfo );
	}
	else
	{
		NumDuplicatedMemoryOperations++;

#if	UE_BUILD_DEBUG
		const FString FoundCallstack = FStatsCallstack::GetHumanReadable( Found->EncodedCallstack );
		const FString AllocCallstack = FStatsCallstack::GetHumanReadable( AllocInfo.EncodedCallstack );
		UE_LOG( LogStats, VeryVerbose, TEXT( "DuplicatedAlloc" ) );
		UE_LOG( LogStats, VeryVerbose, TEXT( "FoundCallstack: %s [%s]" ), *FoundCallstack, Found->Op==EMemoryOperation::Alloc ? TEXT("Alloc") : TEXT("Realloc") );
		UE_LOG( LogStats, VeryVerbose, TEXT( "AllocCallstack: %s [%s]" ), *AllocCallstack, AllocInfo.Op==EMemoryOperation::Alloc ? TEXT("Alloc") : TEXT("Realloc") );
		UE_LOG( LogStats, VeryVerbose, TEXT( "Size: %i/%i Ptr: %llu/%llu Tag: %i/%i" ), Found->Size, AllocInfo.Size, Found->Ptr, AllocInfo.Ptr, Found->SequenceTag, AllocInfo.SequenceTag );

		// Store the old pointer.
		DuplicatedAllocMap.Add( FoundCallstack, *Found );
#endif // UE_BUILD_DEBUG

		// Replace pointer.
		AllocationMap.Add( AllocInfo.Ptr, AllocInfo );
	}
}

void FRawStatsMemoryProfiler::ProcessFree( const FAllocationInfo& FreeInfo, TMap<uint64, FAllocationInfo>& AllocationMap, const bool bReallocFree )
{
	// bReallocFree is not needed here, but it's easier to read the code.
	const uint64 PtrToBeFreed = bReallocFree ? FreeInfo.OldPtr : FreeInfo.Ptr;
	const FAllocationInfo* Found = AllocationMap.Find( PtrToBeFreed );
	if (Found)
	{
		const bool bIsValid = FreeInfo.SequenceTag > Found->SequenceTag;
		if (!bIsValid)
		{
			UE_LOG( LogStats, Warning, TEXT( "InvalidFree Ptr: %llu, Seq: %i/%i" ), PtrToBeFreed, FreeInfo.SequenceTag, Found->SequenceTag );
		}
		AllocationMap.Remove( PtrToBeFreed );
	}
	else
	{
#if	UE_BUILD_DEBUG
		const FString FWACallstack = FStatsCallstack::GetHumanReadable( FreeInfo.EncodedCallstack );
		UE_LOG( LogStats, VeryVerbose, TEXT( "FreeWithoutAlloc: %s, %llu" ), *FWACallstack, PtrToBeFreed );
#endif // UE_BUILD_DEBUG
	}
}

void FRawStatsMemoryProfiler::UpdateGenerateMemoryMapProgress( const int32 AllocationIndex )
{
	const double CurrentSeconds = FPlatformTime::Seconds();
	if (CurrentSeconds > LastUpdateTime + NumSecondsBetweenUpdates)
	{
		const int32 PercentagePos = int32( 100.0*AllocationIndex / SequenceAllocationArray.Num() );
		StageProgress.Set( PercentagePos );
		UE_LOG( LogStats, Verbose, TEXT( "Processing allocations %3i%% (%10i/%10i)" ), PercentagePos, AllocationIndex, SequenceAllocationArray.Num() );
		LastUpdateTime = CurrentSeconds;
	}
	
	// Abandon support.
	if (bShouldStopProcessing == true)
	{
		SetProcessingStage( EStatsProcessingStage::SPS_Stopped );
	}
}

void FRawStatsMemoryProfiler::ProcessSpecialMessageMarkerOperation( const FStatMessage& Message, const FStackState& StackState )
{
	const FName RawName = Message.NameAndInfo.GetRawName();
	if (RawName == FStatConstants::RAW_NamedMarker)
	{
		const FName NamedMarker = Message.GetValue_FName();
		Snapshots.Emplace( LastSequenceTagForNamedMarker, NamedMarker );
	}
}

void FRawStatsMemoryProfiler::ProcessMemoryOperation( EMemoryOperation MemOp, uint64 Ptr, uint64 NewPtr, int64 Size, uint32 SequenceTag, const FStackState& StackState )
{
	if (MemOp == EMemoryOperation::Alloc)
	{
		NumMemoryOperations++;
	
		// Add a new allocation.
		SequenceAllocationArray.Add(
			FAllocationInfo(
			0,
			Ptr,
			Size,
			StackState.Stack,
			SequenceTag,
			EMemoryOperation::Alloc,
			StackState.bIsBrokenCallstack
			) );
		LastSequenceTagForNamedMarker = SequenceTag;

	}
	else if (MemOp == EMemoryOperation::Realloc)
	{
		NumMemoryOperations++;
		const uint64 OldPtr = Ptr;

		// Add a new realloc.
		SequenceAllocationArray.Add(
			FAllocationInfo(
			OldPtr,
			NewPtr,
			Size,
			StackState.Stack,
			SequenceTag,
			EMemoryOperation::Realloc,
			StackState.bIsBrokenCallstack
			) );
		LastSequenceTagForNamedMarker = SequenceTag;
	}
	else if (MemOp == EMemoryOperation::Free)
	{
		NumMemoryOperations++;

		// Add a new free.
		SequenceAllocationArray.Add(
			FAllocationInfo(
			0,
			Ptr,
			0,
			StackState.Stack,
			SequenceTag,
			EMemoryOperation::Free,
			StackState.bIsBrokenCallstack
			) );
	}
}

void FRawStatsMemoryProfiler::SortSequenceAllocations()
{
	FScopeLogTime SLT( TEXT( "SortSequenceAllocations" ), nullptr, FScopeLogTime::ScopeLog_Milliseconds );

	// Sort all memory operation by the sequence tag, iterate through all operation and generate memory usage.
	SequenceAllocationArray.Sort( FAllocationInfoSequenceTagLess() );

	// Abandon support.
	if (bShouldStopProcessing == true)
	{
		SetProcessingStage( EStatsProcessingStage::SPS_Stopped );
	}
}

void FRawStatsMemoryProfiler::GenerateScopedTreeAllocations( const TMap<FName, FCombinedAllocationInfo>& ScopedAllocations, FNodeAllocationInfo& out_Root )
{
	FScopeLogTime SLT( TEXT( "GenerateScopedTreeAllocations" ), nullptr, FScopeLogTime::ScopeLog_Milliseconds );

	// Decode all scoped allocations, generate tree for allocations and combine them.
	for (const auto& It : ScopedAllocations)
	{
		const FName& EncodedCallstack = It.Key;
		const FCombinedAllocationInfo& CombinedAllocation = It.Value;

		// Decode callstack.
		TArray<FName> DecodedCallstack;
		FStatsCallstack::DecodeToNames( EncodedCallstack, DecodedCallstack );

		const int32 AllocationLenght = DecodedCallstack.Num();
		check( DecodedCallstack.Num() > 0 );

		FNodeAllocationInfo* CurrentNode = &out_Root;
		// Accumulate with thread root node.
		CurrentNode->Accumulate( CombinedAllocation );

		// Iterate through the callstack and prepare all nodes if needed, and accumulate memory.
		TArray<FName> CurrentCallstack;
		const int32 NumEntries = DecodedCallstack.Num();
		for (int32 Idx1 = 0; Idx1 < NumEntries; ++Idx1)
		{
			const FName NodeName = DecodedCallstack[Idx1];
			CurrentCallstack.Add( NodeName );

			FNodeAllocationInfo* Node = nullptr;
			const bool bContainsNode = CurrentNode->ChildNodes.Contains( NodeName );
			if (!bContainsNode)
			{
				Node = new FNodeAllocationInfo;
				Node->Depth = Idx1;
				Node->PrepareCallstackData( CurrentCallstack );

				CurrentNode->ChildNodes.Add( NodeName, Node );
			}
			else
			{
				Node = CurrentNode->ChildNodes.FindChecked( NodeName );
			}

			// Accumulate memory usage and num allocations for all nodes in the callstack.
			Node->Accumulate( CombinedAllocation );

			// Move to the next node.
			Node->Parent = CurrentNode;
			CurrentNode = Node;
		}
	}

	out_Root.SortBySize();
}

void FRawStatsMemoryProfiler::ProcessAndDumpUObjectAllocations( const FName SnapshotName )
{
	if (!SnapshotsWithAllocationMap.Contains(SnapshotName))
	{
		UE_LOG( LogStats, Warning, TEXT( "Snapshot not found: %s" ), *SnapshotName.ToString() );
		return;
	}

	const TMap<uint64, FAllocationInfo>& AllocationMap = SnapshotsWithAllocationMap.FindChecked( SnapshotName );

	FScopeLogTime SLT( TEXT( "ProcessingUObjectAllocations" ), nullptr, FScopeLogTime::ScopeLog_Seconds );
	UE_LOG( LogStats, Warning, TEXT( "Processing UObject allocations" ) );

	const FString ReportName = FString::Printf( TEXT( "%s-Memory-UObject" ), *GetPlatformName() );
	FDiagnosticTableViewer MemoryReport( *FDiagnosticTableViewer::GetUniqueTemporaryFilePath( *ReportName ), true );

	// Write a row of headings for the table's columns.
	MemoryReport.AddColumn( TEXT( "Size (bytes)" ) );
	MemoryReport.AddColumn( TEXT( "Size (MB)" ) );
	MemoryReport.AddColumn( TEXT( "Count" ) );
	MemoryReport.AddColumn( TEXT( "UObject class" ) );
	MemoryReport.CycleRow();

	TMap<FName, FCombinedAllocationInfo> UObjectAllocations;

	// To minimize number of calls to expensive DecodeCallstack.
	TMap<FName, FName> UObjectCallstackToClassMapping;

	uint64 NumAllocations = 0;
	uint64 TotalAllocatedMemory = 0;
	for (const auto& It : AllocationMap)
	{
		const FAllocationInfo& Alloc = It.Value;

		FName UObjectClass = UObjectCallstackToClassMapping.FindRef( Alloc.EncodedCallstack );
		if (UObjectClass == NAME_None)
		{
			TArray<FName> DecodedCallstack;
			FStatsCallstack::DecodeToNames( Alloc.EncodedCallstack, DecodedCallstack );

			for (int32 Index = DecodedCallstack.Num() - 1; Index >= 0; --Index)
			{
				const FName LongName = DecodedCallstack[Index];
				const bool bValid = UObjectRawNames.Contains( LongName );
				if (bValid)
				{
					const FString ObjectName = FStatNameAndInfo::GetShortNameFrom( LongName ).GetPlainNameString();
					UObjectClass = *ObjectName.Left( ObjectName.Find( TEXT( "//" ) ) );;
					UObjectCallstackToClassMapping.Add( Alloc.EncodedCallstack, UObjectClass );
					break;
				}
			}
		}

		if (UObjectClass != NAME_None)
		{
			FCombinedAllocationInfo& CombinedAllocation = UObjectAllocations.FindOrAdd( UObjectClass );
			CombinedAllocation += Alloc;

			TotalAllocatedMemory += Alloc.Size;
			NumAllocations++;
		}
	}

	// Dump memory to the log.
	UObjectAllocations.ValueSort( FCombinedAllocationInfoSizeGreater() );

	const float MaxPctDisplayed = 0.90f;
	int32 CurrentIndex = 0;
	uint64 DisplayedSoFar = 0;
	UE_LOG( LogStats, VeryVerbose, TEXT( "Index, Size (Size MB), Count, UObject class" ) );
	for (const auto& It : UObjectAllocations)
	{
		const FCombinedAllocationInfo& CombinedAllocation = It.Value;
		const FName& UObjectClass = It.Key;

		UE_LOG( LogStats, VeryVerbose, TEXT( "%2i, %llu (%.2f MB), %llu, %s" ),
				CurrentIndex,
				CombinedAllocation.Size,
				CombinedAllocation.Size / 1024.0f / 1024.0f,
				CombinedAllocation.Count,
				*UObjectClass.GetPlainNameString() );

		// Dump stats
		MemoryReport.AddColumn( TEXT( "%llu" ), CombinedAllocation.Size );
		MemoryReport.AddColumn( TEXT( "%.2f MB" ), CombinedAllocation.Size / 1024.0f / 1024.0f );
		MemoryReport.AddColumn( TEXT( "%llu" ), CombinedAllocation.Count );
		MemoryReport.AddColumn( *UObjectClass.GetPlainNameString() );
		MemoryReport.CycleRow();

		CurrentIndex++;
		DisplayedSoFar += CombinedAllocation.Size;

		const float CurrentPct = (float)DisplayedSoFar / (float)TotalAllocatedMemory;
		if (CurrentPct > MaxPctDisplayed)
		{
			break;
		}
	}

	UE_LOG( LogStats, VeryVerbose, TEXT( "Allocated memory: %llu bytes (%.2f MB)" ), TotalAllocatedMemory, TotalAllocatedMemory / 1024.0f / 1024.0f );

	// Add a total row.
	MemoryReport.CycleRow();
	MemoryReport.CycleRow();
	MemoryReport.CycleRow();
	MemoryReport.AddColumn( TEXT( "%llu" ), TotalAllocatedMemory );
	MemoryReport.AddColumn( TEXT( "%.2f MB" ), TotalAllocatedMemory / 1024.0f / 1024.0f );
	MemoryReport.AddColumn( TEXT( "%llu" ), NumAllocations );
	MemoryReport.AddColumn( TEXT( "TOTAL" ) );
	MemoryReport.CycleRow();
}

void FRawStatsMemoryProfiler::DumpScopedAllocations( const TCHAR* Name, const TMap<FString, FCombinedAllocationInfo>& CombinedAllocations )
{
	if (CombinedAllocations.Num() == 0)
	{
		UE_LOG( LogStats, Warning, TEXT( "No scoped allocations: %s" ), Name );
		return;
	}

	FScopeLogTime SLT( TEXT( "ProcessingScopedAllocations" ), nullptr, FScopeLogTime::ScopeLog_Seconds );
	UE_LOG( LogStats, Warning, TEXT( "Dumping scoped allocations: %s" ), Name );

	const FString ReportName = FString::Printf( TEXT( "%s-Memory-Scoped-%s" ), *GetPlatformName(), Name );
	FDiagnosticTableViewer MemoryReport( *FDiagnosticTableViewer::GetUniqueTemporaryFilePath( *ReportName ), true );

	// Write a row of headings for the table's columns.
	MemoryReport.AddColumn( TEXT( "Size (bytes)" ) );
	MemoryReport.AddColumn( TEXT( "Size (MB)" ) );
	MemoryReport.AddColumn( TEXT( "Count" ) );
	MemoryReport.AddColumn( TEXT( "Callstack" ) );
	MemoryReport.CycleRow();

	FCombinedAllocationInfo Total;

	const float MaxPctDisplayed = 0.90f;
	int32 CurrentIndex = 0;
	UE_LOG( LogStats, VeryVerbose, TEXT( "Index, Size (Size MB), Count, Stat desc" ) );
	for (const auto& It : CombinedAllocations)
	{
		const FCombinedAllocationInfo& CombinedAllocation = It.Value;
		//const FName& EncodedCallstack = It.Key;
		const FString AllocCallstack = It.Key;// GetCallstack( EncodedCallstack );

		UE_LOG( LogStats, VeryVerbose, TEXT( "%2i, %llu (%.2f MB), %llu, %s" ),
				CurrentIndex,
				CombinedAllocation.Size,
				CombinedAllocation.Size / 1024.0f / 1024.0f,
				CombinedAllocation.Count,
				*AllocCallstack );

		// Dump stats
		MemoryReport.AddColumn( TEXT( "%llu" ), CombinedAllocation.Size );
		MemoryReport.AddColumn( TEXT( "%.2f MB" ), CombinedAllocation.Size / 1024.0f / 1024.0f );
		MemoryReport.AddColumn( TEXT( "%llu" ), CombinedAllocation.Count );
		MemoryReport.AddColumn( *AllocCallstack );
		MemoryReport.CycleRow();

		CurrentIndex++;
		Total += CombinedAllocation;
	}

	UE_LOG( LogStats, VeryVerbose, TEXT( "Allocated memory: %llu bytes (%.2f MB)" ), Total.Size, Total.SizeMB );

	// Add a total row.
	MemoryReport.CycleRow();
	MemoryReport.CycleRow();
	MemoryReport.CycleRow();
	MemoryReport.AddColumn( TEXT( "%llu" ), Total.Size );
	MemoryReport.AddColumn( TEXT( "%.2f MB" ), Total.SizeMB );
	MemoryReport.AddColumn( TEXT( "%llu" ), Total.Count );
	MemoryReport.AddColumn( TEXT( "TOTAL" ) );
	MemoryReport.CycleRow();
}

void FRawStatsMemoryProfiler::GenerateScopedAllocations( const TMap<uint64, FAllocationInfo>& InAllocationMap, TMap<FName, FCombinedAllocationInfo>& out_CombinedAllocations, uint64& TotalAllocatedMemory, uint64& NumAllocations )
{
	FScopeLogTime SLT( TEXT( "GenerateScopedAllocations" ), nullptr, FScopeLogTime::ScopeLog_Milliseconds );

	for (const auto& It : InAllocationMap)
	{
		const FAllocationInfo& Alloc = It.Value;
		FCombinedAllocationInfo& CombinedAllocation = out_CombinedAllocations.FindOrAdd( Alloc.EncodedCallstack );
		CombinedAllocation += Alloc;

		TotalAllocatedMemory += Alloc.Size;
		NumAllocations++;
	}

	// Sort by size.
	out_CombinedAllocations.ValueSort( FCombinedAllocationInfoSizeGreater() );
}

void FRawStatsMemoryProfiler::PrepareSnapshot( const FName SnapshotName, const TMap<uint64, FAllocationInfo>& InAllocationMap )
{
	FScopeLogTime SLT( TEXT( "PrepareSnapshot" ), nullptr, FScopeLogTime::ScopeLog_Milliseconds );

	// Make sure the snapshot name is unique.
	FName UniqueSnapshotName = SnapshotName;
	while (SnapshotNamesSet.Contains( UniqueSnapshotName ))
	{
		UniqueSnapshotName = FName( UniqueSnapshotName, UniqueSnapshotName.GetNumber() + 1 );
	}
	SnapshotNamesSet.Add( UniqueSnapshotName );

	SnapshotsWithAllocationMap.Add( UniqueSnapshotName, InAllocationMap );

	TMap<FName, FCombinedAllocationInfo> SnapshotCombinedAllocations;
	uint64 TotalAllocatedMemory = 0;
	uint64 NumAllocations = 0;
	GenerateScopedAllocations( InAllocationMap, SnapshotCombinedAllocations, TotalAllocatedMemory, NumAllocations );
	SnapshotsWithScopedAllocations.Add( UniqueSnapshotName, SnapshotCombinedAllocations );

	// Decode callstacks.
	// Replace encoded callstacks with human readable name. For easier debugging.
	TMap<FString, FCombinedAllocationInfo> SnapshotDecodedCombinedAllocations;
	for (auto& It : SnapshotCombinedAllocations)
	{
		const FString HumanReadableCallstack = FStatsCallstack::GetHumanReadable( It.Key );
		SnapshotDecodedCombinedAllocations.Add( HumanReadableCallstack, It.Value );
	}
	SnapshotsWithDecodedScopedAllocations.Add( UniqueSnapshotName, SnapshotDecodedCombinedAllocations );

	UE_LOG( LogStats, Warning, TEXT( "PrepareSnapshot: %s Alloc: %i Scoped: %i Total: %.2f MB" ), *UniqueSnapshotName.ToString(), InAllocationMap.Num(), SnapshotCombinedAllocations.Num(), TotalAllocatedMemory / 1024.0f / 1024.0f );
}

void FRawStatsMemoryProfiler::CompareSnapshots( const FName BeginSnaphotName, const FName EndSnaphotName, TMap<FName, FCombinedAllocationInfo>& out_Result )
{
	FScopeLogTime SLT( TEXT( "CompareSnapshots" ), nullptr, FScopeLogTime::ScopeLog_Milliseconds );

	const auto BeginSnaphotPtr = SnapshotsWithScopedAllocations.Find( BeginSnaphotName );
	const auto EndSnapshotPtr = SnapshotsWithScopedAllocations.Find( EndSnaphotName );
	if (BeginSnaphotPtr && EndSnapshotPtr)
	{
		// Process data.
		TMap<FName, FCombinedAllocationInfo> BeginSnaphot = *BeginSnaphotPtr;
		TMap<FName, FCombinedAllocationInfo> EndSnaphot = *EndSnapshotPtr;
		TMap<FName, FCombinedAllocationInfo> Result;

		for (const auto& It : EndSnaphot)
		{
			const FName Callstack = It.Key;
			const FCombinedAllocationInfo EndCombinedAlloc = It.Value;

			const FCombinedAllocationInfo* BeginCombinedAllocPtr = BeginSnaphot.Find( Callstack );
			if (BeginCombinedAllocPtr)
			{
				FCombinedAllocationInfo CombinedAllocation;
				CombinedAllocation += EndCombinedAlloc;
				CombinedAllocation -= *BeginCombinedAllocPtr;

				if (CombinedAllocation.IsAlive())
				{
					out_Result.Add( Callstack, CombinedAllocation );
				}
			}
			else
			{
				out_Result.Add( Callstack, EndCombinedAlloc );
			}
		}

		// Sort by size.
		out_Result.ValueSort( FCombinedAllocationInfoSizeGreater() );
	}
}

void FRawStatsMemoryProfiler::CompareSnapshotsHumanReadable( const FName BeginSnaphotName, const FName EndSnaphotName, TMap<FString, FCombinedAllocationInfo>& out_Result )
{
	FScopeLogTime SLT( TEXT( "CompareSnapshotsHumanReadable" ), nullptr, FScopeLogTime::ScopeLog_Milliseconds );

	const auto BeginSnaphotPtr = SnapshotsWithDecodedScopedAllocations.Find( BeginSnaphotName );
	const auto EndSnapshotPtr = SnapshotsWithDecodedScopedAllocations.Find( EndSnaphotName );
	if (BeginSnaphotPtr && EndSnapshotPtr)
	{
		// Process data.
		TMap<FString, FCombinedAllocationInfo> BeginSnaphot = *BeginSnaphotPtr;
		TMap<FString, FCombinedAllocationInfo> EndSnaphot = *EndSnapshotPtr;

		for (const auto& It : EndSnaphot)
		{
			const FString& Callstack = It.Key;
			const FCombinedAllocationInfo EndCombinedAlloc = It.Value;

			const FCombinedAllocationInfo* BeginCombinedAllocPtr = BeginSnaphot.Find( Callstack );
			if (BeginCombinedAllocPtr)
			{
				FCombinedAllocationInfo CombinedAllocation;
				CombinedAllocation += EndCombinedAlloc;
				CombinedAllocation -= *BeginCombinedAllocPtr;

				if (CombinedAllocation.IsAlive())
				{
					out_Result.Add( Callstack, CombinedAllocation );
				}
			}
			else
			{
				out_Result.Add( Callstack, EndCombinedAlloc );
			}
		}

		// Sort by size.
		out_Result.ValueSort( FCombinedAllocationInfoSizeGreater() );
	}
}
