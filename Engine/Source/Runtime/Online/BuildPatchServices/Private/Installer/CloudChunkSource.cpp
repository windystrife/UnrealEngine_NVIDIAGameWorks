// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/CloudChunkSource.h"
#include "CoreMinimal.h"
#include "Async/Async.h"
#include "Async/Future.h"
#include "Core/Platform.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkStore.h"
#include "Installer/DownloadService.h"
#include "Installer/InstallerError.h"
#include "Common/StatsCollector.h"
#include "IBuildInstaller.h"
#include "BuildPatchUtil.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Misc/ScopeLock.h"

DECLARE_LOG_CATEGORY_EXTERN(LogCloudChunkSource, Warning, All);
DEFINE_LOG_CATEGORY(LogCloudChunkSource);

namespace BuildPatchServices
{
	/**
	 * A class used to monitor the average chunk download time and standard deviation.
	 */
	class FMeanChunkTime
	{
	public:
		FMeanChunkTime();

		void Reset();
		bool IsReliable() const;
		void GetValues(double& Mean, double& Std) const;
		void AddSample(double Sample);

	private:
		double GetMean() const;
		double GetStd(double Mean) const;

	private:
		uint64 Count;
		double Total;
		double TotalSqs;
	};

	FMeanChunkTime::FMeanChunkTime()
		: Count(0)
		, Total(0)
		, TotalSqs(0)
	{
	}

	void FMeanChunkTime::Reset()
	{
		Count = 0;
		Total = 0;
		TotalSqs = 0;
	}

	bool FMeanChunkTime::IsReliable() const
	{
		return Count > 10;
	}

	void FMeanChunkTime::GetValues(double& Mean, double& Std) const
	{
		Mean = GetMean();
		Std = GetStd(Mean);
	}

	void FMeanChunkTime::AddSample(double Sample)
	{
		Total += Sample;
		TotalSqs += Sample * Sample;
		++Count;
	}

	double FMeanChunkTime::GetMean() const
	{
		checkSlow(Count > 0);
		return Total / Count;
	}

	double FMeanChunkTime::GetStd(double Mean) const
	{
		return FMath::Sqrt((TotalSqs / Count) - (Mean * Mean));
	}

	/**
	 * A class used to monitor the download success rate.
	 */
	class FChunkSuccessRate
	{
	public:
		FChunkSuccessRate();

		double GetOverall() const;
		void AddSuccess();
		void AddFail();

	private:
		uint64 TotalSuccess;
		uint64 Count;
	};

	FChunkSuccessRate::FChunkSuccessRate()
		: TotalSuccess(0)
		, Count(0)
	{
	}

	double FChunkSuccessRate::GetOverall() const
	{
		if (Count == 0)
		{
			return 1.0;
		}
		const double Num = TotalSuccess;
		const double Denom = Count;
		return Num / Denom;
	}

	void FChunkSuccessRate::AddSuccess()
	{
		++TotalSuccess;
		++Count;
	}

	void FChunkSuccessRate::AddFail()
	{
		++Count;
	}

	/**
	 * The concrete implementation of ICloudChunkSource
	 */
	class FCloudChunkSource
		: public ICloudChunkSource
	{
	private:
		/**
		 * This is a wrapper class for binding thread safe shared ptr delegates for the download service, without having to enforce that
		 * this service should be made using TShared* reference controllers.
		 */
		class FDownloadDelegates
		{
		public:
			FDownloadDelegates(FCloudChunkSource& InCloudChunkSource);

		public:
			void OnDownloadProgress(int32 RequestId, int32 BytesSoFar);
			void OnDownloadComplete(int32 RequestId, const FDownloadRef& Download);

		private:
			FCloudChunkSource& CloudChunkSource;
		};
		
		/**
		 * This struct holds variable for each individual task.
		 */
		struct FTaskInfo
		{
		public:
			FTaskInfo();

		public:
			FString UrlUsed;
			int32 RetryNum;
			int32 ExpectedSize;
			double SecondsAtRequested;
			double SecondsAtFail;
		};

	public:
		FCloudChunkSource(FCloudSourceConfig InConfiguration, IPlatform* Platform, IChunkStore* InChunkStore, IDownloadService* InDownloadService, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, IMessagePump* InMessagePump, IInstallerError* InInstallerError, ICloudChunkSourceStat* InCloudChunkSourceStat, FBuildPatchAppManifestRef InInstallManifest, TSet<FGuid> InInitialDownloadSet);
		~FCloudChunkSource();

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IChunkSource interface begin.
		virtual IChunkDataAccess* Get(const FGuid& DataId) override;
		virtual TSet<FGuid> AddRuntimeRequirements(TSet<FGuid> NewRequirements) override;
		virtual void SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback) override;
		// IChunkSource interface end.

	private:
		void EnsureAquiring(const FGuid& DataId);
		const FString& GetCloudRoot(int32 RetryNum) const;
		float GetRetryDelay(int32 RetryNum);
		EBuildPatchDownloadHealth GetDownloadHealth(bool bIsDisconnected, float ChunkSuccessRate);
		FGuid GetNextTask(const TMap<FGuid, FTaskInfo>& TaskInfos, const TMap<int32, FGuid>& InFlightDownloads, const TSet<FGuid>& PriorityRequests, const TSet<FGuid>& FailedDownloads, const TSet<FGuid>& Stored, TArray<FGuid>& DownloadQueue);
		void ThreadRun();
		void OnDownloadProgress(int32 RequestId, int32 BytesSoFar);
		void OnDownloadComplete(int32 RequestId, const FDownloadRef& Download);

	private:
		TSharedRef<FDownloadDelegates, ESPMode::ThreadSafe> DownloadDelegates;
		const FCloudSourceConfig Configuration;
		IPlatform* Platform;
		IChunkStore* ChunkStore;
		IDownloadService* DownloadService;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IChunkDataSerialization* ChunkDataSerialization;
		IMessagePump* MessagePump;
		IInstallerError* InstallerError;
		ICloudChunkSourceStat* CloudChunkSourceStat;
		FBuildPatchAppManifestRef InstallManifest;
		const TSet<FGuid> InitialDownloadSet;
		TFuture<void> Future;
		FDownloadProgressDelegate OnDownloadProgressDelegate;
		FDownloadCompleteDelegate OnDownloadCompleteDelegate;

		// Tracking health and connection state
		volatile int64 CyclesAtLastData;

		// Communication from external process requesting pause/abort.
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;

		// Communication from download thread to processing thread.
		FCriticalSection CompletedDownloadsCS;
		TMap<int32, FDownloadRef> CompletedDownloads;

		// Communication from request threads to processing thread.
		FCriticalSection RequestedDownloadsCS;
		TArray<FGuid> RequestedDownloads;

		// Communication and storage of incoming additional requirements.
		TQueue<TSet<FGuid>, EQueueMode::Spsc> RuntimeRequestMessages;
		TSet<FGuid> RuntimeRequests;
	};

	FCloudChunkSource::FDownloadDelegates::FDownloadDelegates(FCloudChunkSource& InCloudChunkSource)
		: CloudChunkSource(InCloudChunkSource)
	{
	}

	void FCloudChunkSource::FDownloadDelegates::OnDownloadProgress(int32 RequestId, int32 BytesSoFar)
	{
		CloudChunkSource.OnDownloadProgress(RequestId, BytesSoFar);
	}

	void FCloudChunkSource::FDownloadDelegates::OnDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		CloudChunkSource.OnDownloadComplete(RequestId, Download);
	}

	FCloudChunkSource::FTaskInfo::FTaskInfo()
		: UrlUsed()
		, RetryNum(0)
		, ExpectedSize(0)
		, SecondsAtFail(0)
	{
	}

	FCloudChunkSource::FCloudChunkSource(FCloudSourceConfig InConfiguration, IPlatform* InPlatform, IChunkStore* InChunkStore, IDownloadService* InDownloadService, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, IMessagePump* InMessagePump, IInstallerError* InInstallerError, ICloudChunkSourceStat* InCloudChunkSourceStat, FBuildPatchAppManifestRef InInstallManifest, TSet<FGuid> InInitialDownloadSet)
		: DownloadDelegates(MakeShareable(new FDownloadDelegates(*this)))
		, Configuration(MoveTemp(InConfiguration))
		, Platform(InPlatform)
		, ChunkStore(InChunkStore)
		, DownloadService(InDownloadService)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, ChunkDataSerialization(InChunkDataSerialization)
		, MessagePump(InMessagePump)
		, InstallerError(InInstallerError)
		, CloudChunkSourceStat(InCloudChunkSourceStat)
		, InstallManifest(MoveTemp(InInstallManifest))
		, InitialDownloadSet(MoveTemp(InInitialDownloadSet))
		, Future()
		, OnDownloadProgressDelegate(FDownloadProgressDelegate::CreateThreadSafeSP(DownloadDelegates, &FDownloadDelegates::OnDownloadProgress))
		, OnDownloadCompleteDelegate(FDownloadCompleteDelegate::CreateThreadSafeSP(DownloadDelegates, &FDownloadDelegates::OnDownloadComplete))
		, CyclesAtLastData(0)
		, bIsPaused(false)
		, bShouldAbort(false)
		, CompletedDownloadsCS()
		, CompletedDownloads()
		, RequestedDownloadsCS()
		, RequestedDownloads()
	{
		TFunction<void()> Task = [this]() { return ThreadRun(); };
		Future = Async(EAsyncExecution::Thread, MoveTemp(Task));
	}

	FCloudChunkSource::~FCloudChunkSource()
	{
		bShouldAbort = true;
		Future.Wait();
	}

	void FCloudChunkSource::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FCloudChunkSource::Abort()
	{
		bShouldAbort = true;
	}

	IChunkDataAccess* FCloudChunkSource::Get(const FGuid& DataId)
	{
		IChunkDataAccess* ChunkData = ChunkStore->Get(DataId);
		if (ChunkData == nullptr)
		{
			// Ensure this chunk is on the list.
			EnsureAquiring(DataId);
			// Wait for the chunk to be available.
			while ((ChunkData = ChunkStore->Get(DataId)) == nullptr && !bShouldAbort)
			{
				Platform->Sleep(0.01f);
			}
		}
		return ChunkData;
	}

	TSet<FGuid> FCloudChunkSource::AddRuntimeRequirements(TSet<FGuid> NewRequirements)
	{
		RuntimeRequestMessages.Enqueue(MoveTemp(NewRequirements));
		// We don't have a concept of being unavailable yet.
		return TSet<FGuid>();
	}

	void FCloudChunkSource::SetUnavailableChunksCallback(TFunction<void(TSet<FGuid>)> Callback)
	{
		// We don't have a concept of being unavailable yet.
	}

	void FCloudChunkSource::EnsureAquiring(const FGuid& DataId)
	{
		FScopeLock ScopeLock(&RequestedDownloadsCS);
		RequestedDownloads.Add(DataId);
	}

	const FString& FCloudChunkSource::GetCloudRoot(int32 RetryNum) const
	{
		return Configuration.CloudRoots[RetryNum % Configuration.CloudRoots.Num()];
	}

	float FCloudChunkSource::GetRetryDelay(int32 RetryNum)
	{
		const int32 RetryTimeIndex = FMath::Clamp<int32>(RetryNum - 1, 0, Configuration.RetryDelayTimes.Num() - 1);
		return Configuration.RetryDelayTimes[RetryTimeIndex];
	}

	EBuildPatchDownloadHealth FCloudChunkSource::GetDownloadHealth(bool bIsDisconnected, float ChunkSuccessRate)
	{
		EBuildPatchDownloadHealth DownloadHealth;
		if (bIsDisconnected)
		{
			DownloadHealth = EBuildPatchDownloadHealth::Disconnected;
		}
		else if (ChunkSuccessRate >= Configuration.HealthPercentages[(int32)EBuildPatchDownloadHealth::Excellent])
		{
			DownloadHealth = EBuildPatchDownloadHealth::Excellent;
		}
		else if (ChunkSuccessRate >= Configuration.HealthPercentages[(int32)EBuildPatchDownloadHealth::Good])
		{
			DownloadHealth = EBuildPatchDownloadHealth::Good;
		}
		else if (ChunkSuccessRate >= Configuration.HealthPercentages[(int32)EBuildPatchDownloadHealth::OK])
		{
			DownloadHealth = EBuildPatchDownloadHealth::OK;
		}
		else
		{
			DownloadHealth = EBuildPatchDownloadHealth::Poor;
		}
		return DownloadHealth;
	}

	FGuid FCloudChunkSource::GetNextTask(const TMap<FGuid, FTaskInfo>& TaskInfos, const TMap<int32, FGuid>& InFlightDownloads, const TSet<FGuid>& PriorityRequests, const TSet<FGuid>& FailedDownloads, const TSet<FGuid>& Stored, TArray<FGuid>& DownloadQueue)
	{
		// Check for aborting.
		if (bShouldAbort)
		{
			return FGuid();
		}

		// Check priority request.
		if (PriorityRequests.Num() > 0)
		{
			return *PriorityRequests.CreateConstIterator();
		}

		// Check retries.
		const double SecondsNow = FStatsCollector::GetSeconds();
		FGuid ChunkToRetry;
		for (auto FailedIt = FailedDownloads.CreateConstIterator(); FailedIt && !ChunkToRetry.IsValid(); ++FailedIt)
		{
			const FTaskInfo& FailedDownload = TaskInfos[*FailedIt];
			const double SecondsSinceFailure = SecondsNow - FailedDownload.SecondsAtFail;
			if (SecondsSinceFailure >= GetRetryDelay(FailedDownload.RetryNum))
			{
				ChunkToRetry = *FailedIt;
			}
		}
		if (ChunkToRetry.IsValid())
		{
			return ChunkToRetry;
		}

		// Check if we can start more.
		int32 NumProcessing = InFlightDownloads.Num() + FailedDownloads.Num();
		if (NumProcessing < Configuration.NumSimultaneousDownloads)
		{
			// Find the next chunks to get if we completed the last batch.
			if (DownloadQueue.Num() == 0)
			{
				// Process new runtime requests.
				TSet<FGuid> Temp;
				while (RuntimeRequestMessages.Dequeue(Temp))
				{
					RuntimeRequests.Append(MoveTemp(Temp));
				}

				// Select the next X chunks that we initially instructed to download.
				TFunction<bool(const FGuid&)> SelectPredicate = [this](const FGuid& ChunkId)
				{
					return InitialDownloadSet.Contains(ChunkId) || RuntimeRequests.Contains(ChunkId);
				};
				// Clamp fetch count between min and max according to current space in the store.
				int32 StoreSlack = ChunkStore->GetSlack();
				int32 PreFetchCount = FMath::Clamp(StoreSlack, Configuration.PreFetchMinimum, Configuration.PreFetchMaximum);
				DownloadQueue = ChunkReferenceTracker->GetNextReferences(PreFetchCount, SelectPredicate);
				// Remove already downloading or complete chunks.
				TFunction<bool(const FGuid&)> RemovePredicate = [&TaskInfos, &FailedDownloads, &Stored](const FGuid& ChunkId)
				{
					return TaskInfos.Contains(ChunkId) || FailedDownloads.Contains(ChunkId) || Stored.Contains(ChunkId);
				};
				DownloadQueue.RemoveAll(RemovePredicate);
				// Reverse so the array is a stack for popping.
				Algo::Reverse(DownloadQueue);
			}

			// Return the next chunk in the queue
			if (DownloadQueue.Num() > 0)
			{
				const bool bAllowShrinking = false;
				return DownloadQueue.Pop(bAllowShrinking);
			}
		}

		return FGuid();
	}

	void FCloudChunkSource::ThreadRun()
	{
		TMap<FGuid, FTaskInfo> TaskInfos;
		TMap<int32, FGuid> InFlightDownloads;
		TSet<FGuid> FailedDownloads;
		TSet<FGuid> PlacedInStore;
		TSet<FGuid> PriorityRequests;
		TArray<FGuid> DownloadQueue;
		bool bIsChunkData = InstallManifest->IsFileDataManifest() == false;
		bool bDownloadsStarted = Configuration.bBeginDownloadsOnFirstGet == false;
		bool bTotalRequiredTrimmed = false;
		FMeanChunkTime MeanChunkTime;
		FChunkSuccessRate ChunkSuccessRate;
		EBuildPatchDownloadHealth TrackedDownloadHealth = EBuildPatchDownloadHealth::Excellent;
		int32 TrackedActiveRequestCount = 0;
		TSet<FGuid> TotalRequiredChunks = InitialDownloadSet;
		int64 TotalReceivedData = 0;

		// Provide initial stat values.
		CloudChunkSourceStat->OnRequiredDataUpdated(InstallManifest->GetDataSize(TotalRequiredChunks));
		CloudChunkSourceStat->OnReceivedDataUpdated(TotalReceivedData);
		CloudChunkSourceStat->OnDownloadHealthUpdated(TrackedDownloadHealth);
		CloudChunkSourceStat->OnSuccessRateUpdated(ChunkSuccessRate.GetOverall());
		CloudChunkSourceStat->OnActiveRequestCountUpdated(TrackedActiveRequestCount);

		while (!bShouldAbort)
		{
			// Grab incoming requests as a priority.
			TArray<FGuid> FrameRequestedDownloads;
			RequestedDownloadsCS.Lock();
			FrameRequestedDownloads = MoveTemp(RequestedDownloads);
			RequestedDownloadsCS.Unlock();
			for (const FGuid& FrameRequestedDownload : FrameRequestedDownloads)
			{
				bDownloadsStarted = true;
				if (PlacedInStore.Contains(FrameRequestedDownload) == false && TaskInfos.Contains(FrameRequestedDownload) == false)
				{
					PriorityRequests.Add(FrameRequestedDownload);
					if (!TotalRequiredChunks.Contains(FrameRequestedDownload))
					{
						TotalRequiredChunks.Add(FrameRequestedDownload);
						CloudChunkSourceStat->OnRequiredDataUpdated(InstallManifest->GetDataSize(TotalRequiredChunks));
					}
				}
			}

			// Trim our initial download list on first begin.
			if (!bTotalRequiredTrimmed && bDownloadsStarted)
			{
				bTotalRequiredTrimmed = true;
				TotalRequiredChunks = TotalRequiredChunks.Intersect(ChunkReferenceTracker->GetReferencedChunks());
				CloudChunkSourceStat->OnRequiredDataUpdated(InstallManifest->GetDataSize(TotalRequiredChunks));
			}

			// Process completed downloads.
			TMap<int32, FDownloadRef> FrameCompletedDownloads;
			CompletedDownloadsCS.Lock();
			FrameCompletedDownloads = MoveTemp(CompletedDownloads);
			CompletedDownloadsCS.Unlock();
			for (const TPair<int32, FDownloadRef>& FrameCompletedDownload : FrameCompletedDownloads)
			{
				const int32& RequestId = FrameCompletedDownload.Key;
				const FDownloadRef& Download = FrameCompletedDownload.Value;
				const FGuid& DownloadId = InFlightDownloads[RequestId];
				FTaskInfo& TaskInfo = TaskInfos.FindOrAdd(DownloadId);
				bool bDownloadSuccess = Download->WasSuccessful();
				if (bDownloadSuccess)
				{
					// HTTP module gives const access to downloaded data, and we need to change it.
					// @TODO: look into refactor serialization it can already know SHA list? Or consider adding SHA params to public API.
					TArray<uint8> DownloadedData = Download->GetData();

					// If we know the SHA for this chunk, inject to data for verification.
					FSHAHashData LegacyHashType;
					if (InstallManifest->GetChunkShaHash(DownloadId, LegacyHashType))
					{
						FSHAHash ChunkShaHash;
						FMemory::Memcpy(ChunkShaHash.Hash, LegacyHashType.Hash, FSHA1::DigestSize);
						ChunkDataSerialization->InjectShaToChunkData(DownloadedData, ChunkShaHash);
					}

					EChunkLoadResult LoadResult;
					TUniquePtr<IChunkDataAccess> ChunkDataAccess(ChunkDataSerialization->LoadFromMemory(DownloadedData, LoadResult));
					bDownloadSuccess = LoadResult == EChunkLoadResult::Success;
					if (bDownloadSuccess)
					{
						TotalReceivedData += TaskInfo.ExpectedSize;
						CloudChunkSourceStat->OnReceivedDataUpdated(TotalReceivedData);
						TaskInfos.Remove(DownloadId);
						PlacedInStore.Add(DownloadId);
						ChunkStore->Put(DownloadId, MoveTemp(ChunkDataAccess));
					}
					else
					{
						CloudChunkSourceStat->OnDownloadCorrupt(DownloadId, TaskInfo.UrlUsed, LoadResult);
					}
				}
				else
				{
					CloudChunkSourceStat->OnDownloadFailed(DownloadId, TaskInfo.UrlUsed);
				}

				// Handle failed
				if (!bDownloadSuccess)
				{
					ChunkSuccessRate.AddFail();
					FailedDownloads.Add(DownloadId);
					if (Configuration.MaxRetryCount >= 0 && TaskInfo.RetryNum >= Configuration.MaxRetryCount)
					{
						InstallerError->SetError(EBuildPatchInstallError::DownloadError, DownloadErrorCodes::OutOfRetries);
						bShouldAbort = true;
					}
					++TaskInfo.RetryNum;
					TaskInfo.SecondsAtFail = FStatsCollector::GetSeconds();
				}
				else
				{
					const double ChunkTime = FStatsCollector::GetSeconds() - TaskInfo.SecondsAtRequested;
					MeanChunkTime.AddSample(ChunkTime);
					ChunkSuccessRate.AddSuccess();
				}
				InFlightDownloads.Remove(RequestId);
			}

			// Update connection status and health.
			bool bAllDownloadsRetrying = FailedDownloads.Num() > 0 || InFlightDownloads.Num() > 0;
			for (auto InFlightIt = InFlightDownloads.CreateConstIterator(); InFlightIt && bAllDownloadsRetrying; ++InFlightIt)
			{
				if (TaskInfos.FindOrAdd(InFlightIt.Value()).RetryNum == 0)
				{
					bAllDownloadsRetrying = false;
				}
			}
			const double SecondsSinceData = FStatsCollector::CyclesToSeconds(FStatsCollector::GetCycles() - CyclesAtLastData);
			const bool bDisconnect = (bAllDownloadsRetrying && SecondsSinceData > Configuration.DisconnectedDelay);
			const float SuccessRate = ChunkSuccessRate.GetOverall();
			EBuildPatchDownloadHealth DownloadHealth = GetDownloadHealth(bDisconnect, SuccessRate);
			if (TrackedDownloadHealth != DownloadHealth)
			{
				TrackedDownloadHealth = DownloadHealth;
				CloudChunkSourceStat->OnDownloadHealthUpdated(TrackedDownloadHealth);
			}
			if (FrameCompletedDownloads.Num() > 0)
			{
				CloudChunkSourceStat->OnSuccessRateUpdated(SuccessRate);
			}

			// Kick off new downloads.
			if (bDownloadsStarted)
			{
				FGuid NextTask;
				while ((NextTask = GetNextTask(TaskInfos, InFlightDownloads, PriorityRequests, FailedDownloads, PlacedInStore, DownloadQueue)).IsValid())
				{
					FTaskInfo& TaskInfo = TaskInfos.FindOrAdd(NextTask);
					TaskInfo.UrlUsed = FBuildPatchUtils::GetDataFilename(InstallManifest, GetCloudRoot(TaskInfo.RetryNum), NextTask);
					TaskInfo.ExpectedSize = InstallManifest->GetDataSize(NextTask);
					TaskInfo.SecondsAtRequested = FStatsCollector::GetSeconds();
					int32 RequestId = DownloadService->RequestFile(TaskInfo.UrlUsed, OnDownloadCompleteDelegate, OnDownloadProgressDelegate);
					InFlightDownloads.Add(RequestId, NextTask);
					PriorityRequests.Remove(NextTask);
					FailedDownloads.Remove(NextTask);
					CloudChunkSourceStat->OnDownloadRequested(NextTask);
				}
			}

			// Update request count.
			int32 ActiveRequestCount = InFlightDownloads.Num() + FailedDownloads.Num();;
			if (TrackedActiveRequestCount != ActiveRequestCount)
			{
				TrackedActiveRequestCount = ActiveRequestCount;
				CloudChunkSourceStat->OnActiveRequestCountUpdated(TrackedActiveRequestCount);
			}

			// Check for abnormally slow downloads. This was originally implemented as a temporary measure to fix major stall anomalies and zero size tcp window issue.
			// It remains until proven unrequired.
			if (bIsChunkData && MeanChunkTime.IsReliable())
			{
				bool bResetMeanChunkTime = false;
				for (const TPair<int32, FGuid>& InFlightDownload : InFlightDownloads)
				{
					FTaskInfo& TaskInfo = TaskInfos.FindOrAdd(InFlightDownload.Value);
					if (TaskInfo.RetryNum == 0)
					{
						double DownloadTime = FStatsCollector::GetSeconds() - TaskInfo.SecondsAtRequested;
						double DownloadTimeMean, DownloadTimeStd;
						MeanChunkTime.GetValues(DownloadTimeMean, DownloadTimeStd);
						// The point at which we decide the chunk is delayed, with a sane minimum
						double BreakingPoint = FMath::Max<double>(Configuration.TcpZeroWindowMinimumSeconds, DownloadTimeMean + (DownloadTimeStd * 4.0));
						if (DownloadTime > BreakingPoint)
						{
							bResetMeanChunkTime = true;
							DownloadService->RequestCancel(InFlightDownload.Key);
							CloudChunkSourceStat->OnDownloadAborted(InFlightDownload.Value, TaskInfo.UrlUsed, DownloadTimeMean, DownloadTimeStd, DownloadTime, BreakingPoint);
						}
					}
				}
				if (bResetMeanChunkTime)
				{
					MeanChunkTime.Reset();
				}
			}

			// Wait while paused
			while (bIsPaused && !bShouldAbort)
			{
				Platform->Sleep(0.1f);
			}

			// Give other threads some time.
			Platform->Sleep(0.01f);
		}

		// Provide final stat values.
		CloudChunkSourceStat->OnDownloadHealthUpdated(TrackedDownloadHealth);
		CloudChunkSourceStat->OnSuccessRateUpdated(ChunkSuccessRate.GetOverall());
		CloudChunkSourceStat->OnActiveRequestCountUpdated(0);
	}

	void FCloudChunkSource::OnDownloadProgress(int32 RequestId, int32 BytesSoFar)
	{
		FPlatformAtomics::InterlockedExchange(&CyclesAtLastData, FStatsCollector::GetCycles());
	}

	void FCloudChunkSource::OnDownloadComplete(int32 RequestId, const FDownloadRef& Download)
	{
		FScopeLock ScopeLock(&CompletedDownloadsCS);
		CompletedDownloads.Add(RequestId, Download);
	}

	ICloudChunkSource* FCloudChunkSourceFactory::Create(FCloudSourceConfig Configuration, IPlatform* Platform, IChunkStore* ChunkStore, IDownloadService* DownloadService, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, IMessagePump* MessagePump, IInstallerError* InstallerError, ICloudChunkSourceStat* CloudChunkSourceStat, FBuildPatchAppManifestRef InstallManifest, TSet<FGuid> InitialDownloadSet)
	{
		check(Platform != nullptr);
		check(ChunkStore != nullptr);
		check(DownloadService != nullptr);
		check(ChunkReferenceTracker != nullptr);
		check(ChunkDataSerialization != nullptr);
		check(MessagePump != nullptr);
		check(InstallerError != nullptr);
		check(CloudChunkSourceStat != nullptr);
		return new FCloudChunkSource(MoveTemp(Configuration), Platform, ChunkStore, DownloadService, ChunkReferenceTracker, ChunkDataSerialization, MessagePump, InstallerError, CloudChunkSourceStat, MoveTemp(InstallManifest), MoveTemp(InitialDownloadSet));
	}
}