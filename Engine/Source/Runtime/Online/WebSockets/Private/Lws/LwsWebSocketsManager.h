/// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WEBSOCKETS && WITH_LIBWEBSOCKETS

#include "IWebSocketsManager.h"
#include "LwsWebSocket.h"
#include "HAL/Runnable.h"
#include "Misc/SingleThreadRunnable.h"
#include "HAL/ThreadSafeCounter.h"

#if PLATFORM_WINDOWS
#	include "AllowWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#	include "HideWindowsPlatformTypes.h"
#endif

class FRunnableThread;

class FLwsWebSocketsManager
	: public IWebSocketsManager
	, public FRunnable
	, public FSingleThreadRunnable

{
public:
	/** Default constructor */
	FLwsWebSocketsManager();

	static FLwsWebSocketsManager& Get();

	/**
	 * Start processing a websocket on our thread.  Called by FLwsWebSocket on the game thread.
	 * @param Socket websocket to process on our thread
	 */
	void StartProcessingWebSocket(FLwsWebSocket* Socket);

private:

	// IWebSocketsManager
	virtual void InitWebSockets(TArrayView<const FString> Protocols) override;
	virtual void ShutdownWebSockets() override;
	virtual TSharedRef<IWebSocket> CreateWebSocket(const FString& Url, const TArray<FString>& Protocols, const FString& UpgradeHeader) override;

	//~ Begin FRunnable Interface
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
	//~ End FRunnable Interface
	
	// FSingleThreadRunnable
	/**
	 * FSingleThreadRunnable accessor for ticking this FRunnable when multi-threading is disabled.
 	 * @return FSingleThreadRunnable Interface for this FRunnable object.
	 */
	virtual class FSingleThreadRunnable* GetSingleThreadInterface() override { return this; }

	virtual void Tick() override;

	// FLwsWebSocketsManager
	/** Game thread tick to flush events etc */
	bool GameThreadTick(float DeltaTime);
	/** Static callback on events for a libwebsockets connection */
	static int StaticCallbackWrapper(lws* Connection, lws_callback_reasons Reason, void* UserData, void* Data, size_t Length);
	/** Callback on events for a libwebsockets connection */
	int CallbackWrapper(lws* Connection, lws_callback_reasons Reason, void* UserData, void* Data, size_t Length);

	/** libwebsockets context */
	lws_context* LwsContext;
	/** array of protocols that we have registered with libwebsockets */
	TArray<lws_protocols> LwsProtocols;

	/** Array of all WebSockets we know about.  Shared ref count modifications only occurs on the game thread */
	TArray<TSharedRef<FLwsWebSocket>> Sockets;
	/** Array of all WebSockets we are ticking on our thread */
	TArray<FLwsWebSocket*> SocketsTickingOnThread;
	/** Queue of WebSockets to start processing on our thread */
	TQueue<FLwsWebSocket*> SocketsToStart;
	/** Queue of WebSockets that we are done processing on our thread and want to be removed from our Sockets array */
	TQueue<FLwsWebSocket*> SocketsToStop;
	/** Array of WebSockets destroyed during our call to lws_service, to be added to SocketsToStop after lws_service completes */
	TArray<FLwsWebSocket*> SocketsDestroyedDuringService;

	/** Delegate for callbacks to GameThreadTick */
	FDelegateHandle TickHandle;

	// Thread variables
	/** Pointer to Runnable Thread */
	FRunnableThread* Thread;

	/** signal request to stop and exit thread */
	FThreadSafeCounter ExitRequest;

	/** Target frame time for our thread's tick */
	double ThreadTargetFrameTimeInSeconds;
	/** Minimum time to sleep in our thread's tick, even if the sleep makes us exceed our target frame time */
	double ThreadMinimumSleepTimeInSeconds;
};

#endif // WITH_WEBSOCKETS && WITH_LIBWEBSOCKETS