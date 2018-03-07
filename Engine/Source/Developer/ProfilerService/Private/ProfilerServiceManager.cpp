// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProfilerServiceManager.h"
#include "MessageEndpointBuilder.h"
#include "Misc/App.h"
#include "ProfilerServiceMessages.h"
#include "Serialization/MemoryReader.h"
#include "Stats/StatsData.h"
#include "Stats/StatsFile.h"


DEFINE_LOG_CATEGORY(LogProfilerService);


/* FProfilerServiceStatsStream
 *****************************************************************************/

#if STATS

/** Helper class for writing the condensed messages with related metadata. */
class FProfilerServiceStatsStream : protected FStatsWriteStream
{
public:
	void WriteFrameMessagesWithMetadata( int64 TargetFrame, bool bNeedFullMetadata )
	{
		FMemoryWriter Ar( OutData, false, true );

		if (bNeedFullMetadata)
		{
			WriteMetadata( Ar );
		}

		WriteCondensedMessages( Ar, TargetFrame );
	}

	/** Non-const for MoveTemp. */
	TArray<uint8>& GetOutData()
	{
		return OutData;
	}
};

#endif //STATS


/* FProfilerServiceManager structors
 *****************************************************************************/

FProfilerServiceManager::FProfilerServiceManager()
	: FileTransferRunnable( nullptr )
	, MetadataSize( 0 )
{
	PingDelegate = FTickerDelegate::CreateRaw(this, &FProfilerServiceManager::HandlePing);
}


/* IProfilerServiceManager interface
 *****************************************************************************/

void FProfilerServiceManager::StartCapture()
{
#if STATS
	DirectStatsCommand(TEXT("stat startfile"));
#endif
}


void FProfilerServiceManager::StopCapture()
{
#if STATS
	DirectStatsCommand(TEXT("stat stopfile"),true);
	// Not thread-safe, but in this case it is ok, because we are waiting for completion.
	LastStatsFilename = FCommandStatsFile::Get().LastFileSaved;
#endif
}


/* FProfilerServiceManager implementation
 *****************************************************************************/

void FProfilerServiceManager::Init()
{
	// get the instance id
	SessionId = FApp::GetSessionId();
	InstanceId = FApp::GetInstanceId();

	// connect to message bus
	MessageEndpoint = FMessageEndpoint::Builder("FProfilerServiceModule")
		.Handling<FProfilerServiceCapture>(this, &FProfilerServiceManager::HandleServiceCaptureMessage)	
		.Handling<FProfilerServicePong>(this, &FProfilerServiceManager::HandleServicePongMessage)
		.Handling<FProfilerServicePreview>(this, &FProfilerServiceManager::HandleServicePreviewMessage)
		.Handling<FProfilerServiceRequest>(this, &FProfilerServiceManager::HandleServiceRequestMessage)
		.Handling<FProfilerServiceFileChunk>(this, &FProfilerServiceManager::HandleServiceFileChunkMessage)
		.Handling<FProfilerServiceSubscribe>(this, &FProfilerServiceManager::HandleServiceSubscribeMessage)
		.Handling<FProfilerServiceUnsubscribe>(this, &FProfilerServiceManager::HandleServiceUnsubscribeMessage);

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Subscribe<FProfilerServiceSubscribe>();
		MessageEndpoint->Subscribe<FProfilerServiceUnsubscribe>();
	}

	FileTransferRunnable = new FFileTransferRunnable( MessageEndpoint );
}


void FProfilerServiceManager::Shutdown()
{
	delete FileTransferRunnable;
	FileTransferRunnable = nullptr;

	MessageEndpoint.Reset();
}


TSharedPtr<IProfilerServiceManager> FProfilerServiceManager::CreateSharedServiceManager()
{
	static TSharedPtr<IProfilerServiceManager> ProfilerServiceManager;

	if (!ProfilerServiceManager.IsValid())
	{
		ProfilerServiceManager = MakeShareable(new FProfilerServiceManager());
	}

	return ProfilerServiceManager;
}


void FProfilerServiceManager::AddNewFrameHandleStatsThread()
{
#if	STATS
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	NewFrameDelegateHandle = Stats.NewFrameDelegate.AddRaw( this, &FProfilerServiceManager::HandleNewFrame );
	StatsMasterEnableAdd();
	MetadataSize = 0;
#endif //STATS
}


void FProfilerServiceManager::RemoveNewFrameHandleStatsThread()
{
#if	STATS
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	Stats.NewFrameDelegate.Remove( NewFrameDelegateHandle );
	StatsMasterEnableSubtract();
	MetadataSize = 0;
#endif //STATS
}


void FProfilerServiceManager::SetPreviewState( const FMessageAddress& ClientAddress, const bool bRequestedPreviewState )
{
#if STATS
	FClientData* Client = ClientData.Find( ClientAddress );
	if (MessageEndpoint.IsValid() && Client)
	{
		const bool bIsPreviewing = Client->Preview;

		if( bRequestedPreviewState != bIsPreviewing )
		{
			if( bRequestedPreviewState )
			{
				// Enable stat capture.
				if (PreviewClients.Num() == 0)
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
					(
						FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::AddNewFrameHandleStatsThread ),
						TStatId(), nullptr,
						FPlatformProcess::SupportsMultithreading() ? ENamedThreads::StatsThread : ENamedThreads::GameThread
					);
				}
				PreviewClients.Add(ClientAddress);
				Client->Preview = true;

				MessageEndpoint->Send( new FProfilerServicePreviewAck( InstanceId ), ClientAddress );
			}
			else
			{
				PreviewClients.Remove(ClientAddress);
				Client->Preview = false;

				// Disable stat capture.
				if (PreviewClients.Num() == 0)
				{
					FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
					(
						FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::RemoveNewFrameHandleStatsThread ),
						TStatId(), nullptr,
						FPlatformProcess::SupportsMultithreading() ? ENamedThreads::StatsThread : ENamedThreads::GameThread
					);
					
				}	
			}
		}

		UE_LOG( LogProfilerService, Verbose, TEXT( "SetPreviewState: %i, InstanceId: %s, ClientAddress: %s" ), (int32)bRequestedPreviewState, *InstanceId.ToString(), *ClientAddress.ToString() );
	}
#endif //STATS
}


/* FProfilerServiceManager callbacks
 *****************************************************************************/

bool FProfilerServiceManager::HandlePing( float DeltaTime )
{
#if STATS
	// check the active flags and reset if true, remove the client if false
	TArray<FMessageAddress> Clients;
	for (auto Iter = ClientData.CreateIterator(); Iter; ++Iter)
	{
		FMessageAddress ClientAddress = Iter.Key();
		if (Iter.Value().Active)
		{
			Iter.Value().Active = false;
			Clients.Add(Iter.Key());
			UE_LOG( LogProfilerService, Verbose, TEXT( "Ping Active 0: %s, InstanceId: %s, ClientAddress: %s" ), *Iter.Key().ToString(), *InstanceId.ToString(), *ClientAddress.ToString() );
		}
		else
		{
			UE_LOG( LogProfilerService, Verbose, TEXT( "Ping Remove: %s, InstanceId: %s, ClientAddress: %s" ), *Iter.Key().ToString(), *InstanceId.ToString(), *ClientAddress.ToString() );
			SetPreviewState( ClientAddress, false );

			Iter.RemoveCurrent();
			FileTransferRunnable->AbortFileSending( ClientAddress );
		}
	}

	// send the ping message
	if (MessageEndpoint.IsValid() && Clients.Num() > 0)
	{
		MessageEndpoint->Send(new FProfilerServicePing(), Clients);
	}
	return (ClientData.Num() > 0);
#endif //STATS

	return false;
}


void FProfilerServiceManager::HandleServiceCaptureMessage( const FProfilerServiceCapture& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
#if STATS
	const bool bRequestedCaptureState = Message.bRequestedCaptureState;
	const bool bIsCapturing = FCommandStatsFile::Get().IsStatFileActive();

	if( bRequestedCaptureState != bIsCapturing )
	{
		if( bRequestedCaptureState && !bIsCapturing )
		{
			UE_LOG( LogProfilerService, Verbose, TEXT( "StartCapture, InstanceId: %s, GetSender: %s" ), *InstanceId.ToString(), *Context->GetSender().ToString() );
			StartCapture();
		}
		else if( !bRequestedCaptureState && bIsCapturing )
		{
			UE_LOG( LogProfilerService, Verbose, TEXT( "StopCapture, InstanceId: %s, GetSender: %s" ), *InstanceId.ToString(), *Context->GetSender().ToString() );
			StopCapture();
		}
	}
#endif //STATS
}


void FProfilerServiceManager::HandleServicePongMessage( const FProfilerServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
#if STATS
	FClientData* Data = ClientData.Find(Context->GetSender());
	
	if (Data != nullptr)
	{
		Data->Active = true;
		UE_LOG( LogProfilerService, Verbose, TEXT( "Pong InstanceId: %s, GetSender: %s" ), *InstanceId.ToString(), *Context->GetSender().ToString() );
	}
#endif //STATS
}


void FProfilerServiceManager::HandleServicePreviewMessage( const FProfilerServicePreview& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	SetPreviewState( Context->GetSender(), Message.bRequestedPreviewState );
}


void FProfilerServiceManager::HandleServiceRequestMessage( const FProfilerServiceRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	if( Message.Request == EProfilerRequestType::PRT_SendLastCapturedFile )
	{
		if( LastStatsFilename.IsEmpty() == false )
		{
			FileTransferRunnable->EnqueueFileToSend( LastStatsFilename, Context->GetSender(), InstanceId );
			LastStatsFilename.Empty();
		}
	}
}


void FProfilerServiceManager::HandleServiceFileChunkMessage(const FProfilerServiceFileChunk& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context)
{
	FMemoryReader Reader(Message.Header);
	FProfilerFileChunkHeader Header;
	Reader << Header;
	Header.Validate();

	if (Header.ChunkType == EProfilerFileChunkType::SendChunk)
	{
		// Send this file chunk again.
		FileTransferRunnable->EnqueueFileChunkToSend(new FProfilerServiceFileChunk(Message, FProfilerServiceFileChunk::FNullTag()), true);
	}
	else if (Header.ChunkType == EProfilerFileChunkType::FinalizeFile)
	{
		// Finalize file.
		FileTransferRunnable->FinalizeFileSending(Message.Filename);
	}
}


void FProfilerServiceManager::HandleServiceSubscribeMessage( const FProfilerServiceSubscribe& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
#if STATS
	const FMessageAddress& SenderAddress = Context->GetSender();
	if( MessageEndpoint.IsValid() && Message.SessionId == SessionId && Message.InstanceId == InstanceId && !ClientData.Contains( SenderAddress ) )
	{
		UE_LOG( LogProfilerService, Log, TEXT( "Subscribe Session: %s, Instance: %s" ), *SessionId.ToString(), *InstanceId.ToString() );

		FClientData Data;
		Data.Active = true;
		Data.Preview = false;

		// Add to the client list.
		ClientData.Add( SenderAddress, Data );

		// Send authorize.
		MessageEndpoint->Send( new FProfilerServiceAuthorize( SessionId, InstanceId ), SenderAddress );
		// Eventually send the metadata if needed.

		// Initiate the ping callback
		if (ClientData.Num() == 1)
		{
			PingDelegateHandle = FTicker::GetCoreTicker().AddTicker(PingDelegate, 5.0f);
		}
	}
#endif //STATS
}


void FProfilerServiceManager::HandleServiceUnsubscribeMessage( const FProfilerServiceUnsubscribe& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
#if	STATS
	const FMessageAddress SenderAddress = Context->GetSender();
	if (Message.SessionId == SessionId && Message.InstanceId == InstanceId)
	{
		UE_LOG( LogProfilerService, Log, TEXT( "Unsubscribe Session: %s, Instance: %s" ), *SessionId.ToString(), *InstanceId.ToString() );

		// clear out any previews
		while (PreviewClients.Num() > 0)
		{
			SetPreviewState( SenderAddress, false );
		}

		// remove from the client list
		ClientData.Remove( SenderAddress );
		FileTransferRunnable->AbortFileSending( SenderAddress );

		// stop the ping messages if we have no clients
		if (ClientData.Num() == 0)
		{
			FTicker::GetCoreTicker().RemoveTicker(PingDelegateHandle);
		}
	}
#endif //STATS
}


void FProfilerServiceManager::HandleNewFrame(int64 Frame)
{
	// Called from the stats thread.
#if STATS
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FProfilerServiceManager::HandleNewFrame" ), STAT_FProfilerServiceManager_HandleNewFrame, STATGROUP_Profiler );
	
	const FStatsThreadState& Stats = FStatsThreadState::GetLocalState();
	const int32 CurrentMetadataSize = Stats.ShortNameToLongName.Num();

	bool bNeedFullMetadata = false;
	if (MetadataSize < CurrentMetadataSize)
	{
		// Write the whole metadata.
		bNeedFullMetadata = true;
		MetadataSize = CurrentMetadataSize;
	}

	// Write frame.
	FProfilerServiceStatsStream StatsStream;
	StatsStream.WriteFrameMessagesWithMetadata( Frame, bNeedFullMetadata );

	// Task graph
	TArray<uint8>* DataToTask = new TArray<uint8>( MoveTemp( StatsStream.GetOutData() ) );

	// Compression and encoding is done on the task graph
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::CompressDataAndSendToGame, DataToTask, Frame ),
		TStatId()
	);
#endif //STATS
}


#if STATS

void FProfilerServiceManager::CompressDataAndSendToGame( TArray<uint8>* DataToTask, int64 Frame )
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "FProfilerServiceManager::CompressDataAndSendToGame" ), STAT_FProfilerServiceManager_CompressDataAndSendToGame, STATGROUP_Profiler );

	const uint8* UncompressedPtr = DataToTask->GetData();
	const int32 UncompressedSize = DataToTask->Num();

	TArray<uint8> CompressedBuffer;
	CompressedBuffer.Reserve( UncompressedSize );
	int32 CompressedSize = UncompressedSize;

	// We assume that compression cannot fail.
	const bool bResult = FCompression::CompressMemory( COMPRESS_ZLIB, CompressedBuffer.GetData(), CompressedSize, UncompressedPtr, UncompressedSize );
	check( bResult );

	// Convert to hex.
	FString HexData = FString::FromHexBlob( CompressedBuffer.GetData(), CompressedSize );

	// Create a temporary profiler data and prepare all data.
	FProfilerServiceData2* ToGameThread = new FProfilerServiceData2( InstanceId, Frame, HexData, CompressedSize, UncompressedSize );

	const float CompressionRatio = (float)UncompressedSize / (float)CompressedSize;
	UE_LOG( LogProfilerService, VeryVerbose, TEXT( "Frame: %i, UncompressedSize: %i/%f, InstanceId: %i" ), ToGameThread->Frame, UncompressedSize, CompressionRatio, *InstanceId.ToString() );

	// Send to the game thread. PreviewClients is not thread-safe, so we cannot send the data here.
	FSimpleDelegateGraphTask::CreateAndDispatchWhenReady
	(
		FSimpleDelegateGraphTask::FDelegate::CreateRaw( this, &FProfilerServiceManager::HandleNewFrameGT, ToGameThread ),
		TStatId(), nullptr, ENamedThreads::GameThread
	);

	delete DataToTask;
}


void FProfilerServiceManager::HandleNewFrameGT( FProfilerServiceData2* ToGameThread )
{
	if (MessageEndpoint.IsValid())
	{
		// Send through the Message Bus.
		MessageEndpoint->Send( ToGameThread, PreviewClients );
	}
}

#endif //STATS
