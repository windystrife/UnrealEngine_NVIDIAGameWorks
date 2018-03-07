// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "LwsWebSocketsManager.h"

#if WITH_WEBSOCKETS && WITH_LIBWEBSOCKETS

#include "WebSocketsModule.h"
#include "RunnableThread.h"
#include "Ssl.h"
#include "WebSocketsLog.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ConfigCacheIni.h"

namespace {
	static const struct lws_extension LwsExtensions[] = {
		{
			"permessage-deflate",
			lws_extension_callback_pm_deflate,
			"permessage-deflate; client_max_window_bits"
		},
		{
			"deflate-frame",
			lws_extension_callback_pm_deflate,
			"deflate_frame"
		},
		// zero terminated:
		{ nullptr, nullptr, nullptr }
	};
}

static void LwsLog(int Level, const char* LogLine);

// FLwsWebSocketsManager
FLwsWebSocketsManager::FLwsWebSocketsManager()
	: LwsContext(nullptr)
	, Thread(nullptr)
{
	ThreadTargetFrameTimeInSeconds = 1.0f / 30.0f; // 30Hz
	GConfig->GetDouble(TEXT("WebSockets.LibWebSockets"), TEXT("ThreadTargetFrameTimeInSeconds"), ThreadTargetFrameTimeInSeconds, GEngineIni);

	ThreadMinimumSleepTimeInSeconds = 0.0f;
	GConfig->GetDouble(TEXT("WebSockets.LibWebSockets"), TEXT("ThreadMinimumSleepTimeInSeconds"), ThreadMinimumSleepTimeInSeconds, GEngineIni);
}

FLwsWebSocketsManager& FLwsWebSocketsManager::Get()
{
	FLwsWebSocketsManager* Manager = static_cast<FLwsWebSocketsManager*>(FWebSocketsModule::Get().WebSocketsManager);
	check(Manager);
	return *Manager;
}

void FLwsWebSocketsManager::InitWebSockets(TArrayView<const FString> Protocols)
{
	check(!Thread && LwsProtocols.Num() == 0);

	LwsProtocols.Reserve(Protocols.Num() + 1);
	for (const FString& Protocol : Protocols)
	{
		FTCHARToUTF8 ConvertName(*Protocol);

		// We need to hold on to the converted strings
		ANSICHAR* Converted = static_cast<ANSICHAR*>(FMemory::Malloc(ConvertName.Length() + 1));
		FCStringAnsi::Strcpy(Converted, ConvertName.Length(), ConvertName.Get());
		lws_protocols LwsProtocol;
		FMemory::Memset(&LwsProtocol, 0, sizeof(LwsProtocol));
		LwsProtocol.name = Converted;
		LwsProtocol.callback = &FLwsWebSocketsManager::StaticCallbackWrapper;
		LwsProtocol.per_session_data_size = 0;	// libwebsockets has two methods of specifying userdata that is used in callbacks
												// we can set it ourselves (during lws_client_connect_via_info - we do this, or via lws_set_wsi_user)
												// or libwebsockets can allocate memory for us using this parameter.  We want to set it ourself, so set this to 0.
		LwsProtocol.rx_buffer_size = 65536; // Largest frame size we support
		
		LwsProtocols.Emplace(MoveTemp(LwsProtocol));
	}

	// LWS requires a zero terminator (we don't pass the length)
	LwsProtocols.Add({ nullptr, nullptr, 0, 0 });

	// Subscribe to log events.  Everything except LLL_PARSER
	lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG | LLL_HEADER | LLL_EXT | LLL_CLIENT | LLL_LATENCY, &LwsLog);

	struct lws_context_creation_info ContextInfo = {};

	ContextInfo.port = CONTEXT_PORT_NO_LISTEN;
	ContextInfo.protocols = LwsProtocols.GetData();
	ContextInfo.uid = -1;
	ContextInfo.gid = -1;
	ContextInfo.options |= LWS_SERVER_OPTION_PEER_CERT_NOT_REQUIRED | LWS_SERVER_OPTION_DISABLE_OS_CA_CERTS;
	ContextInfo.ssl_cipher_list = nullptr;
	// TODO:  Investigate why enabling LwsExtensions prevents us from receiving packets larger than 1023 bytes, and also why lws_remaining_packet_payload returns 0 in that case
	ContextInfo.extensions = nullptr;// LwsExtensions;
	LwsContext = lws_create_context(&ContextInfo);
	if (LwsContext == nullptr)
	{
		UE_LOG(LogWebSockets, Error, TEXT("Failed to initialize libwebsockets"));
		return;
	}
	
	// TODO:  Determine thread stack size
	Thread = FRunnableThread::Create(this, TEXT("LibwebsocketsThread"), 128 * 1024, TPri_Normal);
	if (!Thread)
	{
		UE_LOG(LogWebSockets, Error, TEXT("FLwsWebSocketsManager failed to initialize thread!"));
		lws_context_destroy(LwsContext);
		LwsContext = nullptr;
		return;
	}

	// Setup our game thread tick
	FTicker& Ticker = FTicker::GetCoreTicker();
	FTickerDelegate TickDelegate = FTickerDelegate::CreateRaw(this, &FLwsWebSocketsManager::GameThreadTick);
	TickHandle = Ticker.AddTicker(TickDelegate, 0.0f);
}

void FLwsWebSocketsManager::ShutdownWebSockets()
{
	if (Thread)
	{
		Thread->Kill(true);
		delete Thread;
		Thread = nullptr;
	}

	if (LwsContext)
	{
		lws_context_destroy(LwsContext);
		LwsContext = nullptr;
	}

	// Cleanup our allocated protocols
	for (const lws_protocols& Protocol : LwsProtocols)
	{
		FMemory::Free(const_cast<ANSICHAR*>(Protocol.name));
	}
	LwsProtocols.Reset();

	SocketsToStart.Empty();
	SocketsToStop.Empty(); // TODO:  Should we trigger the OnClosed/OnConnectionError delegates?
	Sockets.Empty();
}

bool FLwsWebSocketsManager::Init()
{
	return true;
}

uint32 FLwsWebSocketsManager::Run()
{
	while (!ExitRequest.GetValue())
	{
		double BeginTime = FPlatformTime::Seconds();
		Tick();
		double EndTime = FPlatformTime::Seconds();

		double TotalTime = EndTime - BeginTime;
		double SleepTime = FMath::Max(ThreadTargetFrameTimeInSeconds - TotalTime, ThreadMinimumSleepTimeInSeconds);
		FPlatformProcess::SleepNoStats(SleepTime);
	}

	return 0;
}

void FLwsWebSocketsManager::Stop()
{
	ExitRequest.Set(true);
	// Safe to call from other threads
	lws_cancel_service(LwsContext);
}

void FLwsWebSocketsManager::Exit()
{
	for (FLwsWebSocket* Socket : SocketsTickingOnThread)
	{
		SocketsToStop.Enqueue(Socket);
	}
	SocketsTickingOnThread.Empty();
}

static inline bool LwsLogLevelIsWarning(int Level)
{
	return Level == LLL_ERR ||
		Level == LLL_WARN;
}

static inline const TCHAR* LwsLogLevelToString(int Level)
{
	switch (Level)
	{
	case LLL_ERR: return TEXT("Error");
	case LLL_WARN: return TEXT("Warning");
	case LLL_NOTICE: return TEXT("Notice");
	case LLL_INFO: return TEXT("Info");
	case LLL_DEBUG: return TEXT("Debug");
	case LLL_PARSER: return TEXT("Parser");
	case LLL_HEADER: return TEXT("Header");
	case LLL_EXT: return TEXT("Ext");
	case LLL_CLIENT: return TEXT("Client");
	case LLL_LATENCY: return TEXT("Latency");
	}
	return TEXT("Invalid");
}

static void LwsLog(int Level, const char* LogLine)
{
	bool bIsWarning = LwsLogLevelIsWarning(Level);
	if (bIsWarning || UE_LOG_ACTIVE(LogWebSockets, Verbose))
	{
		FUTF8ToTCHAR Converter(LogLine);
		// Trim trailing newline
		FString ConvertedLogLine(Converter.Get());
		if (ConvertedLogLine.EndsWith(TEXT("\n")))
		{
			ConvertedLogLine[ConvertedLogLine.Len() - 1] = TEXT(' ');
		}
		if (bIsWarning)
		{
			UE_LOG(LogWebSockets, Warning, TEXT("Lws(%s): %s"), LwsLogLevelToString(Level), *ConvertedLogLine);
		}
		else
		{
			UE_LOG(LogWebSockets, Verbose, TEXT("Lws(%s): %s"), LwsLogLevelToString(Level), *ConvertedLogLine);
		}
	}
}

int FLwsWebSocketsManager::StaticCallbackWrapper(lws* Connection, lws_callback_reasons Reason, void* UserData, void* Data, size_t Length)
{
	FLwsWebSocketsManager& This = FLwsWebSocketsManager::Get();
	return This.CallbackWrapper(Connection, Reason, UserData, Data, Length);
}

int FLwsWebSocketsManager::CallbackWrapper(lws* Connection, lws_callback_reasons Reason, void* UserData, void* Data, size_t Length)
{
	FLwsWebSocket* Socket = static_cast<FLwsWebSocket*>(UserData);

	switch (Reason)
	{
	case LWS_CALLBACK_RECEIVE_PONG:
		return 0;
	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS:
	case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
	{
		FSslModule::Get().GetCertificateManager().AddCertificatesToSslContext(static_cast<SSL_CTX*>(UserData));
		return 0;
	}
	case LWS_CALLBACK_WSI_DESTROY:
	{
		SocketsDestroyedDuringService.Add(Socket);
		break;
	}
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
	case LWS_CALLBACK_CLIENT_RECEIVE:
	case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
	case LWS_CALLBACK_CLOSED:
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
	case LWS_CALLBACK_CLIENT_WRITEABLE:
	case LWS_CALLBACK_SERVER_WRITEABLE:
	case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
		break;
	default:
		// Only process the callback reasons defined below this block
		return 0;
	}

	return Socket->LwsCallback(Connection, Reason, Data, Length);
}

void FLwsWebSocketsManager::Tick()
{
	{
		FLwsWebSocket* SocketToStart;
		while (SocketsToStart.Dequeue(SocketToStart))
		{
			if (LwsContext && SocketToStart->LwsThreadInitialize(*LwsContext))
			{
				SocketsTickingOnThread.Add(SocketToStart);
			}
			else
			{
				SocketsToStop.Enqueue(SocketToStart);
			}
		}
	}
	for (FLwsWebSocket* Socket : SocketsTickingOnThread)
	{
		Socket->LwsThreadTick();
	}
	if (LwsContext)
	{
		lws_service(LwsContext, 0);
	}
	for (FLwsWebSocket* Socket : SocketsDestroyedDuringService)
	{
		SocketsTickingOnThread.RemoveSwap(Socket);
		SocketsToStop.Enqueue(Socket);
	}
	SocketsDestroyedDuringService.Empty();
}

TSharedRef<IWebSocket> FLwsWebSocketsManager::CreateWebSocket(const FString& Url, const TArray<FString>& Protocols, const FString& UpgradeHeader)
{
	FLwsWebSocketRef Socket = MakeShared<FLwsWebSocket>(Url, Protocols, UpgradeHeader);
	return Socket;
}

void FLwsWebSocketsManager::StartProcessingWebSocket(FLwsWebSocket* Socket)
{
	Sockets.Emplace(Socket->AsShared());
	SocketsToStart.Enqueue(Socket);
}

bool FLwsWebSocketsManager::GameThreadTick(float DeltaTime)
{
	for (const FLwsWebSocketRef& Socket : Sockets)
	{
		Socket->GameThreadTick();
	}
	{
		FLwsWebSocket* Socket;
		while (SocketsToStop.Dequeue(Socket))
		{
			// Add reference then remove from Sockets, so the final callback delegates can let the owner immediately re-use the socket
			TSharedRef<FLwsWebSocket> SocketRef(Socket->AsShared());
			Sockets.RemoveSwap(SocketRef);
			// Trigger final delegates.
			Socket->GameThreadFinalize();
		}
	}
	return true;
}

#endif // #if WITH_WEBSOCKETS
