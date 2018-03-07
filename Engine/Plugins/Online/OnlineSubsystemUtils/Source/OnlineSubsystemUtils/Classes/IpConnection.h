// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
// Ip based implementation of a network connection used by the net driver class
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetConnection.h"
#include "IpConnection.generated.h"

class FInternetAddr;

UCLASS(transient, config=Engine)
class ONLINESUBSYSTEMUTILS_API UIpConnection : public UNetConnection
{
    GENERATED_UCLASS_BODY()
	// Variables.
	TSharedPtr<FInternetAddr>	RemoteAddr;
	class FSocket*				Socket;
	class FResolveInfo*			ResolveInfo;

	//~ Begin NetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBytes, int32 CountBits) override;
	FString LowLevelGetRemoteAddress(bool bAppendPort=false) override;
	FString LowLevelDescribe() override;
	virtual int32 GetAddrAsInt(void) override;
	virtual int32 GetAddrPort(void) override;
	virtual FString RemoteAddressToString() override;
	//~ End NetConnection Interface
};
