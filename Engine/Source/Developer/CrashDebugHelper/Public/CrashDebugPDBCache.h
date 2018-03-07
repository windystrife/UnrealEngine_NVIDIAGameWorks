// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "CrashDebugHelper.h"

class FArchive;

typedef TSharedRef<FPDBCacheEntry> FPDBCacheEntryRef;
typedef TSharedPtr<FPDBCacheEntry> FPDBCacheEntryPtr;

/** Helper struct that holds various information about one PDB Cache entry. */
struct FPDBCacheEntry
{
	/** Default constructor. */
	FPDBCacheEntry( const FDateTime InLastAccessTime )
		: LastAccessTime( InLastAccessTime )
		, SizeGB( 0 )
	{}

	/** Initialization constructor. */
	FPDBCacheEntry( const TArray<FString>& InFiles, const FString& InDirectory, const FDateTime InLastAccessTime, const int32 InSizeGB )
		: Files( InFiles )
		, Directory( InDirectory )
		, LastAccessTime( InLastAccessTime )
		, SizeGB( InSizeGB )
	{}

	void SetLastAccessTimeToNow()
	{
		LastAccessTime = FDateTime::UtcNow();
	}

	/** Paths to files associated with this PDB Cache entry. */
	TArray<FString> Files;

	/** The path associated with this PDB Cache entry. */
	FString Directory;

	/** Last access time, changed every time this PDB cache entry is used. */
	FDateTime LastAccessTime;

	/** Size of the cache entry, in GBs. Rounded-up. */
	const int32 SizeGB;

	/**
	* Serializer.
	*/
	friend FArchive& operator<<(FArchive& Ar, FPDBCacheEntry& Entry)
	{
		return Ar << Entry.Files << (FString&)Entry.Directory << (int32&)Entry.SizeGB;
	}
};

struct FPDBCacheEntryByAccessTime
{
	FORCEINLINE bool operator()( const FPDBCacheEntryRef& A, const FPDBCacheEntryRef& B ) const
	{
		return A->LastAccessTime.GetTicks() < B->LastAccessTime.GetTicks();
	}
};

/** Implements PDB cache. */
struct FPDBCache
{
protected:

	// Defaults for the PDB cache.
	enum
	{
		/** Size of the PDB cache, in GBs. */
		PDB_CACHE_SIZE_GB = 300,
		MIN_FREESPACE_GB = 64,
		/** Age of file when it should be deleted from the PDB cache. */
		DAYS_TO_DELETE_UNUSED_FILES = 14,

		/**
		*	Number of iterations inside the CleanPDBCache method.
		*	Mostly to verify that MinDiskFreeSpaceGB requirement is met.
		*/
		CLEAN_PDBCACHE_NUM_ITERATIONS = 2,

		/** Number of bytes per one gigabyte. */
		NUM_BYTES_PER_GB = 1024 * 1024 * 1024
	};

	/** Dummy file used to read/set the file timestamp. */
	static const TCHAR* PDBTimeStampFileNoMeta;

	/** Data file used to read/set the file timestamp, contains all metadata. */
	static const TCHAR* PDBTimeStampFile;

	/** Map of the PDB Cache entries. */
	TMap<FString, FPDBCacheEntryRef> PDBCacheEntries;

	/** Path to the folder where the PDB cache is stored. */
	FString PDBCachePath;

	/** Depot root. */
	FString DepotRoot;

	/** Age of file when it should be deleted from the PDB cache. */
	int32 DaysToDeleteUnusedFilesFromPDBCache;

	/** Size of the PDB cache, in GBs. */
	int32 PDBCacheSizeGB;

	/**
	*	Minimum disk free space that should be available on the disk where the PDB cache is stored, in GBs.
	*	Minidump diagnostics runs usually on the same drive as the crash reports drive, so we need to leave some space for the crash receiver.
	*	If there is not enough disk free space, we will run the clean-up process.
	*/
	int32 MinDiskFreeSpaceGB;

	/** Whether to use the PDB cache. */
	bool bUsePDBCache;

public:
	/** Default constructor. */
	FPDBCache()
		: DaysToDeleteUnusedFilesFromPDBCache( DAYS_TO_DELETE_UNUSED_FILES )
		, PDBCacheSizeGB( PDB_CACHE_SIZE_GB )
		, MinDiskFreeSpaceGB( MIN_FREESPACE_GB )
		, bUsePDBCache( false )
	{}

	/** Basic initialization, reading config etc.. */
	void Init();

	/**
	* @return whether to use the PDB cache.
	*/
	bool UsePDBCache() const
	{
		return bUsePDBCache;
	}

	/** @return the path the depot root. */
	const FString& GetDepotRoot() const
	{
		return DepotRoot;
	}

	/** Accesses the singleton. */
	CRASHDEBUGHELPER_API static FPDBCache& Get()
	{
		static FPDBCache Instance;
		return Instance;
	}

	/**
	* @return true, if the PDB Cache contains the specified label.
	*/
	bool ContainsPDBCacheEntry( const FString& PathOrLabel ) const
	{
		return PDBCacheEntries.Contains( EscapePath( PathOrLabel ) );
	}

	/**
	*	Touches a PDB Cache entry by updating the timestamp.
	*/
	void TouchPDBCacheEntry( const FString& Directory );

	/**
	* @return a PDB Cache entry for the specified label and touches it at the same time
	*/
	FPDBCacheEntryRef FindAndTouchPDBCacheEntry( const FString& PathOrLabel );

	/**
	*	Creates a new PDB Cache entry, initializes it and adds to the database.
	*/
	FPDBCacheEntryRef CreateAndAddPDBCacheEntry( const FString& OriginalLabelName, const FString& DepotName, const TArray<FString>& FilesToBeCached );

	/**
	*	Creates a new PDB Cache entry, initializes it and adds to the database.
	*/
	FPDBCacheEntryRef CreateAndAddPDBCacheEntryMixed( const FString& ProductVersion, const TMap<FString, FString>& FilesToBeCached );

protected:
	/** Initializes the PDB Cache. */
	void InitializePDBCache();

	/**
	* @return the size of the PDB cache entry, in GBs.
	*/
	int32 GetPDBCacheEntrySizeGB( const FString& PathOrLabel ) const
	{
		return PDBCacheEntries.FindChecked( PathOrLabel )->SizeGB;
	}

	/**
	* @return the size of the PDB cache directory, in GBs.
	*/
	int32 GetPDBCacheSizeGB() const
	{
		int32 Result = 0;
		if( bUsePDBCache )
		{
			for( const auto& It : PDBCacheEntries )
			{
				Result += It.Value->SizeGB;
			}
		}
		return Result;
	}

	/**
	* Cleans the PDB Cache.
	*
	* @param DaysToKeep - removes all PDB Cache entries that are older than this value
	* @param NumberOfGBsToBeCleaned - if specifies, will try to remove as many PDB Cache entries as needed
	* to free disk space specified by this value
	*
	*/
	void CleanPDBCache( int32 DaysToKeep, int32 NumberOfGBsToBeCleaned = 0 );


	/**
	*	Reads an existing PDB Cache entry.
	*/
	FPDBCacheEntryPtr ReadPDBCacheEntry( const FString& Directory );

	/**
	*	Sort PDB Cache entries by last access time.
	*/
	void SortPDBCache()
	{
		PDBCacheEntries.ValueSort( FPDBCacheEntryByAccessTime() );
	}

	/**
	*	Removes a PDB Cache entry from the database.
	*	Also removes all external files associated with this PDB Cache entry.
	*/
	void RemovePDBCacheEntry( const FString& Directory );

	/** Replaces all invalid chars with + for the specified name. */
	FString EscapePath( const FString& PathOrLabel ) const
	{
		// @see AutomationTool.CommandUtils.EscapePath 
		return PathOrLabel.Replace( TEXT( ":" ), TEXT( "" ) ).Replace( TEXT( "/" ), TEXT( "+" ) ).Replace( TEXT( "\\" ), TEXT( "+" ) ).Replace( TEXT( " " ), TEXT( "+" ) );
	}
};
