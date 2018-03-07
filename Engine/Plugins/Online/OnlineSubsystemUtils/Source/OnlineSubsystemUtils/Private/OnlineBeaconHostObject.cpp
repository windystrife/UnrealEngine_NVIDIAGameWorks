// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineBeaconHostObject.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "OnlineBeaconHost.h"
#include "OnlineBeaconClient.h"

AOnlineBeaconHostObject::AOnlineBeaconHostObject(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	BeaconTypeName(TEXT("UNDEFINED"))
{
	PrimaryActorTick.bCanEverTick = true;
}

AOnlineBeaconClient* AOnlineBeaconHostObject::SpawnBeaconActor(UNetConnection* ClientConnection)
{
	if (ClientBeaconActorClass)
	{
		FActorSpawnParameters SpawnInfo;
		AOnlineBeaconClient* BeaconActor = GetWorld()->SpawnActor<AOnlineBeaconClient>(ClientBeaconActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
		if (BeaconActor)
		{
			BeaconActor->SetBeaconOwner(this);
		}

		return BeaconActor;
	}
	
	UE_LOG(LogBeacon, Warning, TEXT("Invalid client beacon actor class of type %s"), *GetBeaconType());
	return nullptr;
}

void AOnlineBeaconHostObject::OnClientConnected(AOnlineBeaconClient* NewClientActor, UNetConnection* ClientConnection)
{
	UE_LOG(LogBeacon, Verbose, TEXT("OnClientConnected %s from (%s)"),
		NewClientActor ? *NewClientActor->GetName() : TEXT("NULL"),
		NewClientActor ? *NewClientActor->GetNetConnection()->LowLevelDescribe() : TEXT("NULL"));

	ClientActors.Add(NewClientActor);
}

void AOnlineBeaconHostObject::DisconnectClient(AOnlineBeaconClient* ClientActor)
{
	AOnlineBeaconHost* BeaconHost = Cast<AOnlineBeaconHost>(GetOwner());
	if (BeaconHost)
	{
		BeaconHost->DisconnectClient(ClientActor);
	}
}

void AOnlineBeaconHostObject::NotifyClientDisconnected(AOnlineBeaconClient* LeavingClientActor)
{
	UE_LOG(LogBeacon, Verbose, TEXT("NotifyClientDisconnected %s"),
		LeavingClientActor ? *LeavingClientActor->GetName() : TEXT("NULL"));

	// Remove from local list of clients
	if (LeavingClientActor)
	{
		ClientActors.RemoveSingleSwap(LeavingClientActor);
	}

	// Remove from global list of clients
	AOnlineBeaconHost* BeaconHost = Cast<AOnlineBeaconHost>(GetOwner());
	if (BeaconHost)
	{
		BeaconHost->RemoveClientActor(LeavingClientActor);
	}
}

void AOnlineBeaconHostObject::Unregister()
{
	// Kill all the client connections associated with this host object
	for (AOnlineBeaconClient* ClientActor : ClientActors)
	{
		DisconnectClient(ClientActor);
	}

	SetOwner(nullptr);
}

FName AOnlineBeaconHostObject::GetNetDriverName() const
{
	AActor* BeaconHost = GetOwner();
	if (BeaconHost)
	{
		return BeaconHost->GetNetDriverName();
	}

	return NAME_None;
}

EBeaconState::Type AOnlineBeaconHostObject::GetBeaconState() const
{
	AOnlineBeaconHost* BeaconHost = Cast<AOnlineBeaconHost>(GetOwner());
	if (BeaconHost)
	{
		return BeaconHost->GetBeaconState();
	}

	return EBeaconState::DenyRequests;
}
