// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Net/UnitTestActorChannel.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"

#include "NetcodeUnitTest.h"

#include "UnitTestManager.h"
#include "ClientUnitTest.h"
#include "MinimalClient.h"



/**
 * Default constructor
 */
UUnitTestActorChannel::UUnitTestActorChannel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MinClient(nullptr)
{
#if !UE_BUILD_SHIPPING
	bBlockChannelFailure = true;
#endif
}

void UUnitTestActorChannel::Init(UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally)
{
	MinClient = UMinimalClient::GetMinClientFromConn(InConnection);

	Super::Init(InConnection, InChIndex, InOpenedLocally);
}

void UUnitTestActorChannel::ReceivedBunch(FInBunch& Bunch)
{
	// If Actor is nullptr, and we are processing a bunch, then we are most likely initializing the actor channel
	if (Actor == nullptr)
	{
		GIsInitializingActorChan = true;
	}

	Super::ReceivedBunch(Bunch);

	GIsInitializingActorChan = false;
}

void UUnitTestActorChannel::Tick()
{
	UNetConnection* OldReceiveUnitConn = GActiveReceiveUnitConnection;

	// If Actor is nullptr and we're about to process queued bunches, we may be about to do a (delayed) actor channel initialization
	if (Actor == nullptr && QueuedBunches.Num() > 0)
	{
		GIsInitializingActorChan = true;
		GActiveReceiveUnitConnection = Connection;
	}

	Super::Tick();

	GActiveReceiveUnitConnection = OldReceiveUnitConn;
	GIsInitializingActorChan = false;
}

void UUnitTestActorChannel::NotifyActorChannelOpen(AActor* InActor, FInBunch& InBunch)
{
	APlayerController* PC = Cast<APlayerController>(InActor);

	// Override 'UNetConnection::HandleClientPlayer', as called through PlayerController::OnActorChannelOpen
	if (PC != nullptr)
	{
		uint8 NetPlayerIndex = 0;

		InBunch << NetPlayerIndex;

		// Does not support child connections at the moment
		check(NetPlayerIndex == 0);

		if (NetPlayerIndex == 0 && Connection->Driver != nullptr && Connection == Connection->Driver->ServerConnection)
		{
			// Implement only essential parts of the original function, as we want to block most of it (triggers level change code)
			PC->Role = ROLE_AutonomousProxy;
			PC->NetConnection = Connection;

			Connection->PlayerController = PC;
			Connection->OwningActor = PC;

			if (PC->Player == nullptr)
			{
				PC->Player = NewObject<ULocalPlayer>(GEngine, GEngine->LocalPlayerClass);
			}

			if (MinClient != nullptr)
			{
				MinClient->HandleClientPlayerDel.ExecuteIfBound(PC, Connection);
			}
		}
	}
	else
	{
		Super::NotifyActorChannelOpen(InActor, InBunch);
	}
}
