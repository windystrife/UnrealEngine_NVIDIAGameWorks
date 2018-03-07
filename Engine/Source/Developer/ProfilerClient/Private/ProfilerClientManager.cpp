// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerClientManager.h"
#include "HAL/FileManager.h"
#include "MessageEndpointBuilder.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/MemoryReader.h"
#include "ProfilerServiceMessages.h"


DEFINE_LOG_CATEGORY_STATIC(LogProfilerClient, Log, All);

DECLARE_CYCLE_STAT(TEXT("HandleDataReceived"),	STAT_PC_HandleDataReceived,			STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("ReadStatMessages"),	STAT_PC_ReadStatMessages,			STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("AddStatMessages"),	STAT_PC_AddStatMessages,			STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("GenerateDataFrame"),	STAT_PC_GenerateDataFrame,			STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("AddStatFName"),		STAT_PC_AddStatFName,				STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("AddGroupFName"),		STAT_PC_AddGroupFName,				STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("GenerateCycleGraph"),	STAT_PC_GenerateCycleGraph,			STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("GenerateAccumulator"),STAT_PC_GenerateAccumulator,		STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("FindOrAddStat"),		STAT_PC_FindOrAddStat,				STATGROUP_Profiler);
DECLARE_CYCLE_STAT(TEXT("FindOrAddThread"),	STAT_PC_FindOrAddThread,			STATGROUP_Profiler);


/* FProfilerClientManager structors
 *****************************************************************************/

FProfilerClientManager::FProfilerClientManager(const TSharedRef<IMessageBus, ESPMode::ThreadSafe>& InMessageBus)
{
#if STATS
	MessageBus = InMessageBus;
	MessageEndpoint = FMessageEndpoint::Builder("FProfilerClientModule", InMessageBus)
		.Handling<FProfilerServiceAuthorize>(this, &FProfilerClientManager::HandleServiceAuthorizeMessage)
		.Handling<FProfilerServiceData2>(this, &FProfilerClientManager::HandleProfilerServiceData2Message)
		.Handling<FProfilerServicePreviewAck>(this, &FProfilerClientManager::HandleServicePreviewAckMessage)
		.Handling<FProfilerServiceFileChunk>(this, &FProfilerClientManager::HandleServiceFileChunk)  
		.Handling<FProfilerServicePing>(this, &FProfilerClientManager::HandleServicePingMessage);

	if (MessageEndpoint.IsValid())
	{
		OnShutdownMessageBusDelegateHandle = InMessageBus->OnShutdown().AddRaw(this, &FProfilerClientManager::HandleMessageBusShutdown);
		MessageEndpoint->Subscribe<FProfilerServicePing>();
	}

	TickDelegate = FTickerDelegate::CreateRaw(this, &FProfilerClientManager::HandleTicker);
	MessageDelegate = FTickerDelegate::CreateRaw(this, &FProfilerClientManager::HandleMessagesTicker);
	LastPingTime = FDateTime::Now();
	RetryTime = 5.f;

	LoadConnection = nullptr;
	MessageDelegateHandle = FTicker::GetCoreTicker().AddTicker(MessageDelegate, 0.1f);
#endif
}


FProfilerClientManager::~FProfilerClientManager()
{
#if STATS
	Shutdown();

	Unsubscribe();

	if (MessageBus.IsValid())
	{
		MessageBus->OnShutdown().Remove(OnShutdownMessageBusDelegateHandle);
	}

	LoadConnection = nullptr;
#endif
}


/* IProfilerClient interface
 *****************************************************************************/

void FProfilerClientManager::Subscribe(const FGuid& Session)
{
#if STATS
	FGuid OldSessionId = ActiveSessionId;
	PendingSessionId = Session;
	if (MessageEndpoint.IsValid())
	{
		if (OldSessionId.IsValid())
		{
			TArray<FGuid> Instances;
			Connections.GenerateKeyArray(Instances);
			for (int32 i = 0; i < Instances.Num(); ++i)
			{
				MessageEndpoint->Publish(new FProfilerServiceUnsubscribe(OldSessionId, Instances[i]), EMessageScope::Network);

				// fire the disconnection delegate
				ProfilerClientDisconnectedDelegate.Broadcast(ActiveSessionId, Instances[i]);
			}

			ActiveSessionId.Invalidate();
		}
		ActiveSessionId = PendingSessionId;
	}

	Connections.Reset();

	UE_LOG(LogProfilerClient, Log, TEXT("Subscribe Session: %s"), *Session.ToString());
#endif
}


void FProfilerClientManager::Track(const FGuid& Instance)
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid() && !PendingInstances.Contains(Instance))
	{
		PendingInstances.Add(Instance);

		MessageEndpoint->Publish(new FProfilerServiceSubscribe(ActiveSessionId, Instance), EMessageScope::Network);

		RetryTime = 5.f;
		TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, RetryTime);

		UE_LOG(LogProfilerClient, Verbose, TEXT("Track Session: %s, Instance: %s"), *ActiveSessionId.ToString(), *Instance.ToString());
	}
#endif
}


void FProfilerClientManager::Untrack(const FGuid& Instance)
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		MessageEndpoint->Publish(new FProfilerServiceUnsubscribe(ActiveSessionId, Instance), EMessageScope::Network);
		Connections.Remove(Instance);

		// fire the disconnection delegate
		ProfilerClientDisconnectedDelegate.Broadcast(ActiveSessionId, Instance);

		UE_LOG(LogProfilerClient, Verbose, TEXT("Untrack Session: %s, Instance: %s"), *ActiveSessionId.ToString(), *Instance.ToString());
	}
#endif
}


void FProfilerClientManager::Unsubscribe()
{
#if STATS
	PendingSessionId.Invalidate();
	Subscribe(PendingSessionId);
#endif
}


void FProfilerClientManager::SetCaptureState(const bool bRequestedCaptureState, const FGuid& InstanceId /*= FGuid()*/ )
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		if(!InstanceId.IsValid())
		{
			TArray<FMessageAddress> Instances;
			for (auto It = Connections.CreateConstIterator(); It; ++It)
			{
				Instances.Add(It.Value().ProfilerServiceAddress);
			}
			MessageEndpoint->Send(new FProfilerServiceCapture(bRequestedCaptureState), Instances);
			UE_LOG(LogProfilerClient, Verbose, TEXT("SetCaptureState Session: %s, Instance: %s, State: %i"), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedCaptureState);
		}
		else
		{
			const FMessageAddress& MessageAddress = Connections.Find(InstanceId)->ProfilerServiceAddress;
			MessageEndpoint->Send(new FProfilerServiceCapture(bRequestedCaptureState), MessageAddress);

			UE_LOG(LogProfilerClient, Verbose, TEXT("SetCaptureState Session: %s, Instance: %s, State: %i"), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedCaptureState);
		}
	}
#endif
}


void FProfilerClientManager::SetPreviewState(const bool bRequestedPreviewState, const FGuid& InstanceId /*= FGuid()*/)
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		if(!InstanceId.IsValid())
		{
			TArray<FMessageAddress> Instances;
			for (auto It = Connections.CreateConstIterator(); It; ++It)
			{
				Instances.Add(It.Value().ProfilerServiceAddress);
			}
			MessageEndpoint->Send(new FProfilerServicePreview(bRequestedPreviewState), Instances);
			UE_LOG(LogProfilerClient, Verbose, TEXT("SetPreviewState Session: %s, Instance: %s, State: %i"), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedPreviewState);
		}
		else
		{
			const FMessageAddress& MessageAddress = Connections.Find(InstanceId)->ProfilerServiceAddress;
			MessageEndpoint->Send(new FProfilerServicePreview(bRequestedPreviewState), MessageAddress);
			UE_LOG(LogProfilerClient, Verbose, TEXT("SetPreviewState Session: %s, Instance: %s, State: %i"), *ActiveSessionId.ToString(), *InstanceId.ToString(), (int32)bRequestedPreviewState);
		}
	}
#endif
}


/*-----------------------------------------------------------------------------
	New read test, still temporary, but around 4x faster
-----------------------------------------------------------------------------*/

class FNewStatsReader : public FStatsReadFile
{
	friend struct FStatsReader<FNewStatsReader>;
	typedef FStatsReadFile Super;

public:

	/** Initialize. */
	void Initialize(FProfilerClientManager* InProfilerClientManager, FServiceConnection* InLoadConnection)
	{
		ProfilerClientManager = InProfilerClientManager;
		LoadConnection = InLoadConnection;
	}

protected:

	/** Initialization constructor. */
	FNewStatsReader(const TCHAR* InFilename)
		: FStatsReadFile(InFilename, false)
		, ProfilerClientManager(nullptr)
		, LoadConnection(nullptr)
	{
		// Keep only the last frame.
		SetHistoryFrames(1);
	}	

	/** Called every each frame has been read from the file. */
	virtual void ReadStatsFrame(const TArray<FStatMessage>& CondensedMessages, const int64 Frame) override
	{
		SCOPE_CYCLE_COUNTER(STAT_PC_GenerateDataFrame);

		FProfilerDataFrame& DataFrame = LoadConnection->CurrentData;

		DataFrame.Frame = Frame;
		DataFrame.FrameStart = 0.0;
		DataFrame.CountAccumulators.Reset();
		DataFrame.CycleGraphs.Reset();
		DataFrame.FloatAccumulators.Reset();
		DataFrame.MetaDataUpdated = false;

		// Get the stat stack root and the non frame stats.
		FRawStatStackNode Stack;
		TArray<FStatMessage> NonFrameStats;
		State.UncondenseStackStats(CondensedMessages, Stack, nullptr, &NonFrameStats);

		LoadConnection->GenerateCycleGraphs(Stack, DataFrame.CycleGraphs);
		LoadConnection->GenerateAccumulators(NonFrameStats, DataFrame.CountAccumulators, DataFrame.FloatAccumulators);

		// Create a copy of the stats metadata.
		FStatMetaData* MetaDataPtr = nullptr;

		if (DataFrame.MetaDataUpdated)
		{
			MetaDataPtr = new FStatMetaData();
			*MetaDataPtr = LoadConnection->StatMetaData;
		}

		// Create a copy of the stats data.
		FProfilerDataFrame* DataFramePtr = new FProfilerDataFrame();
		*DataFramePtr = DataFrame;

		// Send to game thread.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(ProfilerClientManager, &FProfilerClientManager::SendProfilerDataFrameToGame, DataFramePtr, MetaDataPtr, LoadConnection->InstanceId),
			TStatId(), nullptr, ENamedThreads::GameThread
		);
	}

	/** Called after reading all data from the file. */
	virtual void PreProcessStats() override
	{
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(ProfilerClientManager, &FProfilerClientManager::FinalizeLoading, LoadConnection->InstanceId),
			TStatId(), NULL, ENamedThreads::GameThread
		);
	}

	FProfilerClientManager* ProfilerClientManager;
	FServiceConnection* LoadConnection;
};


void FServiceConnection::LoadCapture(const FString& DataFilepath, FProfilerClientManager* ProfilerClientManager)
{
	StatsReader = FStatsReader<FNewStatsReader>::Create(*DataFilepath);
	if (StatsReader)
	{
		StatsReader->Initialize(ProfilerClientManager, this);
		StatsReader->ReadAndProcessAsynchronously();
	}
}


void FProfilerClientManager::LoadCapture(const FString& DataFilepath, const FGuid& ProfileId)
{
#if STATS
	// Start an async load.
	LoadConnection = &Connections.FindOrAdd(ProfileId);
	LoadConnection->InstanceId = ProfileId;
	LoadConnection->StatMetaData.SecondsPerCycle = FPlatformTime::GetSecondsPerCycle(); // fix this by adding a message which specifies this

	ProfilerLoadStartedDelegate.Broadcast(ProfileId);
	LoadConnection->LoadCapture(DataFilepath, this);

	RetryTime = 0.05f;
	TickDelegateHandle = FTicker::GetCoreTicker().AddTicker(TickDelegate, RetryTime);	
#endif
}


void FProfilerClientManager::RequestLastCapturedFile(const FGuid& InstanceId /*= FGuid()*/)
{
#if STATS
	if (MessageEndpoint.IsValid() && ActiveSessionId.IsValid())
	{
		if(!InstanceId.IsValid())
		{
			TArray<FMessageAddress> Instances;
			for (auto It = Connections.CreateConstIterator(); It; ++It)
			{
				Instances.Add(It.Value().ProfilerServiceAddress);
			}
			MessageEndpoint->Send(new FProfilerServiceRequest(EProfilerRequestType::PRT_SendLastCapturedFile), Instances);
		}
		else
		{
			const FMessageAddress& MessageAddress = Connections.Find(InstanceId)->ProfilerServiceAddress;
			MessageEndpoint->Send(new FProfilerServiceRequest(EProfilerRequestType::PRT_SendLastCapturedFile), MessageAddress);
		}
	}
#endif
}


const FStatMetaData& FProfilerClientManager::GetStatMetaData(const FGuid& InstanceId) const
{
	return Connections.Find(InstanceId)->StatMetaData;
}


FProfilerClientDataDelegate& FProfilerClientManager::OnProfilerData()
{
	return ProfilerDataDelegate;
}


FProfilerFileTransferDelegate& FProfilerClientManager::OnProfilerFileTransfer()
{
	return ProfilerFileTransferDelegate;
}


FProfilerClientConnectedDelegate& FProfilerClientManager::OnProfilerClientConnected()
{
	return ProfilerClientConnectedDelegate;
}


FProfilerClientDisconnectedDelegate& FProfilerClientManager::OnProfilerClientDisconnected()
{
	return ProfilerClientDisconnectedDelegate;
}


FProfilerMetaDataUpdateDelegate& FProfilerClientManager::OnMetaDataUpdated()
{
	return ProfilerMetaDataUpdatedDelegate;
}


FProfilerLoadStartedDelegate& FProfilerClientManager::OnLoadStarted()
{
	return ProfilerLoadStartedDelegate;
}


FProfilerLoadCompletedDelegate& FProfilerClientManager::OnLoadCompleted()
{
	return ProfilerLoadCompletedDelegate;
}


FProfilerLoadCancelledDelegate& FProfilerClientManager::OnLoadCancelled()
{
	return ProfilerLoadCancelledDelegate;
}


/* FProfilerClientManager event handlers
 *****************************************************************************/

void FProfilerClientManager::HandleMessageBusShutdown()
{
#if STATS
	Shutdown();

	MessageEndpoint.Reset();
	MessageBus.Reset();
#endif
}


void FProfilerClientManager::HandleServiceAuthorizeMessage(const FProfilerServiceAuthorize& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
#if STATS
	if (ActiveSessionId == Message.SessionId && PendingInstances.Contains(Message.InstanceId))
	{
		PendingInstances.Remove(Message.InstanceId);
		FServiceConnection& Connection = Connections.FindOrAdd(Message.InstanceId);
		Connection.Initialize(Message, Context);

		// Fire a meta data update message
		//ProfilerMetaDataUpdatedDelegate.Broadcast(Message.InstanceId);

		// Fire the client connection event
		ProfilerClientConnectedDelegate.Broadcast(ActiveSessionId, Message.InstanceId);

		UE_LOG(LogProfilerClient, Verbose, TEXT("Authorize SessionId: %s, InstanceId: %s"), *Message.SessionId.ToString(), *Message.InstanceId.ToString());
	}
#endif
}


/* FServiceConnection
 *****************************************************************************/

FServiceConnection::FServiceConnection() 
	: StatsReader(nullptr)
{ }


FServiceConnection::~FServiceConnection()
{
	if (StatsReader)
	{
		StatsReader->RequestStop();

		while (StatsReader->IsBusy())
		{
			FPlatformProcess::Sleep(2.0f);
			UE_LOG(LogStats, Log, TEXT("RequestStop: Stage: %s / %3i%%"), *StatsReader->GetProcessingStageAsString(), StatsReader->GetStageProgress());
		}

		delete StatsReader;
		StatsReader = nullptr;
	}

	for (const auto& It : ReceivedData)
	{
		delete It.Value;
	}
}


void FServiceConnection::Initialize(const FProfilerServiceAuthorize& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
#if STATS
	ProfilerServiceAddress = Context->GetSender();
	InstanceId = Message.InstanceId;
	CurrentData.Frame = 0;
#endif
}


bool FProfilerClientManager::CheckHashAndWrite(const FProfilerServiceFileChunk& FileChunk, const FProfilerFileChunkHeader& FileChunkHeader, FArchive* Writer)
{
#if STATS
	const int32 HashSize = 20;
	uint8 LocalHash[HashSize]={0};
	
	// De-hex string into TArray<uint8>
	TArray<uint8> FileChunkData;
	const int32 DataLength = FileChunk.HexData.Len() / 2;
	FileChunkData.Reset(DataLength);
	FileChunkData.AddUninitialized(DataLength);
	FString::ToHexBlob(FileChunk.HexData, FileChunkData.GetData(), DataLength);

	// Hash file chunk data. 
	FSHA1 Sha;
	Sha.Update(FileChunkData.GetData(), FileChunkHeader.ChunkSize);
	// Hash file chunk header.
	Sha.Update(FileChunk.Header.GetData(), FileChunk.Header.Num());
	Sha.Final();
	Sha.GetHash(LocalHash);

	const int32 MemDiff = FMemory::Memcmp(FileChunk.ChunkHash.GetData(), LocalHash, HashSize);

	bool bResult = false;

	if(MemDiff == 0)
	{
		// Write the data to the archive.
		Writer->Seek(FileChunkHeader.ChunkOffset);
		Writer->Serialize((void*)FileChunkData.GetData(), FileChunkHeader.ChunkSize);

		bResult = true;
	}

	return bResult;
#else
	return false;
#endif
}


void FProfilerClientManager::HandleServiceFileChunk(const FProfilerServiceFileChunk& FileChunk, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
#if STATS
	const TCHAR* StrTmp = TEXT(".tmp");

	// Read file chunk header.
	FMemoryReader Reader(FileChunk.Header);
	FProfilerFileChunkHeader FileChunkHeader;
	Reader << FileChunkHeader;
	FileChunkHeader.Validate();

	const bool bValidFileChunk = !FailedTransfer.Contains(FileChunk.Filename);

	if (ActiveSessionId.IsValid() && Connections.Find(FileChunk.InstanceId) != nullptr && bValidFileChunk)
	{
		FReceivedFileInfo* ReceivedFileInfo = ActiveTransfers.Find(FileChunk.Filename);
		if(!ReceivedFileInfo)
		{
			const FString PathName = FPaths::ProfilingDir() + TEXT("UnrealStats/Received/");
			const FString StatFilepath = PathName + FileChunk.Filename + StrTmp;

			UE_LOG(LogProfilerClient, Log, TEXT("Opening stats file for service-client sending: %s"), *StatFilepath);

			FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*StatFilepath);
			if(!FileWriter)
			{
				UE_LOG(LogProfilerClient, Error, TEXT("Could not open: %s"), *StatFilepath);
				return;
			}

			ReceivedFileInfo = &ActiveTransfers.Add(FileChunk.Filename, FReceivedFileInfo(FileWriter,0,StatFilepath));
			ProfilerFileTransferDelegate.Broadcast(FileChunk.Filename, ReceivedFileInfo->Progress, FileChunkHeader.FileSize);
		}

		const bool bSimulateBadFileChunk = true;//FMath::Rand() % 10 != 0;
		const bool bSuccess = CheckHashAndWrite(FileChunk, FileChunkHeader, ReceivedFileInfo->FileWriter) && bSimulateBadFileChunk;	
		if(bSuccess)
		{
			ReceivedFileInfo->Progress += FileChunkHeader.ChunkSize;
			ReceivedFileInfo->Update();

			if(ReceivedFileInfo->Progress == FileChunkHeader.FileSize)
			{
				// File has been successfully sent, so send this information to the profiler service.
				if(MessageEndpoint.IsValid())
				{
					MessageEndpoint->Send(new FProfilerServiceFileChunk(FGuid(),FileChunk.Filename,FProfilerFileChunkHeader(0,0,0,EProfilerFileChunkType::FinalizeFile).AsArray()), Context->GetSender());
					ProfilerFileTransferDelegate.Broadcast(FileChunk.Filename, ReceivedFileInfo->Progress, FileChunkHeader.FileSize);
				}
				
				// Delete the file writer.
				delete ReceivedFileInfo->FileWriter;
				ReceivedFileInfo->FileWriter = nullptr;

				// Rename the stats file.
				IFileManager::Get().Move(*ReceivedFileInfo->DestFilepath.Replace(StrTmp, TEXT("")), *ReceivedFileInfo->DestFilepath);

				ActiveTransfers.Remove(FileChunk.Filename);

				UE_LOG(LogProfilerClient, Log, TEXT("File service-client received successfully: %s"), *FileChunk.Filename);
			}
			else
			{
				ProfilerFileTransferDelegate.Broadcast(FileChunk.Filename, ReceivedFileInfo->Progress, FileChunkHeader.FileSize);			
			}
		}
		else
		{
			// This chunk is a bad chunk, so ask for resending it.
			if(MessageEndpoint.IsValid())
			{
				MessageEndpoint->Send(new FProfilerServiceFileChunk(FileChunk,FProfilerServiceFileChunk::FNullTag()), Context->GetSender());
				UE_LOG(LogProfilerClient, Log, TEXT("Received a bad chunk of file, resending: %5i, %6u, %10u, %s"), FileChunk.HexData.Len(), ReceivedFileInfo->Progress, FileChunkHeader.FileSize, *FileChunk.Filename);
			}
		}
	}
#endif
}


void FProfilerClientManager::HandleServicePingMessage(const FProfilerServicePing& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
#if STATS
	if (MessageEndpoint.IsValid())
	{
		TArray<FMessageAddress> Instances;
		for (auto It = Connections.CreateConstIterator(); It; ++It)
		{
			Instances.Add(It.Value().ProfilerServiceAddress);
		}
		MessageEndpoint->Send(new FProfilerServicePong(), Instances);

		UE_LOG(LogProfilerClient, Verbose, TEXT("Ping GetSender: %s"), *Context->GetSender().ToString());
	}
#endif
}


bool FProfilerClientManager::HandleTicker(float DeltaTime)
{
#if STATS
	if (PendingInstances.Num() > 0 && FDateTime::Now() > LastPingTime + DeltaTime)
	{
		TArray<FGuid> Instances;
		Instances.Append(PendingInstances);

		PendingInstances.Reset();

		for (int32 i = 0; i < Instances.Num(); ++i)
		{
			Track(Instances[i]);
		}
		LastPingTime = FDateTime::Now();
	}
#endif
	return false;
}


bool FProfilerClientManager::HandleMessagesTicker(float DeltaTime)
{
#if STATS
	for (auto It = Connections.CreateIterator(); It; ++It)
	{
		FServiceConnection& Connection = It.Value();

		TArray<int64> Frames;
		Connection.ReceivedData.GenerateKeyArray(Frames);
		Frames.Sort();

		// MessageBus sends all data in out of order fashion.
		// We buffer frame to make sure that all frames are received in the proper order.
		const int32 NUM_BUFFERED_FRAMES = 15;

		for(int32 Index = 0; Index < Frames.Num(); Index++)
		{
			if (Connection.ReceivedData.Num() < NUM_BUFFERED_FRAMES)
			{
				break;
			}

			//FScopeLogTime SLT("HandleMessagesTicker");

			const int64 FrameNum = Frames[Index];
			const TArray<uint8>* const Data = Connection.ReceivedData.FindChecked(FrameNum);
			FStatsReadStream& Stream = Connection.Stream;


			// Read all messages from the uncompressed buffer.
			FMemoryReader MemoryReader(*Data, true);
			while (MemoryReader.Tell() < MemoryReader.TotalSize())
			{
				// Read the message.
				FStatMessage Message(Stream.ReadMessage(MemoryReader));
				new (Connection.PendingStatMessagesMessages)FStatMessage(Message);
			}

			// Adds a new from from the pending messages, the pending messages will be removed after the call.
			Connection.CurrentThreadState.ProcessMetaDataAndLeaveDataOnly(Connection.PendingStatMessagesMessages);
			Connection.CurrentThreadState.AddFrameFromCondensedMessages(Connection.PendingStatMessagesMessages);

			UE_LOG(LogProfilerClient, VeryVerbose, TEXT("Frame=%i/%i, FNamesIndexMap=%i, CurrentMetadataSize=%i"), FrameNum, Frames.Num(), Connection.Stream.FNamesIndexMap.Num(), Connection.CurrentThreadState.ShortNameToLongName.Num());

			// create an old format data frame from the data
			Connection.GenerateProfilerDataFrame();

			// Fire a meta data update message
			if (Connection.CurrentData.MetaDataUpdated)
			{
				ProfilerMetaDataUpdatedDelegate.Broadcast(Connection.InstanceId, Connection.StatMetaData);
			}

			// send the data out
			ProfilerDataDelegate.Broadcast(Connection.InstanceId, Connection.CurrentData);

			delete Data;
			Connection.ReceivedData.Remove(FrameNum);
		}
	}

	// Remove any active transfer that timed out.
	for(auto It = ActiveTransfers.CreateIterator(); It; ++It)
	{
		FReceivedFileInfo& ReceivedFileInfo = It.Value();
		const FString& Filename = It.Key();

		if(ReceivedFileInfo.IsTimedOut())
		{
			UE_LOG(LogProfilerClient, Log, TEXT("File service-client timed out, aborted: %s"), *Filename);
			FailedTransfer.Add(Filename);

			delete ReceivedFileInfo.FileWriter;
			ReceivedFileInfo.FileWriter = nullptr;

			IFileManager::Get().Delete(*ReceivedFileInfo.DestFilepath);
			ProfilerFileTransferDelegate.Broadcast(Filename, -1, -1);
			It.RemoveCurrent();	
		}
	}
#endif

	return true;
}


void FProfilerClientManager::HandleServicePreviewAckMessage(const FProfilerServicePreviewAck& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
#if STATS
	if (ActiveSessionId.IsValid() && Connections.Find(Message.InstanceId) != nullptr)
	{
		FServiceConnection& Connection = *Connections.Find(Message.InstanceId);

		UE_LOG(LogProfilerClient, Verbose, TEXT("PreviewAck InstanceId: %s, GetSender: %s"), *Message.InstanceId.ToString(), *Context->GetSender().ToString());
	}
#endif
}


void FProfilerClientManager::HandleProfilerServiceData2Message(const FProfilerServiceData2& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
#if STATS
	SCOPE_CYCLE_COUNTER(STAT_PC_HandleDataReceived);
	if (ActiveSessionId.IsValid() && Connections.Find(Message.InstanceId) != nullptr)
	{
		// Create a temporary profiler data and prepare all data.
		FProfilerServiceData2* ToProcess = new FProfilerServiceData2(Message.InstanceId, Message.Frame, Message.HexData, Message.CompressedSize, Message.UncompressedSize);

		// Decompression and decoding is done on the task graph.
		FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
		(
			FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FProfilerClientManager::DecompressDataAndSendToGame, ToProcess), 
			TStatId()
		);
	}
#endif
}


void FProfilerClientManager::DecompressDataAndSendToGame(FProfilerServiceData2* ToProcess)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FProfilerClientManager::DecompressDataAndSendToGame"), STAT_FProfilerClientManager_DecompressDataAndSendToGame, STATGROUP_Profiler);

	// De-hex string into TArray<uint8>
	TArray<uint8> CompressedData;
	CompressedData.Reset(ToProcess->CompressedSize);
	CompressedData.AddUninitialized(ToProcess->CompressedSize);
	FString::ToHexBlob(ToProcess->HexData, CompressedData.GetData(), ToProcess->CompressedSize);

	// Decompress data.
	TArray<uint8> UncompressedData;
	UncompressedData.Reset(ToProcess->UncompressedSize);
	UncompressedData.AddUninitialized(ToProcess->UncompressedSize);

	bool bResult = FCompression::UncompressMemory(COMPRESS_ZLIB, UncompressedData.GetData(), ToProcess->UncompressedSize, CompressedData.GetData(), ToProcess->CompressedSize);
	check(bResult);

	// Send to the game thread. Connections is not thread-safe, so we cannot add the data here.
	TArray<uint8>* DateToGame = new TArray<uint8>(MoveTemp(UncompressedData));

	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw(this, &FProfilerClientManager::SendDataToGame, DateToGame, ToProcess->Frame, ToProcess->InstanceId), 
		TStatId(), nullptr, ENamedThreads::GameThread
	);

	delete ToProcess;
}


void FProfilerClientManager::SendDataToGame(TArray<uint8>* DataToGame, int64 Frame, const FGuid InstanceId)
{
	if (ActiveSessionId.IsValid() && Connections.Find(InstanceId) != nullptr)
	{
		FServiceConnection& Connection = *Connections.Find(InstanceId);

		// Add the message to the connections queue.
		UE_LOG(LogProfilerClient, VeryVerbose, TEXT("Frame: %i, UncompressedSize: %i, InstanceId: %s"), Frame, DataToGame->Num(), *InstanceId.ToString());
		Connection.ReceivedData.Add(Frame, DataToGame);
	}
}


void FProfilerClientManager::SendProfilerDataFrameToGame(FProfilerDataFrame* NewData, FStatMetaData* MetaDataPtr, const FGuid InstanceId)
{
	if (Connections.Find(InstanceId) != nullptr)
	{
		if (MetaDataPtr)
		{
			ProfilerMetaDataUpdatedDelegate.Broadcast(InstanceId, *MetaDataPtr);
			delete MetaDataPtr;
			MetaDataPtr = nullptr;
		}

		if (NewData)
		{
			ProfilerDataDelegate.Broadcast(InstanceId, *NewData);
			delete NewData;
			NewData = nullptr;
		}
	}
}


void FProfilerClientManager::Shutdown()
{
	// Delete all active file writers and remove temporary files.
	for (auto It = ActiveTransfers.CreateIterator(); It; ++It)
	{
		FReceivedFileInfo& ReceivedFileInfo = It.Value();

		delete ReceivedFileInfo.FileWriter;
		ReceivedFileInfo.FileWriter = nullptr;

		IFileManager::Get().Delete(*ReceivedFileInfo.DestFilepath);

		UE_LOG(LogProfilerClient, Log, TEXT("File service-client transfer aborted: %s"), *It.Key());
	}

	FTicker::GetCoreTicker().RemoveTicker(MessageDelegateHandle);
	FTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
}


void FProfilerClientManager::FinalizeLoading(const FGuid InstanceId)
{
	if (Connections.Find(InstanceId) != nullptr)
	{
		ProfilerLoadCompletedDelegate.Broadcast(InstanceId);
		LoadConnection = &Connections.FindChecked(InstanceId);
		delete LoadConnection->StatsReader;
		LoadConnection->StatsReader = nullptr;
		LoadConnection = nullptr;
		Connections.Remove(InstanceId);

		RetryTime = 5.f;
	}
}


void FProfilerClientManager::CancelLoading(const FGuid InstanceId)
{
	if (Connections.Find(InstanceId) != nullptr)
	{
		ProfilerLoadCancelledDelegate.Broadcast(InstanceId);
		LoadConnection = &Connections.FindChecked(InstanceId);
		delete LoadConnection->StatsReader;
		LoadConnection->StatsReader = nullptr;
		LoadConnection = nullptr;
		Connections.Remove(InstanceId);
	}
}


#if STATS

int32 FServiceConnection::FindOrAddStat(const FStatNameAndInfo& StatNameAndInfo, uint32 StatType)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_FindOrAddStat);
	const FName LongName = StatNameAndInfo.GetRawName();
	int32* const StatIDPtr = LongNameToStatID.Find(LongName);
	int32 StatID = StatIDPtr != nullptr ? *StatIDPtr : -1;

	if (!StatIDPtr)
	{
		// meta data has been updated
		CurrentData.MetaDataUpdated = true;

		const FName StatName = StatNameAndInfo.GetShortName();
		FName GroupName = StatNameAndInfo.GetGroupName();
		const FString Description = StatNameAndInfo.GetDescription();

		// do some special stats first
		if (StatName == TEXT("STAT_FrameTime"))
		{
			StatID = LongNameToStatID.Add(LongName, 2);
		}
		else if (StatName == FStatConstants::NAME_ThreadRoot)
		{
			StatID = LongNameToStatID.Add(LongName, 1);
			GroupName = TEXT("NoGroup");
		}
		else
		{
			StatID = LongNameToStatID.Add(LongName, LongNameToStatID.Num()+10);
		}
		check(StatID != -1);

		// add a new stat description to the meta data
		FStatDescription StatDescription;
		StatDescription.ID = StatID;
		StatDescription.Name = !Description.IsEmpty() ? Description : StatName.ToString();
		if(StatDescription.Name.Contains(TEXT("STAT_")))
		{
			StatDescription.Name = StatDescription.Name.RightChop(FString(TEXT("STAT_")).Len());
		}
		StatDescription.StatType = StatType;

		if(GroupName == NAME_None && Stream.Header.Version == EStatMagicNoHeader::NO_VERSION)
		{	
			// @todo Add more ways to group the stats.
			const int32 Thread_Pos = StatDescription.Name.Find(TEXT("Thread_"));
			const int32 _0Pos = StatDescription.Name.Find(TEXT("_0"));
			const bool bIsThread = Thread_Pos != INDEX_NONE && _0Pos > Thread_Pos;
			// Add a special group for all threads.
			if(bIsThread)
			{
				GroupName = TEXT("Threads");
			}
			// Add a special group for all objects.
			else
			{
				GroupName = TEXT("Objects");
			}
		}

		int32* const GroupIDPtr = GroupNameArray.Find(GroupName);
		int32 GroupID = GroupIDPtr != nullptr ? *GroupIDPtr : -1;
		if(!GroupIDPtr)
		{
			// add a new group description to the meta data
			GroupID = GroupNameArray.Add(GroupName, GroupNameArray.Num()+10);
			check(GroupID != -1);

			FStatGroupDescription GroupDescription;
			GroupDescription.ID = GroupID;
			GroupDescription.Name = GroupName.ToString();
			GroupDescription.Name.RemoveFromStart(TEXT("STATGROUP_"));

			// add to the meta data
			StatMetaData.GroupDescriptions.Add(GroupDescription.ID, GroupDescription);
		}

		StatDescription.GroupID = GroupID;
		StatMetaData.StatDescriptions.Add(StatDescription.ID, StatDescription);
	}
	// return the stat id
	return StatID;
}


int32 FServiceConnection::FindOrAddThread(const FStatNameAndInfo& Thread)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_FindOrAddThread);

	// The description of a thread group contains the thread id
	const FString Desc = Thread.GetDescription();
	const uint32 ThreadID = FStatsUtils::ParseThreadID(Desc);

	const FName ShortName = Thread.GetShortName();

	// add to the meta data
	const int32 OldNum = StatMetaData.ThreadDescriptions.Num();
	StatMetaData.ThreadDescriptions.Add(ThreadID, ShortName.ToString());
	const int32 NewNum = StatMetaData.ThreadDescriptions.Num();

	// meta data has been updated
	CurrentData.MetaDataUpdated = CurrentData.MetaDataUpdated || OldNum != NewNum;

	return ThreadID;
}


void FServiceConnection::GenerateAccumulators(TArray<FStatMessage>& Stats, TArray<FProfilerCountAccumulator>& CountAccumulators, TArray<FProfilerFloatAccumulator>& FloatAccumulators)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_GenerateAccumulator)
	for (int32 Index = 0; Index < Stats.Num(); ++Index)
	{
		const FStatMessage& StatMessage = Stats[Index];

		uint32 StatType = STATTYPE_Error;
		if (StatMessage.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
		{
			if (StatMessage.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle))
			{
				StatType = STATTYPE_CycleCounter;
			}
			else if (StatMessage.NameAndInfo.GetFlag(EStatMetaFlags::IsMemory))
			{
				StatType = STATTYPE_MemoryCounter;
			}
			else
			{
				StatType = STATTYPE_AccumulatorDWORD;
			}
		}
		else if (StatMessage.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
		{
			StatType = STATTYPE_AccumulatorFLOAT;
		}

		if (StatType != STATTYPE_Error)
		{
			const int32 StatId = FindOrAddStat(StatMessage.NameAndInfo, StatType);

			if (StatMessage.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
			{
				// add a count accumulator
				FProfilerCountAccumulator Data;
				Data.StatId = StatId;
				Data.Value = StatMessage.GetValue_int64();
				CountAccumulators.Add(Data);
			}
			else if (StatMessage.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
			{
				// add a float accumulator
				FProfilerFloatAccumulator Data;
				Data.StatId = StatId;
				Data.Value = StatMessage.GetValue_double();
				FloatAccumulators.Add(Data);

				const FName StatName = StatMessage.NameAndInfo.GetRawName();
				if (StatName == FStatConstants::RAW_SecondsPerCycle)
				{
					StatMetaData.SecondsPerCycle = StatMessage.GetValue_double();
				}
			}
		}
	}
}


void FServiceConnection::CreateGraphRecursively(const FRawStatStackNode* Root, FProfilerCycleGraph& Graph, uint32 InStartCycles)
{
	Graph.FrameStart = InStartCycles;
	Graph.StatId = FindOrAddStat(Root->Meta.NameAndInfo, STATTYPE_CycleCounter);

	// add the data
	if (Root->Meta.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
	{
		if (Root->Meta.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			Graph.CallsPerFrame = FromPackedCallCountDuration_CallCount(Root->Meta.GetValue_int64());
			Graph.Value = FromPackedCallCountDuration_Duration(Root->Meta.GetValue_int64());
		}
		else
		{
			Graph.CallsPerFrame = 1;
			Graph.Value = Root->Meta.GetValue_int64();
		}
	}

	uint32 ChildStartCycles = InStartCycles;
	TArray<FRawStatStackNode*> ChildArray;
	Root->Children.GenerateValueArray(ChildArray);
	ChildArray.Sort(FStatDurationComparer<FRawStatStackNode>());
	for(int32 Index = 0; Index < ChildArray.Num(); ++Index)
	{
		const FRawStatStackNode* ChildStat = ChildArray[Index];

		// create the child graph
		FProfilerCycleGraph ChildGraph;
		ChildGraph.ThreadId = Graph.ThreadId;
		CreateGraphRecursively(ChildStat, ChildGraph, ChildStartCycles);

		// add to the graph
		Graph.Children.Add(ChildGraph);

		// update the start cycles
		ChildStartCycles += ChildGraph.Value;
	}
}


void FServiceConnection::GenerateCycleGraphs(const FRawStatStackNode& Root, TMap<uint32, FProfilerCycleGraph>& CycleGraphs)
{
	SCOPE_CYCLE_COUNTER(STAT_PC_GenerateCycleGraph);

	// Initialize the root stat.
	FindOrAddStat(Root.Meta.NameAndInfo, STATTYPE_CycleCounter);

	// get the cycle graph from each child of the stack root
	TArray<FRawStatStackNode*> ChildArray;
	Root.Children.GenerateValueArray(ChildArray);
	for (int32 Index = 0; Index < ChildArray.Num(); ++Index)
	{
		FRawStatStackNode* ThreadNode = ChildArray[Index];
		FProfilerCycleGraph Graph;

		// determine the thread id
		Graph.ThreadId = FindOrAddThread(ThreadNode->Meta.NameAndInfo);

		// create the thread graph
		CreateGraphRecursively(ThreadNode, Graph, 0);

		// add to the map
		CycleGraphs.Add(Graph.ThreadId, Graph);
	}
}


void FServiceConnection::GenerateProfilerDataFrame()
{
	SCOPE_CYCLE_COUNTER(STAT_PC_GenerateDataFrame);
	FProfilerDataFrame& DataFrame = CurrentData;
	DataFrame.Frame = CurrentThreadState.CurrentGameFrame;
	DataFrame.FrameStart = 0.0;
	DataFrame.CountAccumulators.Reset();
	DataFrame.CycleGraphs.Reset();
	DataFrame.FloatAccumulators.Reset();
	DataFrame.MetaDataUpdated = false;

	// get the stat stack root and the non frame stats
	FRawStatStackNode Stack;
	TArray<FStatMessage> NonFrameStats;
	CurrentThreadState.UncondenseStackStats(CurrentThreadState.CurrentGameFrame, Stack, nullptr, &NonFrameStats);

	// cycle graphs
	GenerateCycleGraphs(Stack, DataFrame.CycleGraphs);

	// accumulators
	GenerateAccumulators(NonFrameStats, DataFrame.CountAccumulators, DataFrame.FloatAccumulators);
}


#endif
