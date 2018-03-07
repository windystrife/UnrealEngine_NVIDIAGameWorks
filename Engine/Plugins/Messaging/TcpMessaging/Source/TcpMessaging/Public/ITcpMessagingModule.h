// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Modules/ModuleInterface.h"


/**
 * IConnectionBasedMessagingModule interface allows adding/removing of connections.
 */
class ITcpMessagingModule
	: public IModuleInterface
{
public:

	virtual void AddOutgoingConnection(const FString& Endpoint) = 0;
	virtual void RemoveOutgoingConnection(const FString& Endpoint) = 0;
};
