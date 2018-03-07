// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MallocProfiler.cpp: Memory profiling support.
=============================================================================*/

#include "ProfilingDebugging/MallocProfiler.h"
#include "Misc/DateTime.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/TlsAutoCleanup.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "ProfilingDebugging/ProfilingHelpers.h"

#if USE_MALLOC_PROFILER

#include "ModuleManager.h"
#include "MemoryMisc.h"
#include "HAL/PlatformStackWalk.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"


CORE_API FMallocProfiler* GMallocProfiler;

/**
 * Maximum depth of stack backtrace.
 * Reducing this will sometimes truncate the amount of callstack info you get but will also reduce
 * the number of over all unique call stacks as some of the script call stacks are REALLY REALLY
 * deep and end up eating a lot of memory which will OOM you on consoles. A good value for consoles 
 * when doing long runs is 50.
 */
#define	MEMORY_PROFILER_MAX_BACKTRACE_DEPTH			75
/** Number of backtrace entries to skip											*/
#define MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES	1
/** Whether to track allocation tags? */
#define MEMORY_PROFILER_INCLUDE_ALLOC_TAGS			1

/** Magic value, determining that file is a memory profiler file.				*/
#define MEMORY_PROFILER_MAGIC						0xDA15F7D8
/** 
 * Version of memory profiler
 * 
 *	5 - Changed uint32 to uint64 in the FProfilerHeader to support profiler files larger than 4.0GB
 *		Removed NumDataFiles, no longer used.
 *
 *	6 - Added meta-data table.
 *
  *	7 - Added allocation tags to alloc/realloc.
 */
#define MEMORY_PROFILER_VERSION						7

/*=============================================================================
	Profiler header.
=============================================================================*/

struct FProfilerHeader
{
	/** Magic to ensure we're opening the right file.	*/
	uint32	Magic;
	/** Version number to detect version mismatches.	*/
	uint32	Version;
	/** Platform that this file was captured on.		*/
	FString	PlatformName;
	/** Whether symbol information is being serialized. */
	uint32	bShouldSerializeSymbolInfo;
	/** Name of executable, used for finding symbols.	*/
	FString ExecutableName;

	/** Offset in file for meta-data table.				*/
	uint64	MetaDataTableOffset;
	/** Number of meta-data table entries.				*/
	uint64	MetaDataTableEntries;

	/** Offset in file for name table.					*/
	uint64	NameTableOffset;
	/** Number of name table entries.					*/
	uint64	NameTableEntries;

	/** Offset in file for callstack address table.		*/
	uint64	CallStackAddressTableOffset;
	/** Number of callstack address entries.			*/
	uint64	CallStackAddressTableEntries;

	/** Offset in file for callstack table.				*/
	uint64	CallStackTableOffset;
	/** Number of callstack entries.					*/
	uint64	CallStackTableEntries;

	/** Offset in file for tags table.					*/
	uint64	TagsTableOffset;
	/** Number of tags entries.							*/
	uint64	TagsTableEntries;

	/** The file offset for module information.			*/
	uint64	ModulesOffset;
	/** The number of module entries.					*/
	uint64	ModuleEntries;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	Header		Header to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerHeader Header )
	{
		Ar	<< Header.Magic
			<< Header.Version;

		Header.PlatformName.SerializeAsANSICharArray(Ar,255);

		Ar	<< Header.bShouldSerializeSymbolInfo
			<< Header.MetaDataTableOffset
			<< Header.MetaDataTableEntries
			<< Header.NameTableOffset
			<< Header.NameTableEntries
			<< Header.CallStackAddressTableOffset
			<< Header.CallStackAddressTableEntries
			<< Header.CallStackTableOffset
			<< Header.CallStackTableEntries
			<< Header.TagsTableOffset
			<< Header.TagsTableEntries
			<< Header.ModulesOffset
			<< Header.ModuleEntries;

		check( Ar.IsSaving() );
		Header.ExecutableName.SerializeAsANSICharArray(Ar,255);
		return Ar;
	}
};

/*=============================================================================
	CallStack address information.
=============================================================================*/

/**
 * Helper structure encapsulating a callstack.
 */
struct FCallStackInfo
{
	/** CRC of program counters for this callstack.				*/
	uint32	CRC;
	/** Array of indices into callstack address info array.		*/
	int32		AddressIndices[MEMORY_PROFILER_MAX_BACKTRACE_DEPTH];

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	AllocInfo	Callstack info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FCallStackInfo CallStackInfo )
	{
		Ar << CallStackInfo.CRC;
		// Serialize valid callstack indices.
		int32 i=0;
		for( ; i<ARRAY_COUNT(CallStackInfo.AddressIndices) && CallStackInfo.AddressIndices[i]!=-1; i++ )
		{
			Ar << CallStackInfo.AddressIndices[i];
		}
		// Terminate list of address indices with -1 if we have a normal callstack.
		int32 Stopper = -1;
		// And terminate with -2 if the callstack was truncated.
		if( i== ARRAY_COUNT(CallStackInfo.AddressIndices) )
		{
			Stopper = -2;
		}

		Ar << Stopper;
		return Ar;
	}
};

/*=============================================================================
	Allocation infos.
=============================================================================*/

/**
 * Relevant information for a single malloc operation.
 *
 * 20 bytes
 */
struct FProfilerAllocInfo
{
	/** Pointer of allocation, lower two bits are used for payload type.	*/
	uint64 Pointer;
	/** Index of callstack.													*/
	int32 CallStackIndex;
	/** Index of tags.														*/
	int32 TagsIndex;
	/** Size of allocation.													*/
	uint32 Size;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	AllocInfo	Allocation info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerAllocInfo AllocInfo )
	{
		Ar	<< AllocInfo.Pointer
			<< AllocInfo.CallStackIndex
			<< AllocInfo.TagsIndex
			<< AllocInfo.Size;
		return Ar;
	}
};

/**
 * Relevant information for a single free operation.
 *
 * 8 bytes
 */
struct FProfilerFreeInfo
{
	/** Free'd pointer, lower two bits are used for payload type.			*/
	uint64 Pointer;
	
	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	FreeInfo	Free info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerFreeInfo FreeInfo )
	{
		Ar	<< FreeInfo.Pointer;
		return Ar;
	}
};

/**
 * Relevant information for a single realloc operation.
 *
 * 28 bytes
 */
struct FProfilerReallocInfo
{
	/** Old pointer, lower two bits are used for payload type.				*/
	uint64 OldPointer;
	/** New pointer, might be identical to old.								*/
	uint64 NewPointer;
	/** Index of callstack.													*/
	int32 CallStackIndex;
	/** Index of tags.														*/
	int32 TagsIndex;
	/** Size of allocation.													*/
	uint32 Size;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	ReallocInfo	Realloc info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerReallocInfo ReallocInfo )
	{
		Ar	<< ReallocInfo.OldPointer
			<< ReallocInfo.NewPointer
			<< ReallocInfo.CallStackIndex
			<< ReallocInfo.TagsIndex
			<< ReallocInfo.Size;
		return Ar;
	}
};

/**
 * Helper structure for misc data like e.g. end of stream marker.
 *
 * 12 bytes (assuming 32 bit pointers)
 */
struct FProfilerOtherInfo
{
	/** Dummy pointer as all tokens start with a pointer (TYPE_Other)		*/
	uint64	DummyPointer;
	/** Subtype.															*/
	int32		SubType;
	/** Subtype specific payload.											*/
	uint32	Payload;

	/**
	 * Serialization operator.
	 *
	 * @param	Ar			Archive to serialize to
	 * @param	OtherInfo	Info to serialize
	 * @return	Passed in archive
	 */
	friend FArchive& operator << ( FArchive& Ar, FProfilerOtherInfo OtherInfo )
	{
		Ar	<< OtherInfo.DummyPointer
			<< OtherInfo.SubType
			<< OtherInfo.Payload;
		return Ar;
	}
};


#if MEMORY_PROFILER_INCLUDE_ALLOC_TAGS

/*=============================================================================
	TLS storage for allocation tags
=============================================================================*/

/** Container for all of the active tags for a single thread. This will only be accessed by the thread that owns it. */
class FMallocProfilerTags : public FUseSystemMallocForNew, public FTlsAutoCleanup, private FNoncopyable
{
public:
	void AddTag(const FName InTag)
	{
		const int32 ExistingTagAndCountIndex = Tags.IndexOfByPredicate([&](const FTagNameAndCount& InTagAndCount) { return InTagAndCount.TagName == InTag; });

		if (ExistingTagAndCountIndex != INDEX_NONE)
		{
			Tags[ExistingTagAndCountIndex].RefCount += 1;
		}
		else
		{
			Tags.Add(FTagNameAndCount(InTag));
		}
	}

	void RemoveTag(const FName InTag)
	{
		const int32 ExistingTagAndCountIndex = Tags.IndexOfByPredicate([&](const FTagNameAndCount& InTagAndCount) { return InTagAndCount.TagName == InTag; });

		if (ExistingTagAndCountIndex != INDEX_NONE)
		{
			Tags[ExistingTagAndCountIndex].RefCount -= 1;
			if (Tags[ExistingTagAndCountIndex].RefCount == 0)
			{
				Tags.RemoveAt(ExistingTagAndCountIndex);
			}
		}
	}

	FString AsString() const
	{
		FString FlatTags;
		for (const FTagNameAndCount& TagAndCount : Tags)
		{
			FlatTags += TagAndCount.TagName.ToString();
			FlatTags.AppendChar(TEXT(';'));
		}
		return FlatTags;
	}

	uint32 GetHash() const
	{
		uint32 Hash = 0;
		for (const FTagNameAndCount& TagAndCount : Tags)
		{
			Hash = HashCombine(Hash, GetTypeHash(TagAndCount.TagName));
		}
		return Hash;
	}

private:
	struct FTagNameAndCount
	{
		FTagNameAndCount(FName InTagName)
			: TagName(InTagName)
			, RefCount(1)
		{
		}

		FName TagName;
		int32 RefCount;
	};

	static const int32 MaxNumTags = 32;

	TArray<FTagNameAndCount, TFixedAllocator<MaxNumTags>> Tags;
};

/** TLS management. Handles the lifespans of the per-thread tag data. */
class FMallocProfilerTagsTls
{
public:
	static void Initialize()
	{
		TlsSlot = FPlatformTLS::AllocTlsSlot();
	}

	static void Shutdown()
	{
		FPlatformTLS::FreeTlsSlot(TlsSlot);
		TlsSlot = 0;
	}

	static const FMallocProfilerTags* GetTagsForCurrentThread()
	{
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));

		return (FMallocProfilerTags*)FPlatformTLS::GetTlsValue(TlsSlot);
	}

	static FMallocProfilerTags& GetMutableTagsForCurrentThread()
	{
		check(FPlatformTLS::IsValidTlsSlot(TlsSlot));

		FMallocProfilerTags* TlsValue = (FMallocProfilerTags*)FPlatformTLS::GetTlsValue(TlsSlot);

		if (!TlsValue)
		{
			TlsValue = new FMallocProfilerTags();
			TlsValue->Register();
			FPlatformTLS::SetTlsValue(TlsSlot, TlsValue);
		}

		return *TlsValue;
	}

private:
	static uint32 TlsSlot;
};

uint32 FMallocProfilerTagsTls::TlsSlot = 0;

#endif // MEMORY_PROFILER_INCLUDE_ALLOC_TAGS


/*=============================================================================
	FMallocProfiler implementation.
=============================================================================*/


/**
 * Constructor, initializing all member variables and potentially loading symbols.
 *
 * @param	InMalloc	The allocator wrapped by FMallocProfiler that will actually do the allocs/deallocs.
 */
FMallocProfiler::FMallocProfiler(FMalloc* InMalloc)
:	UsedMalloc( InMalloc )
,   bEndProfilingHasBeenCalled( false )
,	CallStackInfoBuffer( 512 * 1024, COMPRESS_ZLIB )
,	bOutputFileClosed(false)
,	TrackingDepth(0)
,	MemoryOperationCount( 0 )
{
	StartTime = FPlatformTime::Seconds();

	// attempt to panic dump the mprof file if the system runs out of memory
	FCoreDelegates::GetOutOfMemoryDelegate().AddLambda([this]()
	{
		PanicDump(TYPE_Malloc, nullptr, nullptr);
	});

#if MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
	FMallocProfilerTagsTls::Initialize();
#endif // MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
}

/**
 * Tracks malloc operation.
 *
 * @param	Ptr		Allocated pointer 
 * @param	Size	Size of allocated pointer
 */
void FMallocProfiler::TrackMalloc( void* Ptr, uint32 Size )
{	
	// Avoid tracking operations caused by tracking!
	if( !bEndProfilingHasBeenCalled )
	{
		// Gather information about operation.
		FProfilerAllocInfo AllocInfo;
		AllocInfo.Pointer			= (uint64)(UPTRINT) Ptr | TYPE_Malloc;
		AllocInfo.CallStackIndex	= GetCallStackIndex();
		AllocInfo.TagsIndex			= GetTagsIndex();
		AllocInfo.Size				= Size;

		// Serialize to HDD.
		BufferedFileWriter << AllocInfo;
	}
}

/**
 * Tracks free operation
 *
 * @param	Ptr		Freed pointer
 */
void FMallocProfiler::TrackFree( void* Ptr )
{
	// Avoid tracking operations caused by tracking!
	if( !bEndProfilingHasBeenCalled )
	{
		// Gather information about operation.
		FProfilerFreeInfo FreeInfo;
		FreeInfo.Pointer = (uint64)(UPTRINT) Ptr | TYPE_Free;

		// Serialize to HDD.
		BufferedFileWriter << FreeInfo;
	}
}

/**
 * Tracks realloc operation
 *
 * @param	OldPtr	Previous pointer
 * @param	NewPtr	New pointer
 * @param	NewSize	New size of allocation
 */
void FMallocProfiler::TrackRealloc( void* OldPtr, void* NewPtr, uint32 NewSize )
{
	// Avoid tracking operations caused by tracking!
	if( !bEndProfilingHasBeenCalled )
	{
		// Gather information about operation.
		FProfilerReallocInfo ReallocInfo;
		ReallocInfo.OldPointer		= (uint64)(UPTRINT) OldPtr | TYPE_Realloc;
		ReallocInfo.NewPointer		= (uint64)(UPTRINT) NewPtr;
		ReallocInfo.CallStackIndex	= GetCallStackIndex();
		ReallocInfo.TagsIndex		= GetTagsIndex();
		ReallocInfo.Size			= NewSize;

		// Serialize to HDD.
		BufferedFileWriter << ReallocInfo;
	}
}

/**
 * Tracks memory allocations stats every 1024 memory operations. Used for time line view in memory profiler app.
 */
void FMallocProfiler::TrackSpecialMemory()
{
	if (!bEndProfilingHasBeenCalled && ((MemoryOperationCount++ & 0x3FF) == 0))
	{
		// Write marker snapshot to stream.
		FProfilerOtherInfo SnapshotMarker;
		SnapshotMarker.DummyPointer	= TYPE_Other;
		SnapshotMarker.SubType = SUBTYPE_MemoryAllocationStats;
		SnapshotMarker.Payload = 0;
		BufferedFileWriter << SnapshotMarker;

		WriteMemoryAllocationStats();

	}
}

/**
 * Begins profiling operation and opens file.
 */
void FMallocProfiler::BeginProfiling()
{
	// Serialize dummy header, overwritten in EndProfiling.
	FProfilerHeader DummyHeader;
	FMemory::Memzero( &DummyHeader, sizeof(DummyHeader) );
	BufferedFileWriter << DummyHeader;
}

/** 
 * Adds a tag to the list of tags associated with the calling thread.
 */
void FMallocProfiler::AddTag(const FName InTag)
{
#if MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
	FMallocProfilerTags& ActiveTags = FMallocProfilerTagsTls::GetMutableTagsForCurrentThread();
	ActiveTags.AddTag(InTag);
#endif // MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
}

/** 
 * Removes a tag from the list of tags associated with the calling thread.
 */
void FMallocProfiler::RemoveTag(const FName InTag)
{
#if MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
	FMallocProfilerTags& ActiveTags = FMallocProfilerTagsTls::GetMutableTagsForCurrentThread();
	ActiveTags.RemoveTag(InTag);
#endif // MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
}

/** 
	Added for untracked memory calculation
	Note that this includes all the memory used by dependent malloc profilers, such as FMallocGcmProfiler,
	so they don't need their own version of this function. 
*/
int32 FMallocProfiler::CalculateMemoryProfilingOverhead()
{
	return (int32)(ProgramCounterToIndexMap.GetAllocatedSize()
			+ CallStackAddressInfoArray.GetAllocatedSize()
			+ CRCToCallStackIndexMap.GetAllocatedSize()
			+ CallStackInfoBuffer.GetAllocatedSize()
			+ HashToTagTableIndexMap.GetAllocatedSize()
			+ TagsArray.GetAllocatedSize()
			+ NameToNameTableIndexMap.GetAllocatedSize()
			+ NameArray.GetAllocatedSize()
			+ BufferedFileWriter.GetAllocatedSize());
}

/** 
 * For externing in files where you can't include FMallocProfiler.h. 
 */
void FMallocProfiler_PanicDump(int32 FailedOperation, void* Ptr1, void* Ptr2)
{
	if( GMallocProfiler )
	{
		GMallocProfiler->PanicDump((EProfilingPayloadType)FailedOperation, Ptr1, Ptr2);
	}
}

/** 
 * This function is intended to be called when a fatal error has occurred inside the allocator and
 * you want to dump the current mprof before crashing, so that it can be used to help debug the error.
 * IMPORTANT: This function assumes that this thread already has the allocator mutex.. 
 */
void FMallocProfiler::PanicDump(EProfilingPayloadType FailedOperation, void* Ptr1, void* Ptr2)
{
	FString OperationString = TEXT("Invalid");
	switch (FailedOperation)
	{
		case TYPE_Malloc:
			OperationString = TEXT("Malloc");
			break;

		case TYPE_Free:
			OperationString = TEXT("Free");
			break;

		case TYPE_Realloc:
			OperationString = TEXT("Realloc");
			break;
	}

	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("FMallocProfiler::PanicDump called! Failed operation: %s, Ptr1: %08x, Ptr2: %08x"), *OperationString, Ptr1, Ptr2);

	EndProfiling();
}

/**
 * Ends profiling operation and closes file.
 */
void FMallocProfiler::EndProfiling()
{
	UE_LOG(LogProfilingDebugging, Log, TEXT("FMallocProfiler: dumping file [%s]"),*BufferedFileWriter.FullFilepath);
	{
		FScopeLock Lock( &CriticalSection );
		FScopedMallocProfilerLock MallocProfilerLock(*this);

		bEndProfilingHasBeenCalled = true;

		// Write end of stream marker.
		FProfilerOtherInfo EndOfStream;
		EndOfStream.DummyPointer	= TYPE_Other;
		EndOfStream.SubType			= SUBTYPE_EndOfStreamMarker;
		EndOfStream.Payload			= 0;
		BufferedFileWriter << EndOfStream;

		WriteAdditionalSnapshotMemoryStats();

#if SERIALIZE_SYMBOL_INFO
		double LastTimeUpdatedOnProgress = FPlatformTime::Seconds();
		double kProgressUpdateIntervalSeconds = 60;
		// Look up symbols on platforms supporting it at runtime.
		for( int32 AddressIndex=0; AddressIndex<CallStackAddressInfoArray.Num(); AddressIndex++ )
		{
			double CurrentTime = FPlatformTime::Seconds();
			if (UNLIKELY(CurrentTime - LastTimeUpdatedOnProgress > kProgressUpdateIntervalSeconds))
			{
				LastTimeUpdatedOnProgress = CurrentTime;
				UE_LOG(LogProfilingDebugging, Log, TEXT("FMallocProfiler: %d/%d addresses symbolicated (%f%%)"), AddressIndex, CallStackAddressInfoArray.Num(), 100.0 * (double)AddressIndex / (double)CallStackAddressInfoArray.Num());
			}

			FCallStackAddressInfo&	AddressInfo = CallStackAddressInfoArray[AddressIndex];
			// Look up symbols.
			FProgramCounterSymbolInfo SymbolInfo;
			FPlatformStackWalk::ProgramCounterToSymbolInfo( AddressInfo.ProgramCounter, SymbolInfo );

			// Convert to strings, and clean up in the process.
			const FString ModulName		= FPaths::GetCleanFilename(FString(SymbolInfo.ModuleName));
			const FString FileName		= FString(SymbolInfo.Filename);
			const FString FunctionName	= FString(SymbolInfo.FunctionName);

			// Propagate to our own struct, also populating name table.
			AddressInfo.FilenameNameTableIndex	= GetNameTableIndex( FileName );
			AddressInfo.FunctionNameTableIndex	= GetNameTableIndex( FunctionName );
			AddressInfo.LineNumber				= SymbolInfo.LineNumber;
		}
#endif // SERIALIZE_SYMBO_INFO

		// Archive used to write out symbol information. This will always be written to the first file, which
		// in the case of multiple files won't be pointed to by BufferedFileWriter.
		FArchive* SymbolFileWriter = NULL;
		// Use the BufferedFileWriter.
		SymbolFileWriter = &BufferedFileWriter;

		// Real header, written at start of the file but written out right before we close the file.
		FProfilerHeader Header;
		Header.Magic				= MEMORY_PROFILER_MAGIC;
		Header.Version				= MEMORY_PROFILER_VERSION;
		Header.PlatformName			= FPlatformProperties::PlatformName();
		Header.bShouldSerializeSymbolInfo = SERIALIZE_SYMBOL_INFO ? 1 : 0;
		Header.ExecutableName		= FPlatformProcess::ExecutableName();

		// Write out meta-data table and update header with offset and count.
		{
			TMap<FName, FString> SymbolMetaData = FPlatformStackWalk::GetSymbolMetaData();

			Header.MetaDataTableOffset = SymbolFileWriter->Tell();
			Header.MetaDataTableEntries = SymbolMetaData.Num();
			for (auto MetaDataPair : SymbolMetaData)
			{
				FString KeyString = MetaDataPair.Key.ToString();
				(*SymbolFileWriter) << KeyString;
				(*SymbolFileWriter) << MetaDataPair.Value;
			}
		}

		// Write out name table and update header with offset and count.
		Header.NameTableOffset	= SymbolFileWriter->Tell();
		Header.NameTableEntries	= NameArray.Num();
		for( int32 NameIndex=0; NameIndex<NameArray.Num(); NameIndex++ )
		{
			NameArray[NameIndex].SerializeAsANSICharArray( *SymbolFileWriter );
		}

		// Write out callstack address infos and update header with offset and count.
		Header.CallStackAddressTableOffset	= SymbolFileWriter->Tell();
		Header.CallStackAddressTableEntries	= CallStackAddressInfoArray.Num();
		for( int32 CallStackAddressIndex=0; CallStackAddressIndex<CallStackAddressInfoArray.Num(); CallStackAddressIndex++ )
		{
			(*SymbolFileWriter) << CallStackAddressInfoArray[CallStackAddressIndex];
		}

		// Write out callstack infos and update header with offset and count.
		Header.CallStackTableOffset			= SymbolFileWriter->Tell();
		Header.CallStackTableEntries		= CallStackInfoBuffer.Num();

		CallStackInfoBuffer.Lock();
		for( int32 CallStackIndex=0; CallStackIndex<CallStackInfoBuffer.Num(); CallStackIndex++ )
		{
			FCallStackInfo* CallStackInfo = (FCallStackInfo*) CallStackInfoBuffer.Access( CallStackIndex * sizeof(FCallStackInfo) );
			(*SymbolFileWriter) << (*CallStackInfo);
		}
		CallStackInfoBuffer.Unlock();

		// Write out tags and update the header with offset and count
		Header.TagsTableOffset = SymbolFileWriter->Tell();
		Header.TagsTableEntries = TagsArray.Num();
		for (FString& Tags : TagsArray)
		{
			(*SymbolFileWriter) << Tags;
		}

		Header.ModulesOffset				= SymbolFileWriter->Tell();
		Header.ModuleEntries				= FPlatformStackWalk::GetProcessModuleCount();

		TArray<FStackWalkModuleInfo> ProcModules;
		ProcModules.Reserve(Header.ModuleEntries);

		Header.ModuleEntries = FPlatformStackWalk::GetProcessModuleSignatures(ProcModules.GetData(), ProcModules.Num());

		for(uint32 ModuleIndex = 0; ModuleIndex < Header.ModuleEntries; ++ModuleIndex)
		{
			FStackWalkModuleInfo &CurModule = ProcModules[ModuleIndex];

			(*SymbolFileWriter) << CurModule.BaseOfImage
								<< CurModule.ImageSize
								<< CurModule.TimeDateStamp
								<< CurModule.PdbSig
								<< CurModule.PdbAge
								<< *((uint32*)&CurModule.PdbSig70.Data1)
								<< CurModule.PdbSig70.Data2
								<< CurModule.PdbSig70.Data3
								<< *((uint32*)&CurModule.PdbSig70.Data4[0])
								<< *((uint32*)&CurModule.PdbSig70.Data4[4]);

			FString(CurModule.ModuleName).SerializeAsANSICharArray( *SymbolFileWriter );
			FString(CurModule.ImageName).SerializeAsANSICharArray( *SymbolFileWriter );
			FString(CurModule.LoadedImageName).SerializeAsANSICharArray( *SymbolFileWriter );
		}

		// Seek to the beginning of the file and write out proper header.
		SymbolFileWriter->Seek( 0 );
		(*SymbolFileWriter) << Header;

		// Close file writers.
		SymbolFileWriter->Close();
		if( SymbolFileWriter != &BufferedFileWriter )
		{
			BufferedFileWriter.Close();
		}

		bOutputFileClosed = true;
	}

	UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: done writing file [%s]"), *(BufferedFileWriter.FullFilepath) );

	// Send the final part
	SendDataToPCViaUnrealConsole( TEXT("UE_PROFILER!MEMORY:"), *(BufferedFileWriter.FullFilepath) );
}

/**
 * Returns index of passed in program counter into program counter array. If not found, adds it.
 *
 * @param	ProgramCounter	Program counter to find index for
 * @return	Index of passed in program counter if it's not NULL, INDEX_NONE otherwise
 */
int32 FMallocProfiler::GetProgramCounterIndex( uint64 ProgramCounter )
{
	int32	Index = INDEX_NONE;

	// Look up index in unique array of program counter infos, if we have a valid program counter.
	if( ProgramCounter )
	{
		// Use index if found.
		int32* IndexPtr = ProgramCounterToIndexMap.Find( ProgramCounter );
		if( IndexPtr )
		{
			Index = *IndexPtr;
		}
		// Encountered new program counter, add to array and set index mapping.
		else
		{
			// Add to aray and set mapping for future use.
			Index = CallStackAddressInfoArray.AddZeroed();
			ProgramCounterToIndexMap.Add( ProgramCounter, Index );

			// Only initialize program counter, rest will be filled in at the end, once symbols are loaded.
			FCallStackAddressInfo& CallStackAddressInfo = CallStackAddressInfoArray[Index];
			CallStackAddressInfo.ProgramCounter = ProgramCounter;
		}
		check(Index!=INDEX_NONE);
	}

	return Index;
}

/** 
 * Returns index of callstack, captured by this function into array of callstack infos. If not found, adds it.
 *
 * @return index into callstack array
 */
int32 FMallocProfiler::GetCallStackIndex()
{
	// Index of callstack in callstack info array.
	int32 Index = INDEX_NONE;

	// Capture callstack and create CRC.
	uint64 FullBackTrace[MEMORY_PROFILER_MAX_BACKTRACE_DEPTH + MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES] = { 0 };
	FPlatformStackWalk::CaptureStackBackTrace( FullBackTrace, MEMORY_PROFILER_MAX_BACKTRACE_DEPTH + MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES );
	// Skip first 5 entries as they are inside the allocator.
	uint64* BackTrace = &FullBackTrace[MEMORY_PROFILER_SKIP_NUM_BACKTRACE_ENTRIES];

	uint32 CRC = FCrc::MemCrc32( BackTrace, MEMORY_PROFILER_MAX_BACKTRACE_DEPTH * sizeof(uint64) );

	// Use index if found
	int32* IndexPtr = CRCToCallStackIndexMap.Find( CRC );
	if( IndexPtr )
	{
		Index = *IndexPtr;
	}
	// Encountered new call stack, add to array and set index mapping.
	else
	{
		// Set mapping for future use.
		Index = CallStackInfoBuffer.Num();
		CRCToCallStackIndexMap.Add( CRC, Index );

		// Set up callstack info with captured call stack, translating program counters
		// to indices in program counter array (unique entries).
		FCallStackInfo CallStackInfo;
		CallStackInfo.CRC = CRC;
		for( int32 i=0; i<MEMORY_PROFILER_MAX_BACKTRACE_DEPTH; i++ )
		{
			CallStackInfo.AddressIndices[i] = GetProgramCounterIndex( BackTrace[i] );
		}

		// Append to compressed buffer.
		CallStackInfoBuffer.Append( &CallStackInfo, sizeof(FCallStackInfo) );
	}

	check(Index!=INDEX_NONE);
	return Index;
}	

/** 
 * Returns index of the tags active on the thread making the allocation.
 *
 * @return index into tags array
 */
int32 FMallocProfiler::GetTagsIndex()
{
	int32 Index = INDEX_NONE;

#if MEMORY_PROFILER_INCLUDE_ALLOC_TAGS
	const FMallocProfilerTags* ActiveTags = FMallocProfilerTagsTls::GetTagsForCurrentThread();
	if (ActiveTags)
	{
		const uint32 TagsHash = ActiveTags->GetHash();
		if (TagsHash != 0)
		{
			// Use index if found
			int32* IndexPtr = HashToTagTableIndexMap.Find(TagsHash);
			if (IndexPtr)
			{
				Index = *IndexPtr;
			}
			else
			{
				// Set mapping for future use.
				Index = TagsArray.Num();
				HashToTagTableIndexMap.Add(TagsHash, Index);

				// Add the new tags
				TagsArray.Add(ActiveTags->AsString());
			}
		}
	}
#endif // MEMORY_PROFILER_INCLUDE_ALLOC_TAGS

	return Index;
}

/**
 * Returns index of passed in name into name array. If not found, adds it.
 *
 * @param	Name	Name to find index for
 * @return	Index of passed in name
 */
int32 FMallocProfiler::GetNameTableIndex( const FString& Name )
{
	// Index of name in name table.
	int32 Index = INDEX_NONE;

	// Use index if found.
	int32* IndexPtr = NameToNameTableIndexMap.Find( Name );
	if( IndexPtr )
	{
		Index = *IndexPtr;
	}
	// Encountered new name, add to array and set index mapping.
	else
	{
		Index = NameArray.Num();
		new(NameArray)FString(Name);
		NameToNameTableIndexMap.Add(*Name,Index);
	}

	check(Index!=INDEX_NONE);
	return Index;
}


bool FMallocProfiler::HandleMProfCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd, TEXT("START")))
	{
		if (bEndProfilingHasBeenCalled)
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Memory recording has already been stopped and cannot be restarted."));
		}
		else
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Memory recording is automatically started when the game is run and is still running."));
		}
	}
	else if (FParse::Command(&Cmd, TEXT("STOP")))
	{
		if (bEndProfilingHasBeenCalled)
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Memory recording has already been stopped."));
		}
		else
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Stopping recording."));
			EndProfiling();
		}
	}
	else if (FParse::Command(&Cmd, TEXT("MARK")) || FParse::Command(&Cmd, TEXT("SNAPSHOT")))
	{
		if (bEndProfilingHasBeenCalled == true)
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Memory recording has already been stopped.  Markers have no meaning at this point."));
		}
		else
		{
			FString SnapshotName;
			FParse::Token(Cmd, SnapshotName, true);

			Ar.Logf(TEXT("FMallocProfiler: Recording a snapshot marker %s"), *SnapshotName);
			SnapshotMemory(SUBTYPE_SnapshotMarker, SnapshotName);
		}
	}
	else
	{
		if (bEndProfilingHasBeenCalled)
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Status: Memory recording has been stopped."));
		}
		else
		{
			UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: Status: Memory recording is ongoing."));
			UE_LOG(LogProfilingDebugging, Warning, TEXT("  Use MPROF MARK [FriendlyName] to insert a marker."));
			UE_LOG(LogProfilingDebugging, Warning, TEXT("  Use MPROF STOP to stop recording and write the recording to disk."));
		}
	}
	return true;
}

bool FMallocProfiler::HandleDumpAllocsToFileCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( bEndProfilingHasBeenCalled == true )
	{
		UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: EndProfiling() has been called further actions will not be recorded please restart memory tracking process"));
		return true;
	}

	UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: DUMPALLOCSTOFILE"));
	EndProfiling();
	return true;
}

bool FMallocProfiler::HandleSnapshotMemoryCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( bEndProfilingHasBeenCalled == true )
	{
		UE_LOG(LogProfilingDebugging, Warning, TEXT("FMallocProfiler: EndProfiling() has been called further actions will not be recorded please restart memory tracking process"));
		return true;
	}

	FString SnapshotName;
	FParse::Token(Cmd, SnapshotName, true);

	Ar.Logf(TEXT("FMallocProfiler: SNAPSHOTMEMORY %s"), *SnapshotName);
	SnapshotMemory(SUBTYPE_SnapshotMarker, SnapshotName);
	return true;
}

bool FMallocProfiler::HandleSnapshotMemoryFrameCommand( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (!bEndProfilingHasBeenCalled)
	{
		EmbedFloatMarker(SUBTYPE_FrameTimeMarker, (float)FApp::GetDeltaTime());
	}
	return true;
}

/**
 * Exec handler. Parses command and returns true if handled.
 *
 * @param	Cmd		Command to parse
 * @param	Ar		Output device to use for logging
 * @return	true if handled, false otherwise
 */
bool FMallocProfiler::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	// End profiling.
	if (FParse::Command(&Cmd, TEXT("MPROF")))
	{
		return HandleMProfCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("DUMPALLOCSTOFILE")) )
	{
		return HandleDumpAllocsToFileCommand( Cmd, Ar );
	}
	else if( FParse::Command(&Cmd,TEXT("SNAPSHOTMEMORY")) )
	{
		return HandleSnapshotMemoryCommand( Cmd, Ar );
	}
	// Do not use this.
	else if( FParse::Command(&Cmd,TEXT("SNAPSHOTMEMORYFRAME")) )
	{
		return HandleSnapshotMemoryFrameCommand( Cmd, Ar );
	}

	return UsedMalloc->Exec( InWorld, Cmd, Ar);
}

/**
 * Embeds token into stream to snapshot memory at this point.
 */
void FMallocProfiler::SnapshotMemory(EProfilingPayloadSubType SubType, const FString& MarkerName)
{
	FScopeLock Lock( &CriticalSection );
	FScopedMallocProfilerLock MallocProfilerLock(*this);

	// Write snapshot marker to stream.
	FProfilerOtherInfo SnapshotMarker;
	SnapshotMarker.DummyPointer	= TYPE_Other;
	SnapshotMarker.SubType = SubType;
	SnapshotMarker.Payload = GetNameTableIndex(MarkerName);
	BufferedFileWriter << SnapshotMarker;

	WriteAdditionalSnapshotMemoryStats();
}

/**
 * Embeds token into stream to snapshot memory at this point.
 */
void FMallocProfiler::EmbedFloatMarker(EProfilingPayloadSubType SubType, float DeltaTime)
{
	FScopeLock Lock( &CriticalSection );
	FScopedMallocProfilerLock MallocProfilerLock(*this);

	union { float f; uint32 ui; } TimePacker;
	TimePacker.f = DeltaTime;

	// Write marker snapshot to stream.
	FProfilerOtherInfo SnapshotMarker;
	SnapshotMarker.DummyPointer	= TYPE_Other;
	SnapshotMarker.SubType = SubType;
	SnapshotMarker.Payload = TimePacker.ui;
	BufferedFileWriter << SnapshotMarker;
}

/**
 * Embeds token into stream to snapshot memory at this point.
 */
void FMallocProfiler::EmbedDwordMarker(EProfilingPayloadSubType SubType, uint32 Info)
{
	if (Info != 0)
	{
		FScopeLock Lock( &CriticalSection );
		FScopedMallocProfilerLock MallocProfilerLock(*this);

		// Write marker snapshot to stream.
		FProfilerOtherInfo SnapshotMarker;
		SnapshotMarker.DummyPointer	= TYPE_Other;
		SnapshotMarker.SubType = SubType;
		SnapshotMarker.Payload = Info;
		BufferedFileWriter << SnapshotMarker;
	}
}

/** Writes memory allocations stats. */
void FMallocProfiler::WriteMemoryAllocationStats()
{
	FGenericMemoryStats Stats;
	FPlatformMemory::GetStatsForMallocProfiler( Stats ); 
	UsedMalloc->GetAllocatorStats( Stats );

	const TCHAR* DESC_MemoryProfilingOverhead = TEXT("Memory Profiling Overhead");
	int64 MemoryProfilingOverhead = CalculateMemoryProfilingOverhead();
	Stats.Add( DESC_MemoryProfilingOverhead, MemoryProfilingOverhead );

	check( Stats.Data.Num() < MAX_uint8 );
	uint8 StatsCount = Stats.Data.Num();
	BufferedFileWriter << StatsCount;

	for( const auto& MapIt : Stats.Data )
	{
		FString StatName = MapIt.Key;
		int64 StatValue = (int64)MapIt.Value;
		int32 StatNameIndex = GetNameTableIndex(StatName);
		BufferedFileWriter << StatNameIndex << StatValue;
	}
}

/** 
 * Writes additional memory stats for a snapshot like memory allocations stats, list of all loaded levels and platform dependent memory metrics.
 */
void FMallocProfiler::WriteAdditionalSnapshotMemoryStats()
{
	WriteMemoryAllocationStats();

	// Write "stat levels" information.
	WriteLoadedLevels( NULL );
}

/** Snapshot taken when engine has started the cleaning process before loading a new level. */
void FMallocProfiler::SnapshotMemoryLoadMapStart(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LoadMap_Start, Tag);
	}
}

/** Snapshot taken when a new level has started loading. */
void FMallocProfiler::SnapshotMemoryLoadMapMid(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LoadMap_Mid, Tag);
	}
}

/** Snapshot taken when a new level has been loaded. */
void FMallocProfiler::SnapshotMemoryLoadMapEnd(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LoadMap_End, Tag);
	}
}

/** Snapshot taken when garbage collection has started. */
void FMallocProfiler::SnapshotMemoryGCStart()
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		// GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_GC_Start, FString()); // Disabled due to performance in profiler
	}
}

/** Snapshot taken when garbage collection has finished. */
void FMallocProfiler::SnapshotMemoryGCEnd()
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_GC_End, FString());
	}
}

/** Snapshot taken when a new streaming level has been requested to load. */
void FMallocProfiler::SnapshotMemoryLevelStreamStart(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		// GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LevelStream_Start, Tag); // Disabled due to performance in profiler
	}
}

/** Snapshot taken when a previously  streamed level has been made visible. */
void FMallocProfiler::SnapshotMemoryLevelStreamEnd(const FString& Tag)
{
	if (GMallocProfiler && !GMallocProfiler->bEndProfilingHasBeenCalled)
	{
		// GMallocProfiler->SnapshotMemory(SUBTYPE_SnapshotMarker_LevelStream_End, Tag); // Disabled due to performance in profiler
	}
}

/** 
 * Writes names of currently loaded levels.
 */
void FMallocProfiler::WriteLoadedLevels( UWorld* InWorld )
{
	// Write a 0 count for loaded levels.
	uint16 NumLoadedLevels = 0;
	BufferedFileWriter << NumLoadedLevels;
}

/** 
 * Gather texture memory stats. 
 */
void FMallocProfiler::GetTexturePoolSize( FGenericMemoryStats& out_Stats )
{
}

/*=============================================================================
	FMallocProfilerBufferedFileWriter implementation
=============================================================================*/

/**
 * Constructor. Called before GMalloc is initialized!!!
 */
FMallocProfilerBufferedFileWriter::FMallocProfilerBufferedFileWriter()
:	FileWriter( NULL )
{
	ArIsSaving		= true;
	ArIsPersistent	= true;
	BaseFilePath = TEXT("");
}

/**
 * Destructor, cleaning up FileWriter.
 */
FMallocProfilerBufferedFileWriter::~FMallocProfilerBufferedFileWriter()
{
	if( FileWriter )
	{
		delete FileWriter;
		FileWriter = NULL;
	}
}

// FArchive interface.

void FMallocProfilerBufferedFileWriter::Serialize( void* V, int64 Length )
{
#if (ALLOW_DEBUG_FILES && !HACK_HEADER_GENERATOR)
	// Copy to buffered memory array if GConfig hasn't been set up yet.
	// This isn't the best solution, but due to complexity of the engine initialization order is the safest.
	const bool bIsIniReady = GConfig && GConfig->IsReadyForUse();
	if( !bIsIniReady )
	{
		const int32 Index = BufferedData.AddUninitialized( Length );
		FMemory::Memcpy( &BufferedData[Index], V, Length );
	} 
	// File manager is set up but we haven't created file writer yet, do it now.
	else if( (FileWriter == NULL) && !GMallocProfiler->bOutputFileClosed )
	{
		// Get the base path (only used once to prevent the system time from changing for multi-file-captures
		if (BaseFilePath == TEXT(""))
		{
			const FString SysTime = FDateTime::Now().ToString();
			BaseFilePath = FPaths::ProfilingDir() + TEXT("/") + FApp::GetProjectName() + TEXT("-") + SysTime;
		}
			
		// Create file writer to serialize data to HDD.
		if( FPaths::GetBaseFilename(FullFilepath) == TEXT("") )
		{
			FullFilepath = BaseFilePath + TEXT(".mprof");
		}

		FileWriter = IFileManager::Get().CreateFileWriter( *FullFilepath, FILEWRITE_NoFail );
		check( FileWriter );

		// Serialize existing buffered data and empty array.
		FileWriter->Serialize( BufferedData.GetData(), BufferedData.Num() );
		BufferedData.Empty();
	}

	// Serialize data to HDD via FileWriter if it already has been created.
	if( FileWriter && ((GMallocProfiler == NULL) || !GMallocProfiler->bOutputFileClosed))
	{
		FileWriter->Serialize( V, Length );
	}
#endif
}

void FMallocProfilerBufferedFileWriter::Seek( int64 InPos )
{
	check( FileWriter );
	FileWriter->Seek( InPos );
}

bool FMallocProfilerBufferedFileWriter::Close()
{
	check( FileWriter );

	bool bResult = FileWriter->Close();

	delete FileWriter;
	FileWriter = NULL;

	return bResult;
}

int64 FMallocProfilerBufferedFileWriter::Tell()
{
	check( FileWriter );
	return FileWriter->Tell();
}

uint32 FMallocProfilerBufferedFileWriter::GetAllocatedSize()
{
	//@TODO: Currently there is no way to request the full size of a platform specfic archiver, these are just
	// approximate sizes based on the buffer size in each platform
	uint32 FileWriterSize = sizeof(FArchive);
#if PLATFORM_WINDOWS
	FileWriterSize += 1024;
#else
	FileWriterSize += 4096;
#endif

	return FileWriterSize + BufferedData.GetAllocatedSize();
}

/*-----------------------------------------------------------------------------
	FScopedMallocProfilerLock
-----------------------------------------------------------------------------*/

/** Constructor that performs a lock on the malloc profiler tracking methods. */
FScopedMallocProfilerLock::FScopedMallocProfilerLock(FMallocProfiler& InProfiler)
	: Profiler(InProfiler)
{
	Profiler.TrackingDepth++;
}

/** Destructor that performs a release on the malloc profiler tracking methods. */
FScopedMallocProfilerLock::~FScopedMallocProfilerLock()
{
	Profiler.TrackingDepth--;
}

#else //USE_MALLOC_PROFILER

// Suppress linker warning "warning LNK4221: no public symbols found; archive member will be inaccessible"
int32 FMallocProfilerLinkerHelper;

#endif //
