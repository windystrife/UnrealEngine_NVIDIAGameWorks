// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "PackageDependencyData.h"
#include "HAL/FileManager.h"
#include "HAL/Runnable.h"
#include "DiskCachedAssetData.h"
#include "BackgroundGatherResults.h"

/**
 * Minimal amount of information needed about a discovered asset file
 */
struct FDiscoveredPackageFile
{
	explicit FDiscoveredPackageFile(FString InPackageFilename)
		: PackageFilename(MoveTemp(InPackageFilename))
		, PackageTimestamp(IFileManager::Get().GetTimeStamp(*PackageFilename))
	{
	}

	FDiscoveredPackageFile(FString InPackageFilename, FDateTime InPackageTimestamp)
		: PackageFilename(MoveTemp(InPackageFilename))
		, PackageTimestamp(MoveTemp(InPackageTimestamp))
	{
	}

	FDiscoveredPackageFile(const FDiscoveredPackageFile& Other)
		: PackageFilename(Other.PackageFilename)
		, PackageTimestamp(Other.PackageTimestamp)
	{
	}

	FDiscoveredPackageFile(FDiscoveredPackageFile&& Other)
		: PackageFilename(MoveTemp(Other.PackageFilename))
		, PackageTimestamp(MoveTemp(Other.PackageTimestamp))
	{
	}

	FDiscoveredPackageFile& operator=(const FDiscoveredPackageFile& Other)
	{
		if (this != &Other)
		{
			PackageFilename = Other.PackageFilename;
			PackageTimestamp = Other.PackageTimestamp;
		}
		return *this;
	}

	FDiscoveredPackageFile& operator=(FDiscoveredPackageFile&& Other)
	{
		if (this != &Other)
		{
			PackageFilename = MoveTemp(Other.PackageFilename);
			PackageTimestamp = MoveTemp(Other.PackageTimestamp);
		}
		return *this;
	}

	/** The name of the package file on disk */
	FString PackageFilename;

	/** The modification timestamp of the package file (when it was discovered) */
	FDateTime PackageTimestamp;
};


/**
 * Async task for discovering files that FAssetDataGatherer should search
 */
class FAssetDataDiscovery : public FRunnable
{
public:
	/** Constructor */
	FAssetDataDiscovery(const TArray<FString>& InPaths, bool bInIsSynchronous);
	virtual ~FAssetDataDiscovery();

	// FRunnable implementation
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** Signals to end the thread and waits for it to close before returning */
	void EnsureCompletion();

	/** Gets search results from the file discovery */
	bool GetAndTrimSearchResults(TArray<FString>& OutDiscoveredPaths, TArray<FDiscoveredPackageFile>& OutDiscoveredFiles, int32& OutNumPathsToSearch);

	/** Adds a root path to the search queue. Only works when searching asynchronously */
	void AddPathToSearch(const FString& Path);

	/** If assets are currently being asynchronously scanned in the specified path, this will cause them to be scanned before other assets. */
	void PrioritizeSearchPath(const FString& PathToPrioritize);

private:
	/** Sort the paths so that items belonging to the current priority path is processed first */
	void SortPathsByPriority(const int32 MaxNumToSort);

	/** A critical section to protect data transfer to other threads */
	FCriticalSection WorkerThreadCriticalSection;

	/** The current path to prioritize. Files and folders under this location will be bumped to the top of the processing list as they are discovered */
	FString FilenamePathToPrioritize;

	/** True if this gather request is synchronous (i.e, IsRunningCommandlet()) */
	bool bIsSynchronous;

	/** True if in the process of discovering files in PathsToSearch */
	bool bIsDiscoveringFiles;

	/** The directories in which to discover assets and paths */
	// IMPORTANT: This variable may be modified by from a different thread via a call to AddPathToSearch(), so access 
	//            to this array should be handled very carefully.
	TArray<FString> DirectoriesToSearch;

	/** The paths found during the search. It is not threadsafe to directly access this array */
	TArray<FString> DiscoveredPaths;

	/** List of priority files that need to be processed by the gatherer. It is not threadsafe to directly access this array */
	TArray<FDiscoveredPackageFile> PriorityDiscoveredFiles;

	/** List of non-priority files that need to be processed by the gatherer. It is not threadsafe to directly access this array */
	TArray<FDiscoveredPackageFile> NonPriorityDiscoveredFiles;

	/** > 0 if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter StopTaskCounter;

	/** Thread to run the discovery FRunnable on */
	FRunnableThread* Thread;
};


/**
 * Used to control the cache location and behavior of an FAssetDataGatherer cache
 */
enum class EAssetDataCacheMode : uint8
{
	/** Do not cache */
	NoCache,

	/** Use the monolithic CachedAssetRegistry.bin cache (should only be used for the main asset registry scan) */
	UseMonolithicCache,

	/** Use a modular cache file based on the given paths (should be used for synchronous scans, eg, from object libraries) */
	UseModularCache,
};


/**
 * Async task for gathering asset data from from the file list in FAssetRegistry
 */
class FAssetDataGatherer : public FRunnable
{
public:
	/** Constructor */
	FAssetDataGatherer(const TArray<FString>& Paths, const TArray<FString>& SpecificFiles, bool bInIsSynchronous, EAssetDataCacheMode AssetDataCacheMode);
	virtual ~FAssetDataGatherer();

	// FRunnable implementation
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;

	/** Signals to end the thread and waits for it to close before returning */
	void EnsureCompletion();

	/** Gets search results from the data gatherer */
	bool GetAndTrimSearchResults(TBackgroundGatherResults<FAssetData*>& OutAssetResults, TBackgroundGatherResults<FString>& OutPathResults, TBackgroundGatherResults<FPackageDependencyData>& OutDependencyResults, TBackgroundGatherResults<FString>& OutCookedPackageNamesWithoutAssetDataResults, TArray<double>& OutSearchTimes, int32& OutNumFilesToSearch, int32& OutNumPathsToSearch, bool& OutIsDiscoveringFiles);

	/** Adds a root path to the search queue. Only works when searching asynchronously */
	void AddPathToSearch(const FString& Path);

	/** Adds specific files to the search queue. Only works when searching asynchronously */
	void AddFilesToSearch(const TArray<FString>& Files);

	/** If assets are currently being asynchronously scanned in the specified path, this will cause them to be scanned before other assets. */
	void PrioritizeSearchPath(const FString& PathToPrioritize);

private:
	/** Sort the paths so that items belonging to the current priority path is processed first */
	void SortPathsByPriority(const int32 MaxNumToSort);

	/**
	 * Reads FAssetData information out of a file
	 *
	 * @param AssetFilename the name of the file to read
	 * @param AssetDataList the FAssetData for every asset found in the file
	 * @param DependencyData the FPackageDependencyData for every asset found in the file
	 * @param OutCanRetry Set to true if this file failed to load, but might be loadable later (due to missing modules)
	 *
	 * @return true if the file was successfully read
	 */
	bool ReadAssetFile(const FString& AssetFilename, TArray<FAssetData*>& AssetDataList, FPackageDependencyData& DependencyData, TArray<FString>& CookedPackagesToLoadUponDiscovery, bool& OutCanRetry) const;

	/** Serializes the timestamped cache of discovered assets. Used for quick loading of data for assets that have not changed on disk */
	void SerializeCache(FArchive& Ar);

private:
	/** A critical section to protect data transfer to the main thread */
	FCriticalSection WorkerThreadCriticalSection;

	/** The current path to prioritize. Files and folders under this location will be bumped to the top of the processing list as they are discovered */
	FString FilenamePathToPrioritize;

	/** List of files that need to be processed by the search. It is not threadsafe to directly access this array */
	TArray<FDiscoveredPackageFile> FilesToSearch;

	/** > 0 if we've been asked to abort work in progress at the next opportunity */
	FThreadSafeCounter StopTaskCounter;

	/** True if this gather request is synchronous (i.e, IsRunningCommandlet()) */
	bool bIsSynchronous;

	/** True if in the process of discovering files in PathsToSearch */
	bool bIsDiscoveringFiles;

	/** The current search start time */
	double SearchStartTime;

	/** The asset data gathered from the searched files */
	TArray<FAssetData*> AssetResults;

	/** Dependency data for scanned packages */
	TArray<FPackageDependencyData> DependencyResults;

	/** A list of cooked packages that did not have asset data in them. These assets may still contain assets (if they were older for example) */
	TArray<FString> CookedPackageNamesWithoutAssetDataResults;

	/** All the search times since the last main thread tick */
	TArray<double> SearchTimes;

	/** The paths found during the search */
	TArray<FString> DiscoveredPaths;

	/** True if dependency data should be gathered */
	bool bGatherDependsData;

	/** The cached value of the NumPathsToSearch returned by FAssetDataDiscovery the last time we synchronized with it */
	int32 NumPathsToSearchAtLastSyncPoint;

	/** Background package file discovery (when running async) */
	TSharedPtr<FAssetDataDiscovery> BackgroundPackageFileDiscovery;

	///////////////////////////////////////////////////////////////
	// Asset discovery caching
	///////////////////////////////////////////////////////////////

	/** True if this gather request should both load and save the asset cache. Only one gatherer should do this at a time! */
	bool bLoadAndSaveCache;

	/** True if we have finished discovering our first wave of files */
	bool bFinishedInitialDiscovery;

	/** The name of the file that contains the timestamped cache of discovered assets */
	FString CacheFilename;

	/** An array of all cached data that was newly discovered this run. This array is just used to make sure they are all deleted at shutdown */
	TArray<FDiskCachedAssetData*> NewCachedAssetData;

	/** 
	 * Map of PackageName to cached discovered assets that were loaded from disk. 
	 * This should only be modified in the loading section of SerializeCache and remain const after
	 * since NewCachedAssetDataMap keeps pointers to the values in this map.
	 */
	TMap<FName, FDiskCachedAssetData> DiskCachedAssetDataMap;

	/** Map of PackageName to cached discovered assets that will be written to disk at shutdown */
	TMap<FName, FDiskCachedAssetData*> NewCachedAssetDataMap;

	/** Thread to run the cleanup FRunnable on */
	FRunnableThread* Thread;
};
