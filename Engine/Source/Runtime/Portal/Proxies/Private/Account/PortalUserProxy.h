// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IMessageRpcClient.h"
#include "IPortalService.h"

class FPortalUserProxyFactory
{
public:
	static TSharedRef<IPortalService> Create(const TSharedRef<IMessageRpcClient>& RpcClient);
}; 
