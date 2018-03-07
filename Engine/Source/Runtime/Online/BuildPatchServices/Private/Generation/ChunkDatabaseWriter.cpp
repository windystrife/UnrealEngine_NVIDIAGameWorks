// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Generation/ChunkDatabaseWriter.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Async/Async.h"
#include "Algo/Transform.h"
#include "Serialization/MemoryWriter.h"
#include "Core/AsyncHelpers.h"
#include "Data/ChunkData.h"
#include "Common/FileSystem.h"
#include "Installer/InstallerError.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkSource.h"
#include "Interfaces/IBuildInstaller.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkDatabaseWriter, Log, All);
DEFINE_LOG_CATEGORY(LogChunkDatabaseWriter);

namespace BuildPatchServices
{
	// Using initial data buffer of 2MB.
	static const int32 DataMessageBufferSize = 1024 * 1024 * 2;

	struct FDataMessage
	{
	public:
		FDataMessage(FString InFilename)
			: Filename(InFilename)
			, Pos(INDEX_NONE)
		{
		}

		FDataMessage(int64 InPos)
			: Filename()
			, Pos(InPos)
		{
			Memory.Reset(DataMessageBufferSize);
		}

	public:
		FString Filename;
		int64 Pos;
		TArray<uint8> Memory;

	private:
		FDataMessage(){}
	};

	class FChunkDatabaseWriter
		: public IChunkDatabaseWriter
	{
	public:
		FChunkDatabaseWriter(IChunkSource* ChunkSource, IFileSystem* FileSystem, IInstallerError* InstallerError, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, TArray<FChunkDatabaseFile> ChunkDatabaseList, TFunction<void(bool)> OnComplete);

		~FChunkDatabaseWriter();

	private:
		void ProcessingWorkerThread();
		void OutputWorkerThread();

	private:
		IChunkSource* ChunkSource;
		IFileSystem* FileSystem;
		IInstallerError* InstallerError;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IChunkDataSerialization* ChunkDataSerialization;
		TArray<FChunkDatabaseFile> ChunkDatabaseList;
		TFunction<void(bool)> OnComplete;
		TFuture<void> ProcessingWorkerFuture;
		TFuture<void> OutputWorkerFuture;
		FThreadSafeBool bShouldCancel;
		FThreadSafeBool bProcessingComplete;
		TQueue<TSharedPtr<FDataMessage, ESPMode::ThreadSafe>, EQueueMode::Spsc> DataPipe;
		FEvent* ThreadTrigger;
	};

	FChunkDatabaseWriter::FChunkDatabaseWriter(IChunkSource* InChunkSource, IFileSystem* InFileSystem, IInstallerError* InInstallerError, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, TArray<FChunkDatabaseFile> InChunkDatabaseList, TFunction<void(bool)> InOnComplete)
		: ChunkSource(InChunkSource)
		, FileSystem(InFileSystem)
		, InstallerError(InInstallerError)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, ChunkDataSerialization(InChunkDataSerialization)
		, ChunkDatabaseList(MoveTemp(InChunkDatabaseList))
		, OnComplete(MoveTemp(InOnComplete))
		, bShouldCancel(false)
		, bProcessingComplete(false)
	{
		ThreadTrigger = FPlatformProcess::GetSynchEventFromPool(true);
		ProcessingWorkerFuture = Async<void>(EAsyncExecution::Thread, [this]()
		{
			ProcessingWorkerThread();
		});
		OutputWorkerFuture = Async<void>(EAsyncExecution::Thread, [this]()
		{
			OutputWorkerThread();
		});
	}

	FChunkDatabaseWriter::~FChunkDatabaseWriter()
	{
		bShouldCancel = true;
		ProcessingWorkerFuture.Wait();
		OutputWorkerFuture.Wait();
		FPlatformProcess::ReturnSynchEventToPool(ThreadTrigger);
	}

	void FChunkDatabaseWriter::ProcessingWorkerThread()
	{
		bool bSuccess = true;

		// For every entry in the provided ChunkDatabaseList, create the chunkdb, and send serialized data to the output thread for it.
		for (int32 ChunkDatabaseIdx = 0; ChunkDatabaseIdx < ChunkDatabaseList.Num() && bSuccess && !bShouldCancel && !InstallerError->HasError(); ++ChunkDatabaseIdx)
		{
			const FChunkDatabaseFile& ChunkDatabaseFile = ChunkDatabaseList[ChunkDatabaseIdx];
			UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Start processing chunk database %s"), *ChunkDatabaseFile.DatabaseFilename);

			// Send file create message.
			DataPipe.Enqueue(MakeShareable(new FDataMessage(ChunkDatabaseFile.DatabaseFilename)));
			ThreadTrigger->Trigger();

			// Populate header with all required entries.
			FChunkDatabaseHeader ChunkDbHeader;
			Algo::Transform(ChunkDatabaseFile.DataList, ChunkDbHeader.Contents, [](const FGuid& DataId)
			{
				return FChunkLocation{DataId, 0, 0};
			});
			int64 FileDataPos = 0;

			// Write the header.
			TUniquePtr<FDataMessage> DataMessage(new FDataMessage(FileDataPos));
			TUniquePtr<FMemoryWriter> MemoryWriter(new FMemoryWriter(DataMessage->Memory));
			*MemoryWriter << ChunkDbHeader;
			FileDataPos += DataMessage->Memory.Num();
			DataPipe.Enqueue(MakeShareable(DataMessage.Release()));
			ThreadTrigger->Trigger();

			// Serialize and write each of the chunk files.
			for (int32 ChunkDataIdx = 0; ChunkDataIdx < ChunkDatabaseFile.DataList.Num() && bSuccess && !bShouldCancel && !InstallerError->HasError(); ChunkDataIdx++)
			{
				const FGuid& ChunkDataId = ChunkDatabaseFile.DataList[ChunkDataIdx];
				IChunkDataAccess* ChunkDataAccess = ChunkSource->Get(ChunkDataId);
				bSuccess = ChunkDataAccess != nullptr;
				if (bSuccess)
				{
					// Prepare new message.
					DataMessage.Reset(new FDataMessage(FileDataPos));
					MemoryWriter.Reset(new FMemoryWriter(DataMessage->Memory));

					// Write to message.
					EChunkSaveResult SaveResult = ChunkDataSerialization->SaveToArchive(*MemoryWriter, ChunkDataAccess);
					bSuccess = SaveResult == EChunkSaveResult::Success;
					if (!bSuccess)
					{
						const TCHAR* ErrorCode = SaveResult == EChunkSaveResult::FileCreateFail ? ConstructionErrorCodes::FileCreateFail
						                       : SaveResult == EChunkSaveResult::SerializationError ? ConstructionErrorCodes::SerializationError
						                       : ConstructionErrorCodes::UnknownFail;
						InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ErrorCode);
					}

					// Set the positional data in the header.
					FChunkLocation& Location = ChunkDbHeader.Contents[ChunkDataIdx];
					Location.ByteStart = FileDataPos;
					Location.ByteSize = DataMessage->Memory.Num();

					// Advance file position.
					FileDataPos += Location.ByteSize;

					// Send the data message.
					DataPipe.Enqueue(MakeShareable(DataMessage.Release()));
					ThreadTrigger->Trigger();

					// Pop the chunk we just saved out.
					bSuccess = ChunkReferenceTracker->PopReference(ChunkDataId);
					if (!bSuccess)
					{
						InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::ChunkReferenceTracking);
					}
				}
				if (!bSuccess)
				{
					UE_LOG(LogChunkDatabaseWriter, Log, TEXT("    Failed chunk %s"), *ChunkDataId.ToString());
				}
			}
			if (bSuccess)
			{
				// Write back the header with all chunk positions now filled out accurately.
				ChunkDbHeader.DataSize = FileDataPos - ChunkDbHeader.HeaderSize;
				DataMessage.Reset(new FDataMessage(0));
				MemoryWriter.Reset(new FMemoryWriter(DataMessage->Memory));
				*MemoryWriter << ChunkDbHeader;
				DataPipe.Enqueue(MakeShareable(DataMessage.Release()));
				ThreadTrigger->Trigger();
			}
		}

		// Mark completed.
		bProcessingComplete = true;
		ThreadTrigger->Trigger();
		UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Processer complete! bSuccess:%d"), bSuccess);
	}

	void FChunkDatabaseWriter::OutputWorkerThread()
	{
		bool bSuccess = true;

		TArray<FString> FilesCreated;
		TSharedPtr<FDataMessage, ESPMode::ThreadSafe> DataMessage;
		TUniquePtr<FArchive> CurrentFile;
		while (bSuccess && !bShouldCancel && !InstallerError->HasError())
		{
			// See if we have a message.
			if (DataPipe.Dequeue(DataMessage))
			{
				// Process a file create message.
				if (DataMessage->Pos == INDEX_NONE)
				{
					UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Writing chunk database %s"), *DataMessage->Filename);
					CurrentFile = FileSystem->CreateFileWriter(*DataMessage->Filename);
					FilesCreated.Add(DataMessage->Filename);
					bSuccess = CurrentFile.IsValid();
				}
				// Process a data serialize.
				else if (CurrentFile.IsValid())
				{
					if (CurrentFile->Tell() != DataMessage->Pos)
					{
						CurrentFile->Seek(DataMessage->Pos);
					}
					CurrentFile->Serialize(DataMessage->Memory.GetData(), DataMessage->Memory.Num());
				}
				// An error if we do not have a file open and we were sent data.
				else
				{
					bSuccess = false;
					UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Output fail, data message without a file"));
					InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
				}
			}
			// Quit if no more messages
			else if (bProcessingComplete)
			{
				break;
			}
			// Wait up to 1 second for an enqueue trigger.
			else
			{
				ThreadTrigger->Wait(1000);
				ThreadTrigger->Reset();
			}
		}

		// Close the last open file.
		CurrentFile.Reset();

		// Check whether the process was canceled or an error occurred.
		bSuccess = bSuccess && !bShouldCancel && !InstallerError->HasError();
		UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Writer complete! bSuccess:%d"), bSuccess);

		// Delete any created files if we failed.
		if (!bSuccess)
		{
			for (const FString& FileToDelete : FilesCreated)
			{
				FileSystem->DeleteFile(*FileToDelete);
			}
		}

		// We're done so call the complete callback.
		AsyncHelpers::ExecuteOnGameThread(OnComplete, bSuccess).Wait();
	}

	IChunkDatabaseWriter* FChunkDatabaseWriterFactory::Create(IChunkSource* ChunkSource, IFileSystem* FileSystem, IInstallerError* InstallerError, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, TArray<FChunkDatabaseFile> ChunkDatabaseList, TFunction<void(bool)> OnComplete)
	{
		check(ChunkSource != nullptr);
		check(FileSystem != nullptr);
		check(InstallerError != nullptr);
		check(ChunkReferenceTracker != nullptr);
		return new FChunkDatabaseWriter(ChunkSource, FileSystem, InstallerError, ChunkReferenceTracker, ChunkDataSerialization, MoveTemp(ChunkDatabaseList), MoveTemp(OnComplete));
	}
}
