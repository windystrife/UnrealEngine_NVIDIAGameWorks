// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "OnlineBeaconHostObject.h"
#include "TestBeaconHost.generated.h"

class AOnlineBeaconClient;

/**
 * A beacon host used for taking reservations for an existing game session
 */
UCLASS(transient, notplaceable, config=Engine)
class ONLINESUBSYSTEMUTILS_API ATestBeaconHost : public AOnlineBeaconHostObject
{
	GENERATED_UCLASS_BODY()

	//~ Begin AOnlineBeaconHost Interface 
	virtual AOnlineBeaconClient* SpawnBeaconActor(class UNetConnection* ClientConnection) override;
	virtual void OnClientConnected(class AOnlineBeaconClient* NewClientActor, class UNetConnection* ClientConnection) override;
	//~ End AOnlineBeaconHost Interface 

	virtual bool Init();
};
