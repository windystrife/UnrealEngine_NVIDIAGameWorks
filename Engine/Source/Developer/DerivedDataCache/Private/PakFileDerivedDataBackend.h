// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "DerivedDataBackendInterface.h"
#include "ProfilingDebugging/CookStats.h"
#include "DerivedDataCacheUsageStats.h"
#include "Misc/ScopeLock.h"
#include "Templates/ScopedPointer.h"
#include "Misc/FileHelper.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UniquePtr.h"

class Error;

/** 
 * A simple thread safe, pak file based backend. 
**/
class FPakFileDerivedDataBackend : public FDerivedDataBackendInterface
{
public:
	FPakFileDerivedDataBackend(const TCHAR* InFilename, bool bInWriting);
	~FPakFileDerivedDataBackend();

	void Close();

	/** return true if this cache is writable **/
	virtual bool IsWritable() override;

	virtual bool BackfillLowerCacheLevels() override;

	/**
	 * Synchronous test for the existence of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @return				true if the data probably will be found, this can't be guaranteed because of concurrency in the backends, corruption, etc
	 */
	virtual bool CachedDataProbablyExists(const TCHAR* CacheKey) override;

	/**
	 * Synchronous retrieve of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	OutData		Buffer to receive the results, if any were found
	 * @return				true if any data was found, and in this case OutData is non-empty
	 */
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData) override;

	/**
	 * Asynchronous, fire-and-forget placement of a cache item
	 *
	 * @param	CacheKey	Alphanumeric+underscore key of this cache item
	 * @param	InData		Buffer containing the data to cache, can be destroyed after the call returns, immediately
	 * @param	bPutEvenIfExists	If true, then do not attempt skip the put even if CachedDataProbablyExists returns true
	 */
	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) override;
	virtual void RemoveCachedData(const TCHAR* CacheKey, bool bTransient) override;

	/**
	 * Save the cache to disk
	 * @return	true if file was saved sucessfully
	 */
	bool SaveCache();

	/**
	 * Load the cache to disk	 * @param	Filename	Filename to load
	 * @return	true if file was loaded sucessfully
	 */
	bool LoadCache(const TCHAR* InFilename);

	/**
	 * Merges another cache file into this one.
	 * @return true on success
	 */
	void MergeCache(FPakFileDerivedDataBackend* OtherPak);
	
	const FString& GetFilename() const
	{
		return Filename;
	}

	static bool SortAndCopy(const FString &InputFilename, const FString &OutputFilename);

	virtual void GatherUsageStats(TMap<FString, FDerivedDataCacheUsageStats>& UsageStatsMap, FString&& GraphPath) override;

private:
	FDerivedDataCacheUsageStats UsageStats;

	struct FCacheValue
	{
		int64 Offset;
		int64 Size;
		uint32 Crc;
		FCacheValue(int64 InOffset, uint32 InSize, uint32 InCrc)
			: Offset(InOffset)
			, Size(InSize)
			, Crc(InCrc)
		{
		}
	};

	/** When set to true, we are a pak writer (we don't do reads). */
	bool bWriting;
	/** When set to true, we are a pak writer and we saved, so we shouldn't be used anymore. Also, a read cache that failed to open. */
	bool bClosed;
	/** Object used for synchronization via a scoped lock						*/
	FCriticalSection	SynchronizationObject;
	/** Set of files that are being written to disk asynchronously. */
	TMap<FString, FCacheValue> CacheItems;
	/** File handle of pak. */
	TUniquePtr<FArchive> FileHandle;
	/** File name of pak. */
	FString Filename;
	enum 
	{
		/** Magic number to use in header */
		PakCache_Magic = 0x0c7c0ddc,
	};
};

class FCompressedPakFileDerivedDataBackend : public FPakFileDerivedDataBackend
{
public:
	FCompressedPakFileDerivedDataBackend(const TCHAR* InFilename, bool bInWriting);

	virtual void PutCachedData(const TCHAR* CacheKey, TArray<uint8>& InData, bool bPutEvenIfExists) override;
	virtual bool GetCachedData(const TCHAR* CacheKey, TArray<uint8>& OutData) override;

private:
	static const ECompressionFlags CompressionFlags = (ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory);
};
