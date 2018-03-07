// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/Guid.h"
#include "IMessageContext.h"
#include "IProfilerServiceManager.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"
#include "Containers/Ticker.h"
#include "MessageEndpoint.h"

struct FProfilerServiceCapture;
struct FProfilerServiceData2;
struct FProfilerServiceFileChunk;
struct FProfilerServicePong;
struct FProfilerServicePreview;
struct FProfilerServiceRequest;
struct FProfilerServiceSubscribe;
struct FProfilerServiceUnsubscribe;

DECLARE_LOG_CATEGORY_EXTERN(LogProfilerService, Log, All);

/**
* Thread used to read, prepare and send files through the message bus.
* Supports resending bad file chunks and basic synchronization between service and client.
*/
class FFileTransferRunnable
	: public FRunnable
{
	typedef TKeyValuePair<FArchive*, FMessageAddress> FReaderAndAddress;

public:

	/** Default constructor. */
	FFileTransferRunnable(TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe>& InMessageEndpoint);

	/** Destructor. */
	~FFileTransferRunnable();

	// Begin FRunnable interface.
	virtual bool Init();

	virtual uint32 Run();

	virtual void Stop()
	{
		StopTaskCounter.Increment();
	}

	virtual void Exit();
	// End FRunnable interface

	void EnqueueFileToSend(const FString& StatFilename, const FMessageAddress& RecipientAddress, const FGuid& ServiceInstanceId);

	/** Enqueues a file chunk. */
	void EnqueueFileChunkToSend(FProfilerServiceFileChunk* FileChunk, bool bTriggerWorkEvent = false);

	/** Prepare the chunks to be sent through the message bus. */
	void PrepareFileForSending(FProfilerServiceFileChunk*& FileChunk);

	/** Removes file from the list of the active transfers, must be confirmed by the profiler client. */
	void FinalizeFileSending(const FString& Filename);

	/** Aborts file sending to the specified client, probably client disconnected or exited. */
	void AbortFileSending(const FMessageAddress& Recipient);

	/** Checks if there has been any stop requests. */
	FORCEINLINE bool ShouldStop() const
	{
		return StopTaskCounter.GetValue() > 0;
	}

protected:

	/** Deletes the file reader. */
	void DeleteFileReader(FReaderAndAddress& ReaderAndAddress);

	/** Reads the data from the archive and generates hash. */
	void ReadAndSetHash(FProfilerServiceFileChunk* FileChunk, const FProfilerFileChunkHeader& FileChunkHeader, FArchive* Reader);

	/** Thread that is running this task. */
	FRunnableThread* Runnable;

	/** Event used to signaling that work is available. */
	FEvent* WorkEvent;

	/** Holds the messaging endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** > 0 if we have been asked to abort work in progress at the next opportunity. */
	FThreadSafeCounter StopTaskCounter;

	/** Added on the main thread, processed on the async thread. */
	TQueue<FProfilerServiceFileChunk*, EQueueMode::Mpsc> SendQueue;

	/** Critical section used to synchronize. */
	FCriticalSection SyncActiveTransfers;

	/** Active transfers, stored as a filename -> reader and destination address. Assumes that filename is unique and never will be the same. */
	TMap<FString, FReaderAndAddress> ActiveTransfers;
};


#if STATS

/**
 * struct that holds the client information
 */
struct FClientData
{
	/** Connection is active. */
	bool Active;

	/** Connection is previewing. */
	bool Preview;

	/** Default constructor. */
	FClientData()
		: Active(false)
		, Preview(false)
	{ }
};

#endif //STATS


/**
 * Implements the Profile Service Manager
 */
class FProfilerServiceManager
	: public TSharedFromThis<FProfilerServiceManager>
	, public IProfilerServiceManager
{
public:

	/** Default constructor. */
	FProfilerServiceManager();

public:

	//~ IProfilerServiceManager interface

	virtual void StartCapture() override;
	virtual void StopCapture() override;

public:

	/**
	 * Creates a profiler service manager for shared use
	 */
	static TSharedPtr<IProfilerServiceManager> CreateSharedServiceManager();


	/** Initializes the manager. */
	void Init();

	/** Shuts down the manager. */
	void Shutdown();

private:

	/**
	 * Changes the data preview state for the given client to the specified value.
	 */
	void SetPreviewState(const FMessageAddress& ClientAddress, const bool bRequestedPreviewState);

	/** Callback for a tick, used to ping the clients */
	bool HandlePing(float DeltaTime);


	/** Handles FProfilerServiceCapture messages. */
	void HandleServiceCaptureMessage(const FProfilerServiceCapture& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FProfilerServicePong messages. */
	void HandleServicePongMessage(const FProfilerServicePong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FProfilerServicePreview messages. */
	void HandleServicePreviewMessage(const FProfilerServicePreview& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FProfilerServiceRequest messages. */
	void HandleServiceRequestMessage(const FProfilerServiceRequest& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FProfilerServiceFileChunk messages. */
	void HandleServiceFileChunkMessage(const FProfilerServiceFileChunk& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FProfilerServiceSubscribe messages. */
	void HandleServiceSubscribeMessage(const FProfilerServiceSubscribe& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles FProfilerServiceUnsubscribe messages. */
	void HandleServiceUnsubscribeMessage(const FProfilerServiceUnsubscribe& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context);

	/** Handles a new frame from the stats system. Called from the stats thread. */
	void HandleNewFrame(int64 Frame);

#if STATS

	/** Compresses all stats data and send to the game thread. */
	void CompressDataAndSendToGame(TArray<uint8>* DataToTask, int64 Frame);

	/** Handles a new frame from the stats system. Called from the game thread. */
	void HandleNewFrameGT(FProfilerServiceData2* ToGameThread);

#endif //STATS

	void AddNewFrameHandleStatsThread();

	void RemoveNewFrameHandleStatsThread();

	/** Holds the messaging endpoint. */
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	/** Holds the session and instance identifier. */
	FGuid SessionId;
	FGuid InstanceId;

	/** Holds the message addresses for registered clients */
	TArray<FMessageAddress> PreviewClients;

#if	STATS

	/** Holds the client data for registered clients */
	TMap<FMessageAddress, FClientData> ClientData;

#endif //STATS

	/** Thread used to read, prepare and send file chunks through the message bus. */
	FFileTransferRunnable* FileTransferRunnable;

	/** Filename of last capture file. */
	FString LastStatsFilename;

	/** Size of the stats metadata. */
	int32 MetadataSize;

	/** Holds a delegate to be invoked for client pings */
	FTickerDelegate PingDelegate;

	/** Handle to the registered PingDelegate */
	FDelegateHandle PingDelegateHandle;

	/** Handle to the registered HandleNewFrame delegate */
	FDelegateHandle NewFrameDelegateHandle;
};
