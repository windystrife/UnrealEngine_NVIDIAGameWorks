// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnArchive.cpp: Core archive classes.
=============================================================================*/
#include "Serialization/Archive.h"
#include "Math/UnrealMathUtility.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "Misc/Parse.h"
#include "UObject/ObjectVersion.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/NameAsStringProxyArchive.h"
#include "Misc/CommandLine.h"
#include "Internationalization/Text.h"
#include "Stats/StatsMisc.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Serialization/CustomVersion.h"
#include "Misc/EngineVersion.h"
#include "Misc/NetworkVersion.h"
#include "Interfaces/ITargetPlatform.h"
#include "Serialization/CompressedChunkInfo.h"

/*-----------------------------------------------------------------------------
	FArchive implementation.
-----------------------------------------------------------------------------*/

FArchive::FArchive()
{
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	ActiveFPLB = &InlineFPLB;
#endif
	CustomVersionContainer = nullptr;

#if USE_STABLE_LOCALIZATION_KEYS
	LocalizationNamespacePtr = nullptr;
#endif // USE_STABLE_LOCALIZATION_KEYS

	Reset();
}

FArchive::FArchive(const FArchive& ArchiveToCopy)
{
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	ActiveFPLB = &InlineFPLB;
#endif
#if USE_STABLE_LOCALIZATION_KEYS
	LocalizationNamespacePtr = nullptr;
#endif // USE_STABLE_LOCALIZATION_KEYS

	CopyTrivialFArchiveStatusMembers(ArchiveToCopy);

	// Don't know why this is set to false, but this is what the original copying code did
	ArIsFilterEditorOnly  = false;

	bCustomVersionsAreReset = ArchiveToCopy.bCustomVersionsAreReset;
	if (ArchiveToCopy.CustomVersionContainer)
	{
		CustomVersionContainer = new FCustomVersionContainer(*ArchiveToCopy.CustomVersionContainer);
	}
	else
	{
		CustomVersionContainer = nullptr;
	}
}

FArchive& FArchive::operator=(const FArchive& ArchiveToCopy)
{
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	ActiveFPLB = &InlineFPLB;
	ActiveFPLB->Reset();
#endif
	CopyTrivialFArchiveStatusMembers(ArchiveToCopy);

	// Don't know why this is set to false, but this is what the original copying code did
	ArIsFilterEditorOnly  = false;

	bCustomVersionsAreReset = ArchiveToCopy.bCustomVersionsAreReset;
	if (ArchiveToCopy.CustomVersionContainer)
	{
		if (!CustomVersionContainer)
		{
			CustomVersionContainer = new FCustomVersionContainer(*ArchiveToCopy.CustomVersionContainer);
		}
		else
		{
			*CustomVersionContainer = *ArchiveToCopy.CustomVersionContainer;
		}
	}
	else if (CustomVersionContainer)
	{
		delete CustomVersionContainer;
		CustomVersionContainer = nullptr;
	}

	return *this;
}

FArchive::~FArchive()
{
	delete CustomVersionContainer;

#if USE_STABLE_LOCALIZATION_KEYS
	delete LocalizationNamespacePtr;
#endif // USE_STABLE_LOCALIZATION_KEYS
}

// Resets all of the base archive members
void FArchive::Reset()
{
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	ActiveFPLB->Reset();
#endif
	ArUE4Ver							= GPackageFileUE4Version;
	ArLicenseeUE4Ver					= GPackageFileLicenseeUE4Version;
	ArEngineVer							= FEngineVersion::Current();
	ArEngineNetVer						= FNetworkVersion::GetEngineNetworkProtocolVersion();
	ArGameNetVer						= FNetworkVersion::GetGameNetworkProtocolVersion();
	ArIsLoading							= false;
	ArIsSaving							= false;
	ArIsTransacting						= false;
	ArWantBinaryPropertySerialization	= false;
	ArForceUnicode						= false;
	ArIsPersistent						= false;
	ArIsError							= false;
	ArIsCriticalError					= false;
	ArContainsCode						= false;
	ArContainsMap						= false;
	ArRequiresLocalizationGather		= false;
	ArForceByteSwapping					= false;
	ArSerializingDefaults				= false;
	ArIgnoreArchetypeRef				= false;
	ArNoDelta							= false;
	ArIgnoreOuterRef					= false;
	ArIgnoreClassGeneratedByRef			= false;
	ArIgnoreClassRef					= false;
	ArAllowLazyLoading					= false;
	ArIsObjectReferenceCollector		= false;
	ArIsModifyingWeakAndStrongReferences= false;
	ArIsCountingMemory					= false;
	ArPortFlags							= 0;
	ArShouldSkipBulkData				= false;
	ArMaxSerializeSize					= 0;
	ArIsFilterEditorOnly				= false;
	ArIsSaveGame						= false;
	ArCustomPropertyList				= nullptr;
	ArUseCustomPropertyList				= false;
	CookingTargetPlatform = nullptr;
	SerializedProperty = nullptr;
#if WITH_EDITORONLY_DATA
	EditorOnlyPropertyStack = 0;
#endif
#if USE_STABLE_LOCALIZATION_KEYS
	SetBaseLocalizationNamespace(FString());
#endif // USE_STABLE_LOCALIZATION_KEYS
#if WITH_EDITOR
	ArDebugSerializationFlags			= 0;
#endif
	// Reset all custom versions to the current registered versions.
	ResetCustomVersions();
}

void FArchive::CopyTrivialFArchiveStatusMembers(const FArchive& ArchiveToCopy)
{
	ArUE4Ver                             = ArchiveToCopy.ArUE4Ver;
	ArLicenseeUE4Ver                     = ArchiveToCopy.ArLicenseeUE4Ver;
	ArEngineVer                          = ArchiveToCopy.ArEngineVer;
	ArEngineNetVer						 = ArchiveToCopy.ArEngineNetVer;
	ArGameNetVer						 = ArchiveToCopy.ArGameNetVer;
	ArIsLoading                          = ArchiveToCopy.ArIsLoading;
	ArIsSaving                           = ArchiveToCopy.ArIsSaving;
	ArIsTransacting                      = ArchiveToCopy.ArIsTransacting;
	ArWantBinaryPropertySerialization    = ArchiveToCopy.ArWantBinaryPropertySerialization;
	ArForceUnicode                       = ArchiveToCopy.ArForceUnicode;
	ArIsPersistent                       = ArchiveToCopy.ArIsPersistent;
	ArIsError                            = ArchiveToCopy.ArIsError;
	ArIsCriticalError                    = ArchiveToCopy.ArIsCriticalError;
	ArContainsCode                       = ArchiveToCopy.ArContainsCode;
	ArContainsMap                        = ArchiveToCopy.ArContainsMap;
	ArRequiresLocalizationGather		 = ArchiveToCopy.ArRequiresLocalizationGather;
	ArForceByteSwapping                  = ArchiveToCopy.ArForceByteSwapping;
	ArSerializingDefaults                = ArchiveToCopy.ArSerializingDefaults;
	ArIgnoreArchetypeRef                 = ArchiveToCopy.ArIgnoreArchetypeRef;
	ArNoDelta                            = ArchiveToCopy.ArNoDelta;
	ArIgnoreOuterRef                     = ArchiveToCopy.ArIgnoreOuterRef;
	ArIgnoreClassGeneratedByRef			 = ArchiveToCopy.ArIgnoreClassGeneratedByRef;
	ArIgnoreClassRef                     = ArchiveToCopy.ArIgnoreClassRef;
	ArAllowLazyLoading                   = ArchiveToCopy.ArAllowLazyLoading;
	ArIsObjectReferenceCollector         = ArchiveToCopy.ArIsObjectReferenceCollector;
	ArIsModifyingWeakAndStrongReferences = ArchiveToCopy.ArIsModifyingWeakAndStrongReferences;
	ArIsCountingMemory                   = ArchiveToCopy.ArIsCountingMemory;
	ArPortFlags                          = ArchiveToCopy.ArPortFlags;
	ArShouldSkipBulkData                 = ArchiveToCopy.ArShouldSkipBulkData;
	ArMaxSerializeSize                   = ArchiveToCopy.ArMaxSerializeSize;
	ArIsFilterEditorOnly                 = ArchiveToCopy.ArIsFilterEditorOnly;
	ArIsSaveGame                         = ArchiveToCopy.ArIsSaveGame;
	ArCustomPropertyList				 = ArchiveToCopy.ArCustomPropertyList;
	ArUseCustomPropertyList				 = ArchiveToCopy.ArUseCustomPropertyList;
	CookingTargetPlatform                = ArchiveToCopy.CookingTargetPlatform;
	SerializedProperty = ArchiveToCopy.SerializedProperty;
#if WITH_EDITORONLY_DATA
	EditorOnlyPropertyStack = ArchiveToCopy.EditorOnlyPropertyStack;
#endif
#if USE_STABLE_LOCALIZATION_KEYS
	SetBaseLocalizationNamespace(ArchiveToCopy.GetBaseLocalizationNamespace());
#endif // USE_STABLE_LOCALIZATION_KEYS
}

/**
 * Returns the name of the Archive.  Useful for getting the name of the package a struct or object
 * is in when a loading error occurs.
 *
 * This is overridden for the specific Archive Types
 **/
FString FArchive::GetArchiveName() const
{
	return TEXT("FArchive");
}

#if USE_STABLE_LOCALIZATION_KEYS
void FArchive::SetBaseLocalizationNamespace(const FString& InLocalizationNamespace)
{
	if (InLocalizationNamespace.IsEmpty())
	{
		delete LocalizationNamespacePtr;
		LocalizationNamespacePtr = nullptr;
	}
	else
	{
		if (!LocalizationNamespacePtr)
		{
			LocalizationNamespacePtr = new FString();
		}
		*LocalizationNamespacePtr = InLocalizationNamespace;
	}
}
FString FArchive::GetBaseLocalizationNamespace() const
{
	return LocalizationNamespacePtr ? *LocalizationNamespacePtr : FString();
}
void FArchive::SetLocalizationNamespace(const FString& InLocalizationNamespace)
{
	SetBaseLocalizationNamespace(InLocalizationNamespace);
}
FString FArchive::GetLocalizationNamespace() const
{
	return GetBaseLocalizationNamespace();
}
#endif // USE_STABLE_LOCALIZATION_KEYS

#if WITH_EDITOR
FArchive::FScopeAddDebugData::FScopeAddDebugData(FArchive& InAr, const FName& DebugData) : Ar(InAr)
{
	Ar.PushDebugDataString(DebugData);
}

void FArchive::PushDebugDataString(const FName& DebugData)
{
}
#endif

FArchive& FArchive::operator<<( FText& Value )
{
	FText::SerializeText(*this, Value);
	return *this;
}

FArchive& FArchive::operator<<(struct FLazyObjectPtr& Value)
{
	// The base FArchive does not implement this method. Use FArchiveUObject instead.
	UE_LOG(LogSerialization, Fatal, TEXT("FArchive does not support FLazyObjectPtr serialization. Use FArchiveUObject instead."));
	return *this;
}

FArchive& FArchive::operator<<(struct FSoftObjectPtr& Value)
{
	// The base FArchive does not implement this method. Use FArchiveUObject instead.
	UE_LOG(LogSerialization, Fatal, TEXT("FArchive does not support FSoftObjectPtr serialization. Use FArchiveUObject instead."));
	return *this;
}

FArchive& FArchive::operator<<(struct FSoftObjectPath& Value)
{
	// The base FArchive does not implement this method. Use FArchiveUObject instead.
	UE_LOG(LogSerialization, Fatal, TEXT("FArchive does not support FSoftObjectPath serialization. Use FArchiveUObject instead."));
	return *this;
}

FArchive& FArchive::operator<<(struct FWeakObjectPtr& Value)
{
	// The base FArchive does not implement this method. Use FArchiveUObject instead.
	UE_LOG(LogSerialization, Fatal, TEXT("FArchive does not support FWeakObjectPtr serialization. Use FArchiveUObject instead."));
	return *this;
}

#if WITH_EDITOR
void FArchive::SerializeBool( bool& D )
{
	// Serialize bool as if it were UBOOL (legacy, 32 bit int).
	uint32 OldUBoolValue;
#if DEVIRTUALIZE_FLinkerLoad_Serialize
	const uint8 * RESTRICT Src = this->ActiveFPLB->StartFastPathLoadBuffer;
	if (Src + sizeof(uint32) <= this->ActiveFPLB->EndFastPathLoadBuffer)
	{
#if PLATFORM_SUPPORTS_UNALIGNED_INT_LOADS
		OldUBoolValue = *(uint32* RESTRICT)Src;
#else
		static_assert(sizeof(uint32) == 4, "assuming sizeof(uint32) == 4");
		OldUBoolValue = Src[0] | Src[1] | Src[2] | Src[3];
#endif
		this->ActiveFPLB->StartFastPathLoadBuffer += 4;
	}
	else
#endif
	{
		OldUBoolValue = D ? 1 : 0;
		this->Serialize(&OldUBoolValue, sizeof(OldUBoolValue));
	}
	if (OldUBoolValue > 1)
	{
		UE_LOG( LogSerialization, Error, TEXT("Invalid boolean encountered while reading archive - stream is most likely corrupted."));

		this->ArIsError = true;
	}
	D = !!OldUBoolValue;
}
#endif

const FCustomVersionContainer& FArchive::GetCustomVersions() const
{
	if (!CustomVersionContainer)
	{
		CustomVersionContainer = new FCustomVersionContainer;
	}

	if (bCustomVersionsAreReset)
	{
		bCustomVersionsAreReset = false;

		// If the archive is for reading then we want to use currently registered custom versions, otherwise we expect
		// serialization code to use UsingCustomVersion to populate the container.
		if (ArIsLoading)
		{
			*CustomVersionContainer = FCustomVersionContainer::GetRegistered();
		}
		else
		{
			CustomVersionContainer->Empty();
		}
	}

	return *CustomVersionContainer;
}

void FArchive::SetCustomVersions(const FCustomVersionContainer& NewVersions)
{
	if (!CustomVersionContainer)
	{
		CustomVersionContainer = new FCustomVersionContainer(NewVersions);
	}
	else
	{
		*CustomVersionContainer = NewVersions;
	}
	bCustomVersionsAreReset = false;
}

void FArchive::ResetCustomVersions()
{
	bCustomVersionsAreReset = true;
}

void FArchive::UsingCustomVersion(const FGuid& Key)
{
	// If we're loading, we want to use the version that the archive was serialized with, not register a new one.
	if (IsLoading())
	{
		return;
	}

	auto* RegisteredVersion = FCustomVersionContainer::GetRegistered().GetVersion(Key);

	// Ensure that the version has already been registered.
	// If this fails, you probably don't have an FCustomVersionRegistration variable defined for this GUID.
	check(RegisteredVersion);

	const_cast<FCustomVersionContainer&>(GetCustomVersions()).SetVersion(Key, RegisteredVersion->Version, RegisteredVersion->GetFriendlyName());
}

int32 FArchive::CustomVer(const FGuid& Key) const
{
	auto* CustomVersion = GetCustomVersions().GetVersion(Key);

	// If this fails, you have forgotten to make an Ar.UsingCustomVersion call
	// before serializing your custom version-dependent object.
	check(IsLoading() || CustomVersion);

	return CustomVersion ? CustomVersion->Version : -1;
}

void FArchive::SetCustomVersion(const FGuid& Key, int32 Version, FName FriendlyName)
{
	const_cast<FCustomVersionContainer&>(GetCustomVersions()).SetVersion(Key, Version, FriendlyName);
}

FString FArchiveProxy::GetArchiveName() const
{
	return InnerArchive.GetArchiveName();
}

#if USE_STABLE_LOCALIZATION_KEYS
void FArchiveProxy::SetLocalizationNamespace(const FString& InLocalizationNamespace)
{
	InnerArchive.SetLocalizationNamespace(InLocalizationNamespace);
}
FString FArchiveProxy::GetLocalizationNamespace() const
{
	return InnerArchive.GetLocalizationNamespace();
}
#endif // USE_STABLE_LOCALIZATION_KEYS

/**
 * Serialize the given FName as an FString
 */
FArchive& FNameAsStringProxyArchive::operator<<( class FName& N )
{
	if (IsLoading())
	{
		FString LoadedString;
		InnerArchive << LoadedString;
		N = FName(*LoadedString);
	}
	else
	{
		FString SavedString(N.ToString());
		InnerArchive << SavedString;
	}
	return *this;
}

/** Accumulative time spent in IsSaving portion of SerializeCompressed. */
CORE_API double GArchiveSerializedCompressedSavingTime = 0;



// MT compression disabled on console due to memory impact and lack of beneficial usage case.
#define WITH_MULTI_THREADED_COMPRESSION (WITH_EDITORONLY_DATA)
#if WITH_MULTI_THREADED_COMPRESSION
// Helper structure to keep information about async chunks that are in-flight.
class FAsyncCompressionChunk : public FNonAbandonableTask
{
public:
	/** Pointer to source (uncompressed) memory.								*/
	void* UncompressedBuffer;
	/** Pointer to destination (compressed) memory.								*/
	void* CompressedBuffer;
	/** Compressed size in bytes as passed to/ returned from compressor.		*/
	int32 CompressedSize;
	/** Uncompressed size in bytes as passed to compressor.						*/
	int32 UncompressedSize;
	/** Target platform for compressed data										*/
	int32 BitWindow;
	/** Flags to control compression											*/
	ECompressionFlags Flags;

	/**
	 * Constructor, zeros everything
	 */
	FAsyncCompressionChunk()
		: UncompressedBuffer(0)
		, CompressedBuffer(0)
		, CompressedSize(0)
		, UncompressedSize(0)
		, BitWindow(DEFAULT_ZLIB_BIT_WINDOW)
		, Flags(ECompressionFlags(0))
	{
	}
	/**
	 * Performs the async compression
	 */
	void DoWork()
	{
		// Compress from memory to memory.
		verify( FCompression::CompressMemory( Flags, CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, BitWindow) );
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncCompressionChunk, STATGROUP_ThreadPoolAsyncTasks);
	}
};
#endif		// WITH_MULTI_THREADED_COMPRESSION

/**
 * Serializes and compresses/ uncompresses data. This is a shared helper function for compression
 * support. The data is saved in a way compatible with FIOSystem::LoadCompressedData.
 *
 * @note: the way this code works needs to be in line with FIOSystem::LoadCompressedData implementations
 * @note: the way this code works needs to be in line with FAsyncIOSystemBase::FulfillCompressedRead
 *
 * @param	V		Data pointer to serialize data from/to, or a FileReader if bTreatBufferAsFileReader is true
 * @param	Length	Length of source data if we're saving, unused otherwise
 * @param	Flags	Flags to control what method to use for [de]compression and optionally control memory vs speed when compressing
 * @param	bTreatBufferAsFileReader true if V is actually an FArchive, which is used when saving to read data - helps to avoid single huge allocations of source data
 * @param	bUsePlatformBitWindow use a platform specific bitwindow setting
 */
void FArchive::SerializeCompressed( void* V, int64 Length, ECompressionFlags Flags, bool bTreatBufferAsFileReader, bool bUsePlatformBitWindow )
{
	if( IsLoading() )
	{
		// Serialize package file tag used to determine endianess.
		FCompressedChunkInfo PackageFileTag;
		PackageFileTag.CompressedSize	= 0;
		PackageFileTag.UncompressedSize	= 0;
		*this << PackageFileTag;
		bool bWasByteSwapped = PackageFileTag.CompressedSize != PACKAGE_FILE_TAG;

		// Read in base summary.
		FCompressedChunkInfo Summary;
		*this << Summary;

		bool bHeaderWasValid = true;

		if (bWasByteSwapped)
		{
			bHeaderWasValid = PackageFileTag.CompressedSize == PACKAGE_FILE_TAG_SWAPPED;
			if (bHeaderWasValid)
			{
				Summary.CompressedSize = BYTESWAP_ORDER64(Summary.CompressedSize);
				Summary.UncompressedSize = BYTESWAP_ORDER64(Summary.UncompressedSize);
				PackageFileTag.UncompressedSize = BYTESWAP_ORDER64(PackageFileTag.UncompressedSize);
			}
		}
		else
		{
			bHeaderWasValid = PackageFileTag.CompressedSize == PACKAGE_FILE_TAG;
		}

		if (!bHeaderWasValid)
		{
			UE_LOG(LogSerialization, Log, TEXT("ArchiveName: %s"), *GetArchiveName());
			UE_LOG(LogSerialization, Log, TEXT("Archive UE4 Version: %d"), UE4Ver());
			UE_LOG(LogSerialization, Log, TEXT("Archive Licensee Version: %d"), LicenseeUE4Ver());
			UE_LOG(LogSerialization, Log, TEXT("Position: %lld"), Tell());
			UE_LOG(LogSerialization, Log, TEXT("Read Size: %lld"), Length);
			UE_LOG(LogSerialization, Fatal, TEXT("BulkData compressed header read error. This package may be corrupt!"));
		}

		// Handle change in compression chunk size in backward compatible way.
		int64 LoadingCompressionChunkSize = PackageFileTag.UncompressedSize;
		if (LoadingCompressionChunkSize == PACKAGE_FILE_TAG)
		{
			LoadingCompressionChunkSize = LOADING_COMPRESSION_CHUNK_SIZE;
		}

		// Figure out how many chunks there are going to be based on uncompressed size and compression chunk size.
		int64	TotalChunkCount	= (Summary.UncompressedSize + LoadingCompressionChunkSize - 1) / LoadingCompressionChunkSize;
		
		// Allocate compression chunk infos and serialize them, keeping track of max size of compression chunks used.
		FCompressedChunkInfo*	CompressionChunks	= new FCompressedChunkInfo[TotalChunkCount];
		int64						MaxCompressedSize	= 0;
		for( int32 ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			*this << CompressionChunks[ChunkIndex];
			if (bWasByteSwapped)
			{
				CompressionChunks[ChunkIndex].CompressedSize	= BYTESWAP_ORDER64( CompressionChunks[ChunkIndex].CompressedSize );
				CompressionChunks[ChunkIndex].UncompressedSize	= BYTESWAP_ORDER64( CompressionChunks[ChunkIndex].UncompressedSize );
			}
			MaxCompressedSize = FMath::Max( CompressionChunks[ChunkIndex].CompressedSize, MaxCompressedSize );
		}

		int64 Padding = 0;
		const int32 CompressionBitWindow = bUsePlatformBitWindow ? FPlatformMisc::GetPlatformCompression()->GetCompressionBitWindow() : DEFAULT_ZLIB_BIT_WINDOW;

		// Set up destination pointer and allocate memory for compressed chunk[s] (one at a time).
		uint8*	Dest				= (uint8*) V;
		void*	CompressedBuffer	= FMemory::Malloc( MaxCompressedSize + Padding );

		// Iterate over all chunks, serialize them into memory and decompress them directly into the destination pointer
		for( int64 ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			const FCompressedChunkInfo& Chunk = CompressionChunks[ChunkIndex];
			// Read compressed data.
			Serialize( CompressedBuffer, Chunk.CompressedSize );
			// Decompress into dest pointer directly.
			verify( FCompression::UncompressMemory( Flags, Dest, Chunk.UncompressedSize, CompressedBuffer, Chunk.CompressedSize, (Padding > 0) ? true : false, CompressionBitWindow ) );
			// And advance it by read amount.
			Dest += Chunk.UncompressedSize;
		}

		// Free up allocated memory.
		FMemory::Free( CompressedBuffer );
		delete [] CompressionChunks;
	}
	else if( IsSaving() )
	{	
		SCOPE_SECONDS_COUNTER(GArchiveSerializedCompressedSavingTime);
		check( Length > 0 );

		// Serialize package file tag used to determine endianess in LoadCompressedData.
		FCompressedChunkInfo PackageFileTag;
		PackageFileTag.CompressedSize	= PACKAGE_FILE_TAG;
		PackageFileTag.UncompressedSize	= GSavingCompressionChunkSize;
		*this << PackageFileTag;

		// Figure out how many chunks there are going to be based on uncompressed size and compression chunk size.
		int64	TotalChunkCount	= (Length + GSavingCompressionChunkSize - 1) / GSavingCompressionChunkSize + 1;
		
		// Keep track of current position so we can later seek back and overwrite stub compression chunk infos.
		int64 StartPosition = Tell();

		// Allocate compression chunk infos and serialize them so we can later overwrite the data.
		FCompressedChunkInfo* CompressionChunks	= new FCompressedChunkInfo[TotalChunkCount];
		for( int64 ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			*this << CompressionChunks[ChunkIndex];
		}

		// The uncompressd size is equal to the passed in length.
		CompressionChunks[0].UncompressedSize	= Length;
		// Zero initialize compressed size so we can update it during chunk compression.
		CompressionChunks[0].CompressedSize		= 0;

#if WITH_MULTI_THREADED_COMPRESSION

#define MAX_COMPRESSION_JOBS (16)
		// Don't scale more than 16x to avoid going overboard wrt temporary memory.
		FAsyncTask<FAsyncCompressionChunk> AsyncChunks[MAX_COMPRESSION_JOBS];

		// used to keep track of which job is the next one we need to retire
		int32 AsyncChunkIndex[MAX_COMPRESSION_JOBS]={0};

		static uint32 GNumUnusedThreads_SerializeCompressed = -1;
		if (GNumUnusedThreads_SerializeCompressed == (uint32)-1)
		{
			// one-time initialization
			GNumUnusedThreads_SerializeCompressed = 1;
			// if we should use all available cores then we want to compress with all
			if( FParse::Param(FCommandLine::Get(), TEXT("USEALLAVAILABLECORES")) == true )
			{
				GNumUnusedThreads_SerializeCompressed = 0;
			}
		}

		// Maximum number of concurrent async tasks we're going to kick off. This is based on the number of processors
		// available in the system.
		int32 MaxConcurrentAsyncChunks = FMath::Clamp<int32>( FPlatformMisc::NumberOfCores() - GNumUnusedThreads_SerializeCompressed, 1, MAX_COMPRESSION_JOBS );
		if (FParse::Param(FCommandLine::Get(), TEXT("MTCHILD")))
		{
			// throttle this back when doing MT cooks
			MaxConcurrentAsyncChunks = FMath::Min<int32>( MaxConcurrentAsyncChunks,4 );
		}

		// Number of chunks left to finalize.
		int64 NumChunksLeftToFinalize	= (Length + GSavingCompressionChunkSize - 1) / GSavingCompressionChunkSize;
		// Number of chunks left to kick off
		int64 NumChunksLeftToKickOff	= NumChunksLeftToFinalize;
		// Start at index 1 as first chunk info is summary.
		int64	CurrentChunkIndex		= 1;
		// Start at index 1 as first chunk info is summary.
		int64	RetireChunkIndex		= 1;
	
		// Number of bytes remaining to kick off compression for.
		int64 BytesRemainingToKickOff	= Length;
		// Pointer to src data if buffer is memory pointer, NULL if it's a FArchive.
		uint8* SrcBuffer = bTreatBufferAsFileReader ? NULL : (uint8*)V;

		check(!bTreatBufferAsFileReader || ((FArchive*)V)->IsLoading());
		check(NumChunksLeftToFinalize);

		// Loop while there is work left to do based on whether we have finalized all chunks yet.
		while( NumChunksLeftToFinalize )
		{
			// If true we are waiting for async tasks to complete and should wait to complete some
			// if there are no async tasks finishing this iteration.
			bool bNeedToWaitForAsyncTask = false;

			// Try to kick off async tasks if there are chunks left to kick off.
			if( NumChunksLeftToKickOff )
			{
				// Find free index based on looking at uncompressed size. We can't use the thread counter
				// for this as that might be a chunk ready for finalization.
				int32 FreeIndex = INDEX_NONE;
				for( int32 i=0; i<MaxConcurrentAsyncChunks; i++ )
				{
					if( !AsyncChunkIndex[i] )
					{
						FreeIndex = i;
						check(AsyncChunks[FreeIndex].IsIdle()); // this is not supposed to be in use
						break;
					}
				}

				// Kick off async compression task if we found a chunk for it.
				if( FreeIndex != INDEX_NONE )
				{
					FAsyncCompressionChunk& NewChunk = AsyncChunks[FreeIndex].GetTask();
					// 2 times the uncompressed size should be more than enough; the compressed data shouldn't be that much larger
					NewChunk.CompressedSize	= 2 * GSavingCompressionChunkSize;
					// Allocate compressed buffer placeholder on first use.
					if( NewChunk.CompressedBuffer == NULL )
					{
						NewChunk.CompressedBuffer = FMemory::Malloc( NewChunk.CompressedSize	);
					}

					// By default everything is chunked up into GSavingCompressionChunkSize chunks.
					NewChunk.UncompressedSize	= FMath::Min( BytesRemainingToKickOff, (int64)GSavingCompressionChunkSize );
					check(NewChunk.UncompressedSize>0);

					// Need to serialize source data if passed in pointer is an FArchive.
					if( bTreatBufferAsFileReader )
					{
						// Allocate memory on first use. We allocate the maximum amount to allow reuse.
						if( !NewChunk.UncompressedBuffer )
						{
							NewChunk.UncompressedBuffer = FMemory::Malloc(GSavingCompressionChunkSize);
						}
						((FArchive*)V)->Serialize(NewChunk.UncompressedBuffer, NewChunk.UncompressedSize);
					}
					// Advance src pointer by amount to be compressed.
					else
					{
						NewChunk.UncompressedBuffer = SrcBuffer;
						SrcBuffer += NewChunk.UncompressedSize;
					}

					if (!bUsePlatformBitWindow)
					{
						NewChunk.BitWindow = DEFAULT_ZLIB_BIT_WINDOW;
					}
					else if (CookingTargetPlatform)
					{
						NewChunk.BitWindow = CookingTargetPlatform->GetCompressionBitWindow();
					}
					else
					{
						IPlatformCompression* PlatformCompression = FPlatformMisc::GetPlatformCompression();
						check(PlatformCompression);
						NewChunk.BitWindow = PlatformCompression->GetCompressionBitWindow();
					}

					// Update status variables for tracking how much work is left, what to do next.
					BytesRemainingToKickOff -= NewChunk.UncompressedSize;
					AsyncChunkIndex[FreeIndex] = CurrentChunkIndex++;
					NewChunk.Flags = Flags;
					NumChunksLeftToKickOff--;

					AsyncChunks[FreeIndex].StartBackgroundTask();
				}
				// No chunks were available to use, complete some
				else
				{
					bNeedToWaitForAsyncTask = true;
				}
			}

			// Index of oldest chunk, needed as we need to serialize in order.
			int32 OldestAsyncChunkIndex = INDEX_NONE;
			for( int32 i=0; i<MaxConcurrentAsyncChunks; i++ )
			{
				check(AsyncChunkIndex[i] == 0 || AsyncChunkIndex[i] >= RetireChunkIndex);
				check(AsyncChunkIndex[i] < RetireChunkIndex + MaxConcurrentAsyncChunks);
				if (AsyncChunkIndex[i] == RetireChunkIndex)
				{
					OldestAsyncChunkIndex = i;
				}
			}
			check(OldestAsyncChunkIndex != INDEX_NONE);  // the retire chunk better be outstanding


			bool ChunkReady;
			if (bNeedToWaitForAsyncTask)
			{
				// This guarantees that the async work has finished, doing it on this thread if it hasn't been started
				AsyncChunks[OldestAsyncChunkIndex].EnsureCompletion();
				ChunkReady = true;
			}
			else
			{
				ChunkReady = AsyncChunks[OldestAsyncChunkIndex].IsDone();
			}
			if (ChunkReady)
			{
				FAsyncCompressionChunk& DoneChunk = AsyncChunks[OldestAsyncChunkIndex].GetTask();
				// Serialize the data via archive.
				Serialize( DoneChunk.CompressedBuffer, DoneChunk.CompressedSize );

				// Update associated chunk.
				int64 CompressionChunkIndex = RetireChunkIndex++;
				check(CompressionChunkIndex<TotalChunkCount);
				CompressionChunks[CompressionChunkIndex].CompressedSize		= DoneChunk.CompressedSize;
				CompressionChunks[CompressionChunkIndex].UncompressedSize	= DoneChunk.UncompressedSize;

				// Keep track of total compressed size, stored in first chunk.
				CompressionChunks[0].CompressedSize	+= DoneChunk.CompressedSize;

				// Clean up chunk. Src and dst buffer are not touched as the contain allocations we keep till the end.
				AsyncChunkIndex[OldestAsyncChunkIndex] = 0;
				DoneChunk.CompressedSize	= 0;
				DoneChunk.UncompressedSize = 0;

				// Finalized one :)
				NumChunksLeftToFinalize--;
				bNeedToWaitForAsyncTask = false;
			}
		}

		// Free intermediate buffer storage.
		for( int32 i=0; i<MaxConcurrentAsyncChunks; i++ )
		{
			// Free temporary compressed buffer storage.
			FMemory::Free( AsyncChunks[i].GetTask().CompressedBuffer );
			AsyncChunks[i].GetTask().CompressedBuffer = NULL;
			// Free temporary uncompressed buffer storage if data was serialized in.
			if( bTreatBufferAsFileReader )
			{
				FMemory::Free( AsyncChunks[i].GetTask().UncompressedBuffer );
				AsyncChunks[i].GetTask().UncompressedBuffer = NULL;
			}
		}

#else
		// Set up source pointer amount of data to copy (in bytes)
		uint8*	Src;
		// allocate memory to read into
		if (bTreatBufferAsFileReader)
		{
			Src = (uint8*)FMemory::Malloc(GSavingCompressionChunkSize);
			check(((FArchive*)V)->IsLoading());
		}
		else
		{
			Src = (uint8*) V;
		}
		int64		BytesRemaining			= Length;
		// Start at index 1 as first chunk info is summary.
		int32		CurrentChunkIndex		= 1;
		// 2 times the uncompressed size should be more than enough; the compressed data shouldn't be that much larger
		int64		CompressedBufferSize	= 2 * GSavingCompressionChunkSize;
		void*	CompressedBuffer		= FMemory::Malloc( CompressedBufferSize );

		while( BytesRemaining > 0 )
		{
			int64 BytesToCompress = FMath::Min( BytesRemaining, (int64)GSavingCompressionChunkSize );
			int64 CompressedSize	= CompressedBufferSize;

			// read in the next chunk from the reader
			if (bTreatBufferAsFileReader)
			{
				((FArchive*)V)->Serialize(Src, BytesToCompress);
			}

			check(CompressedSize < INT_MAX);
			int32 CompressedSizeInt = (int32)CompressedSize;
			
			int32 BitWindow = 0;
			if (!bUsePlatformBitWindow)
			{
				BitWindow = DEFAULT_ZLIB_BIT_WINDOW;
			}
			else if (CookingTargetPlatform)
			{
				BitWindow = CookingTargetPlatform->GetCompressionBitWindow();
			}
			else
			{
				IPlatformCompression* PlatformCompression = FPlatformMisc::GetPlatformCompression();
				check(PlatformCompression);
				BitWindow = PlatformCompression->GetCompressionBitWindow();
			}
			
			verify( FCompression::CompressMemory( Flags, CompressedBuffer, CompressedSizeInt, Src, BytesToCompress, BitWindow) );
			CompressedSize = CompressedSizeInt;
			// move to next chunk if not reading from file
			if (!bTreatBufferAsFileReader)
			{
				Src += BytesToCompress;
			}
			Serialize( CompressedBuffer, CompressedSize );
			// Keep track of total compressed size, stored in first chunk.
			CompressionChunks[0].CompressedSize	+= CompressedSize;

			// Update current chunk.
			check(CurrentChunkIndex<TotalChunkCount);
			CompressionChunks[CurrentChunkIndex].CompressedSize		= CompressedSize;
			CompressionChunks[CurrentChunkIndex].UncompressedSize	= BytesToCompress;
			CurrentChunkIndex++;
			
			BytesRemaining -= GSavingCompressionChunkSize;
		}

		// free the buffer we read into
		if (bTreatBufferAsFileReader)
		{
			FMemory::Free(Src);
		}

		// Free allocated memory.
		FMemory::Free( CompressedBuffer );
#endif

		// Overrwrite chunk infos by seeking to the beginning, serializing the data and then
		// seeking back to the end.
		auto EndPosition = Tell();
		// Seek to the beginning.
		Seek( StartPosition );
		// Serialize chunk infos.
		for( int32 ChunkIndex=0; ChunkIndex<TotalChunkCount; ChunkIndex++ )
		{
			*this << CompressionChunks[ChunkIndex];
		}
		// Seek back to end.
		Seek( EndPosition );

		// Free intermediate data.
		delete [] CompressionChunks;
	}
}

void FArchive::ByteSwap(void* V, int32 Length)
{
	uint8* Ptr = (uint8*)V;
	int32 Top = Length - 1;
	int32 Bottom = 0;
	while (Bottom < Top)
	{
		Swap(Ptr[Top--], Ptr[Bottom++]);
	}
}

void FArchive::SerializeIntPacked(uint32& Value)
{
	if (IsLoading())
	{
		Value = 0;
		uint8 cnt = 0;
		uint8 more = 1;
		while(more)
		{
			uint8 NextByte;
			Serialize(&NextByte, 1);			// Read next byte

			more = NextByte & 1;				// Check 1 bit to see if theres more after this
			NextByte = NextByte >> 1;			// Shift to get actual 7 bit value
			Value += NextByte << (7 * cnt++);	// Add to total value
		}
	}
	else
	{
		TArray<uint8> PackedBytes;
		uint32 Remaining = Value;
		while(true)
		{
			uint8 nextByte = Remaining & 0x7f;		// Get next 7 bits to write
			Remaining = Remaining >> 7;				// Update remaining
			nextByte = nextByte << 1;				// Make room for 'more' bit
			if( Remaining > 0)
			{
				nextByte |= 1;						// set more bit
				PackedBytes.Add(nextByte);
			}
			else
			{
				PackedBytes.Add(nextByte);
				break;
			}
		}
		Serialize(PackedBytes.GetData(), PackedBytes.Num()); // Actually serialize the bytes we made
	}
}

VARARG_BODY( void, FArchive::Logf, const TCHAR*, VARARG_NONE )
{
	// We need to use malloc here directly as GMalloc might not be safe, e.g. if called from within GMalloc!
	int32		BufferSize	= 1024;
	TCHAR*	Buffer		= NULL;
	int32		Result		= -1;

	while(Result == -1)
	{
		FMemory::SystemFree(Buffer);
		Buffer = (TCHAR*) FMemory::SystemMalloc( BufferSize * sizeof(TCHAR) );
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		BufferSize *= 2;
	};
	Buffer[Result] = 0;

	// Convert to ANSI and serialize as ANSI char.
	for( int32 i=0; i<Result; i++ )
	{
		ANSICHAR Char = CharCast<ANSICHAR>( Buffer[i] );
		Serialize( &Char, 1 );
	}

	// Write out line terminator.
	for( int32 i=0; LINE_TERMINATOR[i]; i++ )
	{
		ANSICHAR Char = LINE_TERMINATOR[i];
		Serialize( &Char, 1 );
	}

	// Free temporary buffers.
	FMemory::SystemFree( Buffer );
}



