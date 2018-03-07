// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Generation/CloudEnumeration.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "BuildPatchManifest.h"
#include "Async/Future.h"
#include "Async/Async.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCloudEnumeration, Log, All);
DEFINE_LOG_CATEGORY(LogCloudEnumeration);

namespace BuildPatchServices
{
	class FCloudEnumeration
		: public ICloudEnumeration
	{
	public:
		FCloudEnumeration(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const FStatsCollectorRef& StatsCollector);
		virtual ~FCloudEnumeration();

		virtual TSet<FGuid> GetChunkSet(uint64 ChunkHash) const override;
		virtual TMap<uint64, TSet<FGuid>> GetChunkInventory() const override;
		virtual TMap<FGuid, int64> GetChunkFileSizes() const override;
		virtual TMap<FGuid, FSHAHash> GetChunkShaHashes() const override;
	private:
		void EnumerateCloud();
		void EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest);

	private:
		const FString CloudDirectory;
		const FDateTime ManifestAgeThreshold;
		mutable FCriticalSection InventoryCS;
		TMap<uint64, TSet<FGuid>> ChunkInventory;
		TMap<FGuid, int64> ChunkFileSizes;
		TMap<FGuid, FSHAHash> ChunkShaHashes;
		FStatsCollectorRef StatsCollector;
		TFuture<void> Future;
		volatile FStatsCollector::FAtomicValue* StatManifestsLoaded;
		volatile FStatsCollector::FAtomicValue* StatManifestsRejected;
		volatile FStatsCollector::FAtomicValue* StatChunksEnumerated;
		volatile FStatsCollector::FAtomicValue* StatChunksRejected;
		volatile FStatsCollector::FAtomicValue* StatTotalTime;
	};

	FCloudEnumeration::FCloudEnumeration(const FString& InCloudDirectory, const FDateTime& InManifestAgeThreshold, const FStatsCollectorRef& InStatsCollector)
		: CloudDirectory(InCloudDirectory)
		, ManifestAgeThreshold(InManifestAgeThreshold)
		, StatsCollector(InStatsCollector)
	{
		// Create statistics.
		StatManifestsLoaded = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Manifests Loaded"), EStatFormat::Value);
		StatManifestsRejected = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Manifests Rejected"), EStatFormat::Value);
		StatChunksEnumerated = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Chunks Enumerated"), EStatFormat::Value);
		StatChunksRejected = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Chunks Rejected"), EStatFormat::Value);
		StatTotalTime = StatsCollector->CreateStat(TEXT("Cloud Enumeration: Enumeration Time"), EStatFormat::Timer);

		// Queue thread.
		TFunction<void()> Task = [this]() { EnumerateCloud(); };
		Future = Async(EAsyncExecution::Thread, MoveTemp(Task));
	}

	FCloudEnumeration::~FCloudEnumeration()
	{
	}

	TSet<FGuid> FCloudEnumeration::GetChunkSet(uint64 ChunkHash) const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkInventory.FindRef(ChunkHash);
		}
	}


	TMap<uint64, TSet<FGuid>> FCloudEnumeration::GetChunkInventory() const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkInventory;
		}
	}

	TMap<FGuid, int64> FCloudEnumeration::GetChunkFileSizes() const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkFileSizes;
		}
	}

	TMap<FGuid, FSHAHash> FCloudEnumeration::GetChunkShaHashes() const
	{
		Future.Wait();
		{
			FScopeLock ScopeLock(&InventoryCS);
			return ChunkShaHashes;
		}
	}

	void FCloudEnumeration::EnumerateCloud()
	{
		FScopeLock ScopeLock(&InventoryCS);
		uint64 EnumerationTimer;

		IFileManager& FileManager = IFileManager::Get();

		// Find all manifest files
		FStatsCollector::AccumulateTimeBegin(EnumerationTimer);
		if (FileManager.DirectoryExists(*CloudDirectory))
		{
			TArray<FString> AllManifests;
			FileManager.FindFiles(AllManifests, *(CloudDirectory / TEXT("*.manifest")), true, false);
			FStatsCollector::AccumulateTimeEnd(StatTotalTime, EnumerationTimer);
			FStatsCollector::AccumulateTimeBegin(EnumerationTimer);

			// Load all manifest files
			for (const auto& ManifestFile : AllManifests)
			{
				// Determine chunks from manifest file
				const FString ManifestFilename = CloudDirectory / ManifestFile;
				if (IFileManager::Get().GetTimeStamp(*ManifestFilename) < ManifestAgeThreshold)
				{
					FStatsCollector::Accumulate(StatManifestsRejected, 1);
					continue;
				}
				FBuildPatchAppManifestRef BuildManifest = MakeShareable(new FBuildPatchAppManifest());
				if (BuildManifest->LoadFromFile(ManifestFilename))
				{
					FStatsCollector::Accumulate(StatManifestsLoaded, 1);
					EnumerateManifestData(BuildManifest);
				}
				else
				{
					FStatsCollector::Accumulate(StatManifestsRejected, 1);
					UE_LOG(LogCloudEnumeration, Warning, TEXT("Could not read Manifest file. Data recognition will suffer (%s)"), *ManifestFilename);
				}
				FStatsCollector::AccumulateTimeEnd(StatTotalTime, EnumerationTimer);
				FStatsCollector::AccumulateTimeBegin(EnumerationTimer);
			}
		}
		else
		{
			UE_LOG(LogCloudEnumeration, Log, TEXT("Cloud directory does not exist: %s"), *CloudDirectory);
		}
		FStatsCollector::AccumulateTimeEnd(StatTotalTime, EnumerationTimer);
	}

	void FCloudEnumeration::EnumerateManifestData(const FBuildPatchAppManifestRef& Manifest)
	{
		TArray<FGuid> DataList;
		Manifest->GetDataList(DataList);
		if (!Manifest->IsFileDataManifest())
		{
			FSHAHashData DataShaHash;
			uint64 DataChunkHash;
			for (const auto& DataGuid : DataList)
			{
				if (Manifest->GetChunkHash(DataGuid, DataChunkHash))
				{
					if (DataChunkHash != 0)
					{
						TSet<FGuid>& HashChunkList = ChunkInventory.FindOrAdd(DataChunkHash);
						if (!HashChunkList.Contains(DataGuid))
						{
							HashChunkList.Add(DataGuid);
							ChunkFileSizes.Add(DataGuid, Manifest->GetDataSize(DataGuid));
							FStatsCollector::Accumulate(StatChunksEnumerated, 1);
						}
					}
					else
					{
						FStatsCollector::Accumulate(StatChunksRejected, 1);
					}
				}
				else
				{
					FStatsCollector::Accumulate(StatChunksRejected, 1);
					UE_LOG(LogCloudEnumeration, Warning, TEXT("Missing chunk hash for %s in manifest %s %s"), *DataGuid.ToString(), *Manifest->GetAppName(), *Manifest->GetVersionString());
				}
				if (Manifest->GetChunkShaHash(DataGuid, DataShaHash))
				{
					FSHAHash& ShaHash = ChunkShaHashes.FindOrAdd(DataGuid);
					FMemory::Memcpy(ShaHash.Hash, DataShaHash.Hash, FSHA1::DigestSize);
				}
			}
		}
		else
		{
			FStatsCollector::Accumulate(StatManifestsRejected, 1);
		}
	}

	ICloudEnumerationRef FCloudEnumerationFactory::Create(const FString& CloudDirectory, const FDateTime& ManifestAgeThreshold, const FStatsCollectorRef& StatsCollector)
	{
		return MakeShareable(new FCloudEnumeration(CloudDirectory, ManifestAgeThreshold, StatsCollector));
	}
}
