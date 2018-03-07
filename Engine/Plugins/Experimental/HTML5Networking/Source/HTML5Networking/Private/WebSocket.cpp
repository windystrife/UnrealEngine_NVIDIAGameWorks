// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "WebSocket.h"
#include "HTML5NetworkingPrivate.h"
#include "IPAddress.h"

#if PLATFORM_HTML5
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <assert.h>
#include <emscripten/emscripten.h>
#endif

#if !PLATFORM_HTML5
// Work around a conflict between a UI namespace defined by engine code and a typedef in OpenSSL
#define UI UI_ST
THIRD_PARTY_INCLUDES_START
#include "libwebsockets.h"
THIRD_PARTY_INCLUDES_END
#undef UI
#endif

#if !PLATFORM_HTML5
static int unreal_networking_client(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

static void lws_debugLogS(int level, const char *line)
{
	UE_LOG(LogHTML5Networking, Log, TEXT("client: %s"), ANSI_TO_TCHAR(line));
}
#endif

FWebSocket::FWebSocket(
		const FInternetAddr& ServerAddress
)
:IsServerSide(false)
{

#if !PLATFORM_HTML5

#if !UE_BUILD_SHIPPING
	lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_DEBUG | LLL_INFO, lws_debugLogS);
#endif

	Protocols = new lws_protocols[3];
	FMemory::Memzero(Protocols, sizeof(lws_protocols) * 3);

	Protocols[0].name = "binary";
	Protocols[0].callback = unreal_networking_client;
	Protocols[0].per_session_data_size = 0;
	Protocols[0].rx_buffer_size = 10 * 1024 * 1024;

	Protocols[1].name = nullptr;
	Protocols[1].callback = nullptr;
	Protocols[1].per_session_data_size = 0;

	struct lws_context_creation_info Info;
	memset(&Info, 0, sizeof Info);

	Info.port = CONTEXT_PORT_NO_LISTEN;
	Info.protocols = &Protocols[0];
	Info.gid = -1;
	Info.uid = -1;
	Info.user = this;

	Context = lws_create_context(&Info);

	check(Context);

	struct lws_client_connect_info ConnectInfo = {
			Context, TCHAR_TO_ANSI(*ServerAddress.ToString(false)), ServerAddress.GetPort(), false, "/", TCHAR_TO_ANSI(*ServerAddress.ToString(false)), TCHAR_TO_ANSI(*ServerAddress.ToString(false)), Protocols[1].name, -1, this
	};
	Wsi = lws_client_connect_via_info(&ConnectInfo);
	check(Wsi);

#else // PLATFORM_HTML5

	SockFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SockFd == -1)
	{
		UE_LOG(LogHTML5Networking, Error, TEXT("Socket creationg failed "));
	}
	else
	{
		UE_LOG(LogHTML5Networking, Warning, TEXT(" Socked %d created "), SockFd);
	}

	fcntl(SockFd, F_SETFL, O_NONBLOCK);

#endif

	// Windows XP does not have support for inet_pton
#if PLATFORM_WINDOWS && _WIN32_WINNT <= 0x0502
	memset(&RemoteAddr, 0, sizeof(RemoteAddr));
	int32 SizeOfRemoteAddr = sizeof(RemoteAddr);

	// Force ServerAddress into non-const array. API doesn't modify contents but old API still requires non-const string
	if (WSAStringToAddress(ServerAddress.ToString(false).GetCharArray().GetData(), AF_INET, NULL, (sockaddr*)&RemoteAddr, &SizeOfRemoteAddr) != 0)
	{
		UE_LOG(LogHTML5Networking, Warning, TEXT("WSAStringToAddress failed "));
		return;
	}

	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_port = htons(ServerAddress.GetPort());
#else
	memset(&RemoteAddr, 0, sizeof(RemoteAddr));
	RemoteAddr.sin_family = AF_INET;
	RemoteAddr.sin_port = htons(ServerAddress.GetPort());

	if (inet_pton(AF_INET, TCHAR_TO_ANSI(*ServerAddress.ToString(false)), &RemoteAddr.sin_addr) != 1)
	{
		UE_LOG(LogHTML5Networking, Warning, TEXT("inet_pton failed "));
		return;
	}
#endif

#if PLATFORM_HTML5
	int Ret = connect(SockFd, (struct sockaddr *)&RemoteAddr, sizeof(RemoteAddr));
	UE_LOG(LogHTML5Networking, Warning, TEXT(" Connect socket returned %d"), Ret);
#endif
}

#if !PLATFORM_HTML5
FWebSocket::FWebSocket(WebSocketInternalContext* InContext, WebSocketInternal* InWsi)
	: Context(InContext)
	, Wsi(InWsi)
	, Protocols(nullptr)
	, IsServerSide(true)
{
	int sock = lws_get_socket_fd(Wsi);
	socklen_t len = sizeof RemoteAddr;
	getpeername(sock, (struct sockaddr*)&RemoteAddr, &len);
}
#endif

bool FWebSocket::Send(uint8* Data, uint32 Size)
{
	TArray<uint8> Buffer;

#if !PLATFORM_HTML5
	Buffer.AddDefaulted(LWS_PRE); // Reserve space for WS header data
#endif

	Buffer.Append((uint8*)&Size, sizeof (uint32)); // insert size.
	Buffer.Append((uint8*)Data, Size);
	OutgoingBuffer.Add(Buffer);

	return true;
}

void FWebSocket::SetRecieveCallBack(FWebsocketPacketRecievedCallBack CallBack)
{
	RecievedCallBack = CallBack;
}

FString FWebSocket::RemoteEndPoint(bool bAppendPort)
{
	// Windows XP does not have support for inet_ntop
#if PLATFORM_WINDOWS && _WIN32_WINNT <= 0x0502
	TCHAR Buffer[INET_ADDRSTRLEN];
	::DWORD BufferSize = INET_ADDRSTRLEN;
	FString remote = "";
	sockaddr_in AddressToConvert = RemoteAddr;
	if (!bAppendPort)
	{
		AddressToConvert.sin_port = 0;
	}
	if (WSAAddressToString((sockaddr*)&AddressToConvert, sizeof(AddressToConvert), NULL, Buffer, &BufferSize) == 0)
	{
		remote = Buffer;
	}
#else
	ANSICHAR Buffer[INET_ADDRSTRLEN];
	FString remote(ANSI_TO_TCHAR(inet_ntop(AF_INET, &RemoteAddr.sin_addr, Buffer, INET_ADDRSTRLEN)));
	if ( bAppendPort )
	{
		remote += FString::Printf(TEXT(":%i"), ntohs(RemoteAddr.sin_port));
	}
#endif

	return remote;
}

struct sockaddr_in* FWebSocket::GetRemoteAddr()
{
	return &RemoteAddr;
}

FString FWebSocket::LocalEndPoint(bool bAppendPort)
{
#if !PLATFORM_HTML5
	int sock = lws_get_socket_fd(Wsi);
	struct sockaddr_in addr;
	socklen_t len = sizeof addr;
	getsockname(sock, (struct sockaddr*)&addr, &len);

	// Windows XP does not have support for inet_ntop
#if PLATFORM_WINDOWS && _WIN32_WINNT <= 0x0502
	TCHAR Buffer[INET_ADDRSTRLEN];
	::DWORD BufferSize = INET_ADDRSTRLEN;
	FString remote = "";
	if (!bAppendPort)
	{
		addr.sin_port = 0;
	}
	if (WSAAddressToString((sockaddr*)&addr, sizeof(addr), NULL, Buffer, &BufferSize) == 0)
	{
		remote = Buffer;
	}
#else
	ANSICHAR Buffer[INET_ADDRSTRLEN];
	FString remote(ANSI_TO_TCHAR(inet_ntop(AF_INET, &addr.sin_addr, Buffer, INET_ADDRSTRLEN)));
	if ( bAppendPort )
	{
		remote += FString::Printf(TEXT(":%i"), ntohs(addr.sin_port));
	}
#endif

	return remote;
#else
	// NOTE: there's no way to get this info from browsers...
	// return generic localhost without port #
	return FString(TEXT("127.0.0.1"));
#endif
}

void FWebSocket::Tick()
{
	HandlePacket();
}

void FWebSocket::HandlePacket()
{
#if !PLATFORM_HTML5

	lws_service(Context, 0);
	if (!IsServerSide)
		lws_callback_on_writable_all_protocol(Context, &Protocols[0]);

#else // PLATFORM_HTML5

	fd_set Fdr;
	fd_set Fdw;
	int Res;

	// make sure that server.fd is ready to read / write
	FD_ZERO(&Fdr);
	FD_ZERO(&Fdw);
	FD_SET(SockFd, &Fdr);
	FD_SET(SockFd, &Fdw);
	Res = select(64, &Fdr, &Fdw, NULL, NULL);

	if (Res == -1) {
		UE_LOG(LogHTML5Networking, Warning, TEXT("Select Failed!"));
		return;
	}

	if (FD_ISSET(SockFd, &Fdr)) {
		// we can read!
		OnRawRecieve(NULL, NULL);
	}

	if (FD_ISSET(SockFd, &Fdw)) {
		// we can write
		OnRawWebSocketWritable(NULL);
	}

#endif
}

void FWebSocket::Flush()
{
	auto PendingMesssages = OutgoingBuffer.Num();
	while (OutgoingBuffer.Num() > 0 && !IsServerSide)
	{
#if !PLATFORM_HTML5
		if (Protocols)
		{
			lws_callback_on_writable_all_protocol(Context, &Protocols[0]);
		}
		else
		{
			lws_callback_on_writable(Wsi);
		}
#endif
		HandlePacket();
		if (PendingMesssages >= OutgoingBuffer.Num())
		{
			UE_LOG(LogHTML5Networking, Warning, TEXT("Unable to flush all of OutgoingBuffer in FWebSocket."));
			break;
		}
	};
}

void FWebSocket::SetConnectedCallBack(FWebsocketInfoCallBack CallBack)
{
	ConnectedCallBack = CallBack;
}

void FWebSocket::SetErrorCallBack(FWebsocketInfoCallBack CallBack)
{
	ErrorCallBack = CallBack;
}

void FWebSocket::OnRawRecieve(void* Data, uint32 Size)
{
#if !PLATFORM_HTML5

	RecievedBuffer.Append((uint8*)Data, Size); // consumes all of Data
	while (RecievedBuffer.Num() > sizeof(uint32))
	{
		uint32 BytesToBeRead = *(uint32*)RecievedBuffer.GetData();
		if (BytesToBeRead <= ((uint32)RecievedBuffer.Num() - sizeof(uint32)))
		{
			RecievedCallBack.ExecuteIfBound((void*)((uint8*)RecievedBuffer.GetData() + sizeof(uint32)), BytesToBeRead);
			RecievedBuffer.RemoveAt(0, sizeof(uint32) + BytesToBeRead );
		}
		else
		{
			break;
		}
	}

#else // PLATFORM_HTML5

	// browser was crashing when using RecievedBuffer...

	check(Data == NULL); // jic this is not obvious, Data will be resigned to Buffer below

	uint8 Buffer[1024]; // should be at MAX PACKET SIZE.
	Size = recv(SockFd, Buffer, sizeof(Buffer), 0);
//	while ( Size > sizeof(uint32) )
	if ( Size > sizeof(uint32) )
	{
		Data = (void*)Buffer;
		uint32 BytesToBeRead = *(uint32*)Data;
		uint32 BytesLeft = Size;
		while ( ( BytesToBeRead > 0 ) && ( BytesToBeRead < BytesLeft ) )
		{
			Data = (void*)((uint8*)Data + sizeof(uint32));
			RecievedCallBack.ExecuteIfBound(Data, BytesToBeRead);

			// "RecievedBuffer.RemoveAt()"
			Data = (void*)((uint8*)Data + BytesToBeRead);
			BytesLeft -= BytesToBeRead + sizeof(uint32);
			if ( (uint8*)Data >= (Buffer+Size)  // hard cap
				||	BytesLeft == 0	)           // soft limit
			{
				break;
			}
			BytesToBeRead = *(uint32*)Data;
		}
//		Size = recv(SockFd, Buffer, sizeof(Buffer), 0);
	}

#endif
}

void FWebSocket::OnRawWebSocketWritable(WebSocketInternal* wsi)
{
	if (OutgoingBuffer.Num() == 0)
		return;

	TArray <uint8>& Packet = OutgoingBuffer[0];

#if !PLATFORM_HTML5

	uint32 TotalDataSize = Packet.Num() - LWS_PRE;
	uint32 DataToSend = TotalDataSize;
	while (DataToSend)
	{
		int Sent = lws_write(Wsi, Packet.GetData() + LWS_PRE + (DataToSend-TotalDataSize), DataToSend, (lws_write_protocol)LWS_WRITE_BINARY);
		if (Sent < 0)
		{
			ErrorCallBack.ExecuteIfBound();
			return;
		}
		if ((uint32)Sent < DataToSend)
		{
			UE_LOG(LogHTML5Networking, Warning, TEXT("Could not write all '%d' bytes to socket"), DataToSend);
		}
		DataToSend-=Sent;
	}

	check(Wsi == wsi);

#else // PLATFORM_HTML5

	uint32 TotalDataSize = Packet.Num();
	uint32 DataToSend = TotalDataSize;
	while (DataToSend)
	{
		// send actual data in one go.
		int Result = send(SockFd, Packet.GetData()+(DataToSend-TotalDataSize),DataToSend, 0);
		if (Result == -1)
		{
			// we are caught with our pants down. fail.
			UE_LOG(LogHTML5Networking, Error, TEXT("Could not write %d bytes"), Packet.Num());
			ErrorCallBack.ExecuteIfBound();
			return;
		}
		UE_CLOG((uint32)Result < DataToSend, LogHTML5Networking, Warning, TEXT("Could not write all '%d' bytes to socket"), DataToSend);
		DataToSend-=Result;
	}

#endif

	// this is very inefficient we need a constant size circular buffer to efficiently not do unnecessary allocations/deallocations.
	OutgoingBuffer.RemoveAt(0);

}

FWebSocket::~FWebSocket()
{
	RecievedCallBack.Unbind();

#if !PLATFORM_HTML5

	Flush();

	if ( !IsServerSide)
	{
		lws_context_destroy(Context);
		Context = NULL;
		delete Protocols;
		Protocols = NULL;
	}

#else // PLATFORM_HTML5

	close(SockFd);

#endif

}

#if !PLATFORM_HTML5
static int unreal_networking_client(
		struct lws *Wsi,
		enum lws_callback_reasons Reason,
		void *User,
		void *In,
		size_t Len)
{
	struct lws_context *Context = lws_get_context(Wsi);
	FWebSocket* Socket = (FWebSocket*)lws_context_user(Context);
	switch (Reason)
	{
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		{
			Socket->ConnectedCallBack.ExecuteIfBound();
			lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			check(Socket->Wsi == Wsi);
		}
		break;
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		{
			Socket->ErrorCallBack.ExecuteIfBound();
			return -1;
		}
		break;
	case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			// push it on the socket.
			Socket->OnRawRecieve(In, (uint32)Len);
			check(Socket->Wsi == Wsi);
			lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			break;
		}
	case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
			check(Socket->Wsi == Wsi);
			Socket->OnRawWebSocketWritable(Wsi);
			lws_callback_on_writable(Wsi);
			lws_set_timeout(Wsi, NO_PENDING_TIMEOUT, 0);
			break;
		}
	case LWS_CALLBACK_CLOSED:
		{
			Socket->ErrorCallBack.ExecuteIfBound();
			return -1;
		}
	}

	return 0;
}
#endif
