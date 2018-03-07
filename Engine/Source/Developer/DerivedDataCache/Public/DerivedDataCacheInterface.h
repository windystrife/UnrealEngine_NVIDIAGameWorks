// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FDerivedDataCacheUsageStats;

/** 
 * Opaque class for rollup handling
**/
class IDerivedDataRollup
{
public:
	virtual ~IDerivedDataRollup()
	{
	}
};

/** 
 * Interface for the derived data cache
 * This API is fully threadsafe (with the possible exception of the system interface: NotfiyBootComplete, etc).
**/
class FDerivedDataCacheInterface
{
public:
	virtual ~FDerivedDataCacheInterface()
	{
	}

	//--------------------
	// High Level Interface
	//--------------------

	/** 
	 * Synchronously checks the cache and if the item is present, it returns the cached results, otherwise tells the deriver to build the data and then updates the cache
	 * @param	DataDeriver	plugin to produce cache key and in the event of a miss, return the data.
	 * @param	bDataWasBuilt if non-null, set to true if the data returned had to be built instead of retrieved from the DDC. Used for stat tracking.
	 * @return	true if the data was retrieved from the cache or the deriver built the data sucessfully. false can only occur if the plugin returns false.
	**/
	virtual bool GetSynchronous(class FDerivedDataPluginInterface* DataDeriver, TArray<uint8>& OutData, bool* bDataWasBuilt = nullptr) = 0;

	/** 
	 * Starts the async process of checking the cache and if the item is present, retrieving the cached results, otherwise telling the deriver to build the data and then updating the cache
	 * If the plugin does not support threading, all of the above will be completed before the call returns.
	 * @param	DataDeriver	plugin to produce cache key and in the event of a miss, return the data.
	 * @return	a handle that can be used for PollAsynchronousCompletion, WaitAsynchronousCompletion and GetAsynchronousResults
	**/
	virtual uint32 GetAsynchronous(class FDerivedDataPluginInterface* DataDeriver) = 0;
	/** 
	 * Polls a previous GetAsynchronous get for completion.
	 * @param	Handle	Handle returned from GetAsynchronous.
	 * @return			true if the build / retrieve is complete and the results can be obtained.
	**/
	virtual bool PollAsynchronousCompletion(uint32 Handle) = 0;
	/** 
	 * Blocks the current thread until an previous GetAsynchronous request is ready
	 * @param	Handle	Handle returned from GetAsynchronous.
	**/
	virtual void WaitAsynchronousCompletion(uint32 Handle) = 0;
	/** 
	 * Retrieves the results from an async lookup / build. MUST only be called after the results are known to be ready by one of the aforementioned calls.
	 * @param	Handle	Handle returned from GetAsynchronous.
	 * @param	OutData	Array to receive the output results.
	 * @param	bDataWasBuilt if non-null, set to true if the data returned had to be built instead of retrieved from the DDC. Used for stat tracking.
	 * @return			true if the data was retrieved from the cache or the deriver built the data successfully. false can only occur if the plugin returns false.
	**/
	virtual bool GetAsynchronousResults(uint32 Handle, TArray<uint8>& OutData, bool* bDataWasBuilt = nullptr) = 0;

	//--------------------------
	// Low Level Static Helpers
	//--------------------------

	/** 
	 * Static function to make sure a cache key contains only legal characters by using an escape
	 * @param CacheKey							Cache key to sanitize
	 * @return									Sanitized cache key
	**/
	static FString SanitizeCacheKey(const TCHAR* CacheKey)
	{
		FString Output;
		FString Input(CacheKey);
		int32 StartValid = 0;
		int32 NumValid = 0;

		for (int32 i = 0; i < Input.Len(); i++)
		{
			if (FChar::IsAlnum(Input[i]) || FChar::IsUnderscore(Input[i]))
			{
				NumValid++;
			}
			else
			{
				// Copy the valid range so far
				Output += Input.Mid(StartValid, NumValid);

				// Reset valid ranges
				StartValid = i + 1;
				NumValid = 0;

				// Replace the invalid character with a special string
				Output += FString::Printf(TEXT("$%x"), uint32(Input[i]));
			}
		}

		// Just return the input if the entire string was valid
		if (StartValid == 0 && NumValid == Input.Len())
		{
			return Input;
		}
		else if (NumValid > 0)
		{
			// Copy the remaining valid part
			Output += Input.Mid(StartValid, NumValid);
		}

		return Output;
	}

	/** 
	 * Static function to build a cache key out of the plugin name, versions and plugin specific info
	 * @param PluginName						Name of the derived data type
	 * @param VersionString						Version string of the derived data
	 * @param PluginSpecificCacheKeySuffix		GUIDS and / or Hashes, etc to uniquely identify the specific cache entry
	 * @return									Assembled cache key
	**/
	static FString BuildCacheKey(const TCHAR* PluginName, const TCHAR* VersionString, const TCHAR* PluginSpecificCacheKeySuffix)
	{
		return SanitizeCacheKey(*FString::Printf(TEXT("%s_%s_%s"), PluginName, VersionString, PluginSpecificCacheKeySuffix));
	}

	//--------------------
	// Low Level Interface
	//--------------------

	/** 
	 * Starts a rollup. Use this for GetAsynchronous calls, then call close on it
	 * @return	a pointer to a rollup
	**/
	virtual IDerivedDataRollup* StartRollup() = 0;

	/** 
	 * Ends a rollup. Use this for GetAsynchronous calls, then call close on it
	 * @param Rollup	reference to a pointer to the rollup to close...also sets the pointer to NULL
	**/
	virtual void EndRollup(IDerivedDataRollup*& Rollup) = 0;

	/** 
	 * Synchronously checks the cache and if the item is present, it returns the cached results, otherwise it returns false
	 * @param	CacheKey	Key to identify the data
	 * @return	true if the data was retrieved from the cache
	**/
	virtual bool GetSynchronous(const TCHAR* CacheKey, TArray<uint8>& OutData) = 0; 

	/** 
	 * Starts the async process of checking the cache and if the item is present, retrieving the cached results
	 * @param	CacheKey	Key to identify the data
	 * @param	Rollup		Rollup pointer, if this is part of a rollup
	 * @return	a handle that can be used for PollAsynchronousCompletion, WaitAsynchronousCompletion and GetAsynchronousResults
	**/
	virtual uint32 GetAsynchronous(const TCHAR* CacheKey, IDerivedDataRollup* Rollup = NULL) = 0;

	/** 
	 * Puts data into the cache. This is fire-and-forget and typically asynchronous.
	 * @param	CacheKey	Key to identify the data
	 * @param	Data		Data to put in the cache under this key
	**/
	virtual void Put(const TCHAR* CacheKey, TArray<uint8>& Data, bool bPutEvenIfExists = false) = 0;

	/**
	 * Hint that the data associated with the key is transient and may be optionally purged from the cache.
	 * @param	CacheKey	Key that is associated with transient data.
	 */
	virtual void MarkTransient(const TCHAR* CacheKey) = 0;

	/**
	 * Returns true if the data associated with the key is likely to exist in the cache.
	 * Even if this function returns true, a get for this key may still fail!
	 * @param	CacheKey	Key to see if data probably exists.
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) = 0;

	//--------------------
	// System Interface
	//--------------------

	/**
	 * Notify the system that the boot process is complete and so we can write the boot cache and get rid of it
	 */
	virtual void NotifyBootComplete() = 0;

	/**
	 * Adds or subtracts a number from the thread safe counter which tracks outstand async requests. This is used to ensure everything is complete prior to shutdown.
	 */
	virtual void AddToAsyncCompletionCounter(int32 Addend) = 0;

	/**
	 * Wait for all outstanding async DDC operations to complete.
	 */
	virtual void WaitForQuiescence(bool bShutdown = false) = 0;

	/**
	 * Retrieve the directories used by the DDC
	 */
	virtual void GetDirectories(TArray<FString>& OutResults) = 0;

	//--------------------
	// UsageStats Interface
	//--------------------

	/**
	 * Retrieve usage stats by the DDC
	 */
	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStats) = 0;
};

/**
 * Module for the DDC
 */
class IDerivedDataCacheModule : public IModuleInterface
{
public:
	/** Return the DDC interface **/
	virtual FDerivedDataCacheInterface& GetDDC() = 0;
};

