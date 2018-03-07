// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "OnlineBeaconClient.h"
#include "TestBeaconClient.generated.h"

/**
 * A beacon client used for making reservations with an existing game session
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API ATestBeaconClient : public AOnlineBeaconClient
{
	GENERATED_UCLASS_BODY()

	//~ Begin AOnlineBeaconClient Interface
	virtual void OnFailure() override;
	//~ End AOnlineBeaconClient Interface

	/** Send a ping RPC to the client */
	UFUNCTION(client, reliable)
	virtual void ClientPing();

	/** Send a pong RPC to the host */
	UFUNCTION(server, reliable, WithValidation)
	virtual void ServerPong();
};
