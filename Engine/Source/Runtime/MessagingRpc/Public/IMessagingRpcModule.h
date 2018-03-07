// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"

class IMessageRpcClient;
class IMessageRpcServer;


/**
 * Interface for the MessagingRpc module.
 */
class IMessagingRpcModule
	: public IModuleInterface
{
public:

	/**
	 * Create a client for remote procedure calls.
	 *
	 * @return The RPC client.
	 */
	virtual TSharedRef<IMessageRpcClient> CreateRpcClient() = 0;

	/**
	 * Create a server for remote procedure calls.
	 *
	 * @return The RPC server.
	 */
	virtual TSharedRef<IMessageRpcServer> CreateRpcServer() = 0;

public:

	/** Virtual destructor. */
	virtual ~IMessagingRpcModule() { }
};
