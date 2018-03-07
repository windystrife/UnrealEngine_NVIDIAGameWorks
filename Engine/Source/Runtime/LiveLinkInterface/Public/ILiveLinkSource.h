// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Guid.h"

class ILiveLinkClient;

class ILiveLinkSource
{
public:
	virtual ~ILiveLinkSource() {}
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) = 0;

	virtual bool IsSourceStillValid() = 0;

	virtual bool RequestSourceShutdown() = 0;

	virtual FText GetSourceType() const = 0;
	virtual FText GetSourceMachineName() const = 0;
	virtual FText GetSourceStatus() const = 0;
};