// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//
// Ip endpoint based implementation of the net driver
//

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetDriver.h"
#include "IpNetDriver.generated.h"

class Error;
class FInternetAddr;
class FNetworkNotify;
class FSocket;

UCLASS(transient, config=Engine)
class ONLINESUBSYSTEMUTILS_API UIpNetDriver : public UNetDriver
{
    GENERATED_UCLASS_BODY()

	/** Should port unreachable messages be logged */
    UPROPERTY(Config)
    uint32 LogPortUnreach:1;

	/** Does the game allow clients to remain after receiving ICMP port unreachable errors (handles flakey connections) */
    UPROPERTY(Config)
    uint32 AllowPlayerPortUnreach:1;

	/** Number of ports which will be tried if current one is not available for binding (i.e. if told to bind to port N, will try from N to N+MaxPortCountToTry inclusive) */
	UPROPERTY(Config)
	uint32 MaxPortCountToTry;

	/** Local address this net driver is associated with */
	TSharedPtr<FInternetAddr> LocalAddr;

	/** Underlying socket communication */
	FSocket* Socket;

	//~ Begin UNetDriver Interface.
	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error ) override;
	virtual bool InitListen( FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error ) override;
	virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = NULL) override;
	virtual void TickDispatch( float DeltaTime ) override;
	virtual void LowLevelSend(FString Address, void* Data, int32 CountBits) override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual void LowLevelDestroy() override;
	virtual class ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool IsNetResourceValid(void) override
	{
		return Socket != NULL;
	}
	//~ End UNetDriver Interface

	//~ Begin UIpNetDriver Interface.
	virtual FSocket * CreateSocket();

	/**
	 * Returns the port number to use when a client is creating a socket.
	 * Platforms that can't use the default of 0 (system-selected port) may override
	 * this function.
	 *
	 * @return The port number to use for client sockets. Base implementation returns 0.
	 */
	virtual int GetClientPort();
	//~ End UIpNetDriver Interface.

	//~ Begin FExec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) override;
	//~ End FExec Interface

	/**
	 * Exec command handlers
	 */
	bool HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld );

	/** @return TCPIP connection to server */
	class UIpConnection* GetServerConnection();

	// Callback for platform handling when networking is taking a long time in a single frame (by default over 1 second).
	// It may get called multiple times in a single frame if additional processing after a previous alert exceeds the threshold again
	DECLARE_MULTICAST_DELEGATE(FOnNetworkProcessingCausingSlowFrame);
	static FOnNetworkProcessingCausingSlowFrame OnNetworkProcessingCausingSlowFrame;

private:
	/** Number of bytes that will be passed to FSocket::SetReceiveBufferSize when initializing a server. */
	UPROPERTY(Config)
	uint32 ServerDesiredSocketReceiveBufferBytes;

	/** Number of bytes that will be passed to FSocket::SetSendBufferSize when initializing a server. */
	UPROPERTY(Config)
	uint32 ServerDesiredSocketSendBufferBytes;

	/** Number of bytes that will be passed to FSocket::SetReceiveBufferSize when initializing a client. */
	UPROPERTY(Config)
	uint32 ClientDesiredSocketReceiveBufferBytes;

	/** Number of bytes that will be passed to FSocket::SetSendBufferSize when initializing a client. */
	UPROPERTY(Config)
	uint32 ClientDesiredSocketSendBufferBytes;
};
