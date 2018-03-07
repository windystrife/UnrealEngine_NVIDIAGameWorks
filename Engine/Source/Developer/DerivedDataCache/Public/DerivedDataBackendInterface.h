// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"

class FDerivedDataCacheUsageStats;

DECLARE_LOG_CATEGORY_EXTERN(LogDerivedDataCache, Log, All);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Gets"),STAT_DDC_NumGets,STATGROUP_DDC, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Puts"),STAT_DDC_NumPuts,STATGROUP_DDC, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Build"),STAT_DDC_NumBuilds,STATGROUP_DDC, );
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num Exists"),STAT_DDC_NumExist,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Sync Get Time"),STAT_DDC_SyncGetTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("ASync Wait Time"),STAT_DDC_ASyncWaitTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Sync Put Time"),STAT_DDC_PutTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Sync Build Time"),STAT_DDC_SyncBuildTime,STATGROUP_DDC, );
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Exists Time"),STAT_DDC_ExistTime,STATGROUP_DDC, );

/** 
 * Interface for cache server backends. 
 * The entire API should be callable from any thread (except the singleton can be assumed to be called at least once before concurrent access).
**/
class FDerivedDataBackendInterface
{
public:
	virtual ~FDerivedDataBackendInterface()
	{
	}

	/** return true if this cache is writable **/
	virtual bool IsWritable()=0;

	/** 
	 * return true if hits on this cache should propagate to lower cache level. Typically false for a PAK file. 
	 * Caution! This generally isn't propagated, so the thing that returns false must be a direct child of the heirarchical cache.
	 **/
	virtual bool BackfillLowerCacheLevels()
	{
		return true;
	}

	/**
	 * Synchronous test for the existence of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				true if the data probably will be found, this can't be guaranteed because of concurrency in the backends, corruption, etc
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey)=0;
	/**
	 * Synchronous retrieve of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer to receive the results, if any were found
	 * @return				true if any data was found, and in this case OutData is non-empty
	 */
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData)=0;
	/**
	 * Asynchronous, fire-and-forget placement of a cache item
	 *
	 * @param	CacheKey			Alphanumeric+underscore key of this cache item
	 * @param	InData				Buffer containing the data to cache, can be destroyed after the call returns, immediately
	 * @param	bPutEvenIfExists	If true, then do not attempt skip the put even if CachedDataProbablyExists returns true
	 */
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) = 0;

	/**
	 * Remove data from cache (used in the event that corruption is detected at a higher level and possibly house keeping)
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	bTransient	true if the data is transient and it is up to the backend to decide when and if to remove cached data.
	 */
	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient)=0;

	/**
	 * Retrieve usage stats for this backend. If the backend holds inner backends, this is expected to be passed down recursively.
	 * @param	UsageStatsMap		The map of usages. Each backend instance should give itself a unique name if possible (ie, use the filename associated).
	 * @param	GraphPath			Path to the node in the graph. If you have inner nodes, add their index to the current path as ". <n>".
	 *								This will create a path such as "0. 1. 0. 2", which can uniquely identify this node.
	 */
	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) = 0;
};

class FDerivedDataBackend
{
public:
	/**
	 * Singleton to retrieve the GLOBAL backend
	 *
	 * @return Reference to the global cache backend
	 */
	static FDerivedDataBackend& Get();

	/**
	 * Singleton to retrieve the root cache
	 * @return Reference to the global cache root
	 */
	virtual FDerivedDataBackendInterface& GetRoot() = 0;

	//--------------------
	// System Interface, copied from FDerivedDataCacheInterface
	//--------------------

	virtual void NotifyBootComplete() = 0;
	virtual void AddToAsyncCompletionCounter(int32 Addend) = 0;
	virtual void WaitForQuiescence(bool bShutdown = false) = 0;
	virtual void GetDirectories(TArray<FString>& OutResults) = 0;

	/**
	 * Mounts a read-only pak file.
	 *
	 * @param PakFilename Pak filename
	 */
	virtual FDerivedDataBackendInterface* MountPakFile(const TCHAR* PakFilename) = 0;

	/**
	 * Unmounts a read-only pak file.
	 *
	 * @param PakFilename Pak filename
	 */
	virtual bool UnmountPakFile(const TCHAR* PakFilename) = 0;

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStats) = 0;

};
