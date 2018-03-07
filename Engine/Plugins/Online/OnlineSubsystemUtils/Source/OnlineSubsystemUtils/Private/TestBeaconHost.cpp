// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "TestBeaconHost.h"
#include "TestBeaconClient.h"

ATestBeaconHost::ATestBeaconHost(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	ClientBeaconActorClass = ATestBeaconClient::StaticClass();
	BeaconTypeName = ClientBeaconActorClass->GetName();
}

bool ATestBeaconHost::Init()
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogBeacon, Verbose, TEXT("Init"));
#endif
	return true;
}

void ATestBeaconHost::OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
#if !UE_BUILD_SHIPPING
	Super::OnClientConnected(NewClientActor, ClientConnection);

	ATestBeaconClient* BeaconClient = Cast<ATestBeaconClient>(NewClientActor);
	if (BeaconClient != NULL)
	{
		BeaconClient->ClientPing();
	}
#endif
}

AOnlineBeaconClient* ATestBeaconHost::SpawnBeaconActor(UNetConnection* ClientConnection)
{	
#if !UE_BUILD_SHIPPING
	return Super::SpawnBeaconActor(ClientConnection);
#else
	return NULL;
#endif
}
