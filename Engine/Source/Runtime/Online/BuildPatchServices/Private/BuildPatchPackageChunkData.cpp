// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BuildPatchPackageChunkData.h"
#include "BuildPatchManifest.h"
#include "Async/TaskGraphInterfaces.h"
#include "Serialization/MemoryWriter.h"
#include "Core/Platform.h"
#include "Common/HttpManager.h"
#include "Common/FileSystem.h"
#include "Generation/ChunkDatabaseWriter.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkEvictionPolicy.h"
#include "Installer/MemoryChunkStore.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/DownloadService.h"
#include "Installer/MessagePump.h"
#include "Installer/InstallerError.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/InstallerStatistics.h"
#include "BuildPatchProgress.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPackageChunkData, Log, All);
DEFINE_LOG_CATEGORY(LogPackageChunkData);

bool FBuildPackageChunkData::PackageChunkData(const FString& ManifestFilePath, const FString& OutputFile, const FString& CloudDir, uint64 MaxOutputFileSize)
{
	const TCHAR* ChunkDbExtension = TEXT(".chunkdb");
	using namespace BuildPatchServices;
	bool bSuccess = false;
	FBuildPatchAppManifestRef Manifest = MakeShareable(new FBuildPatchAppManifest());
	if (Manifest->LoadFromFile(ManifestFilePath))
	{
		// Programmatically calculate header file size effects, so that we automatically handle any changes to the header spec.
		TArray<uint8> HeaderData;
		FMemoryWriter HeaderWriter(HeaderData);
		FChunkDatabaseHeader ChunkDbHeader;
		HeaderWriter << ChunkDbHeader;
		const uint64 ChunkDbHeaderSize = HeaderData.Num();
		HeaderWriter.Seek(0);
		HeaderData.Reset();
		ChunkDbHeader.Contents.Add({FGuid::NewGuid(), 0, 0});
		HeaderWriter << ChunkDbHeader;
		const uint64 PerEntryHeaderSize = HeaderData.Num() - ChunkDbHeaderSize;

		// Enumerate the chunks, allocating them to chunk db files.
		TUniquePtr<IChunkReferenceTracker> ChunkReferenceTracker(FChunkReferenceTrackerFactory::Create(Manifest));
		TSet<FGuid> FullDataSet = ChunkReferenceTracker->GetReferencedChunks();
		if (FullDataSet.Num() > 0)
		{
			TArray<FChunkDatabaseFile> ChunkDbFiles;
			ChunkDbFiles.AddDefaulted();
			int32 ChunkDbFileCount = 0;

			// Figure out the chunks to write per chunkdb file.
			uint64 AvailableFileSize = MaxOutputFileSize - ChunkDbHeaderSize;
			for (const FGuid& DataId : FullDataSet)
			{
				const uint64 DataSize = Manifest->GetDataSize(DataId) + PerEntryHeaderSize;
				if (AvailableFileSize < DataSize && ChunkDbFiles[ChunkDbFileCount].DataList.Num() > 0)
				{
					ChunkDbFiles.AddDefaulted();
					++ChunkDbFileCount;
					AvailableFileSize = MaxOutputFileSize - ChunkDbHeaderSize;
				}

				ChunkDbFiles[ChunkDbFileCount].DataList.Add(DataId);
				if (AvailableFileSize > DataSize)
				{
					AvailableFileSize -= DataSize;
				}
				else
				{
					AvailableFileSize = 0;
				}
			}

			// Figure out the filenames of each chunkdb.
			if (ChunkDbFiles.Num() > 1)
			{
				// Figure out the per file filename;
				FString FilenameFmt = OutputFile;
				FilenameFmt.RemoveFromEnd(ChunkDbExtension);
				// Make sure we don't confuse any existing % characters when formatting (yes, % is a valid filename character).
				FilenameFmt.ReplaceInline(TEXT("%"), TEXT("%%"));
				FilenameFmt += TEXT(".part%0*d.chunkdb");
				// Technically, there are mathematical solutions to this, however there can be floating point errors in log that cause edge cases there.
				// We'll just use the obvious simple method.
				int32 NumDigitsForParts = FString::Printf(TEXT("%d"), ChunkDbFiles.Num()).Len();
				for (int32 ChunkDbFileIdx = 0; ChunkDbFileIdx < ChunkDbFiles.Num(); ++ChunkDbFileIdx)
				{
					ChunkDbFiles[ChunkDbFileIdx].DatabaseFilename = FString::Printf(*FilenameFmt, NumDigitsForParts, ChunkDbFileIdx + 1);
				}
			}
			else
			{
				// Should have figured out no data already.
				check(ChunkDbFiles.Num() == 1);
				ChunkDbFiles[0].DatabaseFilename = OutputFile;
				if (!ChunkDbFiles[0].DatabaseFilename.EndsWith(ChunkDbExtension))
				{
					ChunkDbFiles[0].DatabaseFilename += ChunkDbExtension;
				}
			}

			// Cloud config.
			FCloudSourceConfig CloudSourceConfig({CloudDir});
			CloudSourceConfig.bBeginDownloadsOnFirstGet = false;
			CloudSourceConfig.MaxRetryCount = 30;

			// Create systems.
			FBuildPatchProgress BuildProgress;
			TUniquePtr<IHttpManager> HttpManager(FHttpManagerFactory::Create());
			TUniquePtr<IFileSystem> FileSystem(FFileSystemFactory::Create());
			TUniquePtr<IPlatform> Platform(FPlatformFactory::Create());
			TUniquePtr<IMessagePump> MessagePump(FMessagePumpFactory::Create());
			TUniquePtr<IInstallerError> InstallerError(FInstallerErrorFactory::Create());
			TUniquePtr<IInstallerAnalytics> InstallerAnalytics(FInstallerAnalyticsFactory::Create(nullptr, nullptr));
			TUniquePtr<IInstallerStatistics> InstallerStatistics(FInstallerStatisticsFactory::Create(InstallerAnalytics.Get(), &BuildProgress));
			TUniquePtr<IChunkDataSerialization> ChunkDataSerialization(FChunkDataSerializationFactory::Create(
				FileSystem.Get()));
			TUniquePtr<IChunkEvictionPolicy> MemoryEvictionPolicy(FChunkEvictionPolicyFactory::Create(
				ChunkReferenceTracker.Get()));
			TUniquePtr<IMemoryChunkStore> CloudChunkStore(FMemoryChunkStoreFactory::Create(
				512,
				MemoryEvictionPolicy.Get(),
				nullptr,
				InstallerStatistics->GetMemoryChunkStoreStat(EMemoryChunkStore::CloudSource)));
			TUniquePtr<IDownloadService> DownloadService(FDownloadServiceFactory::Create(
				FTicker::GetCoreTicker(),
				HttpManager.Get(),
				FileSystem.Get(),
				InstallerStatistics->GetDownloadServiceStat(),
				InstallerAnalytics.Get()));
			TUniquePtr<ICloudChunkSource> CloudChunkSource(FCloudChunkSourceFactory::Create(
				CloudSourceConfig,
				Platform.Get(),
				CloudChunkStore.Get(),
				DownloadService.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				MessagePump.Get(),
				InstallerError.Get(),
				InstallerStatistics->GetCloudChunkSourceStat(),
				Manifest,
				FullDataSet));

			// Start an IO output thread which saves all the chunks to the chunkdbs.
			TUniquePtr<IChunkDatabaseWriter> ChunkDatabaseWriter(FChunkDatabaseWriterFactory::Create(
				CloudChunkSource.Get(),
				FileSystem.Get(),
				InstallerError.Get(),
				ChunkReferenceTracker.Get(),
				ChunkDataSerialization.Get(),
				ChunkDbFiles,
				[](bool){ GIsRequestingExit = true; }));

			// Main loop.
			double DeltaTime = 0.0;
			double LastTime = FPlatformTime::Seconds();

			// Setup desired frame times.
			float MainsFramerate = 30.0f;
			const float MainsFrameTime = 1.0f / MainsFramerate;

			// Run a main tick loop, exiting when complete.
			while (!GIsRequestingExit)
			{
				// Increment global frame counter once for each app tick.
				GFrameCounter++;

				// Update sub-systems.
				FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
				FTicker::GetCoreTicker().Tick(DeltaTime);

				// Flush threaded logs.
				GLog->FlushThreadedLogs();

				// Throttle frame rate.
				FPlatformProcess::Sleep(FMath::Max<float>(0.0f, MainsFrameTime - (FPlatformTime::Seconds() - LastTime)));

				// Calculate deltas.
				const double AppTime = FPlatformTime::Seconds();
				DeltaTime = AppTime - LastTime;
				LastTime = AppTime;
			}

			// Do any success state checks?
			bSuccess = !InstallerError->HasError();
			if (!bSuccess)
			{
				UE_LOG(LogPackageChunkData, Error, TEXT("%s: %s"), *InstallerError->GetErrorCode(), *InstallerError->GetErrorText().BuildSourceString());
			}
		}
		else
		{
			UE_LOG(LogPackageChunkData, Error, TEXT("Manifest has no data"));
		}
	}
	else
	{
		UE_LOG(LogPackageChunkData, Error, TEXT("Failed to load manifest %s"), *ManifestFilePath);
	}
	return bSuccess;
}
