// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Channel.h"
#include "UnitTestChannel.generated.h"

class FInBunch;
class UNetConnection;
class UMinimalClient;

/**
 * A net channel for overriding the implementation of traditional net channels,
 * for e.g. blocking control channel messages, to enable minimal clients
 */
UCLASS(transient)
class UUnitTestChannel : public UChannel
{
	GENERATED_UCLASS_BODY()	

	virtual void Init(UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally) override;

	virtual void ReceivedBunch(FInBunch& Bunch) override;

	virtual void Tick() override;


public:
	/** The minimal client which may require received bunch notifications */
	UMinimalClient* MinClient;

	/** Whether or not this channel should verify it has been opened (resends initial packets until acked, like control channel) */
	bool bVerifyOpen;
};




