// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
// websocket based implementation of the net driver
//

#pragma once

#include "CoreMinimal.h"
#include "Engine/NetDriver.h"
#include "WebSocketNetDriver.generated.h"

class FWebSocket;
class FWebSocketServer;

UCLASS(transient, config = Engine)
class HTML5NETWORKING_API UWebSocketNetDriver : public UNetDriver
{
	GENERATED_UCLASS_BODY()

	/** Websocket server port*/
	UPROPERTY(Config)
	int32 WebSocketPort;

	//~ Begin UNetDriver Interface.
	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void ProcessRemoteFunction(class AActor* Actor, class UFunction* Function, void* Parameters, struct FOutParmRec* OutParms, struct FFrame* Stack, class UObject* SubObject = NULL) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void LowLevelSend(FString Address, void* Data, int32 CountBits) override;
	virtual FString LowLevelGetNetworkNumber() override;
	virtual void LowLevelDestroy() override;
	virtual bool IsNetResourceValid(void) override;

	// stub implementation because for websockets we don't use any underlying socket sub system.
	virtual class ISocketSubsystem* GetSocketSubsystem() override;

	//~ End UNetDriver Interface.

	//~ Begin FExec Interface
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar = *GLog) override;
	//~ End FExec Interface

	/**
	* Exec command handlers
	*/
	bool HandleSocketsCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld);

	/** @return connection to server */
	class UWebSocketConnection* GetServerConnection();

	/************************************************************************/
	/* FWebSocketServer														*/
	/************************************************************************/

	FWebSocketServer* WebSocketServer; 

	/******************************************************************************/
	/* Callback Function for New Connection from a client is accepted by this server   */
	/******************************************************************************/

	void OnWebSocketClientConnected(FWebSocket*); // to the server. 


	/************************************************************************/
	/* Callback Function for when this client Connects to the server				*/
	/************************************************************************/

	void OnWebSocketServerConnected(); // to the client. 


};
