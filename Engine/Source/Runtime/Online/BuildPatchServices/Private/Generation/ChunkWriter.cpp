// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Generation/ChunkWriter.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Data/ChunkData.h"
#include "BuildPatchUtil.h"

namespace BuildPatchServices
{
	static const int32 ChunkQueueSize = 50;

	/* FQueuedChunkWriter implementation
	*****************************************************************************/
	DECLARE_LOG_CATEGORY_EXTERN(LogChunkWriter, Log, All);
	DEFINE_LOG_CATEGORY(LogChunkWriter);
	FChunkWriter::FQueuedChunkWriter::FQueuedChunkWriter()
	{
		bMoreChunks = true;
	}

	FChunkWriter::FQueuedChunkWriter::~FQueuedChunkWriter()
	{
		for (auto QueuedIt = ChunkFileQueue.CreateConstIterator(); QueuedIt; ++QueuedIt)
		{
			IChunkDataAccess* ChunkFile = *QueuedIt;
			delete ChunkFile;
		}
		ChunkFileQueue.Empty();
	}

	bool FChunkWriter::FQueuedChunkWriter::Init()
	{
		IFileManager::Get().MakeDirectory(*ChunkDirectory, true);
		return IFileManager::Get().DirectoryExists(*ChunkDirectory);
	}

	uint32 FChunkWriter::FQueuedChunkWriter::Run()
	{
		StatFileCreateTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Create Time"), EStatFormat::Timer);
		StatCheckExistsTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Check Exist Time"), EStatFormat::Timer);
		StatCompressTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Compress Time"), EStatFormat::Timer);
		StatSerlialiseTime = StatsCollector->CreateStat(TEXT("Chunk Writer: Serialize Time"), EStatFormat::Timer);
		StatChunksSaved = StatsCollector->CreateStat(TEXT("Chunk Writer: Num Saved"), EStatFormat::Value);
		StatDataWritten = StatsCollector->CreateStat(TEXT("Chunk Writer: Data Size Written"), EStatFormat::DataSize);
		StatDataWriteSpeed = StatsCollector->CreateStat(TEXT("Chunk Writer: Data Write Speed"), EStatFormat::DataSpeed);
		StatCompressionRatio = StatsCollector->CreateStat(TEXT("Chunk Writer: Compression Ratio"), EStatFormat::Percentage);

		// Loop until there's no more chunks
		while (ShouldBeRunning())
		{
			TUniquePtr<IChunkDataAccess> ChunkFile(GetNextChunk());
			if (ChunkFile.IsValid())
			{
				FScopeLockedChunkData LockedChunkData(ChunkFile.Get());
				const FGuid& ChunkGuid = LockedChunkData.GetHeader()->Guid;
				const uint64& ChunkHash = LockedChunkData.GetHeader()->RollingHash;
				const FString NewChunkFilename = FBuildPatchUtils::GetChunkNewFilename(EBuildPatchAppManifestVersion::GetLatestVersion(), ChunkDirectory, ChunkGuid, ChunkHash);

				// To be a bit safer, make a few attempts at writing chunks
				int32 RetryCount = 5;
				bool bChunkSaveSuccess = false;
				while (RetryCount > 0)
				{
					// Write out chunks
					bChunkSaveSuccess = WriteChunkData(NewChunkFilename, LockedChunkData, ChunkGuid);

					// Check success
					if (bChunkSaveSuccess)
					{
						RetryCount = 0;
					}
					else
					{
						// Retry after a second if failed
						--RetryCount;
						FPlatformProcess::Sleep(1.0f);
					}
				}

				// If we really could not save out chunk data successfully, this build will never work, so panic flush logs and then cause a hard error.
				if (!bChunkSaveSuccess)
				{
					UE_LOG(LogChunkWriter, Error, TEXT("Could not save out new chunk file %s"), *NewChunkFilename);
					GLog->PanicFlushThreadedLogs();
					check(bChunkSaveSuccess);
				}

				// Small sleep for next chunk
				FPlatformProcess::Sleep(0.0f);
			}
			else
			{
				// Larger sleep for no work
				FPlatformProcess::Sleep(0.1f);
			}
			double TotalTime = FStatsCollector::CyclesToSeconds(*StatFileCreateTime + *StatSerlialiseTime);
			if (TotalTime > 0.0)
			{
				FStatsCollector::Set(StatDataWriteSpeed, *StatDataWritten / TotalTime);
			}
		}
		return 0;
	}

	const bool FChunkWriter::FQueuedChunkWriter::WriteChunkData(const FString& ChunkFilename, FScopeLockedChunkData& LockedChunk, const FGuid& ChunkGuid)
	{
		uint64 TempTimer;
		// Chunks are saved with GUID, so if a file already exists it will never be different.
		// Skip with return true if already exists
		FStatsCollector::AccumulateTimeBegin(TempTimer);
		const int64 ChunkFileSize = IFileManager::Get().FileSize(*ChunkFilename);
		FStatsCollector::AccumulateTimeEnd(StatCheckExistsTime, TempTimer);
		if (ChunkFileSize > 0)
		{
			ChunkFileSizesCS.Lock();
			ChunkFileSizes.Add(ChunkGuid, ChunkFileSize);
			ChunkFileSizesCS.Unlock();
			UE_LOG(LogChunkWriter, Verbose, TEXT("Existing chunk file %s. Size:%lld."), *ChunkGuid.ToString(), ChunkFileSize);
			return true;
		}
		FStatsCollector::AccumulateTimeBegin(TempTimer);
		FArchive* FileOut = IFileManager::Get().CreateFileWriter(*ChunkFilename);
		FStatsCollector::AccumulateTimeEnd(StatFileCreateTime, TempTimer);
		bool bSuccess = FileOut != NULL;
		if (bSuccess)
		{
			// Setup to handle compression
			bool bDataIsCompressed = true;
			uint8* ChunkDataSource = LockedChunk.GetData();
			int32 ChunkDataSourceSize = BuildPatchServices::ChunkDataSize;
			TArray< uint8 > TempCompressedData;
			TempCompressedData.Empty(ChunkDataSourceSize);
			TempCompressedData.AddUninitialized(ChunkDataSourceSize);
			int32 CompressedSize = ChunkDataSourceSize;

			// Compressed can increase in size, but the function will return as failure in that case
			// we can allow that to happen since we would not keep larger compressed data anyway.
			FStatsCollector::AccumulateTimeBegin(TempTimer);
			bDataIsCompressed = FCompression::CompressMemory(
				static_cast<ECompressionFlags>(COMPRESS_ZLIB | COMPRESS_BiasMemory),
				TempCompressedData.GetData(),
				CompressedSize,
				LockedChunk.GetData(),
				BuildPatchServices::ChunkDataSize);
			FStatsCollector::AccumulateTimeEnd(StatCompressTime, TempTimer);

			// If compression succeeded, set data vars
			if (bDataIsCompressed)
			{
				ChunkDataSource = TempCompressedData.GetData();
				ChunkDataSourceSize = CompressedSize;
			}

			// Setup Header
			FStatsCollector::AccumulateTimeBegin(TempTimer);
			FChunkHeader& Header = *LockedChunk.GetHeader();
			*FileOut << Header;
			Header.HeaderSize = FileOut->Tell();
			Header.StoredAs = bDataIsCompressed ? EChunkStorageFlags::Compressed : EChunkStorageFlags::None;
			Header.DataSize = ChunkDataSourceSize;
			Header.HashType = EChunkHashFlags::RollingPoly64;

			// Write out files
			FileOut->Seek(0);
			*FileOut << Header;
			FileOut->Serialize(ChunkDataSource, ChunkDataSourceSize);
			const int64 NewChunkFileSize = FileOut->TotalSize();
			FileOut->Close();
			FStatsCollector::AccumulateTimeEnd(StatSerlialiseTime, TempTimer);
			FStatsCollector::Accumulate(StatChunksSaved, 1);
			FStatsCollector::Accumulate(StatDataWritten, NewChunkFileSize);
			FStatsCollector::SetAsPercentage(StatCompressionRatio, *StatDataWritten / double(*StatChunksSaved * (Header.HeaderSize + BuildPatchServices::ChunkDataSize)));

			ChunkFileSizesCS.Lock();
			ChunkFileSizes.Add(ChunkGuid, NewChunkFileSize);
			ChunkFileSizesCS.Unlock();

			bSuccess = !FileOut->GetError();

			if (bSuccess)
			{
				UE_LOG(LogChunkWriter, Verbose, TEXT("New chunk file saved %s. Compressed:%d, Size:%lld."), *ChunkGuid.ToString(), bDataIsCompressed, NewChunkFileSize);
			}

			delete FileOut;
		}
		// Log errors
		if (!bSuccess)
		{
			UE_LOG(LogChunkWriter, Warning, TEXT("Attempt to save chunk file %s was not successful."), *ChunkFilename);
		}
		return bSuccess;
	}

	const bool FChunkWriter::FQueuedChunkWriter::ShouldBeRunning()
	{
		bool rtn;
		MoreChunksCS.Lock();
		rtn = bMoreChunks;
		MoreChunksCS.Unlock();
		rtn = rtn || HasQueuedChunk();
		return rtn;
	}

	const bool FChunkWriter::FQueuedChunkWriter::HasQueuedChunk()
	{
		bool rtn;
		ChunkFileQueueCS.Lock();
		rtn = ChunkFileQueue.Num() > 0;
		ChunkFileQueueCS.Unlock();
		return rtn;
	}

	const bool FChunkWriter::FQueuedChunkWriter::CanQueueChunk()
	{
		bool rtn;
		ChunkFileQueueCS.Lock();
		rtn = ChunkFileQueue.Num() < BuildPatchServices::ChunkQueueSize;
		ChunkFileQueueCS.Unlock();
		return rtn;
	}

	IChunkDataAccess* FChunkWriter::FQueuedChunkWriter::GetNextChunk()
	{
		IChunkDataAccess* rtn = NULL;
		ChunkFileQueueCS.Lock();
		if (ChunkFileQueue.Num() > 0)
		{
			rtn = ChunkFileQueue.Pop();
		}
		ChunkFileQueueCS.Unlock();
		return rtn;
	}

	void FChunkWriter::FQueuedChunkWriter::QueueChunk(const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash)
	{
		// Create the IChunkDataAccess and copy data
		IChunkDataAccess* NewChunk = FChunkDataAccessFactory::Create(BuildPatchServices::ChunkDataSize);
		FScopeLockedChunkData LockedChunkData(NewChunk);
		LockedChunkData.GetHeader()->Guid = ChunkGuid;
		LockedChunkData.GetHeader()->RollingHash = ChunkHash;
		FMemory::Memcpy(LockedChunkData.GetData(), ChunkData, BuildPatchServices::ChunkDataSize);
		// Wait until we can fit this chunk in the queue
		while (!CanQueueChunk())
		{
			FPlatformProcess::Sleep(0.01f);
		}
		// Queue the chunk
		ChunkFileQueueCS.Lock();
		ChunkFileQueue.Add(NewChunk);
		ChunkFileQueueCS.Unlock();
	}

	void FChunkWriter::FQueuedChunkWriter::SetNoMoreChunks()
	{
		MoreChunksCS.Lock();
		bMoreChunks = false;
		MoreChunksCS.Unlock();
	}

	void FChunkWriter::FQueuedChunkWriter::GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes)
	{
		FScopeLock ScopeLock(&ChunkFileSizesCS);
		OutChunkFileSizes.Append(ChunkFileSizes);
	}

	/* FChunkWriter implementation
	*****************************************************************************/
	FChunkWriter::FChunkWriter(const FString& ChunkDirectory, FStatsCollectorRef StatsCollector)
	{
		QueuedChunkWriter.ChunkDirectory = ChunkDirectory;
		QueuedChunkWriter.StatsCollector = StatsCollector;
		WriterThread = FRunnableThread::Create(&QueuedChunkWriter, TEXT("QueuedChunkWriterThread"));
	}

	FChunkWriter::~FChunkWriter()
	{
		NoMoreChunks();
		WaitForThread();
		if (WriterThread != NULL)
		{
			delete WriterThread;
			WriterThread = NULL;
		}
	}

	void FChunkWriter::QueueChunk(const uint8* ChunkData, const FGuid& ChunkGuid, const uint64& ChunkHash)
	{
		QueuedChunkWriter.QueueChunk(ChunkData, ChunkGuid, ChunkHash);
	}

	void FChunkWriter::NoMoreChunks()
	{
		QueuedChunkWriter.SetNoMoreChunks();
	}

	void FChunkWriter::WaitForThread()
	{
		if (WriterThread != NULL)
		{
			WriterThread->WaitForCompletion();
		}
	}

	void FChunkWriter::GetChunkFilesizes(TMap<FGuid, int64>& OutChunkFileSizes)
	{
		QueuedChunkWriter.GetChunkFilesizes(OutChunkFileSizes);
	}
}
