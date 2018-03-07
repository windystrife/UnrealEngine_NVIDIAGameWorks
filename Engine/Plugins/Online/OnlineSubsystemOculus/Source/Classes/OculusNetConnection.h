// Copyright 2016 Oculus VR, LLC All Rights reserved.

#pragma once

#include "IpConnection.h"
#include "OVR_Platform.h"
#include "OculusNetConnection.generated.h"

/**
 *
 */
UCLASS(transient, config = Engine)
class UOculusNetConnection : public UIpConnection
{
	GENERATED_BODY()
	
private:
	ovrID PeerID;
	/** Should this net connection behave as a passthrough to normal IP */
	bool bIsPassThrough;

public:

	// Begin NetConnection Interface
	virtual void InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket = 0, int32 InPacketOverhead = 0) override;
	virtual void LowLevelSend(void* Data, int32 CountBytes, int32 CountBits) override;
	FString LowLevelGetRemoteAddress(bool bAppendPort = false) override;
	FString LowLevelDescribe() override;
	virtual void FinishDestroy() override;

	virtual FString RemoteAddressToString() override;
	// End NetConnection Interface
	
};
