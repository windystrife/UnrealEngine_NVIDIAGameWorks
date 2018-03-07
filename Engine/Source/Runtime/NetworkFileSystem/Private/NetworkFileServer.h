// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "INetworkFileServer.h"
#include "INetworkFileSystemModule.h"

class FInternetAddr;
class FSocket;
class ITargetPlatform;


/**
 * This class wraps the server thread and network connection
 */
class FNetworkFileServer
	: public FRunnable
	, public INetworkFileServer
{
public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InPort The port number to bind to (0 = any available port).
	 * @param InFileRequestDelegate 
	 */
	FNetworkFileServer( int32 InPort, FNetworkFileDelegateContainer InNetworkFileDelegateContainer, const TArray<ITargetPlatform*>& InActiveTargetPlatforms );

	/**
	 * Destructor.
	 */
	~FNetworkFileServer( );

public:

	// FRunnable Interface

	virtual bool Init( ) override
	{
		return true;
	}

	virtual uint32 Run( ) override;

	virtual void Stop( ) override
	{
		StopRequested.Set(true);
	}

	virtual void Exit( ) override;

public:

	// INetworkFileServer interface

	virtual bool IsItReadyToAcceptConnections(void) const override;
	virtual bool GetAddressList(TArray<TSharedPtr<FInternetAddr> >& OutAddresses) const override;
	virtual FString GetSupportedProtocol() const override;
	virtual int32 NumConnections() const override;
	virtual void Shutdown() override;
private:

	// Holds the server (listening) socket.
	FSocket* Socket;

	// Holds the server thread object.
	FRunnableThread* Thread;

	// Holds the list of all client connections.
	TArray< class FNetworkFileServerClientConnectionThreaded*> Connections;

	// Holds a flag indicating whether the thread should stop executing
	FThreadSafeCounter StopRequested;

	// Is the Listner thread up and running. 
	FThreadSafeCounter Running;

public:

	FNetworkFileDelegateContainer NetworkFileDelegates;

	// cached copy of the active target platforms (if any)
	const TArray<ITargetPlatform*> ActiveTargetPlatforms;

	// Holds the address that the server is bound to.
	TSharedPtr<FInternetAddr> ListenAddr;
};
