// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
// libwebsocket client wrapper.
//
#pragma  once
#include "HTML5NetworkingPrivate.h"
#if !PLATFORM_HTML5
#include "Runtime/Sockets/Private/BSDSockets/SocketSubsystemBSD.h"
#else
#include <netinet/in.h>
#endif

class FWebSocket
{

public:

	// Initialize as client side socket.
	FWebSocket(const FInternetAddr& ServerAddress);

#if !PLATFORM_HTML5
	// Initialize as server side socket.
	FWebSocket(WebSocketInternalContext* InContext, WebSocketInternal* Wsi);
#endif

	// clean up.
	~FWebSocket();

	/************************************************************************/
	/* Set various callbacks for Socket Events                              */
	/************************************************************************/
	void SetConnectedCallBack(FWebsocketInfoCallBack CallBack);
	void SetErrorCallBack(FWebsocketInfoCallBack CallBack);
	void SetRecieveCallBack(FWebsocketPacketRecievedCallBack CallBack);

	/** Send raw data to remote end point. */
	bool Send(uint8* Data, uint32 Size);

	/** service libwebsocket.			   */
	void Tick();
	/** service libwebsocket until outgoing buffer is empty */
	void Flush();

	/** Helper functions to describe end points. */
	FString RemoteEndPoint(bool bAppendPort);
	FString LocalEndPoint(bool bAppendPort);
	struct sockaddr_in* GetRemoteAddr();

// this was made public because of cross-platform build issues
public:

	void HandlePacket();
	void OnRawRecieve(void* Data, uint32 Size);
	void OnRawWebSocketWritable(WebSocketInternal* wsi);

	/************************************************************************/
	/*	Various Socket callbacks											*/
	/************************************************************************/
	FWebsocketPacketRecievedCallBack  RecievedCallBack;
	FWebsocketInfoCallBack ConnectedCallBack;
	FWebsocketInfoCallBack ErrorCallBack;

	/**  Recv and Send Buffers, serviced during the Tick */
	TArray<uint8> RecievedBuffer;
	TArray<TArray<uint8>> OutgoingBuffer;

#if !PLATFORM_HTML5
	/** libwebsocket internal context*/
	WebSocketInternalContext* Context;

	/** libwebsocket web socket */
	WebSocketInternal* Wsi;

	/** libwebsocket Protocols that can be serviced by this implemenation*/
	WebSocketInternalProtocol* Protocols;
#else
	int SockFd;
#endif
	struct sockaddr_in RemoteAddr;

	/** Server side socket or client side*/
	bool IsServerSide;

	friend class FWebSocketServer;
};
