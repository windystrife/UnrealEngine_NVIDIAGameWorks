// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Serialization/MemoryReader.h"
#include "Misc/SecureHash.h"
#include "IMessageContext.h"
#include "ProfilerServiceMessages.h"
#include "IProfilerServiceManager.h"
#include "ProfilerServiceManager.h"


FFileTransferRunnable::FFileTransferRunnable(TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe>& InMessageEndpoint)
	: Runnable(nullptr)
	, WorkEvent(FPlatformProcess::GetSynchEventFromPool(true))
	, MessageEndpoint(InMessageEndpoint)
	, StopTaskCounter(0)
{
	Runnable = FRunnableThread::Create(this, TEXT("FFileTransferRunnable"), 128 * 1024, TPri_BelowNormal);
}


FFileTransferRunnable::~FFileTransferRunnable()
{
	if (Runnable != nullptr)
	{
		Stop();
		Runnable->WaitForCompletion();
		delete Runnable;
		Runnable = nullptr;
	}

	// Delete all active file readers.
	for (auto It = ActiveTransfers.CreateIterator(); It; ++It)
	{
		FReaderAndAddress& ReaderAndAddress = It.Value();
		DeleteFileReader(ReaderAndAddress);

		UE_LOG(LogProfilerService, Log, TEXT("File service-client sending aborted (srv): %s"), *It.Key());
	}

	FPlatformProcess::ReturnSynchEventToPool(WorkEvent);
	WorkEvent = nullptr;
}


bool FFileTransferRunnable::Init()
{
	return true;
}


uint32 FFileTransferRunnable::Run()
{
	while (!ShouldStop())
	{
		if (WorkEvent->Wait(250))
		{
			FProfilerServiceFileChunk* FileChunk;
			while (!ShouldStop() && SendQueue.Dequeue(FileChunk))
			{
				FMemoryReader MemoryReader(FileChunk->Header);
				FProfilerFileChunkHeader FileChunkHeader;
				MemoryReader << FileChunkHeader;
				FileChunkHeader.Validate();

				if (FileChunkHeader.ChunkType == EProfilerFileChunkType::SendChunk)
				{
					// Find the corresponding file archive reader and recipient.
					FArchive* ReaderArchive = nullptr;
					FMessageAddress Recipient;
					{
						FScopeLock Lock(&SyncActiveTransfers);
						const FReaderAndAddress* ReaderAndAddress = ActiveTransfers.Find(FileChunk->Filename);
						if (ReaderAndAddress)
						{
							ReaderArchive = ReaderAndAddress->Key;
							Recipient = ReaderAndAddress->Value;
						}
					}

					// If there is no reader and recipient is invalid, it means that the file transfer is no longer valid, because client disconnected or exited.
					if (ReaderArchive && Recipient.IsValid())
					{
						ReadAndSetHash(FileChunk, FileChunkHeader, ReaderArchive);

						if (MessageEndpoint.IsValid())
						{
							MessageEndpoint->Send(FileChunk, Recipient);
						}
					}
				}
				else if (FileChunkHeader.ChunkType == EProfilerFileChunkType::PrepareFile)
				{
					PrepareFileForSending(FileChunk);
				}
			}

			WorkEvent->Reset();
		}
	}

	return 0;
}


void FFileTransferRunnable::Exit()
{

}


void FFileTransferRunnable::EnqueueFileToSend(const FString& StatFilename, const FMessageAddress& RecipientAddress, const FGuid& ServiceInstanceId)
{
	UE_LOG(LogProfilerService, Log, TEXT("Opening stats file for service-client sending: %s"), *StatFilename);

	const int64 FileSize = IFileManager::Get().FileSize(*StatFilename);
	if (FileSize < 4)
	{
		UE_LOG(LogProfilerService, Error, TEXT("Could not open: %s"), *StatFilename);
		return;
	}

	FArchive* FileReader = IFileManager::Get().CreateFileReader(*StatFilename);
	if (!FileReader)
	{
		UE_LOG(LogProfilerService, Error, TEXT("Could not open: %s"), *StatFilename);
		return;
	}

	{
		FScopeLock Lock(&SyncActiveTransfers);
		check(!ActiveTransfers.Contains(StatFilename));
		ActiveTransfers.Add(StatFilename, FReaderAndAddress(FileReader, RecipientAddress));
	}

	// This is not a real file chunk, but helper to prepare file for sending on the runnable.
	EnqueueFileChunkToSend(new FProfilerServiceFileChunk(ServiceInstanceId, StatFilename, FProfilerFileChunkHeader(0, 0, FileReader->TotalSize(), EProfilerFileChunkType::PrepareFile).AsArray()), true);
}


void FFileTransferRunnable::EnqueueFileChunkToSend(FProfilerServiceFileChunk* FileChunk, bool bTriggerWorkEvent /*= false */)
{
	SendQueue.Enqueue(FileChunk);

	if (bTriggerWorkEvent)
	{
		// Trigger the runnable.
		WorkEvent->Trigger();
	}
}


void FFileTransferRunnable::PrepareFileForSending(FProfilerServiceFileChunk*& FileChunk)
{
	// Find the corresponding file archive and recipient.
	FArchive* Reader = nullptr;
	FMessageAddress Recipient;
	{
		FScopeLock Lock(&SyncActiveTransfers);
		const FReaderAndAddress& ReaderAndAddress = ActiveTransfers.FindChecked(FileChunk->Filename);
		Reader = ReaderAndAddress.Key;
		Recipient = ReaderAndAddress.Value;
	}

	int64 ChunkOffset = 0;
	int64 RemainingSizeToSend = Reader->TotalSize();

	while (RemainingSizeToSend > 0)
	{
		const int64 SizeToCopy = FMath::Min(FProfilerFileChunkHeader::DefChunkSize, RemainingSizeToSend);

		EnqueueFileChunkToSend(new FProfilerServiceFileChunk(FileChunk->InstanceId, FileChunk->Filename, FProfilerFileChunkHeader(ChunkOffset, SizeToCopy, Reader->TotalSize(), EProfilerFileChunkType::SendChunk).AsArray()));

		ChunkOffset += SizeToCopy;
		RemainingSizeToSend -= SizeToCopy;
	}

	// Trigger the runnable.
	WorkEvent->Trigger();

	// Delete this file chunk.
	delete FileChunk;
	FileChunk = nullptr;
}


void FFileTransferRunnable::FinalizeFileSending(const FString& Filename)
{
	FScopeLock Lock(&SyncActiveTransfers);

	check(ActiveTransfers.Contains(Filename));
	FReaderAndAddress ReaderAndAddress = ActiveTransfers.FindAndRemoveChecked(Filename);
	DeleteFileReader(ReaderAndAddress);

	UE_LOG(LogProfilerService, Log, TEXT("File service-client sent successfully : %s"), *Filename);
}


void FFileTransferRunnable::AbortFileSending(const FMessageAddress& Recipient)
{
	FScopeLock Lock(&SyncActiveTransfers);

	for (auto It = ActiveTransfers.CreateIterator(); It; ++It)
	{
		FReaderAndAddress& ReaderAndAddress = It.Value();
		if (ReaderAndAddress.Value == Recipient)
		{
			UE_LOG(LogProfilerService, Log, TEXT("File service-client sending aborted (cl): %s"), *It.Key());
			FReaderAndAddress ActiveReaderAndAddress = ActiveTransfers.FindAndRemoveChecked(It.Key());
			DeleteFileReader(ActiveReaderAndAddress);
		}
	}
}


void FFileTransferRunnable::DeleteFileReader(FReaderAndAddress& ReaderAndAddress)
{
	delete ReaderAndAddress.Key;
	ReaderAndAddress.Key = nullptr;
}


void FFileTransferRunnable::ReadAndSetHash(FProfilerServiceFileChunk* FileChunk, const FProfilerFileChunkHeader& FileChunkHeader, FArchive* Reader)
{
	TArray<uint8> FileChunkData;

	FileChunkData.AddUninitialized(FileChunkHeader.ChunkSize);

	Reader->Seek(FileChunkHeader.ChunkOffset);
	Reader->Serialize(FileChunkData.GetData(), FileChunkHeader.ChunkSize);

	const int32 HashSize = 20;
	uint8 LocalHash[HashSize] = {0};

	// Hash file chunk data. 
	FSHA1 Sha;
	Sha.Update(FileChunkData.GetData(), FileChunkHeader.ChunkSize);
	// Hash file chunk header.
	Sha.Update(FileChunk->Header.GetData(), FileChunk->Header.Num());
	Sha.Final();
	Sha.GetHash(LocalHash);

	FileChunk->ChunkHash.AddUninitialized(HashSize);
	FMemory::Memcpy(FileChunk->ChunkHash.GetData(), LocalHash, HashSize);

	// Limit transfer per second, otherwise we will probably hang the message bus.
	static int64 TotalReadBytes = 0;
#ifdef	_DEBUG
	static const int64 NumBytesPerTick = 128 * 1024;
#else
	static const int64 NumBytesPerTick = 256 * 1024;
#endif // _DEBUG

	// Convert to hex.
	FileChunk->HexData = FString::FromHexBlob(FileChunkData.GetData(), FileChunkData.Num());

	TotalReadBytes += FileChunkHeader.ChunkSize;
	if (TotalReadBytes > NumBytesPerTick)
	{
		FPlatformProcess::Sleep(0.1f);
		TotalReadBytes = 0;
	}
}
