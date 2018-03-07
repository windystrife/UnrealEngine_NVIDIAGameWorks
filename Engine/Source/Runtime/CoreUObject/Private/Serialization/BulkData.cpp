// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Serialization/BulkData.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "Templates/Casts.h"
#include "Async/Async.h"
#include "UObject/LinkerLoad.h"
#include "UObject/LinkerSave.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/DebugSerializationFlags.h"
#include "Serialization/AsyncLoadingPrivate.h"

/*-----------------------------------------------------------------------------
	Constructors and operators
-----------------------------------------------------------------------------*/

/** Whether to track information of how bulk data is being used */
#define TRACK_BULKDATA_USE 0

DECLARE_STATS_GROUP(TEXT("Bulk Data"), STATGROUP_BulkData, STATCAT_Advanced);

#if TRACK_BULKDATA_USE

/** Simple wrapper for tracking the bulk data usage in the thread-safe way. */
struct FThreadSafeBulkDataToObjectMap
{
	static FThreadSafeBulkDataToObjectMap& Get()
	{
		static FThreadSafeBulkDataToObjectMap Instance;
		return Instance;
	}

	void Add( FUntypedBulkData* Key, UObject* Value )
	{
		FScopeLock ScopeLock(&CriticalSection);
		BulkDataToObjectMap.Add(Key,Value);
	}

	void Remove( FUntypedBulkData* Key )
	{
		FScopeLock ScopeLock(&CriticalSection);
		BulkDataToObjectMap.Remove(Key);
	}

	FCriticalSection& GetLock() 
	{
		return CriticalSection;
	}

	const TMap<FUntypedBulkData*,UObject*>::TConstIterator GetIterator() const
	{
		return BulkDataToObjectMap.CreateConstIterator();
	}

protected:
	/** Map from bulk data pointer to object it is contained by */
	TMap<FUntypedBulkData*,UObject*> BulkDataToObjectMap;

	/** CriticalSection. */
	FCriticalSection CriticalSection;
};

/**
 * Helper structure associating an object and a size for sorting purposes.
 */
struct FObjectAndSize
{
	FObjectAndSize( const UObject* InObject, int32 InSize )
	:	Object( InObject )
	,	Size( InSize )
	{}

	/** Object associated with size. */
	const UObject*	Object;
	/** Size associated with object. */
	int32			Size;
};

/** Hash function required for TMap support */
uint32 GetTypeHash( const FUntypedBulkData* BulkData )
{
	return PointerHash(BulkData);
}

#endif


/**
 * Constructor, initializing all member variables.
 */
FUntypedBulkData::FUntypedBulkData()
{
	InitializeMemberVariables();
}

/**
 * Copy constructor. Use the common routine to perform the copy.
 *
 * @param Other the source array to copy
 */
FUntypedBulkData::FUntypedBulkData( const FUntypedBulkData& Other )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::FUntypedBulkData"), STAT_UBD_Constructor, STATGROUP_Memory);

	InitializeMemberVariables();
	BulkDataAlignment = Other.BulkDataAlignment;

	// Prepare bulk data pointer. Can't call any functions that would call virtual GetElementSize on "this" as
	// we're in the constructor of the base class and would hence call a pure virtual.
	ElementCount	= Other.ElementCount;
	BulkData.Reallocate( Other.GetBulkDataSize(), BulkDataAlignment );

	// Copy data over.
	Copy( Other );

#if TRACK_BULKDATA_USE
	FThreadSafeBulkDataToObjectMap::Get().Add( this, NULL );
#endif
}

/**
 * Virtual destructor, free'ing allocated memory.
 */
FUntypedBulkData::~FUntypedBulkData()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::~FUntypedBulkData"), STAT_UBD_Destructor, STATGROUP_Memory);

	check( LockStatus == LOCKSTATUS_Unlocked );
	if (SerializeFuture.IsValid())
	{
		WaitForAsyncLoading();
	}

	// Free memory.
	BulkData     .Deallocate();
	BulkDataAsync.Deallocate();
	
#if WITH_EDITOR
	// Detach from archive.
	if( AttachedAr )
	{
		AttachedAr->DetachBulkData( this, false );
		check( AttachedAr == NULL );
	}
#endif // WITH_EDITOR

#if TRACK_BULKDATA_USE
	FThreadSafeBulkDataToObjectMap::Get().Remove( this );
#endif
}

/**
 * Copies the source array into this one after detaching from archive.
 *
 * @param Other the source array to copy
 */
FUntypedBulkData& FUntypedBulkData::operator=( const FUntypedBulkData& Other )
{
	// Remove bulk data, avoiding potential load in Lock call.
	RemoveBulkData();

	BulkDataAlignment = Other.BulkDataAlignment;

	if (Other.BulkData)
	{
		// Reallocate to size of src.
		Lock(LOCK_READ_WRITE);
		Realloc(Other.GetElementCount());

		// Copy data over.
		Copy( Other );

		// Unlock.
		Unlock();
	}
	else // Otherwise setup the bulk so that the data can be loaded through LoadBulkDataWithFileReader()
	{
		Filename = Other.Filename;
		BulkDataFlags = Other.BulkDataFlags;
		ElementCount = Other.ElementCount;
		BulkDataOffsetInFile = Other.BulkDataOffsetInFile;
		BulkDataSizeOnDisk = Other.BulkDataSizeOnDisk;
	}

	return *this;
}


/*-----------------------------------------------------------------------------
	Static functions.
-----------------------------------------------------------------------------*/

/**
 * Dumps detailed information of bulk data usage.
 *
 * @param Log FOutputDevice to use for logging
 */
void FUntypedBulkData::DumpBulkDataUsage( FOutputDevice& Log )
{
#if TRACK_BULKDATA_USE
	// Arrays about to hold per object and per class size information.
	TArray<FObjectAndSize> PerObjectSizeArray;
	TArray<FObjectAndSize> PerClassSizeArray;

	{
		FScopeLock Lock(&FThreadSafeBulkDataToObjectMap::Get().GetLock());

		// Iterate over all "live" bulk data and add size to arrays if it is loaded.
		for( auto It(FThreadSafeBulkDataToObjectMap::Get().GetIterator()); It; ++It )
		{
			const FUntypedBulkData*	BulkData	= It.Key();
			const UObject*			Owner		= It.Value();
			// Only add bulk data that is consuming memory to array.
			if( Owner && BulkData->IsBulkDataLoaded() && BulkData->GetBulkDataSize() > 0 )
			{
				// Per object stats.
				PerObjectSizeArray.Add( FObjectAndSize( Owner, BulkData->GetBulkDataSize() ) );

				// Per class stats.
				bool bFoundExistingPerClassSize = false;
				// Iterate over array, trying to find existing entry.
				for( int32 PerClassIndex=0; PerClassIndex<PerClassSizeArray.Num(); PerClassIndex++ )
				{
					FObjectAndSize& PerClassSize = PerClassSizeArray[ PerClassIndex ];
					// Add to existing entry if found.
					if( PerClassSize.Object == Owner->GetClass() )
					{
						PerClassSize.Size += BulkData->GetBulkDataSize();
						bFoundExistingPerClassSize = true;
						break;
					}
				}
				// Add new entry if we didn't find an existing one.
				if( !bFoundExistingPerClassSize )
				{
					PerClassSizeArray.Add( FObjectAndSize( Owner->GetClass(), BulkData->GetBulkDataSize() ) );
				}
			}
		}
	}

	/** Compare operator, sorting by size in descending order */
	struct FCompareFObjectAndSize
	{
		FORCEINLINE bool operator()( const FObjectAndSize& A, const FObjectAndSize& B ) const
		{
			return B.Size < A.Size;
		}
	};

	// Sort by size.
	PerObjectSizeArray.Sort( FCompareFObjectAndSize() );
	PerClassSizeArray.Sort( FCompareFObjectAndSize() );

	// Log information.
	UE_LOG(LogSerialization, Log, TEXT(""));
	UE_LOG(LogSerialization, Log, TEXT("Per class summary of bulk data use:"));
	for( int32 PerClassIndex=0; PerClassIndex<PerClassSizeArray.Num(); PerClassIndex++ )
	{
		const FObjectAndSize& PerClassSize = PerClassSizeArray[ PerClassIndex ];
		Log.Logf( TEXT("  %5d KByte of bulk data for Class %s"), PerClassSize.Size / 1024, *PerClassSize.Object->GetPathName() );
	}
	UE_LOG(LogSerialization, Log, TEXT(""));
	UE_LOG(LogSerialization, Log, TEXT("Detailed per object stats of bulk data use:"));
	for( int32 PerObjectIndex=0; PerObjectIndex<PerObjectSizeArray.Num(); PerObjectIndex++ )
	{
		const FObjectAndSize& PerObjectSize = PerObjectSizeArray[ PerObjectIndex ];
		Log.Logf( TEXT("  %5d KByte of bulk data for %s"), PerObjectSize.Size / 1024, *PerObjectSize.Object->GetFullName() );
	}
	UE_LOG(LogSerialization, Log, TEXT(""));
#else
	UE_LOG(LogSerialization, Log, TEXT("Please recompiled with TRACK_BULKDATA_USE set to 1 in UnBulkData.cpp."));
#endif
}


/*-----------------------------------------------------------------------------
	Accessors.
-----------------------------------------------------------------------------*/

/**
 * Returns the number of elements in this bulk data array.
 *
 * @return Number of elements in this bulk data array
 */
int32 FUntypedBulkData::GetElementCount() const
{
	return ElementCount;
}
/**
 * Returns the size of the bulk data in bytes.
 *
 * @return Size of the bulk data in bytes
 */
int32 FUntypedBulkData::GetBulkDataSize() const
{
	return GetElementCount() * GetElementSize();
}
/**
 * Returns the size of the bulk data on disk. This can differ from GetBulkDataSize if
 * BULKDATA_SerializeCompressed is set.
 *
 * @return Size of the bulk data on disk or INDEX_NONE in case there's no association
 */
int32 FUntypedBulkData::GetBulkDataSizeOnDisk() const
{
	return BulkDataSizeOnDisk;
}
/**
 * Returns the offset into the file the bulk data is located at.
 *
 * @return Offset into the file or INDEX_NONE in case there is no association
 */
int64 FUntypedBulkData::GetBulkDataOffsetInFile() const
{
	return BulkDataOffsetInFile;
}
/**
 * Returns whether the bulk data is stored compressed on disk.
 *
 * @return true if data is compressed on disk, false otherwise
 */
bool FUntypedBulkData::IsStoredCompressedOnDisk() const
{
	return (BulkDataFlags & BULKDATA_SerializeCompressed) ? true : false;
}

bool FUntypedBulkData::CanLoadFromDisk() const
{
#if WITH_EDITOR
	return AttachedAr != NULL;
#else
	return Filename != TEXT("") || (Package.IsValid() && Package->LinkerLoad);
#endif // WITH_EDITOR
}

/**
 * Returns flags usable to decompress the bulk data
 * 
 * @return COMPRESS_NONE if the data was not compressed on disk, or valid flags to pass to FCompression::UncompressMemory for this data
 */
ECompressionFlags FUntypedBulkData::GetDecompressionFlags() const
{
	return (BulkDataFlags & BULKDATA_SerializeCompressedZLIB) ? COMPRESS_ZLIB : COMPRESS_None;
}

/**
 * Returns whether the bulk data is currently loaded and resident in memory.
 *
 * @return true if bulk data is loaded, false otherwise
 */
bool FUntypedBulkData::IsBulkDataLoaded() const
{
	return !!BulkData;
}

bool FUntypedBulkData::IsAsyncLoadingComplete()
{
	return SerializeFuture.IsValid() == false || SerializeFuture.WaitFor(FTimespan::Zero());
}

/**
* Returns whether this bulk data is used
* @return true if BULKDATA_Unused is not set
*/
bool FUntypedBulkData::IsAvailableForUse() const
{
	return (BulkDataFlags & BULKDATA_Unused) ? false : true;
}

/*-----------------------------------------------------------------------------
	Data retrieval and manipulation.
-----------------------------------------------------------------------------*/

void FUntypedBulkData::ResetAsyncData()
{
	// Async data should be released by the time we get here
	check(!BulkDataAsync);
	SerializeFuture = TFuture<bool>();
}

/**
 * Retrieves a copy of the bulk data.
 *
 * @param Dest [in/out] Pointer to pointer going to hold copy, can point to NULL pointer in which case memory is allocated
 * @param bDiscardInternalCopy Whether to discard/ free the potentially internally allocated copy of the data
 */
void FUntypedBulkData::GetCopy( void** Dest, bool bDiscardInternalCopy )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::GetCopy"), STAT_UBD_GetCopy, STATGROUP_Memory);

	check( LockStatus == LOCKSTATUS_Unlocked );
	check( Dest );

	// Make sure any async loads have completed and moved the data into BulkData
	FlushAsyncLoading();

	// Passed in memory is going to be used.
	if( *Dest )
	{
		// The data is already loaded so we can simply use a mempcy.
		if( BulkData )
		{
			// Copy data into destination memory.
			FMemory::Memcpy( *Dest, BulkData.Get(), GetBulkDataSize() );
			// Discard internal copy if wanted and we're still attached to an archive or if we're
			// single use bulk data.
			if( bDiscardInternalCopy && (CanLoadFromDisk() || (BulkDataFlags & BULKDATA_SingleUse)) )
			{
				BulkData.Deallocate();
			}
		}
		// Data isn't currently loaded so we need to load it from disk.
		else
		{
			LoadDataIntoMemory( *Dest );
		}
	}
	// Passed in memory is NULL so we need to allocate some.
	else
	{
		// The data is already loaded so we can simply use a mempcy.
		if( BulkData )
		{
			// If the internal copy should be discarded and we are still attached to an archive we can
			// simply "return" the already existing copy and NULL out the internal reference. We can
			// also do this if the data is single use like e.g. when uploading texture data.
			if( bDiscardInternalCopy && (CanLoadFromDisk()|| (BulkDataFlags & BULKDATA_SingleUse)) )
			{
				*Dest = BulkData.ReleaseWithoutDeallocating();
				ResetAsyncData();
			}
			// Can't/ Don't discard so we need to allocate and copy.
			else
			{
				int32 BulkDataSize = GetBulkDataSize();
				if (BulkDataSize != 0)
				{
					// Allocate enough memory for data...
					*Dest = FMemory::Malloc( BulkDataSize, BulkDataAlignment );

					// ... and copy it into memory now pointed to by out parameter.
					FMemory::Memcpy( *Dest, BulkData.Get(), BulkDataSize );
				}
				else
				{
					*Dest = nullptr;
				}
			}
		}
		// Data isn't currently loaded so we need to load it from disk.
		else
		{
			int32 BulkDataSize = GetBulkDataSize();
			if (BulkDataSize != 0)
			{
				// Allocate enough memory for data...
				*Dest = FMemory::Malloc( BulkDataSize, BulkDataAlignment );

				// ... and directly load into it.
				LoadDataIntoMemory( *Dest );
			}
			else
			{
				*Dest = nullptr;
			}
		}
	}
}

/**
 * Locks the bulk data and returns a pointer to it.
 *
 * @param	LockFlags	Flags determining lock behavior
 */
void* FUntypedBulkData::Lock( uint32 LockFlags )
{
	check( LockStatus == LOCKSTATUS_Unlocked );
	
	// Make sure bulk data is loaded.
	MakeSureBulkDataIsLoaded();
		
	// Read-write operations are allowed on returned memory.
	if( LockFlags & LOCK_READ_WRITE )
	{
		LockStatus = LOCKSTATUS_ReadWriteLock;

#if WITH_EDITOR
		// We need to detach from the archive to not be able to clobber changes by serializing
		// over them.
		if( AttachedAr )
		{
			// Detach bulk data. This will call DetachFromArchive which in turn will clear AttachedAr.
			AttachedAr->DetachBulkData( this, false );
			check( AttachedAr == NULL );
		}
#endif // WITH_EDITOR
	}
	// Only read operations are allowed on returned memory.
	else if( LockFlags & LOCK_READ_ONLY )
	{
		LockStatus = LOCKSTATUS_ReadOnlyLock;
	}
	else
	{
		UE_LOG(LogSerialization, Fatal,TEXT("Unknown lock flag %i"),LockFlags);
	}

	return BulkData.Get();
}

const void* FUntypedBulkData::LockReadOnly() const
{
	check(LockStatus == LOCKSTATUS_Unlocked);
	
	FUntypedBulkData* mutable_this = const_cast<FUntypedBulkData*>(this);

	// Make sure bulk data is loaded.
	mutable_this->MakeSureBulkDataIsLoaded();

	// Only read operations are allowed on returned memory.
	mutable_this->LockStatus = LOCKSTATUS_ReadOnlyLock;

	check(BulkData);
	return BulkData.Get();
}

/**
 * Change size of locked bulk data. Only valid if locked via read-write lock.
 *
 * @param InElementCount	Number of elements array should be resized to
 */
void* FUntypedBulkData::Realloc( int32 InElementCount )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::Realloc"), STAT_UBD_Realloc, STATGROUP_Memory);

	check( LockStatus == LOCKSTATUS_ReadWriteLock );
	// Progate element count and reallocate data based on new size.
	ElementCount	= InElementCount;
	BulkData.Reallocate( GetBulkDataSize(), BulkDataAlignment );
	return BulkData.Get();
}

/** 
 * Unlocks bulk data after which point the pointer returned by Lock no longer is valid.
 */
void FUntypedBulkData::Unlock() const
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::Unlock"), STAT_UBD_Unlock, STATGROUP_Memory);

	check(LockStatus != LOCKSTATUS_Unlocked);

	FUntypedBulkData* mutable_this = const_cast<FUntypedBulkData*>(this);

	mutable_this->LockStatus = LOCKSTATUS_Unlocked;

	// Free pointer if we're guaranteed to only to access the data once.
	if (BulkDataFlags & BULKDATA_SingleUse)
	{
		mutable_this->BulkData.Deallocate();
	}
}

/**
 * Clears/ removes the bulk data and resets element count to 0.
 */
void FUntypedBulkData::RemoveBulkData()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::RemoveBulkData"), STAT_UBD_RemoveBulkData, STATGROUP_Memory);

	check( LockStatus == LOCKSTATUS_Unlocked );

#if WITH_EDITOR
	// Detach from archive without loading first.
	if( AttachedAr )
	{
		AttachedAr->DetachBulkData( this, false );
		check( AttachedAr == NULL );
	}
#endif // WITH_EDITOR
	
	// Resize to 0 elements.
	ElementCount	= 0;
	BulkData.Deallocate();
}

// FutureState implementation that loads everything when created.
struct FStateComplete : public TFutureState<bool>
{
public:
	FStateComplete(TFunction<void()> CompletionCallback) : TFutureState<bool>(MoveTemp(CompletionCallback)) { MarkComplete(); }
};

/**
* Load the bulk data using a file reader. Works when no archive is attached to the bulk data.
* @return Whether the operation succeeded.
*/
bool FUntypedBulkData::LoadBulkDataWithFileReader()
{
#if WITH_EDITOR
	if (!BulkData && GIsEditor && !GEventDrivenLoaderEnabled && !SerializeFuture.IsValid())
	{
		SerializeFuture = TFuture<bool>(TSharedPtr<TFutureState<bool>, ESPMode::ThreadSafe>(new FStateComplete([=]() 
		{ 
			AsyncLoadBulkData();
			return true; 
		})));
		return (bool)BulkDataAsync;
	}
#endif
	return false;
}

/**
 * Forces the bulk data to be resident in memory and detaches the archive.
 */
void FUntypedBulkData::ForceBulkDataResident()
{
	// Make sure bulk data is loaded.
	MakeSureBulkDataIsLoaded();

#if WITH_EDITOR
	// Detach from the archive 
	if( AttachedAr )
	{
		// Detach bulk data. This will call DetachFromArchive which in turn will clear AttachedAr.
		AttachedAr->DetachBulkData( this, false );
		check( AttachedAr == NULL );
	}
#endif // WITH_EDITOR
}

/**
 * Sets the passed in bulk data flags.
 *
 * @param BulkDataFlagsToSet	Bulk data flags to set
 */
void FUntypedBulkData::SetBulkDataFlags( uint32 BulkDataFlagsToSet )
{
	BulkDataFlags |= BulkDataFlagsToSet;
}

/**
* Gets the current bulk data flags.
*
* @return Bulk data flags currently set
*/
uint32 FUntypedBulkData::GetBulkDataFlags() const
{
	return BulkDataFlags;
}

/**
 * Sets the passed in bulk data alignment.
 *
 * @param BulkDataAlignmentToSet	Bulk data alignment to set
 */
void FUntypedBulkData::SetBulkDataAlignment( uint32 BulkDataAlignmentToSet )
{
	BulkDataAlignment = BulkDataAlignmentToSet;
}

/**
* Gets the current bulk data alignment.
*
* @return Bulk data alignment currently set
*/
uint32 FUntypedBulkData::GetBulkDataAlignment() const
{
	return BulkDataAlignment;
}

/**
 * Clears the passed in bulk data flags.
 *
 * @param BulkDataFlagsToClear	Bulk data flags to clear
 */
void FUntypedBulkData::ClearBulkDataFlags( uint32 BulkDataFlagsToClear )
{
	BulkDataFlags &= ~BulkDataFlagsToClear;
}

/**
 * Load the resource data in the BulkDataAsync
 *
 * @param BulkDataFlagsToClear	Bulk data flags to clear
 */
void FUntypedBulkData::AsyncLoadBulkData()
{
	BulkDataAsync.Reallocate(GetBulkDataSize(), BulkDataAlignment);

	UE_CLOG(GEventDrivenLoaderEnabled, LogSerialization, Error, TEXT("Attempt to stream bulk data with EDL enabled. This is not desireable. File %s"), *Filename);

	FArchive* FileReaderAr = IFileManager::Get().CreateFileReader(*Filename, FILEREAD_Silent);
	checkf(FileReaderAr != NULL, TEXT("Attempted to load bulk data from an invalid filename '%s'."), *Filename);

	// Seek to the beginning of the bulk data in the file.
	FileReaderAr->Seek(BulkDataOffsetInFile);
	SerializeBulkData(*FileReaderAr, BulkDataAsync.Get());
	delete FileReaderAr;
}


/*-----------------------------------------------------------------------------
	Serialization.
-----------------------------------------------------------------------------*/

void FUntypedBulkData::StartSerializingBulkData(FArchive& Ar, UObject* Owner, int32 Idx, bool bPayloadInline)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::StartSerializingBulkData"), STAT_UBD_StartSerializingBulkData, STATGROUP_Memory);
	check(SerializeFuture.IsValid() == false);	

	SerializeFuture = Async<bool>(EAsyncExecution::ThreadPool, [=]() 
	{ 
		AsyncLoadBulkData(); 
		return true;
	});

	// Skip bulk data in this archive
	if (bPayloadInline)
	{
		Ar.Seek(Ar.Tell() + BulkDataSizeOnDisk);
	}
}

int32 GMinimumBulkDataSizeForAsyncLoading = 131072;
static FAutoConsoleVariableRef CVarMinimumBulkDataSizeForAsyncLoading(
	TEXT("s.MinBulkDataSizeForAsyncLoading"),
	GMinimumBulkDataSizeForAsyncLoading,
	TEXT("Minimum time the time limit exceeded warning will be triggered by."),
	ECVF_Default
	);

bool FUntypedBulkData::ShouldStreamBulkData()
{
	if (GEventDrivenLoaderEnabled && !(BulkDataFlags&BULKDATA_PayloadAtEndOfFile))
	{
		return false; // if it is inline, it is already precached, so use it
	}

	if (GEventDrivenLoaderEnabled)
	{
		const bool bSeperateFile = !!(BulkDataFlags&BULKDATA_PayloadInSeperateFile);

		if (!bSeperateFile)
		{
			check(!"Bulk data should either be inline or stored in a separate file for the new uobject loader.");
			return false; // if it is not in a separate file, then we can't easily find the correct offset in the uexp file; we don't want this case anyway!
		}
	}

	const bool bForceStream = !!(BulkDataFlags & BULKDATA_ForceStreamPayload);

	return (FPlatformProperties::RequiresCookedData() && !Filename.IsEmpty() &&
		FPlatformProcess::SupportsMultithreading() && IsInGameThread() &&
		(bForceStream || GetBulkDataSize() > GMinimumBulkDataSizeForAsyncLoading) &&
		GMinimumBulkDataSizeForAsyncLoading >= 0);
}

/**
* Serialize function used to serialize this bulk data structure.
*
* @param Ar	Archive to serialize with
* @param Owner	Object owning the bulk data
* @param Idx	Index of bulk data item being serialized
*/
void FUntypedBulkData::Serialize( FArchive& Ar, UObject* Owner, int32 Idx )
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::Serialize"), STAT_UBD_Serialize, STATGROUP_Memory);

	check( LockStatus == LOCKSTATUS_Unlocked );

	if(Ar.IsTransacting())
	{
		// Special case for transacting bulk data arrays.

		// constructing the object during load will save it to the transaction buffer. If it tries to load the bulk data now it will try to break it.
		bool bActuallySave = Ar.IsSaving() && (!Owner || !Owner->HasAnyFlags(RF_NeedLoad));

		Ar << bActuallySave;

		if (bActuallySave)
		{
			if(Ar.IsLoading())
			{
				// Flags for bulk data.
				Ar << BulkDataFlags;
				// Number of elements in array.
				Ar << ElementCount;

				// Allocate bulk data.
				BulkData.Reallocate( GetBulkDataSize(), BulkDataAlignment );

				// Deserialize bulk data.
				SerializeBulkData( Ar, BulkData.Get() );
			}
			else if(Ar.IsSaving())
			{
				// Flags for bulk data.
				Ar << BulkDataFlags;
				// Number of elements in array.
				Ar << ElementCount;

				// Don't attempt to load or serialize BulkData if the current size is 0.
				// This could be a newly constructed BulkData that has not yet been loaded, 
				// and allocating 0 bytes now will cause a crash when we load.
				if (GetBulkDataSize() > 0)
				{
					// Make sure bulk data is loaded.
					MakeSureBulkDataIsLoaded();

					// Serialize bulk data.
					SerializeBulkData(Ar, BulkData.Get());
				}
			}
		}
	}
	else if( Ar.IsPersistent() && !Ar.IsObjectReferenceCollector() && !Ar.ShouldSkipBulkData() )
	{
#if TRACK_BULKDATA_USE
		FThreadSafeBulkDataToObjectMap::Get().Add( this, Owner );
#endif
		// Offset where the bulkdata flags are stored
		int64 SavedBulkDataFlagsPos = Ar.Tell();
		{
			FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);
			Ar << BulkDataFlags;
		}


		// Number of elements in array.
		Ar << ElementCount;
		
		// We're loading from the persistent archive.
		if( Ar.IsLoading() )
		{
			Filename = TEXT("");
			
			// @todo when Landscape (and others?) only Lock/Unlock once, we can enable this
			if (false) // FPlatformProperties::RequiresCookedData())
			{
				// Bulk data that is being serialized via seekfree loading is single use only. This allows us 
				// to free the memory as e.g. the bulk data won't be attached to an archive in the case of
				// seek free loading.
				BulkDataFlags |= BULKDATA_SingleUse;
			}

			// Hacky fix for using cooked data in editor. Cooking sets BULKDATA_SingleUse for textures, but PIEing needs to keep bulk data around.
			if (GIsEditor)
			{
				BulkDataFlags &= ~BULKDATA_SingleUse;
			}

			// Size on disk, which in the case of compression is != GetBulkDataSize()
			Ar << BulkDataSizeOnDisk;
			
			Ar << BulkDataOffsetInFile;

			// determine whether the payload is stored inline or at the end of the file
			const bool bPayloadInline = !(BulkDataFlags&BULKDATA_PayloadAtEndOfFile);

			// fix up the file offset, but only if not stored inline
			if (Owner != NULL && Owner->GetLinker() && !bPayloadInline)
			{
				BulkDataOffsetInFile += Owner->GetLinker()->Summary.BulkDataStartOffset;
			}

			// We're allowing defered serialization.
			if( Ar.IsAllowingLazyLoading() && Owner != NULL)
			{				
#if WITH_EDITOR
				Linker = Owner->GetLinker();
				check(Linker);
				Ar.AttachBulkData( Owner, this );
				AttachedAr = &Ar;
				Filename = Linker->Filename;
#else
				Package = Owner->GetOutermost();
				check(Package.IsValid());
				auto Linker = FLinkerLoad::FindExistingLinkerForPackage(Package.Get());
				check(Linker);
				Filename = Linker->Filename;		
#endif // WITH_EDITOR
				if (bPayloadInline)
				{
					if (ShouldStreamBulkData())
					{
						// Start serializing immediately
						StartSerializingBulkData(Ar, Owner, Idx, bPayloadInline);
					}
					else
					{
						// Force non-lazy loading of inline bulk data to prevent PostLoad spikes.
						BulkData.Reallocate(GetBulkDataSize(), BulkDataAlignment);
						// if the payload is stored inline, just serialize it
						SerializeBulkData(Ar, BulkData.Get());
					}
				}
				else if (BulkDataFlags & BULKDATA_PayloadInSeperateFile)
				{
					Filename = FPaths::ChangeExtension(Filename, TEXT(".ubulk"));
				}
			}
			// Serialize the bulk data right away.
			else
			{
				if (Owner && Owner->GetLinker())
				{
					Filename = Owner->GetLinker()->Filename;
				}
				if (ShouldStreamBulkData())
				{
					StartSerializingBulkData(Ar, Owner, Idx, bPayloadInline);
				}
				else
				{
					BulkData.Reallocate( GetBulkDataSize(), BulkDataAlignment );

					if (bPayloadInline)
					{
						// if the payload is stored inline, just serialize it
						SerializeBulkData( Ar, BulkData.Get() );
					}
					else
					{
						// if the payload is NOT stored inline ...
						if (BulkDataFlags & BULKDATA_PayloadInSeperateFile)
						{
							// open seperate bulk data file
							UE_CLOG(GEventDrivenLoaderEnabled, LogSerialization, Error, TEXT("Attempt to sync load bulk data with EDL enabled (separate file). This is not desireable. File %s"), *Filename);

							if (GEventDrivenLoaderEnabled && (Filename.EndsWith(TEXT(".uasset")) || Filename.EndsWith(TEXT(".umap"))))
							{
								BulkDataOffsetInFile -= IFileManager::Get().FileSize(*Filename);
								check(BulkDataOffsetInFile >= 0);
								Filename = FPaths::GetBaseFilename(Filename, false) + TEXT(".uexp");
							}

							FArchive* TargetArchive = IFileManager::Get().CreateFileReader(*Filename);
							// seek to the location in the file where the payload is stored
							TargetArchive->Seek(BulkDataOffsetInFile);
							// serialize the payload
							SerializeBulkData(*TargetArchive, BulkData.Get());
							// cleanup file
							delete TargetArchive;
						}
						else
						{
							UE_CLOG(GEventDrivenLoaderEnabled, LogSerialization, Error, TEXT("Attempt to sync load bulk data with EDL enabled. This is not desireable. File %s"), *Filename);

							// store the current file offset
							int64 CurOffset = Ar.Tell();
							// seek to the location in the file where the payload is stored
							Ar.Seek(BulkDataOffsetInFile);
							// serialize the payload
							SerializeBulkData(Ar, BulkData.Get());
							// seek to the location we came from
							Ar.Seek(CurOffset);
						}
					}
  			}
			}
		}
		// We're saving to the persistent archive.
		else if( Ar.IsSaving() )
		{
			// Remove single element serialization requirement before saving out bulk data flags.
			BulkDataFlags &= ~BULKDATA_ForceSingleElementSerialization;

			// Make sure bulk data is loaded.
			MakeSureBulkDataIsLoaded();
			
			// Only serialize status information if wanted.
			int64 SavedBulkDataSizeOnDiskPos	= INDEX_NONE;
			int64 SavedBulkDataOffsetInFilePos	= INDEX_NONE;
			
			// Keep track of position we are going to serialize placeholder BulkDataSizeOnDisk.
			SavedBulkDataSizeOnDiskPos = Ar.Tell();
			BulkDataSizeOnDisk = INDEX_NONE;

			{
				FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);

				// And serialize the placeholder which is going to be overwritten later.
				Ar << BulkDataSizeOnDisk;
				// Keep track of position we are going to serialize placeholder BulkDataOffsetInFile.
				SavedBulkDataOffsetInFilePos = Ar.Tell();
				BulkDataOffsetInFile = INDEX_NONE;
				// And serialize the placeholder which is going to be overwritten later.
				Ar << BulkDataOffsetInFile;

			}

				// try to get the linkersave object
			FLinkerSave* LinkerSave = Cast<FLinkerSave>(Ar.GetLinker());

			// determine whether we are going to store the payload inline or not.
			bool bStoreInline = !!(BulkDataFlags&BULKDATA_ForceInlinePayload) || !LinkerSave;

			if (IsEventDrivenLoaderEnabledInCookedBuilds() && Ar.IsCooking() && !bStoreInline && !(BulkDataFlags&BULKDATA_Force_NOT_InlinePayload))
			{
				bStoreInline = true;
			}

			if (!bStoreInline)
			{
				// set the flag indicating where the payload is stored
				BulkDataFlags |= BULKDATA_PayloadAtEndOfFile;

				// with no LinkerSave we have to store the data inline
				check(LinkerSave != NULL);				
				
				// add the bulkdata storage info object to the linkersave
				int32 Index = LinkerSave->BulkDataToAppend.AddZeroed(1);
				FLinkerSave::FBulkDataStorageInfo& BulkStore = LinkerSave->BulkDataToAppend[Index];

				BulkStore.BulkDataOffsetInFilePos = SavedBulkDataOffsetInFilePos;
				BulkStore.BulkDataSizeOnDiskPos = SavedBulkDataSizeOnDiskPos;
				BulkStore.BulkDataFlagsPos = SavedBulkDataFlagsPos;
				BulkStore.BulkDataFlags = BulkDataFlags;
				BulkStore.BulkData = this;
				
				// Serialize bulk data into the storage info
				BulkDataSizeOnDisk = -1;
			}
			else
			{
				// set the flag indicating where the payload is stored
				BulkDataFlags &= ~BULKDATA_PayloadAtEndOfFile;

				int64 SavedBulkDataStartPos = Ar.Tell();

				// Serialize bulk data.
				SerializeBulkData( Ar, BulkData.Get() );
				// store the payload endpos
				int64 SavedBulkDataEndPos = Ar.Tell();

				checkf(SavedBulkDataStartPos >= 0 && SavedBulkDataEndPos >= 0,
					   TEXT("Bad archive positions for bulkdata. StartPos=%d EndPos=%d"),
					   SavedBulkDataStartPos, SavedBulkDataEndPos);

				BulkDataSizeOnDisk		= SavedBulkDataEndPos - SavedBulkDataStartPos;
				BulkDataOffsetInFile	= SavedBulkDataStartPos;
			}

			// store current file offset before seeking back
			int64 CurrentFileOffset = Ar.Tell();

			{
				FArchive::FScopeSetDebugSerializationFlags S(Ar, DSF_IgnoreDiff);

				// Seek back and overwrite the flags 
				Ar.Seek(SavedBulkDataFlagsPos);
				Ar << BulkDataFlags;

				// Seek back and overwrite placeholder for BulkDataSizeOnDisk
				Ar.Seek(SavedBulkDataSizeOnDiskPos);
				Ar << BulkDataSizeOnDisk;

				// Seek back and overwrite placeholder for BulkDataOffsetInFile
				Ar.Seek(SavedBulkDataOffsetInFilePos);
				Ar << BulkDataOffsetInFile;
			}

			// Seek to the end of written data so we don't clobber any data in subsequent write 
			// operations
			Ar.Seek(CurrentFileOffset);
		}
	}
}

/*-----------------------------------------------------------------------------
	Class specific virtuals.
-----------------------------------------------------------------------------*/

/**
 * Returns whether single element serialization is required given an archive. This e.g.
 * can be the case if the serialization for an element changes and the single element
 * serialization code handles backward compatibility.
 */
bool FUntypedBulkData::RequiresSingleElementSerialization( FArchive& Ar )
{
	return false;
}

/*-----------------------------------------------------------------------------
	Accessors for friend classes FLinkerLoad and content cookers.
-----------------------------------------------------------------------------*/

#if WITH_EDITOR
/**
 * Detaches the bulk data from the passed in archive. Needs to match the archive we are currently
 * attached to.
 *
 * @param Ar						Archive to detach from
 * @param bEnsureBulkDataIsLoaded	whether to ensure that bulk data is load before detaching from archive
 */
void FUntypedBulkData::DetachFromArchive( FArchive* Ar, bool bEnsureBulkDataIsLoaded )
{
	check( Ar );
	check( Ar == AttachedAr );

	// Make sure bulk data is loaded.
	if( bEnsureBulkDataIsLoaded )
	{
		MakeSureBulkDataIsLoaded();
	}

	// Detach from archive.
	AttachedAr = NULL;
	Linker = NULL;
}
#endif // WITH_EDITOR

/**
 * Sets whether we should store the data compressed on disk.
 *
 * @param CompressionFlags	Flags to use for compressing the data. Use COMPRESS_NONE for no compression, or something like COMPRESS_ZLIB to compress the data
 */
void FUntypedBulkData::StoreCompressedOnDisk( ECompressionFlags CompressionFlags )
{
	if( CompressionFlags != GetDecompressionFlags() )
	{
		//Need to force this to be resident so we don't try to load data as though it were compressed when it isn't.
		ForceBulkDataResident();

		if( CompressionFlags == COMPRESS_None )
		{
			// clear all compression settings
			BulkDataFlags &= ~BULKDATA_SerializeCompressed;
		}
		else
		{
			// make sure a valid compression format was specified
			check(CompressionFlags & COMPRESS_ZLIB);
			BulkDataFlags |= (CompressionFlags & COMPRESS_ZLIB) ? BULKDATA_SerializeCompressedZLIB : BULKDATA_None;

			// make sure we are not forcing the bulkdata to be stored inline if we use compression
			BulkDataFlags &= ~BULKDATA_ForceInlinePayload;
		}
	}
}


/*-----------------------------------------------------------------------------
	Internal helpers
-----------------------------------------------------------------------------*/

/**
 * Copies bulk data from passed in structure.
 *
 * @param	Other	Bulk data object to copy from.
 */
void FUntypedBulkData::Copy( const FUntypedBulkData& Other )
{
	// Only copy if there is something to copy.
	if( Other.GetElementCount() )
	{
		// Make sure src is loaded without calling Lock as the object is const.
		check(Other.BulkData);
		check(BulkData);
		check(ElementCount == Other.GetElementCount());
		// Copy from src to dest.
		FMemory::Memcpy( BulkData.Get(), Other.BulkData.Get(), Other.GetBulkDataSize() );
	}
}

/**
 * Helper function initializing all member variables.
 */
void FUntypedBulkData::InitializeMemberVariables()
{
	BulkDataFlags = BULKDATA_None;
	ElementCount = 0;
	BulkDataOffsetInFile = INDEX_NONE;
	BulkDataSizeOnDisk = INDEX_NONE;
	BulkDataAlignment = DEFAULT_ALIGNMENT;
	LockStatus = LOCKSTATUS_Unlocked;
#if WITH_EDITOR
	Linker = nullptr;
	AttachedAr = nullptr;
#else
	Package = nullptr;
#endif
}

void FUntypedBulkData::SerializeElements(FArchive& Ar, void* Data)
{
	// Serialize each element individually.				
	for (int32 ElementIndex = 0; ElementIndex < ElementCount; ElementIndex++)
	{
		SerializeElement(Ar, Data, ElementIndex);
	}
}

/**
 * Serialize just the bulk data portion to/ from the passed in memory.
 *
 * @param	Ar					Archive to serialize with
 * @param	Data				Memory to serialize either to or from
 */
void FUntypedBulkData::SerializeBulkData( FArchive& Ar, void* Data )
{
	// skip serializing of unused data
	if( BulkDataFlags & BULKDATA_Unused )
	{
		return;
	}

	// Skip serialization for bulk data of zero length
	const int32 BulkDataSize = GetBulkDataSize();
	if(BulkDataSize == 0)
	{
		return;
	}

	// Allow backward compatible serialization by forcing bulk serialization off if required. Saving also always uses single
	// element serialization so errors or oversight when changing serialization code is recoverable.
	bool bSerializeInBulk = true;
	if( RequiresSingleElementSerialization( Ar ) 
	// Set when serialized like a lazy array.
	|| (BulkDataFlags & BULKDATA_ForceSingleElementSerialization) 
	// We use bulk serialization even when saving 1 byte types (texture & sound bulk data) as an optimization for those.
	|| (Ar.IsSaving() && (GetElementSize() > 1) ) )
	{
		bSerializeInBulk = false;
	}

	// Raw serialize the bulk data without any possiblity for potential endian conversion.
	if( bSerializeInBulk )
	{
		// Serialize data compressed.
		if( BulkDataFlags & BULKDATA_SerializeCompressed )
		{
			Ar.SerializeCompressed( Data, GetBulkDataSize(), GetDecompressionFlags(), false, !!(BulkDataFlags&BULKDATA_SerializeCompressedBitWindow) );
		}
		// Uncompressed/ regular serialization.
		else
		{
			Ar.Serialize( Data, GetBulkDataSize() );
		}
	}
	// Serialize an element at a time via the virtual SerializeElement function potentialy allowing and dealing with 
	// endian conversion. Dealing with compression makes this a bit more complex as SerializeCompressed expects the 
	// full data to be compresed en block and not piecewise.
	else
	{
		// Serialize data compressed.
		if( BulkDataFlags & BULKDATA_SerializeCompressed )
		{
			// Placeholder for to be serialized data.
			TArray<uint8> SerializedData;
			
			// Loading, data is compressed in archive and needs to be decompressed.
			if( Ar.IsLoading() )
			{
				// Create space for uncompressed data.
				SerializedData.Empty( GetBulkDataSize() );
				SerializedData.AddUninitialized( GetBulkDataSize() );

				// Serialize data with passed in archive and compress.
				Ar.SerializeCompressed( SerializedData.GetData(), SerializedData.Num(), GetDecompressionFlags(), false, !!(BulkDataFlags&BULKDATA_SerializeCompressedBitWindow) );
				
				// Initialize memory reader with uncompressed data array and propagate forced byte swapping
				FMemoryReader MemoryReader( SerializedData, true );
				MemoryReader.SetByteSwapping( Ar.ForceByteSwapping() );

				// Serialize each element individually via memory reader.
				SerializeElements(MemoryReader, Data);
			}
			// Saving, data is uncompressed in memory and needs to be compressed.
			else if( Ar.IsSaving() )
			{			
				// Initialize memory writer with blank data array and propagate forced byte swapping
				FMemoryWriter MemoryWriter( SerializedData, true );
				MemoryWriter.SetByteSwapping( Ar.ForceByteSwapping() );

				// Serialize each element individually via memory writer.			
				SerializeElements(MemoryWriter, Data);

				// Serialize data with passed in archive and compress.
				Ar.SerializeCompressed( SerializedData.GetData(), SerializedData.Num(), GetDecompressionFlags(), false, !!(BulkDataFlags&BULKDATA_SerializeCompressedBitWindow) );
			}
		}
		// Uncompressed/ regular serialization.
		else
		{
			// We can use the passed in archive if we're not compressing the data.
			SerializeElements(Ar, Data);
		}
	}
}

/**
 * Loads the bulk data if it is not already loaded.
 */
void FUntypedBulkData::MakeSureBulkDataIsLoaded()
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::MakeSureBulkDataIsLoaded"), STAT_UBD_MakeSureBulkDataIsLoaded, STATGROUP_Memory);

	// Nothing to do if data is already loaded.
	if( !BulkData )
	{
		// Look for async request first
		if (SerializeFuture.IsValid())
		{
			WaitForAsyncLoading();
			BulkData = MoveTemp(BulkDataAsync);
			ResetAsyncData();
		}
		else
		{
			const int32 BytesNeeded = GetBulkDataSize();
			// Allocate memory for bulk data.
			BulkData.Reallocate(BytesNeeded, BulkDataAlignment);

			// Only load if there is something to load. E.g. we might have just created the bulk data array
			// in which case it starts out with a size of zero.
			if (BytesNeeded > 0)
			{
				LoadDataIntoMemory(BulkData.Get());
			}
		}
	}
}

void FUntypedBulkData::WaitForAsyncLoading()
{
	check(SerializeFuture.IsValid());
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FUntypedBulkData::WaitForAsyncLoading"), STAT_UBD_WaitForAsyncLoading, STATGROUP_Memory);
	while (!SerializeFuture.WaitFor(FTimespan::FromMilliseconds(1000.0)))
	{
		UE_LOG(LogSerialization, Warning, TEXT("Waiting for %s bulk data (%d) to be loaded longer than 1000ms"), *Filename, GetBulkDataSize());
	}
	check(BulkDataAsync);
}

bool FUntypedBulkData::FlushAsyncLoading()
{
	bool bIsLoadingAsync = SerializeFuture.IsValid();
	if (bIsLoadingAsync)
	{
		WaitForAsyncLoading();
		check(!BulkData);
		BulkData = MoveTemp(BulkDataAsync);
		ResetAsyncData();
	}
	return bIsLoadingAsync;
}

/**
 * Loads the data from disk into the specified memory block. This requires us still being attached to an
 * archive we can use for serialization.
 *
 * @param Dest Memory to serialize data into
 */
void FUntypedBulkData::LoadDataIntoMemory( void* Dest )
{
	// Try flushing async loading before attempting to load
	if (FlushAsyncLoading())
	{
		FMemory::Memcpy(Dest, BulkData.Get(), GetBulkDataSize());
		return;
	}

#if WITH_EDITOR
	checkf( AttachedAr, TEXT( "Attempted to load bulk data without an attached archive. Most likely the bulk data was loaded twice on console, which is not supported" ) );

	FArchive* BulkDataArchive = nullptr;
	if (Linker && Linker->GetFArchiveAsync2Loader() && Linker->GetFArchiveAsync2Loader()->IsCookedForEDLInEditor() &&
		(BulkDataFlags & BULKDATA_PayloadInSeperateFile))
	{
		// The attached archive is a package cooked for EDL loaded in the editor so the actual bulk data sits in a separate ubulk file.
		const FString BulkDataFilename = FPaths::ChangeExtension(Filename, TEXT(".ubulk"));
		BulkDataArchive = IFileManager::Get().CreateFileReader(*BulkDataFilename, FILEREAD_Silent);
	}

	if (!BulkDataArchive)
	{
		BulkDataArchive = AttachedAr;
	}

	// Keep track of current position in file so we can restore it later.
	int64 PushedPos = BulkDataArchive->Tell();
	// Seek to the beginning of the bulk data in the file.
	BulkDataArchive->Seek( BulkDataOffsetInFile );
		
	SerializeBulkData( *BulkDataArchive, Dest );

	// Restore file pointer.
	BulkDataArchive->Seek( PushedPos );
	BulkDataArchive->FlushCache();

	if (BulkDataArchive != AttachedAr)
	{
		delete BulkDataArchive;
		BulkDataArchive = nullptr;
	}

#else
	bool bWasLoadedSuccessfully = false;
	if ((IsInGameThread() || IsInAsyncLoadingThread()) && Package.IsValid() && Package->LinkerLoad && Package->LinkerLoad->GetOwnerThreadId() == FPlatformTLS::GetCurrentThreadId() && ((BulkDataFlags & BULKDATA_PayloadInSeperateFile) == 0))
	{
		FLinkerLoad* LinkerLoad = Package->LinkerLoad;
		if (LinkerLoad && LinkerLoad->Loader)
		{
			FArchive* Ar = LinkerLoad;
			// keep track of current position in this archive
			int64 CurPos = Ar->Tell();

			// Seek to the beginning of the bulk data in the file.
			Ar->Seek( BulkDataOffsetInFile );

			// serialize the bulk data
			SerializeBulkData( *Ar, Dest );

			// seek back to the position the archive was before
			Ar->Seek(CurPos);

			// note that we loaded it
			bWasLoadedSuccessfully = true;
		}
	}
	// if we weren't able to load via linker, load directly by filename
	if (!bWasLoadedSuccessfully)
	{
		// load from the specied filename when the linker has been cleared
		checkf( Filename != TEXT(""), TEXT( "Attempted to load bulk data without a proper filename." ) );
	
		UE_CLOG(GEventDrivenLoaderEnabled && !(IsInGameThread() || IsInAsyncLoadingThread()), LogSerialization, Error, TEXT("Attempt to sync load bulk data with EDL enabled (LoadDataIntoMemory). This is not desireable. File %s"), *Filename);

		if (GEventDrivenLoaderEnabled && (Filename.EndsWith(TEXT(".uasset")) || Filename.EndsWith(TEXT(".umap"))))
		{
			BulkDataOffsetInFile -= IFileManager::Get().FileSize(*Filename);
			check(BulkDataOffsetInFile >= 0);
			Filename = FPaths::GetBaseFilename(Filename, false) + TEXT(".uexp");
		}

		FArchive* Ar = IFileManager::Get().CreateFileReader(*Filename, FILEREAD_Silent);
		checkf( Ar != NULL, TEXT( "Attempted to load bulk data from an invalid filename '%s'." ), *Filename );
	
		// Seek to the beginning of the bulk data in the file.
		Ar->Seek( BulkDataOffsetInFile );
		SerializeBulkData( *Ar, Dest );
		delete Ar;
	}
#endif // WITH_EDITOR
}



/*-----------------------------------------------------------------------------
	uint8 version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
int32 FByteBulkData::GetElementSize() const
{
	return sizeof(uint8);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FByteBulkData::SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
{
	uint8& ByteData = *((uint8*)Data + ElementIndex);
	Ar << ByteData;
}

/*-----------------------------------------------------------------------------
	uint16 version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
int32 FWordBulkData::GetElementSize() const
{
	return sizeof(uint16);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FWordBulkData::SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
{
	uint16& WordData = *((uint16*)Data + ElementIndex);
	Ar << WordData;
}

/*-----------------------------------------------------------------------------
	int32 version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
int32 FIntBulkData::GetElementSize() const
{
	return sizeof(int32);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FIntBulkData::SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
{
	int32& IntData = *((int32*)Data + ElementIndex);
	Ar << IntData;
}

/*-----------------------------------------------------------------------------
	float version of bulk data.
-----------------------------------------------------------------------------*/

/**
 * Returns size in bytes of single element.
 *
 * @return Size in bytes of single element
 */
int32 FFloatBulkData::GetElementSize() const
{
	return sizeof(float);
}

/**
 * Serializes an element at a time allowing and dealing with endian conversion and backward compatiblity.
 * 
 * @param Ar			Archive to serialize with
 * @param Data			Base pointer to data
 * @param ElementIndex	Element index to serialize
 */
void FFloatBulkData::SerializeElement( FArchive& Ar, void* Data, int32 ElementIndex )
{
	float& FloatData = *((float*)Data + ElementIndex);
	Ar << FloatData;
}

void FFormatContainer::Serialize(FArchive& Ar, UObject* Owner, const TArray<FName>* FormatsToSave, bool bSingleUse, uint32 InAlignment)
{
	if (Ar.IsLoading())
	{
		int32 NumFormats = 0;
		Ar << NumFormats;
		for (int32 Index = 0; Index < NumFormats; Index++)
		{
			FName Name;
			Ar << Name;
			FByteBulkData& Bulk = GetFormat(Name);
			Bulk.SetBulkDataAlignment(InAlignment);
			Bulk.Serialize(Ar, Owner);
		}
	}
	else
	{
		check(Ar.IsCooking() && FormatsToSave); // this thing is for cooking only, and you need to provide a list of formats

		int32 NumFormats = 0;
		for (TMap<FName, FByteBulkData*>:: TIterator It(Formats); It; ++It)
		{
			if (FormatsToSave->Contains(It.Key()) && It.Value()->GetBulkDataSize() > 0)
			{
				NumFormats++;
			}
		}
		Ar << NumFormats;
		for (TMap<FName, FByteBulkData*>:: TIterator It(Formats); It; ++It)
		{
			if (FormatsToSave->Contains(It.Key()) && It.Value()->GetBulkDataSize() > 0)
			{
				NumFormats--;
				FName Name = It.Key();
				Ar << Name;
				FByteBulkData* Bulk = It.Value();
				check(Bulk);
				// Force this kind of bulk data (physics, etc) to be stored inline for streaming
				const uint32 OldBulkDataFlags = Bulk->GetBulkDataFlags();
				Bulk->SetBulkDataFlags(bSingleUse ? (BULKDATA_ForceInlinePayload | BULKDATA_SingleUse) : BULKDATA_ForceInlinePayload);				
				Bulk->Serialize(Ar, Owner);
				Bulk->ClearBulkDataFlags(0xFFFFFFFF);
				Bulk->SetBulkDataFlags(OldBulkDataFlags);
			}
		}
		check(NumFormats == 0);
	}
}


