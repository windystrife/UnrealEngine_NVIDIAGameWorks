// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DerivedDataBackendInterface.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheUsageStats.h"

/** 
 * Helper class for placing a footer at the end of of a cache file.
 * No effort is made to byte-swap this as we assume local format.
**/
struct FDerivedDataTrailer
{
	enum
	{
		/** Arbitrary number used to identify corruption **/
		MagicConstant = 0x1e873d89
	};

	/** Arbitrary number used to identify corruption **/
	uint32 Magic;
	/** Version of the backend, for future use **/
	uint32 Version;
	/** CRC of the payload, used to detect corruption **/
	uint32 CRCofPayload;
	/** Size of the payload, used to detect corruption **/
	uint32 SizeOfPayload;

	/** Constructor that zeros everything. This is never valid because even the magic number is wrong **/
	FDerivedDataTrailer() 
		: Magic(0)
		, Version(0)
		, CRCofPayload(0)
		, SizeOfPayload(0)
	{
	}

	/**
		* Constructor that sets the data properly for a given buffer of data
		* @param	Data	Buffer of data to setup this trailer for
		*/
	FDerivedDataTrailer(const TArray<uint8>& Data)
		: Magic(MagicConstant)
		, Version(1)
		, CRCofPayload(FCrc::MemCrc_DEPRECATED(Data.GetData(), Data.Num()))
		, SizeOfPayload(Data.Num())
	{
	}
	/**
		* Comparison operator
		* @param	Other	trailer to compare "this" to
		* @return			true if all elements of the trailer match exactly
		*/
	bool operator==(const FDerivedDataTrailer& Other)
	{
		return Magic == Other.Magic &&
			Version == Other.Version &&
			CRCofPayload == Other.CRCofPayload &&
			SizeOfPayload == Other.SizeOfPayload;
	}
};


/** 
 * A backend wrapper that adds a footer to the data to check the CRC, length, etc.
**/
class FDerivedDataBackendCorruptionWrapper : public FDerivedDataBackendInterface
{
public:

	/**
	 * Constructor
	 *
	 * @param	InInnerBackend	Backend to use for storage, my responsibilities are about corruption
	 */
	FDerivedDataBackendCorruptionWrapper(FDerivedDataBackendInterface* InInnerBackend)
		: InnerBackend(InInnerBackend)
	{
		check(InnerBackend);
	}

	/** return true if this cache is writable **/
	virtual bool IsWritable() override
	{
		return InnerBackend->IsWritable();
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
		bool Result = InnerBackend->CachedDataProbablyExists(CacheKey);
		if (Result)
		{
			COOK_STAT(Timer.AddHit(0));
		}
		return Result;
	}
	/**
	 * Synchronous retrieve of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer to receive the results, if any were found
	 * @return				true if any data was found, and in this case OutData is non-empty
	 */
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData) override
	{
		COOK_STAT(auto Timer = UsageStats.TimeGet());
		bool bOk = InnerBackend->GetCachedData(CacheKey, OutData);
		if (bOk)
		{
			if (OutData.Num() < sizeof(FDerivedDataTrailer))
			{
				UE_LOG(LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendCorruptionWrapper: Corrupted file (short), ignoring and deleting %s."),CacheKey);
				bOk	= false;
			}
			else
			{
				FDerivedDataTrailer Trailer;
				FMemory::Memcpy(&Trailer,&OutData[OutData.Num() - sizeof(FDerivedDataTrailer)], sizeof(FDerivedDataTrailer));
				OutData.RemoveAt(OutData.Num() - sizeof(FDerivedDataTrailer),sizeof(FDerivedDataTrailer), false);
				FDerivedDataTrailer RecomputedTrailer(OutData);
				if (Trailer == RecomputedTrailer)
				{
					UE_LOG(LogDerivedDataCache, Verbose, TEXT("FDerivedDataBackendCorruptionWrapper: cache hit, footer is ok %s"),CacheKey);
				}
				else
				{
					UE_LOG(LogDerivedDataCache, Warning, TEXT("FDerivedDataBackendCorruptionWrapper: Corrupted file, ignoring and deleting %s."),CacheKey);
					bOk	= false;
				}
			}
			if (!bOk)
			{
				// _we_ detected corruption, so _we_ will force a flush of the corrupted data
				InnerBackend->RemoveCachedData(CacheKey, /*bTransient=*/ false);
			}
		}
		if (!bOk)
		{
			OutData.Empty();
		}
		else
		{
			COOK_STAT(Timer.AddHit(OutData.Num()));
		}
		return bOk;
	}
	/**
	 * Asynchronous, fire-and-forget placement of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	InData		Buffer containing the data to cache, can be destroyed after the call returns, immediately
	 * @param	bPutEvenIfExists	If true, then do not attempt skip the put even if CachedDataProbablyExists returns true
	 */
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) override
	{
		COOK_STAT(auto Timer = UsageStats.TimePut());
		if (!InnerBackend->IsWritable())
		{
			return; // no point in continuing down the chain
		}
		COOK_STAT(Timer.AddHit(InData.Num()));
		// Get rid of the double copy!
		TArray<uint8> Data;
		Data.Reset( InData.Num() + sizeof(FDerivedDataTrailer) );
		Data.AddUninitialized(InData.Num());
		FMemory::Memcpy(&Data[0], &InData[0], InData.Num());
		FDerivedDataTrailer Trailer(Data);
		Data.AddUninitialized(sizeof(FDerivedDataTrailer));
		FMemory::Memcpy(&Data[Data.Num() - sizeof(FDerivedDataTrailer)], &Trailer, sizeof(FDerivedDataTrailer));
		InnerBackend->PutCachedData(CacheKey, Data, bPutEvenIfExists);
	}
	
	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override
	{
		if (!InnerBackend->IsWritable())
		{
			return; // no point in continuing down the chain
		}
		return InnerBackend->RemoveCachedData(CacheKey, bTransient);
	}

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) override
	{
		COOK_STAT(
		{
			UsageStatsMap.Add(GraphPath + TEXT(": CorruptionWrapper"), UsageStats);
			if (InnerBackend)
			{
				InnerBackend->GatherUsageStats(UsageStatsMap, GraphPath + TEXT(". 0"));
			}
		});
	}

private:
	FDerivedDataCacheUsageStats UsageStats;

	/** Backend to use for storage, my responsibilities are about corruption **/
	FDerivedDataBackendInterface* InnerBackend;
};

