// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DDCCleanup.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Math/RandomStream.h"

#include "DerivedDataBackendInterface.h"

/** Struct containing a list of directories to cleanup. */
struct FFilesystemInfo
{
	/** Filesystem Path to clean up. **/
	const FString CachePath;
	/** Minimum time a file has not been used for to delete it. */
	const FTimespan UnusedFileTime;
	/** The maximum number of folders to check. <= 0 means all. */
	const int32 MaxNumFoldersToCheck;
	/** The maximum number of files to check before pausing. <= 0 is no limit. */
	const int32 MaxContinuousFileChecks;

	/** The number of folders already checked. */
	int32 FoldersChecked;
	/** Filesystem directories left to clean up. */
	TArray<int32> CacheDirectories;

	FFilesystemInfo( FString& InCachePath, int32 InDaysToDelete, int32 InMaxNumFoldersToCheck, int32 InMaxContinuousFileChecks )
		: CachePath( InCachePath )
		, UnusedFileTime( InDaysToDelete, 0, 0, 0 )
		, MaxNumFoldersToCheck( InMaxNumFoldersToCheck )
		, MaxContinuousFileChecks( InMaxContinuousFileChecks )
		, FoldersChecked( 0 )
	{
		CacheDirectories.Empty( 1000 );
		for( int32 Index = 0; Index < 1000; Index++ )
		{
			CacheDirectories.Add( Index );
		}
			
		// Initialize random stream using Cycles() 
		const uint32 Cycles = FPlatformTime::Cycles();
		FRandomStream RandomStream( *(int32*)&Cycles );
		// Shuffle
		for( int32 Index = CacheDirectories.Num() - 1; Index >= 0; Index-- )
		{
			const int32 RandomIndex = RandomStream.RandHelper(Index + 1);

			Exchange( CacheDirectories[ Index ], CacheDirectories[ RandomIndex ] );
		}
	}
};

FDDCCleanup* FDDCCleanup::Runnable = NULL;

FDDCCleanup::FDDCCleanup()
	: Thread(NULL)
	, StopTaskCounter(0)
{
	// Don't delete the runnable automatically. It's going to be manually deleted in FDDCCleanup::Shutdown.
	Thread = FRunnableThread::Create(this, TEXT("FDDCCleanup"), 0, TPri_BelowNormal, FPlatformAffinity::GetPoolThreadMask());
}

FDDCCleanup::~FDDCCleanup()
{
	delete Thread;
}

void FDDCCleanup::Wait( const float InSeconds, const float InSleepTime )
{
	// Instead of waiting the given amount of seconds doing nothing
	// check periodically if there's been any Stop requests.
	for( float TimeToWait = InSeconds; TimeToWait > 0.0f && ShouldStop() == false; TimeToWait -= InSleepTime )
	{
		FPlatformProcess::Sleep( FMath::Min(InSleepTime, TimeToWait) );
	}
}

bool FDDCCleanup::Init() 
{
	return true;
}

uint32 FDDCCleanup::Run()
{
	// Give up some time to the engine to start up and load everything
	Wait( 120.0f, 0.5f );

	int32 FilesystemToCleanup = 0;
	// Check one directory every 5 seconds
	do
	{
		// Pick one random filesystem.
		TSharedPtr< FFilesystemInfo > FilesystemInfo;
		{
			FScopeLock ScopeLock( &DataLock );
			if( CleanupList.Num() > 0 )
			{
				FilesystemToCleanup %= CleanupList.Num();
				FilesystemInfo = CleanupList[ FilesystemToCleanup++ ];					
			}
		}

		if( FilesystemInfo.IsValid() )
		{
			CleanupFilesystemDirectory( FilesystemInfo );
		}
		Wait( 5.0f );
	}
	while( ShouldStop() == false );

	return 0;
}

bool FDDCCleanup::CleanupFilesystemDirectory( TSharedPtr< FFilesystemInfo > FilesystemInfo )
{
	bool bCleanedUp = false;

	const double StartTime = FPlatformTime::Seconds();

	// Pick one random directory.
	TArray<FString> FileNames;
	do
	{
		const int32 DirectoryIndex = FilesystemInfo->CacheDirectories.Pop();
		const FString DirectoryPath( FilesystemInfo->CachePath / FString::Printf(TEXT("%1d/%1d/%1d/"),(DirectoryIndex/100)%10,(DirectoryIndex/10)%10,DirectoryIndex%10) );

		IFileManager::Get().FindFilesRecursive( FileNames, *DirectoryPath, TEXT("*.*"), true, false );
		if ( FilesystemInfo->CacheDirectories.Num() == 0 )
		{
			// Remove the filesystem and stop checking it
			FScopeLock ScopeLock( &DataLock );
			CleanupList.Remove( FilesystemInfo );
			FilesystemInfo.Reset();
		}
		else if( ++FilesystemInfo->FoldersChecked >= FilesystemInfo->MaxNumFoldersToCheck && FilesystemInfo->MaxNumFoldersToCheck > 0 )
		{
			// Remove the filesystem but keep checking the current folder
			FScopeLock ScopeLock( &DataLock );
			CleanupList.Remove( FilesystemInfo );
		}
	}
	while( FileNames.Num() == 0 && FilesystemInfo.IsValid() && ShouldStop() == false );

	if( FilesystemInfo.IsValid() && ShouldStop() == false )
	{
		// Iterate over all files in the selected folder and check their last access time
		int32 NumFilesChecked = 0;
		for( int32 FileIndex = 0; FileIndex < FileNames.Num() && ShouldStop() == false; FileIndex++ )
		{
			const FDateTime LastModificationTime = IFileManager::Get().GetTimeStamp( *FileNames[ FileIndex ] );
			const FDateTime LastAccessTime = IFileManager::Get().GetAccessTimeStamp( *FileNames[ FileIndex ] );
			if( (LastAccessTime != FDateTime::MinValue()) || (LastModificationTime != FDateTime::MinValue())  )
			{
				const FTimespan TimeSinceLastAccess = FDateTime::UtcNow() - LastAccessTime;
				const FTimespan TimeSinceLastModification = FDateTime::UtcNow() - LastModificationTime;
				if( TimeSinceLastAccess >= FilesystemInfo->UnusedFileTime && TimeSinceLastModification >= FilesystemInfo->UnusedFileTime )
				{
					// Delete the file
					bool Result = IFileManager::Get().Delete( *FileNames[ FileIndex ], false, true, true );
				}
			}

			if( ++NumFilesChecked >= FilesystemInfo->MaxContinuousFileChecks && FilesystemInfo->MaxContinuousFileChecks > 0 && ShouldStop() == false )
			{
				NumFilesChecked = 0;
				Wait( 1.0f );
			}
			else
			{
				// Give up a tiny amount of time so that we're not consuming too much cpu/hdd resources.
				Wait( 0.05f );
			}
		}

		bCleanedUp = true;			
	}

	UE_LOG(LogDerivedDataCache, VeryVerbose, TEXT("DDC Folder Cleanup (%s) took %.4lfs."), *FilesystemInfo->CachePath, FPlatformTime::Seconds() - StartTime);

	return bCleanedUp;
}

void FDDCCleanup::Stop(void)
{
	StopTaskCounter.Increment();
}

void FDDCCleanup::Exit(void)
{
}

void FDDCCleanup::EnsureCompletion()
{
	Stop();
	Thread->WaitForCompletion();
}

void FDDCCleanup::AddFilesystem( FString& InCachePath, int32 InDaysToDelete, int32 InMaxNumFoldersToCheck, int32 InMaxContinuousFileChecks )
{
	FScopeLock Lock( &DataLock );
	CleanupList.Add( MakeShareable( new FFilesystemInfo( InCachePath, InDaysToDelete, InMaxNumFoldersToCheck, InMaxContinuousFileChecks ) ) );
}

FDDCCleanup* FDDCCleanup::Get()
{
	if (!Runnable && FPlatformProcess::SupportsMultithreading())
	{
		Runnable = new FDDCCleanup();			
	}
	return Runnable;
}

void FDDCCleanup::Shutdown()
{
	if (Runnable)
	{
		Runnable->EnsureCompletion();
		delete Runnable;
		Runnable = NULL;
	}
}


