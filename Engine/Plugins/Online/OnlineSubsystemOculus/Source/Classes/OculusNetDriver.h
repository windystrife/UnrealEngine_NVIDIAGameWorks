// Copyright 2016 Oculus VR, LLC All Rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "OculusNetConnection.h"
#include "IpNetDriver.h"
#include "OculusNetDriver.generated.h"

/**
 *
 */
UCLASS(transient, config = Engine)
class UOculusNetDriver : public UIpNetDriver
{
	GENERATED_BODY()

private:

	FDelegateHandle PeerConnectRequestDelegateHandle;
	FDelegateHandle NetworkingConnectionStateChangeDelegateHandle;

	bool AddNewClientConnection(ovrID PeerID);
	/** Should this net driver behave as a passthrough to normal IP */
	bool bIsPassthrough;

public:
	TMap<uint64, UOculusNetConnection*> Connections;

	// Begin UNetDriver interface.
	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void Shutdown() override;
	virtual bool IsNetResourceValid() override;

	virtual class ISocketSubsystem* GetSocketSubsystem() override;

	void OnNewNetworkingPeerRequest(ovrMessageHandle Message, bool bIsError);

	void OnNetworkingConnectionStateChange(ovrMessageHandle Message, bool bIsError);
};
