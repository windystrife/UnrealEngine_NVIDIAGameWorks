// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"

#include "DerivedDataBackendInterface.h"
#include "DDCCleanup.h"

#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheUsageStats.h"

#define MAX_BACKEND_KEY_LENGTH (120)
#define MAX_BACKEND_NUMBERED_SUBFOLDER_LENGTH (9)
#if PLATFORM_LINUX	// PATH_MAX on Linux is 4096 (getconf PATH_MAX /, also see limits.h), so this value can be larger (note that it is still arbitrary).
                    // This should not affect sharing the cache between platforms as the absolute paths will be different anyway.
	#define MAX_CACHE_DIR_LEN (3119)
#else
	#define MAX_CACHE_DIR_LEN (119)
#endif // PLATFORM_LINUX
#define MAX_CACHE_EXTENTION_LEN (4)




/** 
 * Cache server that uses the OS filesystem
 * The entire API should be callable from any thread (except the singleton can be assumed to be called at least once before concurrent access).
**/
class FFileSystemDerivedDataBackend : public FDerivedDataBackendInterface
{
public:
	/**
	 * Constructor that should only be called once by the singleton, grabs the cache path from the ini
	 * @param InCacheDirectory	directory to store the cache in
	 * @param bForceReadOnly	if true, do not attempt to write to this cache
	*/
	FFileSystemDerivedDataBackend(const TCHAR* InCacheDirectory, bool bForceReadOnly, bool bTouchFiles, bool bPurgeTransientData, bool bDeleteOldFiles, int32 InDaysToDeleteUnusedFiles, int32 InMaxNumFoldersToCheck, int32 InMaxContinuousFileChecks)
		: CachePath(InCacheDirectory)
		, bReadOnly(bForceReadOnly)
		, bFailed(true)
		, bTouch(bTouchFiles)
		, bPurgeTransient(bPurgeTransientData)
		, DaysToDeleteUnusedFiles(InDaysToDeleteUnusedFiles)
	{
		// If we find a platform that has more stingent limits, this needs to be rethought.
		static_assert(MAX_BACKEND_KEY_LENGTH + MAX_CACHE_DIR_LEN + MAX_BACKEND_NUMBERED_SUBFOLDER_LENGTH + MAX_CACHE_EXTENTION_LEN < PLATFORM_MAX_FILEPATH_LENGTH,
			"Not enough room left for cache keys in max path.");
		const double SlowInitDuration = 10.0;
		double AccessDuration = 0.0;

		check(CachePath.Len());
		FPaths::NormalizeFilename(CachePath);
		const FString AbsoluteCachePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CachePath);
		if (AbsoluteCachePath.Len() > MAX_CACHE_DIR_LEN)
		{
			const FText ErrorMessage = FText::Format( NSLOCTEXT("DerivedDataCache", "PathTooLong", "Cache path {0} is longer than {1} characters...please adjust [DerivedDataBackendGraph] paths to be shorter (this leaves more room for cache keys)."), FText::FromString( AbsoluteCachePath ), FText::AsNumber( MAX_CACHE_DIR_LEN ) );
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
			UE_LOG(LogDerivedDataCache, Fatal, TEXT("%s"), *ErrorMessage.ToString());
		}
		if (!bReadOnly)
		{
			double TestStart = FPlatformTime::Seconds();
			FString TempFilename = CachePath / FGuid::NewGuid().ToString() + ".tmp";
			FFileHelper::SaveStringToFile(FString("TEST"),*TempFilename);
			int32 TestFileSize = IFileManager::Get().FileSize(*TempFilename);
			if (TestFileSize < 4)
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("Fail to write to %s, derived data cache to this directory will be read only."),*CachePath);
			}
			else
			{
				bFailed = false;
			}			
			if (TestFileSize >= 0)
			{
				IFileManager::Get().Delete(*TempFilename, false, false, true);
			}
			AccessDuration = FPlatformTime::Seconds() - TestStart;
		}
		if (bFailed)
		{
			double StartTime = FPlatformTime::Seconds();
			TArray<FString> FilesAndDirectories;
			IFileManager::Get().FindFiles(FilesAndDirectories,*(CachePath / TEXT("*.*")), true, true);
			AccessDuration = FPlatformTime::Seconds() - StartTime;
			if (FilesAndDirectories.Num() > 0)
			{
				bReadOnly = true;
				bFailed = false;
			}
		}
		if (FString(FCommandLine::Get()).Contains(TEXT("DerivedDataCache")) )
		{
			bTouch = true; // we always touch files when running the DDC commandlet
		}
		// The command line (-ddctouch) enables touch on all filesystem backends if specified. 
		bTouch = bTouch || FParse::Param(FCommandLine::Get(), TEXT("DDCTOUCH"));

		if (bReadOnly)
		{
			bTouch = false; // we won't touch read only paths
		}

		if (bTouch)
		{
			UE_LOG(LogDerivedDataCache, Display, TEXT("Files in %s will be touched."),*CachePath);
		}

		if (!bFailed && AccessDuration > SlowInitDuration && !GIsBuildMachine)
		{
			UE_LOG(LogDerivedDataCache, Warning, TEXT("%s access is very slow (initialization took %.2f seconds), consider disabling it."), *CachePath, AccessDuration);
		}

		if (!bReadOnly && !bFailed && bDeleteOldFiles && !FParse::Param(FCommandLine::Get(),TEXT("NODDCCLEANUP")) && FDDCCleanup::Get())
		{			
			FDDCCleanup::Get()->AddFilesystem( CachePath, InDaysToDeleteUnusedFiles, InMaxNumFoldersToCheck, InMaxContinuousFileChecks );
		}
	}

	/** return true if the cache is usable **/
	bool IsUsable()
	{
		return !bFailed;
	}

	/** return true if this cache is writable **/
	virtual bool IsWritable() override
	{
		return !bReadOnly;
	}
	/**
	 * Synchronous test for the existence of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				true if the data probably will be found, this can't be guaranteed because of concurrency in the backends, corruption, etc
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override
	{
		COOK_STAT(auto Timer = UsageStats.TimeProbablyExists());
		check(!bFailed);
		FString Filename = BuildFilename(CacheKey);
		FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*Filename);
		bool bExists = TimeStamp > FDateTime::MinValue();
		if (bExists)
		{
			// Update file timestamp to prevent it from being deleted by DDC Cleanup.
			if (bTouch || 
				 (!bReadOnly && (FDateTime::UtcNow() - TimeStamp).GetDays() > (DaysToDeleteUnusedFiles / 4)))
			{
				IFileManager::Get().SetTimeStamp(*Filename, FDateTime::UtcNow());
			}

			COOK_STAT(Timer.AddHit(0));
		}
		return bExists;
	}
	/**
	 * Synchronous retrieve of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer to receive the results, if any were found
	 * @return				true if any data was found, and in this case OutData is non-empty
	 */
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& Data) override
	{
		COOK_STAT(auto Timer = UsageStats.TimeGet());
		check(!bFailed);
		FString Filename = BuildFilename(CacheKey);
		double StartTime = FPlatformTime::Seconds();
		if (FFileHelper::LoadFileToArray(Data,*Filename,FILEREAD_Silent))
		{
			if(!GIsBuildMachine)
			{
				double ReadDuration = FPlatformTime::Seconds() - StartTime;
				double ReadSpeed = ReadDuration > 5.0 ? (Data.Num() / ReadDuration) / (1024.0 * 1024.0) : 100.0;
				// Slower than 0.5MB/s?
				UE_CLOG(ReadSpeed < 0.5, LogDerivedDataCache, Warning, TEXT("%s is very slow (%.2fMB/s) when accessing %s, consider disabling it."), *CachePath, ReadSpeed, *Filename);
			}

			UE_LOG(LogDerivedDataCache, Verbose, TEXT("FileSystemDerivedDataBackend: Cache hit on %s"),*Filename);
			COOK_STAT(Timer.AddHit(Data.Num()));
			return true;
		}
		UE_LOG(LogDerivedDataCache, Verbose, TEXT("FileSystemDerivedDataBackend: Cache miss on %s"),*Filename);
		Data.Empty();
		return false;
	}
	/**
	 * Asynchronous, fire-and-forget placement of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer containing the data to cache, can be destroyed after the call returns, immediately
	 * @param	bPutEvenIfExists	If true, then do not attempt skip the put even if CachedDataProbablyExists returns true
	 */
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& Data, bool bPutEvenIfExists) override
	{
		COOK_STAT(auto Timer = UsageStats.TimePut());
		check(!bFailed);
		if (!bReadOnly)
		{
			if (bPutEvenIfExists || !CachedDataProbablyExists(CacheKey))
			{
				COOK_STAT(Timer.AddHit(Data.Num()));
				check(Data.Num());
				FString Filename = BuildFilename(CacheKey);
				FString TempFilename(TEXT("temp.")); 
				TempFilename += FGuid::NewGuid().ToString();
				TempFilename = FPaths::GetPath(Filename) / TempFilename;
				bool bResult;
				{
					bResult = FFileHelper::SaveArrayToFile(Data, *TempFilename);
				}
				if (bResult)
				{
					if (IFileManager::Get().FileSize(*TempFilename) == Data.Num())
					{
						bool DoMove = !CachedDataProbablyExists(CacheKey);
						if (bPutEvenIfExists && !DoMove)
						{
							DoMove = true;
							RemoveCachedData(CacheKey, /*bTransient=*/ false);
						}
						if (DoMove) 
						{
							if (!IFileManager::Get().Move(*Filename, *TempFilename, true, true, false, true))
							{
								UE_LOG(LogDerivedDataCache, Log, TEXT("FFileSystemDerivedDataBackend: Move collision, attempt at redundant update, OK %s."),*Filename);
							}
							else
							{
								UE_LOG(LogDerivedDataCache, Verbose, TEXT("FFileSystemDerivedDataBackend: Successful cache put to %s"),*Filename);
							}
						}
					}
					else
					{
						UE_LOG(LogDerivedDataCache, Warning, TEXT("FFileSystemDerivedDataBackend: Temp file is short %s!"),*TempFilename);
					}
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Warning, TEXT("FFileSystemDerivedDataBackend: Could not write temp file %s!"),*TempFilename);
				}
				// if everything worked, this is not necessary, but we will make every effort to avoid leaving junk in the cache
				if (FPaths::FileExists(TempFilename))
				{
					IFileManager::Get().Delete(*TempFilename, false, false, true);
				}
			}
		}
	}

	void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override
	{
		check(!bFailed);
		if (!bReadOnly && (!bTransient || bPurgeTransient))
		{
			FString Filename = BuildFilename(CacheKey);
			if (bTransient)
			{
				UE_LOG(LogDerivedDataCache,Verbose,TEXT("Deleting transient cached data. Key=%s Filename=%s"),CacheKey,*Filename);
			}
			IFileManager::Get().Delete(*Filename, false, false, true);
		}
	}

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) override
	{
		COOK_STAT(UsageStatsMap.Add(FString::Printf(TEXT("%s: %s.%s"), *GraphPath, TEXT("FileSystem"), *CachePath), UsageStats));
	}

private:
	FDerivedDataCacheUsageStats UsageStats;

	/**
	 * Threadsafe method to compute the filename from the cachekey, currently just adds a path and an extension.
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				filename built from the cache key
	 */
	FString BuildFilename(const TCHAR* CacheKey)
	{
		FString Key = FString(CacheKey).ToUpper();
		for (int32 i = 0; i < Key.Len(); i++)
		{
			check(FChar::IsAlnum(Key[i]) || FChar::IsUnderscore(Key[i]) || Key[i] == L'$');
		}
		uint32 Hash = FCrc::StrCrc_DEPRECATED(*Key);
		// this creates a tree of 1000 directories
		FString HashPath = FString::Printf(TEXT("%1d/%1d/%1d/"),(Hash/100)%10,(Hash/10)%10,Hash%10);
		return CachePath / HashPath / Key + TEXT(".udd");
	}

	/** Base path we are storing the cache files in. **/
	FString	CachePath;
	/** If true, do not attempt to write to this cache **/
	bool		bReadOnly;
	/** If true, we failed to write to this directory and it did not contain anything so we should not be used **/
	bool		bFailed;
	/** If true, CachedDataProbablyExists will update the file timestamps. */
	bool		bTouch;
	/** If true, allow transient data to be removed from the cache. */
	bool		bPurgeTransient;
	/** Age of file when it should be deleted from DDC cache. */
	int32		DaysToDeleteUnusedFiles;
};

FDerivedDataBackendInterface* CreateFileSystemDerivedDataBackend(const TCHAR* CacheDirectory, bool bForceReadOnly /*= false*/, bool bTouchFiles /*= false*/, bool bPurgeTransient /*= false*/, bool bDeleteOldFiles /*= false*/, int32 InDaysToDeleteUnusedFiles /*= 60*/, int32 InMaxNumFoldersToCheck /*= -1*/, int32 InMaxContinuousFileChecks /*= -1*/)
{
	FFileSystemDerivedDataBackend* FileDDB = new FFileSystemDerivedDataBackend(CacheDirectory, bForceReadOnly, bTouchFiles, bPurgeTransient, bDeleteOldFiles, InDaysToDeleteUnusedFiles, InMaxNumFoldersToCheck, InMaxContinuousFileChecks);
	if (!FileDDB->IsUsable())
	{
		delete FileDDB;
		FileDDB = NULL;
	}
	return FileDDB;
}
