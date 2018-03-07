// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/NetConnection.h"
#include "WebSocketConnection.generated.h"


UCLASS(transient, config = Engine)
class HTML5NETWORKING_API UWebSocketConnection : public UNetConnection
{
	GENERATED_UCLASS_BODY()

	class FWebSocket*			WebSocket; 

	//~ Begin NetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBytes, int32 CountBits) override;
	FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	FString LowLevelDescribe() override;
	virtual int32 GetAddrAsInt(void) override;
	virtual int32 GetAddrPort(void) override;
	virtual FString RemoteAddressToString() override;
	virtual void Tick();
	virtual void FinishDestroy();
	virtual void ReceivedRawPacket(void* Data,int32 Count);
	//~ End NetConnection Interface


	void SetWebSocket(FWebSocket* InWebSocket);
	FWebSocket* GetWebSocket();

	bool bChallengeHandshake = false;
};
