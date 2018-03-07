// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/InstallChunkSource.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkStore.h"
#include "Installer/InstallerError.h"
#include "Common/FileSystem.h"
#include "BuildPatchHash.h"
#include "HAL/ThreadSafeBool.h"
#include "Serialization/MemoryReader.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInstallChunkSource, Warning, All);
DEFINE_LOG_CATEGORY(LogInstallChunkSource);

namespace BuildPatchServices
{
	class FInstallChunkSource : public IInstallChunkSource
	{
	public:
		FInstallChunkSource(FInstallSourceConfig InConfiguration, IFileSystem* FileSystem, IChunkStore* InChunkStore, IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallChunkSourceStat* InInstallChunkSourceStat, const TMap<FString, FBuildPatchAppManifestRef>& InInstallationSources, const FBuildPatchAppManifestRef& InstallManifest);
		~FInstallChunkSource();

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IChunkSource interface begin.
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TSet<FGuid> AddRuntimeRequirements(TSet<FGuid> NewRequirements) override;
		virtual void SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback) override;
		// IChunkSource interface end.

		// IInstallChunkSource interface begin.
		virtual const TSet<FGuid>& GetAvailableChunks() const override;
		// IInstallChunkSource interface end.

	private:
		typedef FRollingHash<BuildPatchServices::ChunkDataSize> FChunkHash;
		void FindChunkLocation(const FGuid& DataId, const FString** FoundInstallDirectory, const FBuildPatchAppManifest** FoundInstallManifest) const;
		bool LoadFromBuild(const FGuid& DataId);
		FSHAHashData GetShaHashForDataSet(const uint8* ChunkData);

	private:
		// Configuration.
		const FInstallSourceConfig Configuration;
		// Dependencies.
		IFileSystem* FileSystem;
		IChunkStore* ChunkStore;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IInstallerError* InstallerError;
		IInstallChunkSourceStat* InstallChunkSourceStat;
		// Control signals.
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;
		// Handling of chunks we lose access to.
		TFunction<void(TSet<FGuid>)> UnavailableChunksCallback;
		TSet<FGuid> UnavailableChunks;
		// Storage of enumerated chunks.
		TSet<FGuid> AvailableInBuilds;
		TArray<TPair<FString, FBuildPatchAppManifestRef>> InstallationSources;
		TSet<FGuid> PlacedInStore;
		// Track additional requests.
		TSet<FGuid> RuntimeRequests;
	};

	FInstallChunkSource::FInstallChunkSource(FInstallSourceConfig InConfiguration, IFileSystem* InFileSystem, IChunkStore* InChunkStore, IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallChunkSourceStat* InInstallChunkSourceStat, const TMap<FString, FBuildPatchAppManifestRef>& InInstallationSources, const FBuildPatchAppManifestRef& InstallManifest)
		: Configuration(MoveTemp(InConfiguration))
		, FileSystem(InFileSystem)
		, ChunkStore(InChunkStore)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, InstallerError(InInstallerError)
		, InstallChunkSourceStat(InInstallChunkSourceStat)
		, bIsPaused(false)
		, bShouldAbort(false)
		, UnavailableChunksCallback(nullptr)
	{
		// Cache faster lookup information.
		TSet<FGuid> RequiredChunks;
		InstallManifest->GetDataList(RequiredChunks);
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InInstallationSources)
		{
			if (Pair.Value->EnumerateProducibleChunks(Pair.Key, RequiredChunks, AvailableInBuilds) > 0)
			{
				InstallationSources.Add(Pair);
			}
		}
		UE_LOG(LogInstallChunkSource, Log, TEXT("Useful Sources:%d. Available Chunks:%d."), InstallationSources.Num(), AvailableInBuilds.Num());
	}

	FInstallChunkSource::~FInstallChunkSource()
	{
	}

	void FInstallChunkSource::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FInstallChunkSource::Abort()
	{
		bShouldAbort = true;
	}

	IChunkDataAccess* FInstallChunkSource::Get(const FGuid& DataId)
	{
		// Get from our store
		IChunkDataAccess* ChunkData = ChunkStore->Get(DataId);
		if (ChunkData == nullptr)
		{
			// If available, load next batch into store.
			if (AvailableInBuilds.Contains(DataId))
			{
				// Select the next X chunks that are locally available.
				TFunction<bool(const FGuid&)> SelectPredicate = [this](const FGuid& ChunkId)
				{
					return AvailableInBuilds.Contains(ChunkId) && (!Configuration.ChunkIgnoreSet.Contains(ChunkId) || RuntimeRequests.Contains(ChunkId));
				};
				// Clamp load count between min and max according to current space in the store.
				int32 StoreSlack = ChunkStore->GetSlack();
				int32 BatchFetchCount = FMath::Clamp(StoreSlack, Configuration.BatchFetchMinimum, Configuration.BatchFetchMaximum);
				TArray<FGuid> BatchLoadChunks = ChunkReferenceTracker->GetNextReferences(BatchFetchCount, SelectPredicate);
				// Remove already loaded chunks.
				TFunction<bool(const FGuid&)> RemovePredicate = [this](const FGuid& ChunkId)
				{
					return PlacedInStore.Contains(ChunkId);
				};
				BatchLoadChunks.RemoveAll(RemovePredicate);
				// Ensure requested chunk is in the array.
				BatchLoadChunks.AddUnique(DataId);
				// Load this batch
				for (int32 ChunkIdx = 0; ChunkIdx < BatchLoadChunks.Num() && !bShouldAbort; ++ChunkIdx)
				{
					const FGuid& BatchLoadChunk = BatchLoadChunks[ChunkIdx];
					LoadFromBuild(BatchLoadChunk);
				}
				// Get from store again.
				ChunkData = ChunkStore->Get(DataId);
				// Dump out unavailable chunks on the incoming IO thread.
				if (UnavailableChunksCallback && UnavailableChunks.Num() > 0)
				{
					UnavailableChunksCallback(MoveTemp(UnavailableChunks));
				}
			}
		}
		return ChunkData;
	}

	TSet<FGuid> FInstallChunkSource::AddRuntimeRequirements(TSet<FGuid> NewRequirements)
	{
		RuntimeRequests.Append(NewRequirements.Intersect(AvailableInBuilds));
		return NewRequirements.Difference(AvailableInBuilds);
	}

	void FInstallChunkSource::SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback)
	{
		UnavailableChunksCallback = Callback;
	}

	const TSet<FGuid>& FInstallChunkSource::GetAvailableChunks() const
	{
		return AvailableInBuilds;
	}

	void FInstallChunkSource::FindChunkLocation(const FGuid& DataId, const FString** FoundInstallDirectory, const FBuildPatchAppManifest** FoundInstallManifest) const
	{
		uint64 ChunkHash;
		*FoundInstallDirectory = nullptr;
		*FoundInstallManifest = nullptr;
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InstallationSources)
		{
			// GetChunkHash can be used as a check for whether this manifest references this chunk.
			if (Pair.Value->GetChunkHash(DataId, ChunkHash))
			{
				*FoundInstallDirectory = &Pair.Key;
				*FoundInstallManifest = &Pair.Value.Get();
				return;
			}
		}
	}

	bool FInstallChunkSource::LoadFromBuild(const FGuid& DataId)
	{
		// Find the location of this chunk.
		const FString* FoundInstallDirectory;
		const FBuildPatchAppManifest* FoundInstallManifest;
		FindChunkLocation(DataId, &FoundInstallDirectory, &FoundInstallManifest);
		if (FoundInstallDirectory == nullptr || FoundInstallManifest == nullptr)
		{
			return false;
		}

		// Attempt construction of the chunk from the parts.
		InstallChunkSourceStat->OnLoadStarted(DataId);
		TUniquePtr<FArchive> FileArchive;
		FString FileOpened;
		int64 FileSize = 0;

		// We must have a hash for this chunk or else we cant verify it
		EChunkHashFlags HashType = EChunkHashFlags::None;
		uint64 ChunkHash = 0;
		FSHAHashData ChunkShaHash;
		if (FoundInstallManifest->GetChunkShaHash(DataId, ChunkShaHash))
		{
			HashType |= EChunkHashFlags::Sha1;
		}
		if (FoundInstallManifest->GetChunkHash(DataId, ChunkHash))
		{
			HashType |= EChunkHashFlags::RollingPoly64;
		}
		IInstallChunkSourceStat::ELoadResult LoadResult = HashType == EChunkHashFlags::None ? IInstallChunkSourceStat::ELoadResult::MissingHashInfo : IInstallChunkSourceStat::ELoadResult::Success;
		if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
		{
			// Get the list of data pieces we need to load.
			TArray<FFileChunkPart> FileChunkParts = FoundInstallManifest->GetFilePartsForChunk(DataId);
			LoadResult = FileChunkParts.Num() <= 0 ? IInstallChunkSourceStat::ELoadResult::MissingPartInfo : IInstallChunkSourceStat::ELoadResult::Success;
			if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
			{
				TArray<uint8> TempArray;
				TempArray.AddUninitialized(BuildPatchServices::ChunkDataSize);
				uint8* TempChunkConstruction = TempArray.GetData();
				FMemory::Memzero(TempChunkConstruction, BuildPatchServices::ChunkDataSize);
				for (int32 FileChunkPartsIdx = 0; FileChunkPartsIdx < FileChunkParts.Num() && LoadResult == IInstallChunkSourceStat::ELoadResult::Success && !bShouldAbort; ++FileChunkPartsIdx)
				{
					const FFileChunkPart& FileChunkPart = FileChunkParts[FileChunkPartsIdx];
					FString FullFilename = *FoundInstallDirectory / FileChunkPart.Filename;
					// Close current build file ?
					if (FileArchive.IsValid() && FileOpened != FullFilename)
					{
						FileArchive->Close();
						FileArchive.Reset();
						FileOpened.Empty();
						FileSize = 0;
					}
					// Open build file ?
					if (!FileArchive.IsValid())
					{
						FileArchive = FileSystem->CreateFileReader(*FullFilename);
						LoadResult = !FileArchive.IsValid() ? IInstallChunkSourceStat::ELoadResult::OpenFileFail : IInstallChunkSourceStat::ELoadResult::Success;
						if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
						{
							FileOpened = FullFilename;
							FileSize = FileArchive->TotalSize();
						}
					}
					// Grab the section of the file.
					if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
					{
						// Make sure we don't attempt to read off the end of the file
						const int64 LastRequiredByte = FileChunkPart.FileOffset + FileChunkPart.ChunkPart.Size;
						LoadResult = FileSize < LastRequiredByte ? IInstallChunkSourceStat::ELoadResult::IncorrectFileSize : IInstallChunkSourceStat::ELoadResult::Success;
						if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
						{
							FileArchive->Seek(FileChunkPart.FileOffset);
							FileArchive->Serialize(TempChunkConstruction + FileChunkPart.ChunkPart.Offset, FileChunkPart.ChunkPart.Size);
						}
					}
					// Wait while paused
					while (bIsPaused && !bShouldAbort)
					{
						FPlatformProcess::Sleep(0.5f);
					}
				}

				// Mark not success if we gave up due to being aborted.
				if (bShouldAbort)
				{
					LoadResult = IInstallChunkSourceStat::ELoadResult::Aborted;
				}

				// Check chunk hash
				if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
				{
					const bool bUseSha = (HashType & EChunkHashFlags::Sha1) != EChunkHashFlags::None;
					const bool bHashCheckOk = bUseSha ? GetShaHashForDataSet(TempChunkConstruction) == ChunkShaHash : FChunkHash::GetHashForDataSet(TempChunkConstruction) == ChunkHash;
					if (bHashCheckOk == false)
					{
						LoadResult = IInstallChunkSourceStat::ELoadResult::HashCheckFailed;
					}
				}

				// Save the chunk to the store if all went well.
				if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
				{
					// It was added asynchronously!!
					check(PlacedInStore.Contains(DataId) == false);

					// Create the ChunkFile data structure
					IChunkDataAccess* NewChunkFile = FChunkDataAccessFactory::Create(BuildPatchServices::ChunkDataSize);

					// Lock data
					FChunkHeader* ChunkHeader;
					uint8* ChunkData;
					NewChunkFile->GetDataLock(&ChunkData, &ChunkHeader);

					// Copy the data
					FMemoryReader MemReader(TempArray);
					MemReader.Serialize(ChunkData, BuildPatchServices::ChunkDataSize);

					// Setup the header
					ChunkHeader->Guid = DataId;
					ChunkHeader->StoredAs = EChunkStorageFlags::None;
					ChunkHeader->DataSize = BuildPatchServices::ChunkDataSize; // This would change if compressing/encrypting
					ChunkHeader->HashType = HashType;
					ChunkHeader->RollingHash = ChunkHash;
					FMemory::Memcpy(ChunkHeader->SHAHash.Hash, ChunkShaHash.Hash, FSHA1::DigestSize);

					// Release data
					NewChunkFile->ReleaseDataLock();

					// Add it to our cache.
					PlacedInStore.Add(DataId);
					ChunkStore->Put(DataId, TUniquePtr<IChunkDataAccess>(NewChunkFile));
				}

				// Close any open file
				if (FileArchive.IsValid())
				{
					FileArchive->Close();
					FileArchive.Reset();
				}
			}
		}
		InstallChunkSourceStat->OnLoadComplete(DataId, LoadResult);
		if (LoadResult == IInstallChunkSourceStat::ELoadResult::Success)
		{
			return true;
		}
		else
		{
			UnavailableChunks.Add(DataId);
			return false;
		}
	}

	FSHAHashData FInstallChunkSource::GetShaHashForDataSet(const uint8* ChunkData)
	{
		FSHAHashData ShaHashCheck;
		FSHA1::HashBuffer(ChunkData, BuildPatchServices::ChunkDataSize, ShaHashCheck.Hash);
		return ShaHashCheck;
	}

	IInstallChunkSource* FInstallChunkSourceFactory::Create(FInstallSourceConfig Configuration, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IInstallerError* InstallerError, IInstallChunkSourceStat* InstallChunkSourceStat, const TMap<FString, FBuildPatchAppManifestRef>& InstallationSources, const FBuildPatchAppManifestRef& InstallManifest)
	{
		check(FileSystem != nullptr);
		check(ChunkStore != nullptr);
		check(ChunkReferenceTracker != nullptr);
		check(InstallerError != nullptr);
		check(InstallChunkSourceStat != nullptr);
		return new FInstallChunkSource(MoveTemp(Configuration), FileSystem, ChunkStore, ChunkReferenceTracker, InstallerError, InstallChunkSourceStat, InstallationSources, InstallManifest);
	}
}
