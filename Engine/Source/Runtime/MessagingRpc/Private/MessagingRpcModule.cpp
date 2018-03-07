// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IMessagingRpcModule.h"
#include "Modules/ModuleManager.h"
#include "MessageRpcClient.h"
#include "MessageRpcServer.h"


/**
 * Implements the MessagingRpc module.
 */
class FMessagingRpcModule
	: public IMessagingRpcModule
{
public:

	//~ IMessagingRpcModule interface

	virtual TSharedRef<IMessageRpcClient> CreateRpcClient() override
	{
		return MakeShareable(new FMessageRpcClient);
	}

	virtual TSharedRef<IMessageRpcServer> CreateRpcServer() override
	{
		return MakeShareable(new FMessageRpcServer);
	}

public:

	//~ IModuleInterface interface

	virtual void StartupModule() override { }
	virtual void ShutdownModule() override { }

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}
};


IMPLEMENT_MODULE(FMessagingRpcModule, MessagingRpc);
