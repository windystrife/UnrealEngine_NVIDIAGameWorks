// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/ChunkDbChunkSource.h"
#include "Misc/Guid.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Containers/Algo/Transform.h"
#include "Core/Platform.h"
#include "Common/FileSystem.h"
#include "Common/StatsCollector.h"
#include "Data/ChunkData.h"
#include "Installer/ChunkStore.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/MessagePump.h"
#include "Installer/InstallerError.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
namespace ChunkDbSourceHelpers
{
	int32 DisableOsIntervention()
	{
		return ::SetErrorMode(SEM_FAILCRITICALERRORS);
	}

	void ResetOsIntervention(int32 Previous)
	{
		::SetErrorMode(Previous);
	}
}
#else
namespace ChunkDbSourceHelpers
{
	int32 DisableOsIntervention()
	{
		return 0;
	}

	void ResetOsIntervention(int32 Previous)
	{
	}
}
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogChunkDbChunkSource, Warning, All);
DEFINE_LOG_CATEGORY(LogChunkDbChunkSource);

namespace ChunkDbSourceHelpers
{
	using namespace BuildPatchServices;
	IChunkDbChunkSourceStat::ELoadResult FromSerializer(EChunkLoadResult LoadResult)
	{
		switch (LoadResult)
		{
			case EChunkLoadResult::Success:
			return IChunkDbChunkSourceStat::ELoadResult::Success;

			case EChunkLoadResult::MissingHashInfo:
			return IChunkDbChunkSourceStat::ELoadResult::MissingHashInfo;

			case EChunkLoadResult::HashCheckFailed:
			return IChunkDbChunkSourceStat::ELoadResult::HashCheckFailed;

			default:
			return IChunkDbChunkSourceStat::ELoadResult::SerializationError;
		}
	}
}

namespace BuildPatchServices
{
	/**
	 * Struct holding variables for accessing a chunkdb file's data.
	 */
	struct FChunkDbDataAccess
	{
		FChunkDatabaseHeader Header;
		TUniquePtr<FArchive> Archive;
	};

	/**
	 * Struct holding variables required for accessing a particular chunk.
	 */
	struct FChunkAccessLookup
	{
		FChunkLocation* Location;
		FChunkDbDataAccess* DbFile;
	};

	/**
	 * Struct holding variables for tracking retry attempts on opening chunkdb files.
	 */
	struct FChunkDbRetryInfo
	{
		int32 Count;
		double SecondsAtLastTry;
	};

	/**
	 * The concrete implementation of IChunkDbChunkSource.
	 */
	class FChunkDbChunkSource
		: public IChunkDbChunkSource
	{
	public:
		FChunkDbChunkSource(FChunkDbSourceConfig Configuration, IPlatform* Platform, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, IMessagePump* MessagePump, IInstallerError* InstallerError, IChunkDbChunkSourceStat* ChunkDbChunkSourceStat);
		~FChunkDbChunkSource();

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
		void ThreadRun();
		void LoadChunk(const FGuid& DataId);
		bool HasFailed(const FGuid& DataId);

		typedef TFunction<bool(const FArchive&)> FArchiveSelector;
		typedef TFunction<void(const FArchive&, bool)> FArchiveOpenResult;
		void TryReopenChunkDbFiles(const FArchiveSelector& SelectPredicate, const FArchiveOpenResult& ResultCallback);

	private:
		// Configuration.
		const FChunkDbSourceConfig Configuration;
		// Dependencies.
		IPlatform* Platform;
		IFileSystem* FileSystem;
		IChunkStore* ChunkStore;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IChunkDataSerialization* ChunkDataSerialization;
		IMessagePump* MessagePump;
		IInstallerError* InstallerError;
		IChunkDbChunkSourceStat* ChunkDbChunkSourceStat;
		// The future for our worker thread.
		TFuture<void> Future;
		// Control signals.
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;
		FThreadSafeBool bStartedLoading;
		// Handling of chunks we lose access to.
		TFunction<void(TSet<FGuid>)> UnavailableChunksCallback;
		TSet<FGuid> UnavailableChunks;
		// Storage of our chunkdb and enumerated available chunks lookup.
		TArray<FChunkDbDataAccess> ChunkDbDataAccesses;
		TMap<FGuid, FChunkAccessLookup> ChunkDbDataAccessLookup;
		TMap<FString, FChunkDbRetryInfo> ChunkDbReloadAttempts;
		TSet<FGuid> AvailableChunks;
		TSet<FGuid> PlacedInStore;
		// Communication between the incoming Get calls, and our worker thread.
		TQueue<FGuid, EQueueMode::Spsc> FailedToLoadMessages;
		TSet<FGuid> FailedToLoad;
	};

	FChunkDbChunkSource::FChunkDbChunkSource(FChunkDbSourceConfig InConfiguration, IPlatform* InPlatform, IFileSystem* InFileSystem, IChunkStore* InChunkStore, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, IMessagePump* InMessagePump, IInstallerError* InInstallerError, IChunkDbChunkSourceStat* InChunkDbChunkSourceStat)
		: Configuration(MoveTemp(InConfiguration))
		, Platform(InPlatform)
		, FileSystem(InFileSystem)
		, ChunkStore(InChunkStore)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, ChunkDataSerialization(InChunkDataSerialization)
		, MessagePump(InMessagePump)
		, InstallerError(InInstallerError)
		, ChunkDbChunkSourceStat(InChunkDbChunkSourceStat)
		, bIsPaused(false)
		, bShouldAbort(false)
		, bStartedLoading(InConfiguration.bBeginLoadsOnFirstGet == false)
		, UnavailableChunksCallback(nullptr)
	{
		// Allow OS intervention only once.
		bool bResetOsIntervention = false;
		int32 PreviousOsIntervention = 0;
		// Load each chunkdb's TOC to enumerate available chunks.
		for (const FString& ChunkDbFilename : Configuration.ChunkDbFiles)
		{
			TUniquePtr<FArchive> ChunkDbFile(FileSystem->CreateFileReader(*ChunkDbFilename));
			if (ChunkDbFile.IsValid())
			{
				// Load header.
				FChunkDatabaseHeader Header;
				*ChunkDbFile << Header;
				if (!ChunkDbFile->IsError() && Header.Contents.Num() > 0)
				{
					// Hold on to the handle and header info.
					ChunkDbDataAccesses.Add({MoveTemp(Header), MoveTemp(ChunkDbFile)});
				}
			}
			else if(!bResetOsIntervention)
			{
				bResetOsIntervention = true;
				PreviousOsIntervention = ChunkDbSourceHelpers::DisableOsIntervention();
			}
		}
		// Reset OS intervention if we disabled it.
		if (bResetOsIntervention)
		{
			ChunkDbSourceHelpers::ResetOsIntervention(PreviousOsIntervention);
		}

		// Index all chunks to their location info.
		for (FChunkDbDataAccess& ChunkDbDataAccess : ChunkDbDataAccesses)
		{
			for (FChunkLocation& ChunkLocation : ChunkDbDataAccess.Header.Contents)
			{
				if (!ChunkDbDataAccessLookup.Contains(ChunkLocation.ChunkId))
				{
					ChunkDbDataAccessLookup.Add(ChunkLocation.ChunkId, {&ChunkLocation, &ChunkDbDataAccess});
					AvailableChunks.Add(ChunkLocation.ChunkId);
				}
			}
		}

		// Start threaded load worker.
		TFunction<void()> Task = [this]() { return ThreadRun(); };
		Future = Async(EAsyncExecution::Thread, MoveTemp(Task));
	}

	FChunkDbChunkSource::~FChunkDbChunkSource()
	{
		Abort();
		Future.Wait();
	}

	void FChunkDbChunkSource::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FChunkDbChunkSource::Abort()
	{
		bShouldAbort = true;
	}

	IChunkDataAccess* FChunkDbChunkSource::Get(const FGuid& DataId)
	{
		// Get from our store
		IChunkDataAccess* ChunkData = ChunkStore->Get(DataId);
		if (ChunkData == nullptr)
		{
			bStartedLoading = true;
			if (ChunkDbDataAccessLookup.Contains(DataId) && AvailableChunks.Contains(DataId))
			{
				// Wait for the chunk to be available.
				while (!HasFailed(DataId) && (ChunkData = ChunkStore->Get(DataId)) == nullptr && !bShouldAbort)
				{
					Platform->Sleep(0.01f);
				}

				// Dump out unavailable chunks on the incoming IO thread.
				if (UnavailableChunksCallback && UnavailableChunks.Num() > 0)
				{
					UnavailableChunksCallback(MoveTemp(UnavailableChunks));
				}
			}
		}
		return ChunkData;
	}

	TSet<FGuid> FChunkDbChunkSource::AddRuntimeRequirements(TSet<FGuid> NewRequirements)
	{
		// We can't actually get more than we are already getting, so just return what we don't already have in our list.
		return NewRequirements.Difference(AvailableChunks);
	}

	void FChunkDbChunkSource::SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback)
	{
		UnavailableChunksCallback = Callback;
	}

	const TSet<FGuid>& FChunkDbChunkSource::GetAvailableChunks() const
	{
		return AvailableChunks;
	}

	void FChunkDbChunkSource::ThreadRun()
	{
		while (!bShouldAbort)
		{
			bool bWorkPerformed = false;
			if (bStartedLoading)
			{
				// Select chunks that are contained in our chunk db files.
				TFunction<bool(const FGuid&)> SelectPredicate = [this](const FGuid& ChunkId)
				{
					return AvailableChunks.Contains(ChunkId);
				};
				// Clamp load count between min and max, balancing on store slack.
				int32 StoreSlack = ChunkStore->GetSlack();
				int32 BatchFetchCount = FMath::Clamp(StoreSlack, Configuration.PreFetchMinimum, Configuration.PreFetchMaximum);
				TArray<FGuid> BatchLoadChunks = ChunkReferenceTracker->GetNextReferences(BatchFetchCount, SelectPredicate);
				// Remove already loaded chunks from our todo list.
				// We only grab more chunks as they come into scope.
				TFunction<bool(const FGuid&)> RemovePredicate = [this](const FGuid& ChunkId)
				{
					return PlacedInStore.Contains(ChunkId);
				};
				BatchLoadChunks.RemoveAll(RemovePredicate);

				// Load this batch.
				for (int32 ChunkIdx = 0; ChunkIdx < BatchLoadChunks.Num() && !bShouldAbort; ++ChunkIdx)
				{
					const FGuid& BatchLoadChunk = BatchLoadChunks[ChunkIdx];
					LoadChunk(BatchLoadChunk);
				}

				// Set whether we performed work.
				if (BatchLoadChunks.Num() > 0 && !bShouldAbort)
				{
					bWorkPerformed = true;
				}
			}
			// If we had nothing to do, rest a little.
			if(!bWorkPerformed)
			{
				Platform->Sleep(0.1f);
			}
		}
	}

	void FChunkDbChunkSource::LoadChunk(const FGuid& DataId)
	{
		bool bChunkGood = false;
		if (ChunkDbDataAccessLookup.Contains(DataId))
		{
			ChunkDbChunkSourceStat->OnLoadStarted(DataId);
			FChunkAccessLookup& ChunkAccessLookup = ChunkDbDataAccessLookup[DataId];
			FChunkLocation& ChunkLocation = *ChunkAccessLookup.Location;
			FChunkDbDataAccess& ChunkDbDataAccess = *ChunkAccessLookup.DbFile;
			FArchive* ChunkDbFile = ChunkDbDataAccess.Archive.Get();
			check(ChunkDbFile != nullptr); // We never set null.
			bChunkGood = !ChunkDbFile->IsError();
			if (!bChunkGood)
			{
				const double SecondsNow = FStatsCollector::GetSeconds();
				// Time to retry?
				FChunkDbRetryInfo& ChunkDbRetryInfo = ChunkDbReloadAttempts.FindOrAdd(ChunkDbFile->GetArchiveName());
				const bool bIsFirstFail = ChunkDbRetryInfo.Count == 0;
				if (bIsFirstFail)
				{
					MessagePump->SendMessage(FChunkSourceEvent{FChunkSourceEvent::EType::AccessLost, ChunkDbFile->GetArchiveName()});
					// Also try reopening any chunkdbs files that have no error yet, in case they will also be lost.
					// This gives us control over disabling OS intervention popups when we inevitably try to read from them later.
					int32 PreviousOsIntervention = ChunkDbSourceHelpers::DisableOsIntervention();
					TryReopenChunkDbFiles(
						[this](const FArchive& Ar) -> bool
						{
							return Ar.IsError() == false;
						},
						[this](const FArchive& Ar, bool bSuccess) -> void
						{
							if (!bSuccess)
							{
								// Send a message about losing this chunkdb.
								MessagePump->SendMessage(FChunkSourceEvent{FChunkSourceEvent::EType::AccessLost, Ar.GetArchiveName()});
								ChunkDbReloadAttempts.FindOrAdd(Ar.GetArchiveName()).Count = 1;
							}
						});
					// Reset OS intervention setting.
					ChunkDbSourceHelpers::ResetOsIntervention(PreviousOsIntervention);
				}
				if (bIsFirstFail || (SecondsNow - ChunkDbRetryInfo.SecondsAtLastTry) >= Configuration.ChunkDbOpenRetryTime)
				{
					UE_LOG(LogChunkDbChunkSource, Log, TEXT("Retrying ChunkDb archive which has errored %s"), *ChunkDbFile->GetArchiveName());
					ChunkDbRetryInfo.SecondsAtLastTry = SecondsNow;
					// Retry whilst disabling OS intervention. Any OS dialogs if they exist would have been popped during the read that flagged the error.
					int32 PreviousOsIntervention = ChunkDbSourceHelpers::DisableOsIntervention();
					TUniquePtr<FArchive> NewChunkDbFile(FileSystem->CreateFileReader(*ChunkDbFile->GetArchiveName()));
					if (NewChunkDbFile.IsValid())
					{
						ChunkDbDataAccess.Archive = MoveTemp(NewChunkDbFile);
						ChunkDbFile = ChunkDbDataAccess.Archive.Get();
						bChunkGood = true;
						ChunkDbRetryInfo.Count = 0;
						MessagePump->SendMessage(FChunkSourceEvent{FChunkSourceEvent::EType::AccessRegained, ChunkDbFile->GetArchiveName()});
						// Attempt to regain access to other failed chunkdb files too.
						TryReopenChunkDbFiles(
							[this](const FArchive& Ar) -> bool
							{
								return Ar.IsError() == true;
							},
							[this](const FArchive& Ar, bool bSuccess) -> void
							{
								if (bSuccess)
								{
									// Send a message about regaining this chunkdb.
									MessagePump->SendMessage(FChunkSourceEvent{FChunkSourceEvent::EType::AccessRegained, Ar.GetArchiveName()});
									ChunkDbReloadAttempts.FindOrAdd(Ar.GetArchiveName()).Count = 0;
								}
							});
					}
					else
					{
						++ChunkDbRetryInfo.Count;
					}
					// Reset OS intervention setting.
					ChunkDbSourceHelpers::ResetOsIntervention(PreviousOsIntervention);
				}
			}
			if (bChunkGood)
			{
				// Load the chunk.
				bChunkGood = false;
				const int64 TotalFileSize = ChunkDbFile->TotalSize();
				int64 DataEndPoint = ChunkLocation.ByteStart + ChunkLocation.ByteSize;
				if (TotalFileSize >= DataEndPoint)
				{
					if (ChunkDbFile->Tell() != ChunkLocation.ByteStart)
					{
						ChunkDbFile->Seek(ChunkLocation.ByteStart);
					}
					EChunkLoadResult LoadResult;
					TUniquePtr<IChunkDataAccess> ChunkDataAccess(ChunkDataSerialization->LoadFromArchive(*ChunkDbFile, LoadResult));
					const uint64 End = ChunkDbFile->Tell();
					bChunkGood = LoadResult == EChunkLoadResult::Success && ChunkDataAccess.IsValid() && (End == DataEndPoint);
					ChunkDbChunkSourceStat->OnLoadComplete(DataId, ChunkDbSourceHelpers::FromSerializer(LoadResult));
					if (bChunkGood)
					{
						// Add it to our cache.
						PlacedInStore.Add(DataId);
						ChunkStore->Put(DataId, MoveTemp(ChunkDataAccess));
					}
				}
				else
				{
					ChunkDbChunkSourceStat->OnLoadComplete(DataId, IChunkDbChunkSourceStat::ELoadResult::LocationOutOfBounds);
				}
			}
		}
		if (!bChunkGood)
		{
			FailedToLoadMessages.Enqueue(DataId);
		}
	}

	bool FChunkDbChunkSource::HasFailed(const FGuid& DataId)
	{
		FGuid Temp;
		while (FailedToLoadMessages.Dequeue(Temp))
		{
			FailedToLoad.Add(Temp);
			UnavailableChunks.Add(Temp);
		}
		return FailedToLoad.Contains(DataId);
	}

	void FChunkDbChunkSource::TryReopenChunkDbFiles(const FArchiveSelector& SelectPredicate, const FArchiveOpenResult& ResultCallback)
	{
		for (FChunkDbDataAccess& ChunkDbToCheck : ChunkDbDataAccesses)
		{
			TUniquePtr<FArchive>& ArchiveToCheck = ChunkDbToCheck.Archive;
			if (ArchiveToCheck.IsValid() && SelectPredicate(*ArchiveToCheck.Get()))
			{
				TUniquePtr<FArchive> CanOpenFile(FileSystem->CreateFileReader(*ArchiveToCheck->GetArchiveName()));
				if (CanOpenFile.IsValid())
				{
					// Always set in the case that reopening fixes the currently undetected problem.
					ArchiveToCheck.Reset(CanOpenFile.Release());
					ResultCallback(*ArchiveToCheck.Get(), true);
				}
				else
				{
					// Make sure error is set on the archive so that we know to be retrying.
					ArchiveToCheck->SetError();
					ResultCallback(*ArchiveToCheck.Get(), false);
				}
			}
		}
	}

	IChunkDbChunkSource* FChunkDbChunkSourceFactory::Create(FChunkDbSourceConfig Configuration, IPlatform* Platform, IFileSystem* FileSystem, IChunkStore* ChunkStore, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, IMessagePump* MessagePump, IInstallerError* InstallerError, IChunkDbChunkSourceStat* ChunkDbChunkSourceStat)
	{
		check(Platform != nullptr);
		check(FileSystem != nullptr);
		check(ChunkStore != nullptr);
		check(ChunkReferenceTracker != nullptr);
		check(ChunkDataSerialization != nullptr);
		check(MessagePump != nullptr);
		check(InstallerError != nullptr);
		check(ChunkDbChunkSourceStat != nullptr);
		return new FChunkDbChunkSource(MoveTemp(Configuration), Platform, FileSystem, ChunkStore, ChunkReferenceTracker, ChunkDataSerialization, MessagePump, InstallerError, ChunkDbChunkSourceStat);
	}
}
