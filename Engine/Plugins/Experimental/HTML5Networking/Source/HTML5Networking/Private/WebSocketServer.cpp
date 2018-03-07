// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WebSocketServer.h"
#include "HTML5NetworkingPrivate.h"
#include "WebSocket.h"

#if !PLATFORM_HTML5
// Work around a conflict between a UI namespace defined by engine code and a typedef in OpenSSL
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI
#endif

// a object of this type is associated by libwebsocket to every connected session.
struct PerSessionDataServer
{
	FWebSocket *Socket; // each session is actually a socket to a client
};


#if !PLATFORM_HTML5
// real networking handler.
static int unreal_networking_server(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

#if !UE_BUILD_SHIPPING
	inline void lws_debugLog(int level, const char *line)
	{
		UE_LOG(LogHTML5Networking, Log, TEXT("websocket server: %s"), ANSI_TO_TCHAR(line));
	}
#endif
#endif

bool FWebSocketServer::Init(uint32 Port, FWebsocketClientConnectedCallBack CallBack)
{
#if !PLATFORM_HTML5
#if !UE_BUILD_SHIPPING
	// setup log level.
	lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_DEBUG | LLL_INFO, lws_debugLog);
#endif

	Protocols = new lws_protocols[3];
	FMemory::Memzero(Protocols, sizeof(lws_protocols) * 3);

	Protocols[0].name = "binary";
	Protocols[0].callback = unreal_networking_server;
	Protocols[0].per_session_data_size = sizeof(PerSessionDataServer);
	Protocols[0].rx_buffer_size = 10 * 1024 * 1024;

	Protocols[1].name = nullptr;
	Protocols[1].callback = nullptr;
	Protocols[1].per_session_data_size = 0;

	struct lws_context_creation_info Info;
	memset(&Info, 0, sizeof(lws_context_creation_info));
	// look up libwebsockets.h for details.
	Info.port = Port;
	ServerPort = Port;
	// we listen on all available interfaces.
	Info.iface = NULL;
	Info.protocols = &Protocols[0];
	// no extensions
	Info.extensions = NULL;
	Info.gid = -1;
	Info.uid = -1;
	Info.options = 0;
	// tack on this object.
	Info.user = this;
	Context = lws_create_context(&Info);

	if (Context == NULL)
	{
		ServerPort = 0;
		delete Protocols;
		Protocols = NULL;
		IsAlive = false;
		return false; // couldn't create a server.
	}

	ConnectedCallBack = CallBack;
	IsAlive = true;

#endif
	return true;
}

bool FWebSocketServer::Tick()
{
#if !PLATFORM_HTML5
	if (IsAlive)
	{
		lws_service(Context, 0);
		lws_callback_on_writable_all_protocol(Context, &Protocols[0]);
	}
#endif
	return true;
}

FWebSocketServer::FWebSocketServer()
{}

FWebSocketServer::~FWebSocketServer()
{
#if !PLATFORM_HTML5
	if (Context)
	{
		lws_context_destroy(Context);
		Context = NULL;
	}

	 delete Protocols;
	 Protocols = NULL;

	 IsAlive = false;
#endif
}

FString FWebSocketServer::Info()
{
#if !PLATFORM_HTML5
	return FString::Printf(TEXT("%s:%i"), ANSI_TO_TCHAR(lws_canonical_hostname(Context)), ServerPort);
#else 
	return FString(TEXT("NOT SUPPORTED"));
#endif
}

// callback.
#if !PLATFORM_HTML5
static int unreal_networking_server
	(
		struct lws *Wsi,
		enum lws_callback_reasons Reason,
		void *User,
		void *In,
		size_t Len
	)
{
	struct lws_context *Context = lws_get_context(Wsi);
	PerSessionDataServer* BufferInfo = (PerSessionDataServer*)User;
	FWebSocketServer* Server = (FWebSocketServer*)lws_context_user(Context);
	if (!Server->IsAlive)
	{
		return 0;
	}

	switch (Reason)
	{
		case LWS_CALLBACK_ESTABLISHED:
			{
				BufferInfo->Socket = new FWebSocket(Context, Wsi);
				Server->ConnectedCallBack.ExecuteIfBound(BufferInfo->Socket);
				lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			}
			break;

		case LWS_CALLBACK_RECEIVE:
			{
				BufferInfo->Socket->OnRawRecieve(In, Len);
				lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			}
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			{
				BufferInfo->Socket->OnRawWebSocketWritable(Wsi);
				lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			}
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			{
				BufferInfo->Socket->ErrorCallBack.ExecuteIfBound();
			}
			break;
		case LWS_CALLBACK_WSI_DESTROY:
		case LWS_CALLBACK_PROTOCOL_DESTROY:
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLOSED_HTTP:
			{
				Server->IsAlive = false;
			}
			break;
	}

	return 0;
}
#endif
