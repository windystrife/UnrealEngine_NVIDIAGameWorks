// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "DerivedDataBackendInterface.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheUsageStats.h"
#include "Misc/ScopeLock.h"
#include "Misc/FileHelper.h"

class Error;

/**
 * FDerivedDataBackendVerifyWrapper, a wrapper for derived data that verifies the cache is bit-wise identical by failing all gets and verifying the puts
**/
class FDerivedDataBackendVerifyWrapper : public FDerivedDataBackendInterface
{
public:

	/**
	 * Constructor
	 *
	 * @param	InInnerBackend		Backend to use for storage, my responsibilities are about async puts
	 * @param	InbFixProblems		if true, fix any problems found
	 */
	FDerivedDataBackendVerifyWrapper(FDerivedDataBackendInterface* InInnerBackend, bool InbFixProblems)
		: bFixProblems(InbFixProblems)
		, InnerBackend(InInnerBackend)
	{
		check(InnerBackend);
	}

	virtual bool IsWritable() override
	{
		return true;
	}

	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override
	{
		COOK_STAT(auto Timer = UsageStats.TimeProbablyExists());
		FScopeLock ScopeLock(&SynchronizationObject);
		if (AlreadyTested.Contains(FString(CacheKey)))
		{
			COOK_STAT(Timer.AddHit(0));
			return true;
		}
		return false;
	}
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData) override
	{
		COOK_STAT(auto Timer = UsageStats.TimeGet());
		bool bAlreadyTested = false;
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			if (AlreadyTested.Contains(FString(CacheKey)))
			{
				bAlreadyTested = true;
			}
		}
		if (bAlreadyTested)
		{
			bool bResult = InnerBackend->GetCachedData(CacheKey, OutData);
			if (bResult)
			{
				COOK_STAT(Timer.AddHit(OutData.Num()));
			}
			return bResult;
		}
		return false;
	}
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) override
	{
		COOK_STAT(auto Timer = UsageStats.TimePut());
		bool bAlreadyTested = false;
		{
			FScopeLock ScopeLock(&SynchronizationObject);
			if (AlreadyTested.Contains(FString(CacheKey)))
			{
				bAlreadyTested = true;
			}
			else
			{
				AlreadyTested.Add(FString(CacheKey));
			}
		}
		if (!bAlreadyTested)
		{
			COOK_STAT(Timer.AddHit(InData.Num()));
			TArray<uint8> OutData;
			bool bSuccess = InnerBackend->GetCachedData(CacheKey, OutData);
			if (bSuccess)
			{
				if (OutData != InData)
				{
					UE_LOG(LogDerivedDataCache, Error, TEXT("Verify: Cached data differs from newly generated data %s."), CacheKey);
					FString Cache = FPaths::ProjectSavedDir() / TEXT("VerifyDDC") / CacheKey + TEXT(".fromcache");
					FFileHelper::SaveArrayToFile(OutData, *Cache);
					FString Verify = FPaths::ProjectSavedDir() / TEXT("VerifyDDC") / CacheKey + TEXT(".verify");;
					FFileHelper::SaveArrayToFile(InData, *Verify);
					if (bFixProblems)
					{
						UE_LOG(LogDerivedDataCache, Display, TEXT("Verify: Wrote newly generated data to the cache."), CacheKey);
						InnerBackend->PutCachedData(CacheKey, InData, true);
					}
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Log, TEXT("Verify: Cached data exists and matched %s."), CacheKey);
				}
			}
			else
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("Verify: Cached data didn't exist %s."), CacheKey);
				InnerBackend->PutCachedData(CacheKey, InData, bPutEvenIfExists);
			}
		}
	}
	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override
	{
		InnerBackend->RemoveCachedData(CacheKey, bTransient);
	}

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) override
	{
		COOK_STAT(
		{
			UsageStatsMap.Add(GraphPath + TEXT(": VerifyWrapper"), UsageStats);
			if (InnerBackend)
			{
				InnerBackend->GatherUsageStats(UsageStatsMap, GraphPath + TEXT(". 0"));
			}
		});
	}

private:
	FDerivedDataCacheUsageStats UsageStats;

	/** If problems are encountered, do we fix them?						*/
	bool											bFixProblems;
	/** Object used for synchronization via a scoped lock						*/
	FCriticalSection								SynchronizationObject;
	/** Set of cache keys we already tested **/
	TSet<FString>									AlreadyTested;
	/** Backend to service the actual requests */
	FDerivedDataBackendInterface*					InnerBackend;
};
