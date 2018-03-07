// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineBeacon.h"
#include "Engine/NetConnection.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY(LogBeacon);

AOnlineBeacon::AOnlineBeacon(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	NetDriver(nullptr),
	BeaconState(EBeaconState::DenyRequests)
{
	NetDriverName = FName(TEXT("BeaconDriver"));
	bRelevantForNetworkReplays = false;
}

bool AOnlineBeacon::InitBase()
{
	NetDriver = GEngine->CreateNetDriver(GetWorld(), NAME_BeaconNetDriver);
	if (NetDriver != nullptr)
	{
		HandleNetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &AOnlineBeacon::HandleNetworkFailure);
		SetNetDriverName(NetDriver->NetDriverName);
		return true;
	}

	return false;
}

void AOnlineBeacon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NetDriver)
	{
		GEngine->DestroyNamedNetDriver(GetWorld(), NetDriverName);
		NetDriver = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool AOnlineBeacon::HasNetOwner() const
{
    // Beacons are their own net owners
	return true;
}

void AOnlineBeacon::DestroyBeacon()
{
	UE_LOG(LogBeacon, Verbose, TEXT("Destroying beacon %s, netdriver %s"), *GetName(), NetDriver ? *NetDriver->GetDescription() : TEXT("NULL"));
	GEngine->OnNetworkFailure().Remove(HandleNetworkFailureDelegateHandle);

	if (NetDriver)
	{
		GEngine->DestroyNamedNetDriver(GetWorld(), NetDriverName);
		NetDriver = nullptr;
	}

	Destroy();
}

void AOnlineBeacon::HandleNetworkFailure(UWorld *World, UNetDriver *InNetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	if (InNetDriver && InNetDriver->NetDriverName == NetDriverName)
	{
		UE_LOG(LogBeacon, Verbose, TEXT("NetworkFailure %s: %s"), *GetName(), ENetworkFailure::ToString(FailureType));
		OnFailure();
	}
}

void AOnlineBeacon::OnFailure()
{
	GEngine->OnNetworkFailure().Remove(HandleNetworkFailureDelegateHandle);
	
	if (NetDriver)
	{
		GEngine->DestroyNamedNetDriver(GetWorld(), NetDriverName);
		NetDriver = nullptr;
	}
}

void AOnlineBeacon::OnActorChannelOpen(FInBunch& Bunch, UNetConnection* Connection)
{
	Connection->OwningActor = this;
	Super::OnActorChannelOpen(Bunch, Connection);
}

bool AOnlineBeacon::IsRelevancyOwnerFor(const AActor* ReplicatedActor, const AActor* ActorOwner, const AActor* ConnectionActor) const
{
	bool bRelevantOwner = (ConnectionActor == ReplicatedActor);
	return bRelevantOwner;
}

bool AOnlineBeacon::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// Only replicate to the owner or to connections of the same beacon type (possible that multiple UNetConnections come from the same client)
	bool bIsOwner = GetNetConnection() == ViewTarget->GetNetConnection();
	bool bSameBeaconType = GetClass() == RealViewer->GetClass();
	return bOnlyRelevantToOwner ? bIsOwner : bSameBeaconType;
}

EAcceptConnection::Type AOnlineBeacon::NotifyAcceptingConnection()
{
	check(NetDriver);
	if(NetDriver->ServerConnection)
	{
		// We are a client and we don't welcome incoming connections.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Client refused"));
		return EAcceptConnection::Reject;
	}
	else if(BeaconState == EBeaconState::DenyRequests)
	{
		// Server is down
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s refused"), *GetName());
		return EAcceptConnection::Reject;
	}
	else //if(BeaconState == EBeaconState::AllowRequests)
	{
		// Server is up and running.
		UE_LOG(LogNet, Log, TEXT("NotifyAcceptingConnection: Server %s accept"), *GetName());
		return EAcceptConnection::Accept;
	}
}

void AOnlineBeacon::NotifyAcceptedConnection(UNetConnection* Connection)
{
	check(NetDriver != nullptr);
	check(NetDriver->ServerConnection == nullptr);
	UE_LOG(LogNet, Log, TEXT("NotifyAcceptedConnection: Name: %s, TimeStamp: %s, %s"), *GetName(), FPlatformTime::StrTimestamp(), *Connection->Describe());
}

bool AOnlineBeacon::NotifyAcceptingChannel(UChannel* Channel)
{
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;
	check(NetDriver == Driver);

	if (Driver->ServerConnection)
	{
		// We are a client and the server has just opened up a new channel.
		UE_LOG(LogNet, Log,  TEXT("NotifyAcceptingChannel %i/%i client %s"), Channel->ChIndex, (int32)Channel->ChType, *GetName());
		if (Channel->ChType == CHTYPE_Actor)
		{
			// Actor channel.
			UE_LOG(LogNet, Log,  TEXT("Client accepting actor channel"));
			return 1;
		}
		else if (Channel->ChType == CHTYPE_Voice)
		{
			// Accept server requests to open a voice channel, allowing for custom voip implementations
			// which utilize multiple server controlled voice channels.
			UE_LOG(LogNet, Log,  TEXT("Client accepting voice channel"));
			return 1;
		}
		else
		{
			// Unwanted channel type.
			UE_LOG(LogNet, Log, TEXT("Client refusing unwanted channel of type %i"), (uint8)Channel->ChType);
			return 0;
		}
	}
	else
	{
		// We are the server.
		if (Channel->ChIndex == 0 && Channel->ChType == CHTYPE_Control)
		{
			// The client has opened initial channel.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, *GetFullName());
			return 1;
		}
		else
		{
			// Client can't open any other kinds of channels.
			UE_LOG(LogNet, Log, TEXT("NotifyAcceptingChannel %i %i server %s: Refused"), (uint8)Channel->ChType, Channel->ChIndex, *GetFullName());
			return 0;
		}
	}
}

void AOnlineBeacon::NotifyControlMessage(UNetConnection* Connection, uint8 MessageType, FInBunch& Bunch)
{
}
