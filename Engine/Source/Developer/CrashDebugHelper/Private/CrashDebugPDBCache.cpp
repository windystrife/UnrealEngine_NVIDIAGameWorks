// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CrashDebugPDBCache.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Templates/ScopedPointer.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "CrashDebugHelperPrivate.h"
#include "UniquePtr.h"

/*-----------------------------------------------------------------------------
	PDB Cache implementation
-----------------------------------------------------------------------------*/

const TCHAR* FPDBCache::PDBTimeStampFileNoMeta = TEXT( "PDBTimeStamp.txt" );
const TCHAR* FPDBCache::PDBTimeStampFile = TEXT( "PDBTimeStamp.bin" );

void FPDBCache::Init()
{
	// PDB Cache
	// Default configuration
	//PDBCachePath=G:/CrashReportPDBCache/
	//DepotRoot=F:/depot
	//DaysToDeleteUnusedFilesFromPDBCache=3
	//PDBCacheSizeGB=300
	//MinDiskFreeSpaceGB=256

	// Can be enabled only through the command line.
	FParse::Bool( FCommandLine::Get(), TEXT( "bUsePDBCache=" ), bUsePDBCache );

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "bUsePDBCache is %s" ), bUsePDBCache ? TEXT( "enabled" ) : TEXT( "disabled" ) );

	if (bUsePDBCache)
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("PDBCacheDepotRoot="), DepotRoot))
		{
			GConfig->GetString(TEXT("Engine.CrashDebugHelper"), TEXT("DepotRoot"), DepotRoot, GEngineIni);
		}

		ICrashDebugHelper::SetDepotIndex( DepotRoot );

		const bool bHasDepotRoot = IFileManager::Get().DirectoryExists( *DepotRoot );
		UE_CLOG( !bHasDepotRoot, LogCrashDebugHelper, Warning, TEXT( "DepotRoot: %s is not valid" ), *DepotRoot );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "DepotRoot: %s" ), *DepotRoot );	

		bUsePDBCache = bHasDepotRoot;
	}

	// Get the rest of the PDB cache configuration.
	if( bUsePDBCache )
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("PDBCachePath="), PDBCachePath))
		{
			if (!GConfig->GetString(TEXT("Engine.CrashDebugHelper"), TEXT("PDBCachePath"), PDBCachePath, GEngineIni))
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("Failed to get PDBCachePath from ini file or command line"));
				bUsePDBCache = false;
			}
		}

		ICrashDebugHelper::SetDepotIndex( PDBCachePath );
	}

	if( bUsePDBCache )
	{
		if (!FParse::Value(FCommandLine::Get(), TEXT("PDBCacheSizeGB="), PDBCacheSizeGB))
		{
			if (!GConfig->GetInt(TEXT("Engine.CrashDebugHelper"), TEXT("PDBCacheSizeGB"), PDBCacheSizeGB, GEngineIni))
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("Failed to get PDBCachePath from ini file or command line"));
			}
		}

		if (!FParse::Value(FCommandLine::Get(), TEXT("PDBCacheMinFreeSpaceGB="), MinDiskFreeSpaceGB))
		{
			if (!GConfig->GetInt(TEXT("Engine.CrashDebugHelper"), TEXT("MinDiskFreeSpaceGB"), MinDiskFreeSpaceGB, GEngineIni))
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("Failed to get MinDiskFreeSpaceGB from ini file or command line"));
			}
		}

		if (!FParse::Value(FCommandLine::Get(), TEXT("PDBCacheFileDeleteDays="), DaysToDeleteUnusedFilesFromPDBCache))
		{
			if (!GConfig->GetInt(TEXT("Engine.CrashDebugHelper"), TEXT("DaysToDeleteUnusedFilesFromPDBCache"), DaysToDeleteUnusedFilesFromPDBCache, GEngineIni))
			{
				UE_LOG(LogCrashDebugHelper, Warning, TEXT("Failed to get DaysToDeleteUnusedFilesFromPDBCache from ini file or command line"));
			}
		}

		InitializePDBCache();
		CleanPDBCache( DaysToDeleteUnusedFilesFromPDBCache );

		// Verify that we have enough space to enable the PDB Cache.
		uint64 TotalNumberOfBytes = 0;
		uint64 NumberOfFreeBytes = 0;
		FPlatformMisc::GetDiskTotalAndFreeSpace( PDBCachePath, TotalNumberOfBytes, NumberOfFreeBytes );

		const int32 TotalDiscSpaceGB = int32( TotalNumberOfBytes >> 30 );
		const int32 DiskFreeSpaceGB = int32( NumberOfFreeBytes >> 30 );

		if( DiskFreeSpaceGB < MinDiskFreeSpaceGB || TotalNumberOfBytes == 0 )
		{
			// There is not enough free space, calculate the current PDB cache usage and try removing the old data.
			const int32 CurrentPDBCacheSizeGB = GetPDBCacheSizeGB();
			const int32 DiskFreeSpaceAfterCleanGB = DiskFreeSpaceGB + CurrentPDBCacheSizeGB;
			if( DiskFreeSpaceAfterCleanGB < MinDiskFreeSpaceGB )
			{
				UE_LOG( LogCrashDebugHelper, Error, TEXT( "There is not enough free space. PDB Cache disabled." ) );
				UE_LOG( LogCrashDebugHelper, Error, TEXT( "Current disk free space is %i GBs." ), DiskFreeSpaceGB );
				UE_LOG( LogCrashDebugHelper, Error, TEXT( "To enable the PDB Cache you need to free %i GB of space" ), MinDiskFreeSpaceGB - DiskFreeSpaceAfterCleanGB );
				bUsePDBCache = false;
				// Remove all data.
				CleanPDBCache( 0 );
			}
			else
			{
				// Clean the PDB cache until we get enough free space.
				const int32 MinSpaceRequirement = FMath::Max( MinDiskFreeSpaceGB - DiskFreeSpaceGB, 0 );
				const int32 CacheSpaceRequirement = FMath::Max( CurrentPDBCacheSizeGB - PDBCacheSizeGB, 0 );
				CleanPDBCache( DaysToDeleteUnusedFilesFromPDBCache, FMath::Max( MinSpaceRequirement, CacheSpaceRequirement ) );
			}
		}
	}

	if( bUsePDBCache )
	{
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDBCachePath: %s" ), *PDBCachePath );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDBCacheSizeGB: %i" ), PDBCacheSizeGB );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "MinDiskFreeSpaceGB: %i" ), MinDiskFreeSpaceGB );
		UE_LOG( LogCrashDebugHelper, Log, TEXT( "DaysToDeleteUnusedFilesFromPDBCache: %i" ), DaysToDeleteUnusedFilesFromPDBCache );
	}
}


void FPDBCache::InitializePDBCache()
{
	const double StartTime = FPlatformTime::Seconds();
	IFileManager::Get().MakeDirectory( *PDBCachePath, true );

	TArray<FString> PDBCacheEntryDirectories;
	IFileManager::Get().FindFiles( PDBCacheEntryDirectories, *(PDBCachePath / TEXT( "*" )), false, true );

	for( const auto& Directory : PDBCacheEntryDirectories )
	{
		FPDBCacheEntryPtr Entry = ReadPDBCacheEntry( Directory );
		if (Entry.IsValid())
		{
			PDBCacheEntries.Add( Directory, Entry.ToSharedRef() );
		}
	}

	SortPDBCache();

	const double TotalTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDB Cache initialized in %.2f ms" ), TotalTime*1000.0f );
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "Found %i entries which occupy %i GBs" ), PDBCacheEntries.Num(), GetPDBCacheSizeGB() );
}

void FPDBCache::CleanPDBCache( int32 DaysToDelete, int32 NumberOfGBsToBeCleaned /*= 0 */ )
{
	// Not very efficient, but should do the trick.
	// Revisit it later.
	const double StartTime = FPlatformTime::Seconds();

	TSet<FString> EntriesToBeRemoved;

	// Find all outdated PDB Cache entries and mark them for removal.
	const double DaysToDeleteAsSeconds = FTimespan( DaysToDelete, 0, 0, 0 ).GetTotalSeconds();
	int32 NumGBsCleaned = 0;
	for( const auto& It : PDBCacheEntries )
	{
		const FPDBCacheEntryRef& Entry = It.Value;
		const FString EntryDirectory = PDBCachePath / Entry->Directory;
		const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

		const double EntryFileAge = IFileManager::Get().GetFileAgeSeconds( *EntryTimeStampFilename );
		if( EntryFileAge > DaysToDeleteAsSeconds )
		{
			EntriesToBeRemoved.Add( Entry->Directory );
			NumGBsCleaned += Entry->SizeGB;
		}
	}

	if( NumberOfGBsToBeCleaned > 0 && NumGBsCleaned < NumberOfGBsToBeCleaned )
	{
		// Do the second pass if we need to remove more PDB Cache entries due to the free disk space restriction.
		for( const auto& It : PDBCacheEntries )
		{
			const FPDBCacheEntryRef& Entry = It.Value;

			if( !EntriesToBeRemoved.Contains( Entry->Directory ) )
			{
				EntriesToBeRemoved.Add( Entry->Directory );
				NumGBsCleaned += Entry->SizeGB;

				if( NumGBsCleaned > NumberOfGBsToBeCleaned )
				{
					// Break the loop, we are done.
					break;
				}
			}
		}
	}

	// Remove all marked PDB Cache entries.
	for( const auto& EntryDirectory : EntriesToBeRemoved )
	{
		RemovePDBCacheEntry( EntryDirectory );
	}

	const double TotalTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG( LogCrashDebugHelper, Log, TEXT( "PDB Cache cleaned %i GBs in %.2f ms" ), NumGBsCleaned, TotalTime*1000.0f );
}

FPDBCacheEntryRef FPDBCache::CreateAndAddPDBCacheEntry( const FString& OriginalLabelName, const FString& DepotName, const TArray<FString>& FilesToBeCached )
{
	const FString CleanedLabelName = EscapePath( OriginalLabelName );
	const FString EntryDirectory = PDBCachePath / CleanedLabelName;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	const FString LocalDepotDir = EscapePath( DepotRoot / DepotName );

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache entry: %s is being copied from: %s, it will take some time" ), *CleanedLabelName, *OriginalLabelName );
	for( const auto& Filename : FilesToBeCached )
	{
		const FString SourceDirectoryWithSearch = Filename.Replace( *DepotName, *LocalDepotDir );

		TArray<FString> MatchedFiles;
		IFileManager::Get().FindFiles( MatchedFiles, *SourceDirectoryWithSearch, true, false );

		for( const auto& MatchedFilename : MatchedFiles )
		{
			const FString SrcFilename = FPaths::GetPath( SourceDirectoryWithSearch ) / MatchedFilename;
			const FString DestFilename = EntryDirectory / SrcFilename.Replace( *LocalDepotDir, TEXT( "" ) );
			IFileManager::Get().Copy( *DestFilename, *SrcFilename );
		}
	}


	TArray<FString> CachedFiles;
	IFileManager::Get().FindFilesRecursive( CachedFiles, *EntryDirectory, TEXT( "*.*" ), true, false );

	// Calculate the size of this PDB Cache entry.
	int64 TotalSize = 0;
	for( const auto& Filename : CachedFiles )
	{
		const int64 FileSize = IFileManager::Get().FileSize( *Filename );
		TotalSize += FileSize;
	}

	// Round-up the size.
	int32 SizeGB = (int32)FMath::DivideAndRoundUp( TotalSize, (int64)NUM_BYTES_PER_GB );
	FPDBCacheEntryRef NewCacheEntry = MakeShareable( new FPDBCacheEntry( CachedFiles, CleanedLabelName, FDateTime::Now(), SizeGB ) );

	// Verify there is an entry timestamp file, write the size of a PDB cache to avoid time consuming FindFilesRecursive during initialization.
	TUniquePtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *EntryTimeStampFilename ) );
	UE_CLOG( !FileWriter, LogCrashDebugHelper, Fatal, TEXT( "Couldn't save the timestamp for a file: %s" ), *EntryTimeStampFilename );
	*FileWriter << *NewCacheEntry;

	PDBCacheEntries.Add( CleanedLabelName, NewCacheEntry );
	SortPDBCache();

	return NewCacheEntry;
}

FPDBCacheEntryRef FPDBCache::CreateAndAddPDBCacheEntryMixed( const FString& ProductVersion, const TMap<FString, FString>& FilesToBeCached )
{
	// Enable MDD to parse all minidumps regardless the branch, to fix the issue with the missing executables on the P4 due to the build system changes.
	const FString EntryDirectory = PDBCachePath / ProductVersion;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache entry: %s is being created from %i files, it will take some time" ), *ProductVersion, FilesToBeCached.Num() );
	for( const auto& It : FilesToBeCached )
	{
		const FString& SrcFilename = It.Key;
		const FString DestFilename = EntryDirectory / It.Value;
		IFileManager::Get().Copy( *DestFilename, *SrcFilename );
	}

	TArray<FString> CachedFiles;
	IFileManager::Get().FindFilesRecursive( CachedFiles, *EntryDirectory, TEXT( "*.*" ), true, false );

	// Calculate the size of this PDB Cache entry.
	int64 TotalSize = 0;
	for( const auto& Filename : CachedFiles )
	{
		const int64 FileSize = IFileManager::Get().FileSize( *Filename );
		TotalSize += FileSize;
	}

	// Round-up the size.
	int32 SizeGB = (int32)FMath::DivideAndRoundUp( TotalSize, (int64)NUM_BYTES_PER_GB );
	FPDBCacheEntryRef NewCacheEntry = MakeShareable( new FPDBCacheEntry( CachedFiles, ProductVersion, FDateTime::Now(), SizeGB ) );

	// Verify there is an entry timestamp file, write the size of a PDB cache to avoid time consuming FindFilesRecursive during initialization.
	TUniquePtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *EntryTimeStampFilename ) );
	UE_CLOG( !FileWriter, LogCrashDebugHelper, Fatal, TEXT( "Couldn't save the timestamp for a file: %s" ), *EntryTimeStampFilename );
	*FileWriter << *NewCacheEntry;

	PDBCacheEntries.Add( ProductVersion, NewCacheEntry );
	SortPDBCache();

	return NewCacheEntry;
}

FPDBCacheEntryPtr FPDBCache::ReadPDBCacheEntry( const FString& Directory )
{
	const FString EntryDirectory = PDBCachePath / Directory;
	const FString EntryTimeStampFilenameNoMeta = EntryDirectory / PDBTimeStampFileNoMeta;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	// Verify there is an entry timestamp file.
	const FDateTime LastAccessTimeNoMeta = IFileManager::Get().GetTimeStamp( *EntryTimeStampFilenameNoMeta );
	const FDateTime LastAccessTime = IFileManager::Get().GetTimeStamp( *EntryTimeStampFilename );

	FPDBCacheEntryPtr NewEntry;

	if( LastAccessTime != FDateTime::MinValue() )
	{
		// Read the metadata
		TUniquePtr<FArchive> FileReader( IFileManager::Get().CreateFileReader( *EntryTimeStampFilename ) );
		NewEntry = MakeShareable( new FPDBCacheEntry( LastAccessTime ) );
		*FileReader << *NewEntry;
	}
	else if( LastAccessTimeNoMeta != FDateTime::MinValue() )
	{
		// Calculate the size of this PDB Cache entry and update to the new version.
		TArray<FString> PDBFiles;
		IFileManager::Get().FindFilesRecursive( PDBFiles, *EntryDirectory, TEXT( "*.*" ), true, false );

		// Calculate the size of this PDB Cache entry.
		int64 TotalSize = 0;
		for( const auto& Filename : PDBFiles )
		{
			const int64 FileSize = IFileManager::Get().FileSize( *Filename );
			TotalSize += FileSize;
		}

		// Round-up the size.
		const int32 SizeGB = (int32)FMath::DivideAndRoundUp( TotalSize, (int64)NUM_BYTES_PER_GB );
		NewEntry = MakeShareable( new FPDBCacheEntry( PDBFiles, Directory, LastAccessTimeNoMeta, SizeGB ) );

		// Remove the old file and save the metadata.
		TUniquePtr<FArchive> FileWriter( IFileManager::Get().CreateFileWriter( *EntryTimeStampFilename ) );
		*FileWriter << *NewEntry;

		IFileManager::Get().Delete( *EntryTimeStampFilenameNoMeta );
	}
	else
	{
		// Something wrong.
		ensureMsgf( 0, TEXT( "Invalid symbol cache entry: %s" ), *EntryDirectory );
	}

	return NewEntry;
}

void FPDBCache::TouchPDBCacheEntry( const FString& Directory )
{
	const FString EntryDirectory = PDBCachePath / Directory;
	const FString EntryTimeStampFilename = EntryDirectory / PDBTimeStampFile;

	FPDBCacheEntryRef& Entry = PDBCacheEntries.FindChecked( Directory );
	Entry->SetLastAccessTimeToNow();

	const bool bResult = IFileManager::Get().SetTimeStamp( *EntryTimeStampFilename, Entry->LastAccessTime );
	SortPDBCache();
}

void FPDBCache::RemovePDBCacheEntry( const FString& Directory )
{
	const double StartTime = FPlatformTime::Seconds();

	const FString EntryDirectory = PDBCachePath / Directory;

	FPDBCacheEntryRef& Entry = PDBCacheEntries.FindChecked( Directory );
	IFileManager::Get().DeleteDirectory( *EntryDirectory, true, true );

	const double TotalTime = FPlatformTime::Seconds() - StartTime;
	UE_LOG( LogCrashDebugHelper, Warning, TEXT( "PDB Cache entry %s removed in %.2f ms, restored %i GBs" ), *Directory, TotalTime*1000.0f, Entry->SizeGB );

	PDBCacheEntries.Remove( Directory );
}

FPDBCacheEntryRef FPDBCache::FindAndTouchPDBCacheEntry( const FString& PathOrLabel )
{
	FPDBCacheEntryRef CacheEntry = PDBCacheEntries.FindChecked( EscapePath( PathOrLabel ) );
	TouchPDBCacheEntry( CacheEntry->Directory );
	return CacheEntry;
}
