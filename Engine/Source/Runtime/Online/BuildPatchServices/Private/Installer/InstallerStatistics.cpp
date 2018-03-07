// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/InstallerStatistics.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/DiskChunkStore.h"
#include "Installer/ChunkDbChunkSource.h"
#include "Installer/InstallChunkSource.h"
#include "Installer/DownloadService.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/Verifier.h"
#include "Installer/InstallerAnalytics.h"
#include "Common/StatsCollector.h"
#include "GenericPlatformMath.h"
#include "Algo/Sort.h"
#include "ThreadSafeCounter64.h"
#include "Interfaces/IBuildInstaller.h"
#include "BuildPatchProgress.h"
#include "BuildPatchFileConstructor.h"
#include "Misc/ScopeLock.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInstallerStatistics, Warning, All);
DEFINE_LOG_CATEGORY(LogInstallerStatistics);

namespace BuildPatchServices
{
#if !PLATFORM_HAS_64BIT_ATOMICS
	class FThreadSafeInt64
	{
	public:
		FThreadSafeInt64();

		int64 Increment();
		int64 Add(int64 Amount);
		int64 Decrement();
		int64 Subtract(int64 Amount);
		int64 Set(int64 Value);
		int64 Reset();
		int64 GetValue() const;

	private:
		mutable FCriticalSection CounterCs;
		int64 Counter;
	};

	FThreadSafeInt64::FThreadSafeInt64()
		: CounterCs()
		, Counter(0)
	{
	}

	int64 FThreadSafeInt64::Increment()
	{
		FScopeLock ScopeLock(&CounterCs);
		return ++Counter;
	}

	int64 FThreadSafeInt64::Add(int64 Amount)
	{
		FScopeLock ScopeLock(&CounterCs);
		int64 OldValue = Counter;
		Counter += Amount;
		return OldValue;
	}

	int64 FThreadSafeInt64::Decrement()
	{
		FScopeLock ScopeLock(&CounterCs);
		return --Counter;
	}

	int64 FThreadSafeInt64::Subtract(int64 Amount)
	{
		FScopeLock ScopeLock(&CounterCs);
		int64 OldValue = Counter;
		Counter -= Amount;
		return OldValue;
	}

	int64 FThreadSafeInt64::Set(int64 Value)
	{
		FScopeLock ScopeLock(&CounterCs);
		int64 OldValue = Counter;
		Counter = Value;
		return OldValue;
	}

	int64 FThreadSafeInt64::Reset()
	{
		return Set(0);
	}

	int64 FThreadSafeInt64::GetValue() const
	{
		FScopeLock ScopeLock(&CounterCs);
		return Counter;
	}
#else
	typedef FThreadSafeCounter64 FThreadSafeInt64;
#endif

	class FMemoryChunkStoreStat
		: public IMemoryChunkStoreStat
	{
	public:
		FMemoryChunkStoreStat();
		~FMemoryChunkStoreStat() {}

	public:
		// IMemoryChunkStoreStat interface begin.
		virtual void OnChunkStored(const FGuid& ChunkId) override {}
		virtual void OnChunkReleased(const FGuid& ChunkId) override {}
		virtual void OnChunkBooted(const FGuid& ChunkId) override { NumChunksBooted.Increment(); }
		virtual void OnStoreUseUpdated(int32 ChunkCount) override { NumChunksInStore.Set(ChunkCount); }
		// IMemoryChunkStoreStat interface end.

		int32 GetNumChunksBooted() const { return NumChunksBooted.GetValue(); }

	private:;
		FThreadSafeCounter NumChunksBooted;
		FThreadSafeCounter NumChunksInStore;
	};

	FMemoryChunkStoreStat::FMemoryChunkStoreStat()
		: NumChunksBooted(0)
		, NumChunksInStore(0)
	{
	}

	class FDiskChunkStoreStat
		: public IDiskChunkStoreStat
	{
	public:
		FDiskChunkStoreStat(IInstallerAnalytics* InInstallerAnalytics);
		~FDiskChunkStoreStat() {}

	public:
		// IDiskChunkStoreStat interface begin.
		virtual void OnChunkStored(const FGuid& ChunkId, const FString& ChunkFilename, EChunkSaveResult SaveResult) override;
		virtual void OnChunkLoaded(const FGuid& ChunkId, const FString& ChunkFilename, EChunkLoadResult LoadResult) override;
		virtual void OnCacheUseUpdated(int32 ChunkCount) override {}
		// IDiskChunkStoreStat interface end.

		int32 GetNumSuccessfulLoads() const { return NumSuccessfulLoads.GetValue(); }
		int32 GetNumFailedLoads() const { return NumFailedLoads.GetValue(); }
		int32 GetNumSuccessfulSaves() const { return NumSuccessfulSaves.GetValue(); }
		int32 GetNumFailedSaves() const { return NumFailedSaves.GetValue(); }

	private:
		const TCHAR* GetAnalyticsErrorString(EChunkLoadResult LoadResult) const;
		const TCHAR* GetAnalyticsErrorString(EChunkSaveResult SaveResult) const;

	private:
		IInstallerAnalytics* InstallerAnalytics;
		FThreadSafeCounter NumSuccessfulLoads;
		FThreadSafeCounter NumSuccessfulSaves;
		FThreadSafeCounter NumFailedLoads;
		FThreadSafeCounter NumFailedSaves;
	};

	FDiskChunkStoreStat::FDiskChunkStoreStat(IInstallerAnalytics* InInstallerAnalytics)
		: InstallerAnalytics(InInstallerAnalytics)
		, NumSuccessfulLoads(0)
		, NumSuccessfulSaves(0)
		, NumFailedLoads(0)
		, NumFailedSaves(0)
	{
	}

	void FDiskChunkStoreStat::OnChunkStored(const FGuid& ChunkId, const FString& ChunkFilename, EChunkSaveResult SaveResult)
	{
		if (SaveResult == EChunkSaveResult::Success)
		{
			NumSuccessfulSaves.Increment();
		}
		else
		{
			InstallerAnalytics->RecordChunkCacheError(ChunkId, ChunkFilename, FPlatformMisc::GetLastError(), TEXT("DiskChunkStoreSave"), GetAnalyticsErrorString(SaveResult));
			NumFailedSaves.Increment();
		}
	}

	void FDiskChunkStoreStat::OnChunkLoaded(const FGuid& ChunkId, const FString& ChunkFilename, EChunkLoadResult LoadResult)
	{
		if (LoadResult == EChunkLoadResult::Success)
		{
			NumSuccessfulLoads.Increment();
		}
		else
		{
			InstallerAnalytics->RecordChunkCacheError(ChunkId, ChunkFilename, FPlatformMisc::GetLastError(), TEXT("DiskChunkStoreLoad"), GetAnalyticsErrorString(LoadResult));
			NumFailedLoads.Increment();
		}
	}

	const TCHAR* FDiskChunkStoreStat::GetAnalyticsErrorString(EChunkLoadResult LoadResult) const
	{
		switch (LoadResult)
		{
		case EChunkLoadResult::Success:
			return TEXT("Success");
		case EChunkLoadResult::OpenFileFail:
			return TEXT("OpenFileFail");
		case EChunkLoadResult::CorruptHeader:
			return TEXT("CorruptHeader");
		case EChunkLoadResult::IncorrectFileSize:
			return TEXT("IncorrectFileSize");
		case EChunkLoadResult::UnsupportedStorage:
			return TEXT("UnsupportedStorage");
		case EChunkLoadResult::MissingHashInfo:
			return TEXT("MissingHashInfo");
		case EChunkLoadResult::SerializationError:
			return TEXT("SerializationError");
		case EChunkLoadResult::DecompressFailure:
			return TEXT("DecompressFailure");
		case EChunkLoadResult::HashCheckFailed:
			return TEXT("HashCheckFailed");
		}
		return TEXT("Unknown");
	}

	const TCHAR* FDiskChunkStoreStat::GetAnalyticsErrorString(EChunkSaveResult SaveResult) const
	{
		switch (SaveResult)
		{
		case EChunkSaveResult::Success:
			return TEXT("Success");
		case EChunkSaveResult::FileCreateFail:
			return TEXT("FileCreateFail");
		case EChunkSaveResult::SerializationError:
			return TEXT("SerializationError");
		}
		return TEXT("Unknown");
	}

	class FChunkDbChunkSourceStat
		: public IChunkDbChunkSourceStat
	{
	public:
		FChunkDbChunkSourceStat();
		~FChunkDbChunkSourceStat();

	public:
		// IChunkDbChunkSourceStat interface begin.
		virtual void OnLoadStarted(const FGuid& ChunkId) override;
		virtual void OnLoadComplete(const FGuid& ChunkId, ELoadResult Result) override;
		// IChunkDbChunkSourceStat interface end.

		int32 GetNumSuccessfulLoads() const { return NumSuccessfulLoads.GetValue(); }
		int32 GetNumFailedLoads() const { return NumFailedLoads.GetValue(); }

	private:
		FThreadSafeCounter NumSuccessfulLoads;
		FThreadSafeCounter NumFailedLoads;
	};

	FChunkDbChunkSourceStat::FChunkDbChunkSourceStat()
		: NumSuccessfulLoads(0)
		, NumFailedLoads(0)
	{
	}

	FChunkDbChunkSourceStat::~FChunkDbChunkSourceStat()
	{
	}

	void FChunkDbChunkSourceStat::OnLoadStarted(const FGuid& ChunkId)
	{
	}

	void FChunkDbChunkSourceStat::OnLoadComplete(const FGuid& ChunkId, ELoadResult Result)
	{
		if (Result == ELoadResult::Success)
		{
			NumSuccessfulLoads.Increment();
		}
		else
		{
			NumFailedLoads.Increment();
		}
	}

	class FInstallChunkSourceStat
		: public IInstallChunkSourceStat
	{
	public:
		FInstallChunkSourceStat(IInstallerAnalytics* InInstallerAnalytics);
		~FInstallChunkSourceStat() {}

	public:
		// IInstallChunkSourceStat interface begin.
		virtual void OnLoadStarted(const FGuid& ChunkId) override {}
		virtual void OnLoadComplete(const FGuid& ChunkId, ELoadResult Result) override;
		// IInstallChunkSourceStat interface end.

		int32 GetNumSuccessfulLoads() const { return NumSuccessfulLoads.GetValue(); }
		int32 GetNumFailedLoads() const { return NumFailedLoads.GetValue(); }

	private:
		const TCHAR* GetAnalyticsErrorString(ELoadResult Result) const;

	private:
		IInstallerAnalytics* InstallerAnalytics;
		FThreadSafeCounter NumSuccessfulLoads;
		FThreadSafeCounter NumFailedLoads;
	};

	FInstallChunkSourceStat::FInstallChunkSourceStat(IInstallerAnalytics* InInstallerAnalytics)
		: InstallerAnalytics(InInstallerAnalytics)
		, NumSuccessfulLoads(0)
		, NumFailedLoads(0)
	{
	}

	void FInstallChunkSourceStat::OnLoadComplete(const FGuid& ChunkId, ELoadResult Result)
	{
		if (Result == ELoadResult::Success)
		{
			NumSuccessfulLoads.Increment();
		}
		else
		{
			InstallerAnalytics->RecordChunkCacheError(ChunkId, TEXT(""), FPlatformMisc::GetLastError(), TEXT("InstallChunkSourceLoad"), GetAnalyticsErrorString(Result));
			NumFailedLoads.Increment();
		}
	}

	const TCHAR* FInstallChunkSourceStat::GetAnalyticsErrorString(ELoadResult Result) const
	{
		switch (Result)
		{
		case ELoadResult::Success:
			return TEXT("Success");
		case ELoadResult::MissingHashInfo:
			return TEXT("MissingHashInfo");
		case ELoadResult::MissingPartInfo:
			return TEXT("MissingPartInfo");
		case ELoadResult::OpenFileFail:
			return TEXT("OpenFileFail");
		case ELoadResult::IncorrectFileSize:
			return TEXT("IncorrectFileSize");
		case ELoadResult::HashCheckFailed:
			return TEXT("HashCheckFailed");
		case ELoadResult::Aborted:
			return TEXT("Aborted");
		}
		return TEXT("Unknown");
	}

	class FDownloadServiceStat
		: public IDownloadServiceStat
	{
	public:
		struct FRecord
		{
		public:
			FRecord(const FDownloadRecord& FromDownloadRecord)
				: StartedAt(FromDownloadRecord.StartedAt)
				, CompletedAt(FromDownloadRecord.CompletedAt)
				, BytesReceived(FromDownloadRecord.BytesReceived)
			{}

		public:
			double StartedAt;
			double CompletedAt;
			int32 BytesReceived;
		};

	public:
		FDownloadServiceStat(IInstallerAnalytics* InInstallerAnalytics);
		~FDownloadServiceStat() {}

	public:
		// IDownloadServiceStat interface start.
		virtual void OnDownloadComplete(FDownloadRecord DownloadRecord) override;
		// IDownloadServiceStat interface end.

		int32 GetNumSuccessfulDownloads() const { return NumSuccessfulDownloads.GetValue(); }
		int32 GetNumFailedDownloads() const { return NumFailedDownloads.GetValue(); }
		void GetRecordings(double OverTime, TArray<FRecord>& RecordsToConsider) const;

	private:
		IInstallerAnalytics* InstallerAnalytics;
		mutable FCriticalSection ThreadLockCs;
		FThreadSafeCounter NumSuccessfulDownloads;
		FThreadSafeCounter NumFailedDownloads;
		TArray<FDownloadRecord> DownloadRecords;
	};

	FDownloadServiceStat::FDownloadServiceStat(IInstallerAnalytics* InInstallerAnalytics)
		: InstallerAnalytics(InInstallerAnalytics)
		, ThreadLockCs()
		, NumSuccessfulDownloads(0)
		, NumFailedDownloads(0)
		, DownloadRecords()
	{
	}

	void FDownloadServiceStat::OnDownloadComplete(FDownloadRecord DownloadRecord)
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		if (DownloadRecord.bSuccess)
		{
			NumSuccessfulDownloads.Increment();
		}
		else
		{
			NumFailedDownloads.Increment();
			InstallerAnalytics->RecordChunkDownloadError(DownloadRecord.Uri, DownloadRecord.ResponseCode, TEXT("DownloadFail"));
		}
		DownloadRecords.Add(MoveTemp(DownloadRecord));
	}

	void FDownloadServiceStat::GetRecordings(double OverTime, TArray<FRecord>& RecordsToConsider) const
	{
		// Collect data
		FScopeLock ScopeLock(&ThreadLockCs);
		if (DownloadRecords.Num())
		{
			double RangeEnd = FStatsCollector::GetSeconds();
			double RangeBegin = RangeEnd - OverTime;
			for (int32 DownloadRecordIdx = DownloadRecords.Num() - 1; DownloadRecordIdx >= 0; --DownloadRecordIdx)
			{
				const FDownloadRecord& DownloadRecord = DownloadRecords[DownloadRecordIdx];
				if (DownloadRecord.CompletedAt <= RangeBegin)
				{
					break;
				}
				if (DownloadRecord.StartedAt >= RangeBegin)
				{
					RecordsToConsider.Emplace(DownloadRecord);
				}
				else
				{
					// Interpolate
					RecordsToConsider.Emplace(DownloadRecord);
					FRecord& Last = RecordsToConsider.Last();
					Last.StartedAt = RangeBegin;
					if (DownloadRecord.CompletedAt == DownloadRecord.StartedAt)
					{
						Last.BytesReceived = 0;
					}
					else
					{
						Last.BytesReceived *= (Last.CompletedAt - Last.StartedAt) / (DownloadRecord.CompletedAt - DownloadRecord.StartedAt);
					}
				}
			}
		}
	}

	class FCloudChunkSourceStat
		: public ICloudChunkSourceStat
	{
		static const int32 SuccessRateMultiplier = 10000;
	public:
		FCloudChunkSourceStat(IInstallerAnalytics* InInstallerAnalytics, FBuildPatchProgress* InBuildProgress);

		~FCloudChunkSourceStat()
		{
		}

	public:
		// ICloudChunkSourceStat interface begin.
		virtual void OnDownloadRequested(const FGuid& ChunkId) override {}
		virtual void OnDownloadFailed(const FGuid& ChunkId, const FString& Url) override {}
		virtual void OnDownloadCorrupt(const FGuid& ChunkId, const FString& Url, EChunkLoadResult LoadResult) override;
		virtual void OnDownloadAborted(const FGuid& ChunkId, const FString& Url, double DownloadTimeMean, double DownloadTimeStd, double DownloadTime, double BreakingPoint) override;
		virtual void OnReceivedDataUpdated(int64 TotalBytes) override;
		virtual void OnRequiredDataUpdated(int64 TotalBytes) override;
		virtual void OnDownloadHealthUpdated(EBuildPatchDownloadHealth DownloadHealth) override;
		virtual void OnSuccessRateUpdated(float SuccessRate) override;
		virtual void OnActiveRequestCountUpdated(int32 RequestCount) override;
		// ICloudChunkSourceStat interface end.

		int64 GetTotalBytesReceived() const { return TotalBytesReceived.GetValue(); }
		int64 GetTotalBytesRequired() const { return TotalBytesRequired.GetValue(); }
		int64 GetNumDownloadsCorrupt() const { return NumDownloadsCorrupt.GetValue(); }
		int64 GetNumDownloadsAborted() const { return NumDownloadsAborted.GetValue(); }
		EBuildPatchDownloadHealth GetDownloadHealth() const { FScopeLock Lock(&ThreadLockCs); return CurrentHealth; }
		TArray<float> GetDownloadHealthTimers() const { FScopeLock Lock(&ThreadLockCs); return HealthStateTimes; }
		float GetSuccessRate() const { return (float)ChunkSuccessRate.GetValue() / (float)SuccessRateMultiplier; }
		int32 GetActiveRequestCount() const { return ActiveRequestCount.GetValue(); }

	private:
		const TCHAR* GetAnalyticsErrorString(EChunkLoadResult LoadResult) const;

	private:
		IInstallerAnalytics* InstallerAnalytics;
		FBuildPatchProgress* BuildProgress;
		FThreadSafeInt64 TotalBytesReceived;
		FThreadSafeInt64 TotalBytesRequired;
		FThreadSafeCounter NumDownloadsCorrupt;
		FThreadSafeCounter NumDownloadsAborted;
		FThreadSafeCounter ChunkSuccessRate;
		FThreadSafeCounter ActiveRequestCount;
		mutable FCriticalSection ThreadLockCs;
		EBuildPatchDownloadHealth CurrentHealth;
		int64 CyclesAtLastHealthState;
		TArray<float> HealthStateTimes;
	};

	FCloudChunkSourceStat::FCloudChunkSourceStat(IInstallerAnalytics* InInstallerAnalytics, FBuildPatchProgress* InBuildProgress)
		: InstallerAnalytics(InInstallerAnalytics)
		, BuildProgress(InBuildProgress)
		, TotalBytesReceived(0)
		, TotalBytesRequired(0)
		, NumDownloadsCorrupt(0)
		, NumDownloadsAborted(0)
		, ChunkSuccessRate(0)
		, ActiveRequestCount(0)
		, ThreadLockCs()
		, CurrentHealth(EBuildPatchDownloadHealth::Excellent)
		, CyclesAtLastHealthState(0)
	{
		// Initialise health states to zero time
		HealthStateTimes.AddZeroed((int32)EBuildPatchDownloadHealth::NUM_Values);
	}

	void FCloudChunkSourceStat::OnDownloadCorrupt(const FGuid& ChunkId, const FString& Url, EChunkLoadResult LoadResult)
	{
		InstallerAnalytics->RecordChunkDownloadError(Url, INDEX_NONE, GetAnalyticsErrorString(LoadResult));
		NumDownloadsCorrupt.Increment();
	}

	void FCloudChunkSourceStat::OnDownloadAborted(const FGuid& ChunkId, const FString& Url, double DownloadTimeMean, double DownloadTimeStd, double DownloadTime, double BreakingPoint)
	{
		InstallerAnalytics->RecordChunkDownloadAborted(Url, DownloadTime, DownloadTimeMean, DownloadTimeStd, BreakingPoint);
		NumDownloadsAborted.Increment();
	}

	void FCloudChunkSourceStat::OnReceivedDataUpdated(int64 TotalBytes)
	{
		TotalBytesReceived.Set(TotalBytes);
		int64 Required = TotalBytesRequired.GetValue();
		if (Required > 0)
		{
			BuildProgress->SetStateProgress(EBuildPatchState::Downloading, (double)TotalBytes / (double)Required);
		}
	}

	void FCloudChunkSourceStat::OnRequiredDataUpdated(int64 TotalBytes)
	{
		TotalBytesRequired.Set(TotalBytes);
		int64 Received = TotalBytesReceived.GetValue();
		if (TotalBytes > 0)
		{
			BuildProgress->SetStateProgress(EBuildPatchState::Downloading, (double)Received / (double)TotalBytes);
		}
	}

	void FCloudChunkSourceStat::OnDownloadHealthUpdated(EBuildPatchDownloadHealth DownloadHealth)
	{
		FScopeLock Lock(&ThreadLockCs);
		// Update time in state
		uint64 CyclesNow = FStatsCollector::GetCycles();
		if (CyclesAtLastHealthState > 0)
		{
			HealthStateTimes[(int32)CurrentHealth] += FStatsCollector::CyclesToSeconds(CyclesNow - CyclesAtLastHealthState);
		}
		CurrentHealth = DownloadHealth;
		FPlatformAtomics::InterlockedExchange(&CyclesAtLastHealthState, CyclesNow);
	}

	void FCloudChunkSourceStat::OnSuccessRateUpdated(float SuccessRate)
	{
		// The success rate comes as a 0-1 value. We can multiply it up and use atomics still.
		ChunkSuccessRate.Set(SuccessRate * SuccessRateMultiplier);
	}

	void FCloudChunkSourceStat::OnActiveRequestCountUpdated(int32 RequestCount)
	{
		BuildProgress->SetIsDownloading(RequestCount > 0);
		ActiveRequestCount.Set(RequestCount);
	}

	const TCHAR* FCloudChunkSourceStat::GetAnalyticsErrorString(EChunkLoadResult LoadResult) const
	{
		switch (LoadResult)
		{
		case EChunkLoadResult::Success:
			return TEXT("Success");
		case EChunkLoadResult::OpenFileFail:
			return TEXT("OpenFileFail");
		case EChunkLoadResult::CorruptHeader:
			return TEXT("CorruptHeader");
		case EChunkLoadResult::IncorrectFileSize:
			return TEXT("IncorrectFileSize");
		case EChunkLoadResult::UnsupportedStorage:
			return TEXT("UnsupportedStorage");
		case EChunkLoadResult::MissingHashInfo:
			return TEXT("MissingHashInfo");
		case EChunkLoadResult::SerializationError:
			return TEXT("SerializationError");
		case EChunkLoadResult::DecompressFailure:
			return TEXT("DecompressFailure");
		case EChunkLoadResult::HashCheckFailed:
			return TEXT("HashCheckFailed");
		}
		return TEXT("Unknown");
	}

	class FFileConstructorStat
		: public IFileConstructorStat
	{
	public:
		FFileConstructorStat(FBuildPatchProgress* InBuildProgress);
		~FFileConstructorStat() {}

	public:
		// IFileConstructorStat interface begin.
		virtual void OnResumeStarted() override { BuildProgress->SetStateProgress(EBuildPatchState::Resuming, 0.0f); }
		virtual void OnResumeCompleted() override { BuildProgress->SetStateProgress(EBuildPatchState::Resuming, 1.0f); }
		virtual void OnFileStarted(const FString& Filename, int64 FileSize) override {}
		virtual void OnFileProgress(const FString& Filename, int64 TotalBytes) override {}
		virtual void OnFileCompleted(const FString& Filename, bool bSuccess) override {}
		virtual void OnProcessedDataUpdated(int64 TotalBytes) override;
		virtual void OnTotalRequiredUpdated(int64 TotalBytes) override;
		// IFileConstructorStat interface end.

	private:
		FBuildPatchProgress* BuildProgress;
		FThreadSafeInt64 TotalBytesProcessed;
		FThreadSafeInt64 TotalBytesRequired;
	};

	FFileConstructorStat::FFileConstructorStat(FBuildPatchProgress* InBuildProgress)
		: BuildProgress(InBuildProgress)
		, TotalBytesProcessed(0)
		, TotalBytesRequired(0)
	{
	}

	void FFileConstructorStat::OnProcessedDataUpdated(int64 TotalBytes)
	{
		TotalBytesProcessed.Set(TotalBytes);
		int64 Required = TotalBytesRequired.GetValue();
		if (Required > 0)
		{
			BuildProgress->SetStateProgress(EBuildPatchState::Installing, (double)TotalBytes / (double)Required);
		}
	}

	void FFileConstructorStat::OnTotalRequiredUpdated(int64 TotalBytes)
	{
		TotalBytesRequired.Set(TotalBytes);
		int64 Processed = TotalBytesProcessed.GetValue();
		if (TotalBytes > 0)
		{
			BuildProgress->SetStateProgress(EBuildPatchState::Installing, (double)Processed / (double)TotalBytes);
		}
	}

	class FVerifierStat
		: public IVerifierStat
	{
	public:
		FVerifierStat(FBuildPatchProgress* InBuildProgress);
		~FVerifierStat() {}

	public:
		// IVerifierStat interface begin.
		virtual void OnFileStarted(const FString& Filename, int64 FileSize) override {}
		virtual void OnFileProgress(const FString& Filename, int64 TotalBytes) override {}
		virtual void OnFileCompleted(const FString& Filename, bool bSuccess) override {}
		virtual void OnProcessedDataUpdated(int64 TotalBytes) override;
		virtual void OnTotalRequiredUpdated(int64 TotalBytes) override;
		// IVerifierStat interface end.

	private:
		FBuildPatchProgress* BuildProgress;
		FThreadSafeInt64 TotalBytesProcessed;
		FThreadSafeInt64 TotalBytesRequired;
	};

	FVerifierStat::FVerifierStat(FBuildPatchProgress* InBuildProgress)
		: BuildProgress(InBuildProgress)
		, TotalBytesProcessed(0)
		, TotalBytesRequired(0)
	{
	}

	void FVerifierStat::OnProcessedDataUpdated(int64 TotalBytes)
	{
		TotalBytesProcessed.Set(TotalBytes);
		int64 Required = TotalBytesRequired.GetValue();
		if (Required > 0)
		{
			BuildProgress->SetStateProgress(EBuildPatchState::BuildVerification, (double)TotalBytes / (double)Required);
		}
	}

	void FVerifierStat::OnTotalRequiredUpdated(int64 TotalBytes)
	{
		TotalBytesRequired.Set(TotalBytes);
		int64 Processed = TotalBytesProcessed.GetValue();
		if (TotalBytes > 0)
		{
			BuildProgress->SetStateProgress(EBuildPatchState::BuildVerification, (double)Processed / (double)TotalBytes);
		}
	}

	class FInstallerStatistics
		: public IInstallerStatistics
	{
	public:
		FInstallerStatistics(IInstallerAnalytics* InstallerAnalytics, FBuildPatchProgress* BuildProgress);
		~FInstallerStatistics() {}

	public:
		// IInstallerStatistics interface begin.
		virtual int64 GetBytesDownloaded() const override { return CloudChunkSourceStat->GetTotalBytesReceived(); }
		virtual int32 GetNumSuccessfulChunkDownloads() const override { return DownloadServiceStat->GetNumSuccessfulDownloads(); }
		virtual int32 GetNumFailedChunkDownloads() const override { return DownloadServiceStat->GetNumFailedDownloads(); }
		virtual int32 GetNumCorruptChunkDownloads() const override { return CloudChunkSourceStat->GetNumDownloadsCorrupt(); }
		virtual int32 GetNumAbortedChunkDownloads() const override { return CloudChunkSourceStat->GetNumDownloadsAborted(); }
		virtual int32 GetNumSuccessfulChunkRecycles() const override { return InstallChunkSourceStat->GetNumSuccessfulLoads(); }
		virtual int32 GetNumFailedChunkRecycles() const override { return InstallChunkSourceStat->GetNumFailedLoads(); }
		virtual int32 GetNumSuccessfulChunkDbLoads() const override { return ChunkDbChunkSourceStat->GetNumSuccessfulLoads(); }
		virtual int32 GetNumFailedChunkDbLoads() const override { return ChunkDbChunkSourceStat->GetNumFailedLoads(); }
		virtual int32 GetNumStoreBootedChunks() const override;
		virtual int32 GetNumSuccessfulChunkDiskCacheLoads() const override { return DiskChunkStoreStat->GetNumSuccessfulLoads(); }
		virtual int32 GetNumFailedChunkDiskCacheLoads() const override { return DiskChunkStoreStat->GetNumFailedLoads(); }
		virtual int64 GetRequiredDownloadSize() const override { return CloudChunkSourceStat->GetTotalBytesRequired(); }
		virtual double GetDownloadSpeed(float OverTime) const override;
		virtual float GetDownloadSuccessRate() const override { return CloudChunkSourceStat->GetSuccessRate(); }
		virtual EBuildPatchDownloadHealth GetDownloadHealth() const override { return CloudChunkSourceStat->GetDownloadHealth(); }
		virtual TArray<float> GetDownloadHealthTimers() const override { return CloudChunkSourceStat->GetDownloadHealthTimers(); }
		virtual IMemoryChunkStoreStat* GetMemoryChunkStoreStat(EMemoryChunkStore Instance) override { return MemoryChunkStoreStats[(int32)Instance].Get(); }
		virtual IDiskChunkStoreStat* GetDiskChunkStoreStat() override { return DiskChunkStoreStat.Get(); }
		virtual IChunkDbChunkSourceStat* GetChunkDbChunkSourceStat() override { return ChunkDbChunkSourceStat.Get(); }
		virtual IInstallChunkSourceStat* GetInstallChunkSourceStat() override { return InstallChunkSourceStat.Get(); }
		virtual IDownloadServiceStat* GetDownloadServiceStat() override { return DownloadServiceStat.Get(); }
		virtual ICloudChunkSourceStat* GetCloudChunkSourceStat() override { return CloudChunkSourceStat.Get(); }
		virtual IFileConstructorStat* GetFileConstructorStat() override { return FileConstructorStat.Get(); }
		virtual IVerifierStat* GetVerifierStat() override { return VerifierStat.Get(); }
		// IInstallerStatistics interface end.

	private:
		TArray<TUniquePtr<FMemoryChunkStoreStat>> MemoryChunkStoreStats;
		TUniquePtr<FDiskChunkStoreStat> DiskChunkStoreStat;
		TUniquePtr<FChunkDbChunkSourceStat> ChunkDbChunkSourceStat;
		TUniquePtr<FInstallChunkSourceStat> InstallChunkSourceStat;
		TUniquePtr<FDownloadServiceStat> DownloadServiceStat;
		TUniquePtr<FCloudChunkSourceStat> CloudChunkSourceStat;
		TUniquePtr<FFileConstructorStat> FileConstructorStat;
		TUniquePtr<FVerifierStat> VerifierStat;
	};

	FInstallerStatistics::FInstallerStatistics(IInstallerAnalytics* InstallerAnalytics, FBuildPatchProgress* BuildProgress)
		: MemoryChunkStoreStats()
		, DiskChunkStoreStat(new FDiskChunkStoreStat(InstallerAnalytics))
		, ChunkDbChunkSourceStat(new FChunkDbChunkSourceStat())
		, InstallChunkSourceStat(new FInstallChunkSourceStat(InstallerAnalytics))
		, DownloadServiceStat(new FDownloadServiceStat(InstallerAnalytics))
		, CloudChunkSourceStat(new FCloudChunkSourceStat(InstallerAnalytics, BuildProgress))
		, FileConstructorStat(new FFileConstructorStat(BuildProgress))
		, VerifierStat(new FVerifierStat(BuildProgress))
	{
		for (EMemoryChunkStore MemoryChunkStore : TEnumRange<EMemoryChunkStore>())
		{
			MemoryChunkStoreStats.Emplace(new FMemoryChunkStoreStat());
		}
	}

	int32 FInstallerStatistics::GetNumStoreBootedChunks() const
	{
		int32 Count = 0;
		for (const TUniquePtr<FMemoryChunkStoreStat>& MemoryChunkStoreStat : MemoryChunkStoreStats)
		{
			Count += MemoryChunkStoreStat->GetNumChunksBooted();
		}
		return Count;
	}

	double FInstallerStatistics::GetDownloadSpeed(float OverTime) const
	{
		// Collect data
		TArray<FDownloadServiceStat::FRecord> RecordsToConsider;
		DownloadServiceStat->GetRecordings(OverTime, RecordsToConsider);
		// Calculate
		double TotalTime = 0;
		int64 TotalBytes = 0;
		Algo::SortBy(RecordsToConsider, [](const FDownloadServiceStat::FRecord& Entry) { return Entry.StartedAt; });
		double RecoredEndTime = 0;
		for (const FDownloadServiceStat::FRecord& Record : RecordsToConsider)
		{
			// Do we have some time to count
			if (RecoredEndTime < Record.CompletedAt)
			{
				// Don't count time overlap
				TotalTime += Record.CompletedAt - FMath::Max(Record.StartedAt, RecoredEndTime);
				RecoredEndTime = Record.CompletedAt;
			}
			TotalBytes += Record.BytesReceived;
		}
		return TotalTime > 0 ? TotalBytes / TotalTime : 0;
	}

	IInstallerStatistics* FInstallerStatisticsFactory::Create(IInstallerAnalytics* InstallerAnalytics, FBuildPatchProgress* BuildProgress)
	{
		check(InstallerAnalytics != nullptr);
		check(BuildProgress != nullptr);
		return new FInstallerStatistics(InstallerAnalytics, BuildProgress);
	}
}
