// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetDataGatherer.h"
#include "HAL/PlatformProcess.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "AssetRegistryPrivate.h"
#include "NameTableArchive.h"
#include "PackageReader.h"
#include "AssetRegistry.h"

namespace AssetDataGathererConstants
{
	static const int32 CacheSerializationVersion = 12;
	static const int32 MaxFilesToDiscoverBeforeFlush = 2500;
	static const int32 MaxFilesToGatherBeforeFlush = 250;
	static const int32 MaxFilesToProcessBeforeCacheWrite = 50000;
}

namespace
{
	class FLambdaDirectoryStatVisitor : public IPlatformFile::FDirectoryStatVisitor
	{
	public:
		typedef TFunctionRef<bool(const TCHAR*, const FFileStatData&)> FLambdaRef;
		FLambdaRef Callback;
		explicit FLambdaDirectoryStatVisitor(FLambdaRef InCallback)
			: Callback(MoveTemp(InCallback))
		{
		}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData) override
		{
			return Callback(FilenameOrDirectory, StatData);
		}
	};

	bool IsValidPackageFileToRead(const FString& Filename)
	{
		FString LongPackageName;
		if (FPackageName::TryConvertFilenameToLongPackageName(Filename, LongPackageName))
		{
			// Make sure the path does not contain invalid characters. These packages will not be successfully loaded or read later.
			for (const TCHAR PackageChar : LongPackageName)
			{
				for (const TCHAR InvalidChar : INVALID_LONGPACKAGE_CHARACTERS)
				{
					if (PackageChar == InvalidChar)
					{
						return false;
					}
				}
			}

			return true;
		}

		return false;
	}
}


FAssetDataDiscovery::FAssetDataDiscovery(const TArray<FString>& InPaths, bool bInIsSynchronous)
	: bIsSynchronous(bInIsSynchronous)
	, bIsDiscoveringFiles(false)
	, StopTaskCounter(0)
	, Thread(nullptr)
{
	DirectoriesToSearch.Reserve(InPaths.Num());
	for (const FString& Path : InPaths)
	{
		// Convert the package path to a filename with no extension (directory)
		DirectoriesToSearch.Add(FPackageName::LongPackageNameToFilename(Path / TEXT("")));
	}

	if (bIsSynchronous)
	{
		Run();
	}
	else
	{
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataDiscovery"), 0, TPri_BelowNormal);
	}
}

FAssetDataDiscovery::~FAssetDataDiscovery()
{
}

bool FAssetDataDiscovery::Init()
{
	return true;
}

uint32 FAssetDataDiscovery::Run()
{
	double DiscoverStartTime = FPlatformTime::Seconds();
	int32 NumDiscoveredFiles = 0;

	FString LocalFilenamePathToPrioritize;

	TSet<FString> LocalDiscoveredPathsSet;
	TArray<FString> LocalDiscoveredDirectories;

	TArray<FDiscoveredPackageFile> LocalPriorityFilesToSearch;
	TArray<FDiscoveredPackageFile> LocalNonPriorityFilesToSearch;

	// This set contains the folders that we should hide by default unless they contain assets
	TSet<FString> PathsToHideIfEmpty;
	PathsToHideIfEmpty.Add(TEXT("/Game/Collections"));

	auto FlushLocalResultsIfRequired = [&]()
	{
		if (LocalPriorityFilesToSearch.Num() > 0 || LocalNonPriorityFilesToSearch.Num() > 0 || LocalDiscoveredPathsSet.Num() > 0)
		{
			TArray<FString> LocalDiscoveredPathsArray = LocalDiscoveredPathsSet.Array();

			{
				FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

				// Place all the discovered files into the files to search list
				DiscoveredPaths.Append(MoveTemp(LocalDiscoveredPathsArray));

				PriorityDiscoveredFiles.Append(MoveTemp(LocalPriorityFilesToSearch));
				NonPriorityDiscoveredFiles.Append(MoveTemp(LocalNonPriorityFilesToSearch));
			}
		}

		LocalDiscoveredPathsSet.Reset();

		LocalPriorityFilesToSearch.Reset();
		LocalNonPriorityFilesToSearch.Reset();
	};

	auto IsPriorityFile = [&](const FString& InPackageFilename) -> bool
	{
		return !bIsSynchronous && !LocalFilenamePathToPrioritize.IsEmpty() && InPackageFilename.StartsWith(LocalFilenamePathToPrioritize);
	};

	auto OnIterateDirectoryItem = [&](const TCHAR* InPackageFilename, const FFileStatData& InPackageStatData) -> bool
	{
		if (StopTaskCounter.GetValue() != 0)
		{
			// Requested to stop - break out of the directory iteration
			return false;
		}

		const FString PackageFilenameStr = InPackageFilename;

		if (InPackageStatData.bIsDirectory)
		{
			LocalDiscoveredDirectories.Add(PackageFilenameStr / TEXT(""));

			FString PackagePath;
			if (FPackageName::TryConvertFilenameToLongPackageName(PackageFilenameStr, PackagePath) && !PathsToHideIfEmpty.Contains(PackagePath))
			{
				LocalDiscoveredPathsSet.Add(PackagePath);
			}
		}
		else if (FPackageName::IsPackageFilename(PackageFilenameStr))
		{
			if (IsValidPackageFileToRead(PackageFilenameStr))
			{
				const FString LongPackageNameStr = FPackageName::FilenameToLongPackageName(PackageFilenameStr);

				if (IsPriorityFile(PackageFilenameStr))
				{
					LocalPriorityFilesToSearch.Add(FDiscoveredPackageFile(PackageFilenameStr, InPackageStatData.ModificationTime));
				}
				else
				{
					LocalNonPriorityFilesToSearch.Add(FDiscoveredPackageFile(PackageFilenameStr, InPackageStatData.ModificationTime));
				}

				LocalDiscoveredPathsSet.Add(FPackageName::GetLongPackagePath(LongPackageNameStr));

				++NumDiscoveredFiles;

				// Flush the data if we've processed enough
				if (!bIsSynchronous && (LocalPriorityFilesToSearch.Num() + LocalNonPriorityFilesToSearch.Num()) >= AssetDataGathererConstants::MaxFilesToDiscoverBeforeFlush)
				{
					FlushLocalResultsIfRequired();
				}
			}
		}

		return true;
	};

	bool bIsIdle = true;

	while (StopTaskCounter.GetValue() == 0)
	{
		FString LocalDirectoryToSearch;
		{
			FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

			if (DirectoriesToSearch.Num() > 0)
			{
				bIsDiscoveringFiles = true;

				LocalFilenamePathToPrioritize = FilenamePathToPrioritize;

				// Pop off the first path to search
				LocalDirectoryToSearch = DirectoriesToSearch[0];
				DirectoriesToSearch.RemoveAt(0, 1, false);
			}
		}

		if (LocalDirectoryToSearch.Len() > 0)
		{
			if (bIsIdle)
			{
				bIsIdle = false;

				// About to start work - reset these
				DiscoverStartTime = FPlatformTime::Seconds();
				NumDiscoveredFiles = 0;
			}

			// Iterate the current search directory
			FLambdaDirectoryStatVisitor Visitor(OnIterateDirectoryItem);
			IFileManager::Get().IterateDirectoryStat(*LocalDirectoryToSearch, Visitor);

			{
				FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

				// Push back any newly discovered sub-directories
				if (LocalDiscoveredDirectories.Num() > 0)
				{
					// Use LocalDiscoveredDirectories as scratch space, then move it back out - this puts the directories we just 
					// discovered at the start of the list for the next iteration, which can help with disk locality
					LocalDiscoveredDirectories.Append(MoveTemp(DirectoriesToSearch));
					DirectoriesToSearch = MoveTemp(LocalDiscoveredDirectories);
				}
				LocalDiscoveredDirectories.Reset();

				if (!bIsSynchronous)
				{
					FlushLocalResultsIfRequired();
					SortPathsByPriority(1);
				}
			}
		}
		else
		{
			if (!bIsIdle)
			{
				bIsIdle = true;

				{
					FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
					bIsDiscoveringFiles = false;
				}

				UE_LOG(LogAssetRegistry, Verbose, TEXT("Discovery took %0.6f seconds and found %d files to process"), FPlatformTime::Seconds() - DiscoverStartTime, NumDiscoveredFiles);
			}

			// Ran out of things to do... if we have any pending results, flush those now
			FlushLocalResultsIfRequired();

			if (bIsSynchronous)
			{
				// This is synchronous. Since our work is done, we should safely exit
				Stop();
			}
			else
			{
				// No work to do. Sleep for a little and try again later.
				FPlatformProcess::Sleep(0.1);
			}
		}
	}

	return 0;
}

void FAssetDataDiscovery::Stop()
{
	StopTaskCounter.Increment();
}

void FAssetDataDiscovery::Exit()
{
}

void FAssetDataDiscovery::EnsureCompletion()
{
	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
		DirectoriesToSearch.Empty();
	}

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FAssetDataDiscovery::GetAndTrimSearchResults(TArray<FString>& OutDiscoveredPaths, TArray<FDiscoveredPackageFile>& OutDiscoveredFiles, int32& OutNumPathsToSearch)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

	OutDiscoveredPaths.Append(MoveTemp(DiscoveredPaths));
	DiscoveredPaths.Reset();

	if (PriorityDiscoveredFiles.Num() > 0)
	{
		// Use PriorityDiscoveredFiles as scratch space, then move it back out - this puts the priority files at the start of the final list
		PriorityDiscoveredFiles.Append(MoveTemp(OutDiscoveredFiles));
		PriorityDiscoveredFiles.Append(MoveTemp(NonPriorityDiscoveredFiles));
		OutDiscoveredFiles = MoveTemp(PriorityDiscoveredFiles);
	}
	else
	{
		OutDiscoveredFiles.Append(MoveTemp(NonPriorityDiscoveredFiles));
	}
	PriorityDiscoveredFiles.Reset();
	NonPriorityDiscoveredFiles.Reset();

	OutNumPathsToSearch = DirectoriesToSearch.Num();

	return bIsDiscoveringFiles;
}

void FAssetDataDiscovery::AddPathToSearch(const FString& Path)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

	// Convert the package path to a filename with no extension (directory)
	DirectoriesToSearch.Add(FPackageName::LongPackageNameToFilename(Path / TEXT("")));
}

void FAssetDataDiscovery::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	FString LocalFilenamePathToPrioritize;
	if (FPackageName::TryConvertLongPackageNameToFilename(PathToPrioritize / TEXT(""), LocalFilenamePathToPrioritize))
	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

		FilenamePathToPrioritize = LocalFilenamePathToPrioritize;
		SortPathsByPriority(INDEX_NONE);
	}
}

void FAssetDataDiscovery::SortPathsByPriority(const int32 MaxNumToSort)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

	// Critical section. This code needs to be as fast as possible since it is in a critical section!
	// Swap all priority files to the top of the list
	if (FilenamePathToPrioritize.Len() > 0)
	{
		int32 LowestNonPriorityPathIdx = 0;
		int32 NumSorted = 0;
		const int32 NumToSort = (MaxNumToSort == INDEX_NONE) ? DirectoriesToSearch.Num() : FMath::Min(DirectoriesToSearch.Num(), MaxNumToSort);
		for (int32 DirIdx = 0; DirIdx < DirectoriesToSearch.Num(); ++DirIdx)
		{
			if (DirectoriesToSearch[DirIdx].StartsWith(FilenamePathToPrioritize))
			{
				DirectoriesToSearch.Swap(DirIdx, LowestNonPriorityPathIdx);
				LowestNonPriorityPathIdx++;

				if (++NumSorted >= NumToSort)
				{
					break;
				}
			}
		}
	}
}


FAssetDataGatherer::FAssetDataGatherer(const TArray<FString>& InPaths, const TArray<FString>& InSpecificFiles, bool bInIsSynchronous, EAssetDataCacheMode AssetDataCacheMode)
	: StopTaskCounter( 0 )
	, bIsSynchronous( bInIsSynchronous )
	, bIsDiscoveringFiles( false )
	, SearchStartTime( 0 )
	, NumPathsToSearchAtLastSyncPoint( InPaths.Num() )
	, bLoadAndSaveCache( false )
	, bFinishedInitialDiscovery( false )
	, Thread(nullptr)
{
	bGatherDependsData = (GIsEditor && !FParse::Param( FCommandLine::Get(), TEXT("NoDependsGathering") )) || FParse::Param(FCommandLine::Get(),TEXT("ForceDependsGathering")) ;

	if (FParse::Param(FCommandLine::Get(), TEXT("NoAssetRegistryCache")) || FParse::Param(FCommandLine::Get(), TEXT("multiprocess")))
	{
		bLoadAndSaveCache = false;
	}
	else if (AssetDataCacheMode != EAssetDataCacheMode::NoCache)
	{
		if (AssetDataCacheMode == EAssetDataCacheMode::UseMonolithicCache)
		{
			bLoadAndSaveCache = true;
			CacheFilename = FPaths::ProjectIntermediateDir() / TEXT("CachedAssetRegistry.bin");
		}
		else if (InPaths.Num() > 0)
		{
			// todo: handle hash collisions?
			uint32 CacheHash = GetTypeHash(InPaths[0]);
			for (int32 PathIndex = 1; PathIndex < InPaths.Num(); ++PathIndex)
			{
				CacheHash = HashCombine(CacheHash, GetTypeHash(InPaths[PathIndex]));
			}

			bLoadAndSaveCache = true;
			CacheFilename = FPaths::ProjectIntermediateDir() / TEXT("AssetRegistryCache") / FString::Printf(TEXT("%08x.bin"), CacheHash);
		}
	}

	// Add any specific files before doing search
	AddFilesToSearch(InSpecificFiles);

	if ( bIsSynchronous )
	{
		// Run the package file discovery synchronously
		FAssetDataDiscovery PackageFileDiscovery(InPaths, bIsSynchronous);
		PackageFileDiscovery.GetAndTrimSearchResults(DiscoveredPaths, FilesToSearch, NumPathsToSearchAtLastSyncPoint);

		Run();
	}
	else
	{
		BackgroundPackageFileDiscovery = MakeShareable(new FAssetDataDiscovery(InPaths, bIsSynchronous));
		Thread = FRunnableThread::Create(this, TEXT("FAssetDataGatherer"), 0, TPri_BelowNormal);
	}
}

FAssetDataGatherer::~FAssetDataGatherer()
{
	NewCachedAssetDataMap.Empty();
	DiskCachedAssetDataMap.Empty();

	for ( auto CacheIt = NewCachedAssetData.CreateConstIterator(); CacheIt; ++CacheIt )
	{
		delete (*CacheIt);
	}
	NewCachedAssetData.Empty();
}

bool FAssetDataGatherer::Init()
{
	return true;
}

uint32 FAssetDataGatherer::Run()
{
	int32 CacheSerializationVersion = AssetDataGathererConstants::CacheSerializationVersion;
	
	if ( bLoadAndSaveCache )
	{
		// load the cached data
		FNameTableArchiveReader CachedAssetDataReader(CacheSerializationVersion, CacheFilename);
		if (!CachedAssetDataReader.IsError())
		{
			FAssetRegistryVersion::Type Version = FAssetRegistryVersion::LatestVersion;
			if (FAssetRegistryVersion::SerializeVersion(CachedAssetDataReader, Version))
			{
				SerializeCache(CachedAssetDataReader);

				DependencyResults.Reserve(DiskCachedAssetDataMap.Num());
				AssetResults.Reserve(DiskCachedAssetDataMap.Num());
			}
		}
	}

	TArray<FDiscoveredPackageFile> LocalFilesToSearch;
	TArray<FAssetData*> LocalAssetResults;
	TArray<FPackageDependencyData> LocalDependencyResults;
	TArray<FString> LocalCookedPackageNamesWithoutAssetDataResults;

	const double InitialScanStartTime = FPlatformTime::Seconds();
	int32 NumCachedFiles = 0;
	int32 NumUncachedFiles = 0;

	int32 NumFilesProcessedSinceLastCacheSave = 0;
	auto WriteAssetCacheFile = [&]()
	{
		FNameTableArchiveWriter CachedAssetDataWriter(CacheSerializationVersion, CacheFilename);

		FAssetRegistryVersion::Type Version = FAssetRegistryVersion::LatestVersion;
		FAssetRegistryVersion::SerializeVersion(CachedAssetDataWriter, Version);

		SerializeCache(CachedAssetDataWriter);

		NumFilesProcessedSinceLastCacheSave = 0;
	};

	while ( StopTaskCounter.GetValue() == 0 )
	{
		bool LocalIsDiscoveringFiles = false;

		{
			FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

			// Grab any new package files from the background directory scan
			if (BackgroundPackageFileDiscovery.IsValid())
			{
				bIsDiscoveringFiles = BackgroundPackageFileDiscovery->GetAndTrimSearchResults(DiscoveredPaths, FilesToSearch, NumPathsToSearchAtLastSyncPoint);
				LocalIsDiscoveringFiles = bIsDiscoveringFiles;
			}

			AssetResults.Append(MoveTemp(LocalAssetResults));
			DependencyResults.Append(MoveTemp(LocalDependencyResults));
			CookedPackageNamesWithoutAssetDataResults.Append(MoveTemp(LocalCookedPackageNamesWithoutAssetDataResults));

			if (FilesToSearch.Num() > 0)
			{
				if (SearchStartTime == 0)
				{
					SearchStartTime = FPlatformTime::Seconds();
				}

				const int32 NumFilesToProcess = FMath::Min<int32>(AssetDataGathererConstants::MaxFilesToGatherBeforeFlush, FilesToSearch.Num());
				LocalFilesToSearch.Append(FilesToSearch.GetData(), NumFilesToProcess);
				FilesToSearch.RemoveAt(0, NumFilesToProcess, false);
			}
			else if (SearchStartTime != 0 && !LocalIsDiscoveringFiles)
			{
				SearchTimes.Add(FPlatformTime::Seconds() - SearchStartTime);
				SearchStartTime = 0;
			}
		}

		LocalAssetResults.Reset();
		LocalDependencyResults.Reset();
		LocalCookedPackageNamesWithoutAssetDataResults.Reset();

		TArray<FDiscoveredPackageFile> LocalFilesToRetry;

		if (LocalFilesToSearch.Num() > 0)
		{
			for (const FDiscoveredPackageFile& AssetFileData : LocalFilesToSearch)
			{
				if (StopTaskCounter.GetValue() != 0)
				{
					// We have been asked to stop, so don't read any more files
					break;
				}

				const FName PackageName = FName(*FPackageName::FilenameToLongPackageName(AssetFileData.PackageFilename));

				bool bLoadedFromCache = false;
				if (bLoadAndSaveCache)
				{
					FDiskCachedAssetData* DiskCachedAssetData = DiskCachedAssetDataMap.Find(PackageName);
					if (DiskCachedAssetData)
					{
						const FDateTime& CachedTimestamp = DiskCachedAssetData->Timestamp;
						if (AssetFileData.PackageTimestamp != CachedTimestamp)
						{
							DiskCachedAssetData = nullptr;
						}
					}

					if (DiskCachedAssetData)
					{
						if (DiskCachedAssetData->DependencyData.PackageName != PackageName && DiskCachedAssetData->DependencyData.PackageName != NAME_None)
						{
							UE_LOG(LogAssetRegistry, Display, TEXT("Cached dependency data for package '%s' is invalid. Discarding cached data."), *PackageName.ToString());
							DiskCachedAssetData = nullptr;
						}
					}

					if (DiskCachedAssetData)
					{
						bLoadedFromCache = true;

						++NumCachedFiles;

						LocalAssetResults.Reserve(LocalAssetResults.Num() + DiskCachedAssetData->AssetDataList.Num());
						for (const FAssetData& AssetData : DiskCachedAssetData->AssetDataList)
						{
							LocalAssetResults.Add(new FAssetData(AssetData));
						}

						if (bGatherDependsData)
						{
							LocalDependencyResults.Add(DiskCachedAssetData->DependencyData);
						}

						NewCachedAssetDataMap.Add(PackageName, DiskCachedAssetData);
					}
				}

				if (!bLoadedFromCache)
				{
					TArray<FAssetData*> AssetDataFromFile;
					FPackageDependencyData DependencyData;
					TArray<FString> CookedPackageNamesWithoutAssetData;
					bool bCanAttemptAssetRetry = false;
					if (ReadAssetFile(AssetFileData.PackageFilename, AssetDataFromFile, DependencyData, CookedPackageNamesWithoutAssetData, bCanAttemptAssetRetry))
					{
						++NumUncachedFiles;

						LocalAssetResults.Append(AssetDataFromFile);
						if (bGatherDependsData)
						{
							LocalDependencyResults.Add(DependencyData);
						}
						LocalCookedPackageNamesWithoutAssetDataResults.Append(CookedPackageNamesWithoutAssetData);

						// Don't store info on cooked packages
						bool bCachePackage = bLoadAndSaveCache && LocalCookedPackageNamesWithoutAssetDataResults.Num() == 0;
						if (bCachePackage)
						{
							// Don't store info on cooked packages
							for (const auto& AssetData : AssetDataFromFile)
							{
								if (!!(AssetData->PackageFlags & PKG_FilterEditorOnly))
								{
									bCachePackage = false;
									break;
								}
							}
						}

						if (bCachePackage)
						{
							++NumFilesProcessedSinceLastCacheSave;

							// Update the cache
							FDiskCachedAssetData* NewData = new FDiskCachedAssetData(AssetFileData.PackageTimestamp);
							NewData->AssetDataList.Reserve(AssetDataFromFile.Num());
							for (const FAssetData* BackgroundAssetData : AssetDataFromFile)
							{
								NewData->AssetDataList.Add(*BackgroundAssetData);
							}
							NewData->DependencyData = DependencyData;

							NewCachedAssetData.Add(NewData);
							NewCachedAssetDataMap.Add(PackageName, NewData);
						}
					}
					else if (bCanAttemptAssetRetry)
					{
						LocalFilesToRetry.Add(AssetFileData);
					}
				}
			}

			LocalFilesToSearch.Reset();
			LocalFilesToSearch.Append(LocalFilesToRetry);
			LocalFilesToRetry.Reset();

			if (bLoadAndSaveCache)
			{
				// Save off the cache files if we're processed enough data since the last save
				if (NumFilesProcessedSinceLastCacheSave >= AssetDataGathererConstants::MaxFilesToProcessBeforeCacheWrite)
				{
					WriteAssetCacheFile();
				}
			}
		}
		else
		{
			if (bIsSynchronous)
			{
				// This is synchronous. Since our work is done, we should safely exit
				Stop();
			}
			else
			{
				if (!LocalIsDiscoveringFiles && !bFinishedInitialDiscovery)
				{
					bFinishedInitialDiscovery = true;

					UE_LOG(LogAssetRegistry, Verbose, TEXT("Initial scan took %0.6f seconds (found %d cached assets, and loaded %d)"), FPlatformTime::Seconds() - InitialScanStartTime, NumCachedFiles, NumUncachedFiles);

					// If we are caching discovered assets and this is the first time we had no work to do, save off the cache now in case the user terminates unexpectedly
					if (bLoadAndSaveCache)
					{
						WriteAssetCacheFile();
					}
				}

				// No work to do. Sleep for a little and try again later.
				FPlatformProcess::Sleep(0.1);
			}
		}
	}

	if ( bLoadAndSaveCache )
	{
		WriteAssetCacheFile();
	}

	return 0;
}

void FAssetDataGatherer::Stop()
{
	if (BackgroundPackageFileDiscovery.IsValid())
	{
		BackgroundPackageFileDiscovery->Stop();
	}

	StopTaskCounter.Increment();
}

void FAssetDataGatherer::Exit()
{   
}

void FAssetDataGatherer::EnsureCompletion()
{
	if (BackgroundPackageFileDiscovery.IsValid())
	{
		BackgroundPackageFileDiscovery->EnsureCompletion();
	}

	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
		FilesToSearch.Empty();
	}

	Stop();

	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

bool FAssetDataGatherer::GetAndTrimSearchResults(TBackgroundGatherResults<FAssetData*>& OutAssetResults, TBackgroundGatherResults<FString>& OutPathResults, TBackgroundGatherResults<FPackageDependencyData>& OutDependencyResults, TBackgroundGatherResults<FString>& OutCookedPackageNamesWithoutAssetDataResults, TArray<double>& OutSearchTimes, int32& OutNumFilesToSearch, int32& OutNumPathsToSearch, bool& OutIsDiscoveringFiles)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

	OutAssetResults.Append(MoveTemp(AssetResults));
	AssetResults.Reset();

	OutPathResults.Append(MoveTemp(DiscoveredPaths));
	DiscoveredPaths.Reset();

	OutDependencyResults.Append(MoveTemp(DependencyResults));
	DependencyResults.Reset();

	OutCookedPackageNamesWithoutAssetDataResults.Append(MoveTemp(CookedPackageNamesWithoutAssetDataResults));
	CookedPackageNamesWithoutAssetDataResults.Reset();

	OutSearchTimes.Append(MoveTemp(SearchTimes));
	SearchTimes.Reset();

	OutNumFilesToSearch = FilesToSearch.Num();
	OutNumPathsToSearch = NumPathsToSearchAtLastSyncPoint;
	OutIsDiscoveringFiles = bIsDiscoveringFiles;

	return (SearchStartTime > 0 || bIsDiscoveringFiles);
}

void FAssetDataGatherer::AddPathToSearch(const FString& Path)
{
	if (BackgroundPackageFileDiscovery.IsValid())
	{
		BackgroundPackageFileDiscovery->AddPathToSearch(Path);
	}
}

void FAssetDataGatherer::AddFilesToSearch(const TArray<FString>& Files)
{
	TArray<FString> FilesToAdd;
	for (const FString& Filename : Files)
	{
		if ( IsValidPackageFileToRead(Filename) )
		{
			// Add the path to this asset into the list of discovered paths
			FilesToAdd.Add(Filename);
		}
	}

	if (FilesToAdd.Num() > 0)
	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);
		FilesToSearch.Append(FilesToAdd);
	}
}

void FAssetDataGatherer::PrioritizeSearchPath(const FString& PathToPrioritize)
{
	if (BackgroundPackageFileDiscovery.IsValid())
	{
		BackgroundPackageFileDiscovery->PrioritizeSearchPath(PathToPrioritize);
	}

	FString LocalFilenamePathToPrioritize;
	if (FPackageName::TryConvertLongPackageNameToFilename(PathToPrioritize / TEXT(""), LocalFilenamePathToPrioritize))
	{
		FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

		FilenamePathToPrioritize = LocalFilenamePathToPrioritize;
		SortPathsByPriority(INDEX_NONE);
	}
}

void FAssetDataGatherer::SortPathsByPriority(const int32 MaxNumToSort)
{
	FScopeLock CritSectionLock(&WorkerThreadCriticalSection);

	// Critical section. This code needs to be as fast as possible since it is in a critical section!
	// Swap all priority files to the top of the list
	if (FilenamePathToPrioritize.Len() > 0)
	{
		int32 LowestNonPriorityFileIdx = 0;
		int32 NumSorted = 0;
		const int32 NumToSort = (MaxNumToSort == INDEX_NONE) ? FilesToSearch.Num() : FMath::Min(FilesToSearch.Num(), MaxNumToSort);
		for (int32 FilenameIdx = 0; FilenameIdx < FilesToSearch.Num(); ++FilenameIdx)
		{
			if (FilesToSearch[FilenameIdx].PackageFilename.StartsWith(FilenamePathToPrioritize))
			{
				FilesToSearch.Swap(FilenameIdx, LowestNonPriorityFileIdx);
				LowestNonPriorityFileIdx++;

				if (++NumSorted >= NumToSort)
				{
					break;
				}
			}
		}
	}
}

bool FAssetDataGatherer::ReadAssetFile(const FString& AssetFilename, TArray<FAssetData*>& AssetDataList, FPackageDependencyData& DependencyData, TArray<FString>& CookedPackageNamesWithoutAssetData, bool& OutCanRetry) const
{
	OutCanRetry = false;

	FPackageReader PackageReader;

	FPackageReader::EOpenPackageResult OpenPackageResult;
	if ( !PackageReader.OpenPackageFile(AssetFilename, &OpenPackageResult) )
	{
		// If we're missing a custom version, we might be able to load this package later once the module containing that version is loaded...
		//   -	We can only attempt a retry in editors (not commandlets) that haven't yet finished initializing (!GIsRunning), as we 
		//		have no guarantee that a commandlet or an initialized editor is going to load any more modules/plugins
		//   -	Likewise, we can only attempt a retry for asynchronous scans, as during a synchronous scan we won't be loading any 
		//		modules/plugins so it would last forever
		const bool bAllowRetry = GIsEditor && !IsRunningCommandlet() && !GIsRunning && !bIsSynchronous;
		OutCanRetry = bAllowRetry && OpenPackageResult == FPackageReader::EOpenPackageResult::CustomVersionMissing;
		return false;
	}

	if ( PackageReader.ReadAssetRegistryDataIfCookedPackage(AssetDataList, CookedPackageNamesWithoutAssetData) )
	{
		// Cooked data is special. No further data is found in these packages
		return true;
	}

	if ( !PackageReader.ReadAssetRegistryData(AssetDataList) )
	{
		if ( !PackageReader.ReadAssetDataFromThumbnailCache(AssetDataList) )
		{
			// It's ok to keep reading even if the asset registry data doesn't exist yet
			//return false;
		}
	}

	if ( bGatherDependsData )
	{
		if ( !PackageReader.ReadDependencyData(DependencyData) )
		{
			return false;
		}
	}

	return true;
}

void FAssetDataGatherer::SerializeCache(FArchive& Ar)
{
	double SerializeStartTime = FPlatformTime::Seconds();

	// serialize number of objects
	int32 LocalNumAssets = NewCachedAssetDataMap.Num();
	Ar << LocalNumAssets;

	if (Ar.IsSaving())
	{
		// save out by walking the TMap
		for (TPair<FName, FDiskCachedAssetData*>& Pair : NewCachedAssetDataMap)
		{
			Ar << Pair.Key;
			Pair.Value->SerializeForCache(Ar);
		}
	}
	else
	{
		// allocate one single block for all asset data structs (to reduce tens of thousands of heap allocations)
		DiskCachedAssetDataMap.Empty(LocalNumAssets);

		for (int32 AssetIndex = 0; AssetIndex < LocalNumAssets; ++AssetIndex)
		{
			// Load the name first to add the entry to the tmap below
			FName PackageName;
			Ar << PackageName;
			if (Ar.IsError())
			{
				// There was an error reading the cache. Bail out.
				break;
			}

			// Add to the cached map
			FDiskCachedAssetData& CachedAssetData = DiskCachedAssetDataMap.Add(PackageName);

			// Now load the data
			CachedAssetData.SerializeForCache(Ar);

			if (Ar.IsError())
			{
				// There was an error reading the cache. Bail out.
				break;
			}
		}

		// If there was an error loading the cache, abandon all data loaded from it so we can build a clean one.
		if (Ar.IsError())
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("There was an error loading the asset registry cache. Generating a new one."));
			DiskCachedAssetDataMap.Empty();
		}
	}

	UE_LOG(LogAssetRegistry, Verbose, TEXT("Asset data gatherer serialized in %0.6f seconds"), FPlatformTime::Seconds() - SerializeStartTime);
}
