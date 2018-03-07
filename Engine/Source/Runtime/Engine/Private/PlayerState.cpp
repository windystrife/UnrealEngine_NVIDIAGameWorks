// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PlayerState.cpp: 
=============================================================================*/

#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/EngineMessage.h"
#include "Net/UnrealNetwork.h"
#include "Net/OnlineEngineInterface.h"
#include "GameFramework/GameStateBase.h"

APlayerState::APlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("Sprite")) )
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;
	NetUpdateFrequency = 1;

	// Note: this is very important to set to false. Though all replication infos are spawned at run time, during seamless travel
	// they are held on to and brought over into the new world. In ULevel::InitializeActors, these PlayerStates may be treated as map/startup actors
	// and given static NetGUIDs. This also causes their deletions to be recorded and sent to new clients, which if unlucky due to name conflicts,
	// may end up deleting the new PlayerStates they had just spaned.
	bNetLoadOnClient = false;

	EngineMessageClass = UEngineMessage::StaticClass();
	SessionName = NAME_GameSession;
}

void APlayerState::UpdatePing(float InPing)
{
	// Limit the size of the ping, to avoid overflowing PingBucket values
	InPing = FMath::Min(1.1f, InPing);

	float CurTime = GetWorld()->RealTimeSeconds;

	if ((CurTime - CurPingBucketTimestamp) >= 1.f)
	{
		// Trigger ping recalculation now, while all buckets are 'full'
		//	(misses the latest ping update, but averages a full 4 seconds data)
		RecalculateAvgPing();

		CurPingBucket = (CurPingBucket + 1) % ARRAY_COUNT(PingBucket);
		CurPingBucketTimestamp = CurTime;


		PingBucket[CurPingBucket].PingSum = FMath::FloorToInt(InPing * 1000.f);
		PingBucket[CurPingBucket].PingCount = 1;
	}
	// Limit the number of pings we accept per-bucket, to avoid overflowing PingBucket values
	else if (PingBucket[CurPingBucket].PingCount < 7)
	{
		PingBucket[CurPingBucket].PingSum += FMath::FloorToInt(InPing * 1000.f);
		PingBucket[CurPingBucket].PingCount++;
	}
}

void APlayerState::RecalculateAvgPing()
{
	int32 Sum = 0;
	int32 Count = 0;

	for (uint8 i=0; i<ARRAY_COUNT(PingBucket); i++)
	{
		Sum += PingBucket[i].PingSum;
		Count += PingBucket[i].PingCount;
	}

	// Calculate the average, and divide it by 4 to optimize replication
	ExactPing = (Count > 0 ? ((float)Sum / (float)Count) : 0.f);
	Ping = FMath::Min(255, (int32)(ExactPing * 0.25f));
}

void APlayerState::DispatchOverrideWith(APlayerState* PlayerState)
{
	OverrideWith(PlayerState);
	ReceiveOverrideWith(PlayerState);
}

void APlayerState::DispatchCopyProperties(APlayerState* PlayerState)
{
	CopyProperties(PlayerState);
	ReceiveCopyProperties(PlayerState);
}

void APlayerState::OverrideWith(APlayerState* PlayerState)
{
	bIsSpectator = PlayerState->bIsSpectator;
	bOnlySpectator = PlayerState->bOnlySpectator;
	PlayerName = PlayerState->PlayerName;
	SetUniqueId(PlayerState->UniqueId.GetUniqueNetId());
}


void APlayerState::CopyProperties(APlayerState* PlayerState)
{
	PlayerState->Score = Score;
	PlayerState->Ping = Ping;
	PlayerState->PlayerName = PlayerName;
	PlayerState->PlayerId = PlayerId;
	PlayerState->SetUniqueId(UniqueId.GetUniqueNetId());
	PlayerState->StartTime = StartTime;
	PlayerState->SavedNetworkAddress = SavedNetworkAddress;
}

void APlayerState::OnDeactivated()
{
	// By default we duplicate the inactive player state and destroy the old one
	Destroy();
}

void APlayerState::OnReactivated()
{
	// Stub
}

void APlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UWorld* World = GetWorld();
	AGameStateBase* GameStateBase = World->GetGameState();

	// register this PlayerState with the game state
	if (GameStateBase != nullptr )
	{
		GameStateBase->AddPlayerState(this);
	}

	if ( Role < ROLE_Authority )
	{
		return;
	}

	AController* OwningController = Cast<AController>(GetOwner());
	if (OwningController != NULL)
	{
		bIsABot = (Cast<APlayerController>(OwningController) == nullptr);
	}

	if (GameStateBase)
	{
		StartTime = GameStateBase->GetPlayerStartTime(OwningController);
	}
}

void APlayerState::ClientInitialize(AController* C)
{
	SetOwner(C);
}

void APlayerState::OnRep_Score()
{
}

void APlayerState::OnRep_PlayerName()
{
	OldName = PlayerName;

	if ( GetWorld()->TimeSeconds < 2 )
	{
		bHasBeenWelcomed = true;
		return;
	}

	// new player or name change
	if ( bHasBeenWelcomed )
	{
		if( ShouldBroadCastWelcomeMessage() )
		{
			for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = Iterator->Get();
				if( PlayerController )
				{
					PlayerController->ClientReceiveLocalizedMessage( EngineMessageClass, 2, this );
				}
			}
		}
	}
	else
	{
		int32 WelcomeMessageNum = bOnlySpectator ? 16 : 1;
		bHasBeenWelcomed = true;

		if( ShouldBroadCastWelcomeMessage() )
		{
			for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
			{
				APlayerController* PlayerController = Iterator->Get();
				if( PlayerController )
				{
					PlayerController->ClientReceiveLocalizedMessage( EngineMessageClass, WelcomeMessageNum, this );
				}
			}
		}
	}
}

void APlayerState::OnRep_bIsInactive()
{
	// remove and re-add from the GameState so it's in the right list  
	UWorld* World = GetWorld();
	if (ensure(World && World->GetGameState()))
	{
		World->GetGameState()->RemovePlayerState(this);
		World->GetGameState()->AddPlayerState(this);
	}
}

bool APlayerState::ShouldBroadCastWelcomeMessage(bool bExiting)
{
	return (!bIsInactive && GetNetMode() != NM_Standalone);
}

void APlayerState::Destroyed()
{
	UWorld* World = GetWorld();
	if (World->GetGameState() != nullptr)
	{
		World->GetGameState()->RemovePlayerState(this);
	}

	if( ShouldBroadCastWelcomeMessage(true) )
	{
		for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
		{
			APlayerController* PlayerController = Iterator->Get();
			if( PlayerController )
			{
				PlayerController->ClientReceiveLocalizedMessage( EngineMessageClass, 4, this);
			}
		}
	}

	// Remove the player from the online session
	UnregisterPlayerWithSession();
	Super::Destroyed();
}


void APlayerState::Reset()
{
	Super::Reset();
	Score = 0;
	ForceNetUpdate();
}

FString APlayerState::GetHumanReadableName() const
{
	return PlayerName;
}

void APlayerState::SetPlayerName(const FString& S)
{
	PlayerName = S;

	// RepNotify callback won't get called by net code if we are the server
	ENetMode NetMode = GetNetMode();
	if (NetMode == NM_Standalone || NetMode == NM_ListenServer)
	{
		OnRep_PlayerName();
	}
	OldName = PlayerName;
	ForceNetUpdate();
}

void APlayerState::OnRep_UniqueId()
{
	// Register player with session
	RegisterPlayerWithSession(false);
}

void APlayerState::SetUniqueId(const TSharedPtr<const FUniqueNetId>& InUniqueId)
{
	UniqueId.SetUniqueNetId(InUniqueId);
}

void APlayerState::RegisterPlayerWithSession(bool bWasFromInvite)
{
	if (GetNetMode() != NM_Standalone)
	{
		if (UniqueId.IsValid()) // May not be valid if this is was created via DebugCreatePlayer
		{
			// Register the player as part of the session
			const APlayerState* PlayerState = GetDefault<APlayerState>();
			UOnlineEngineInterface::Get()->RegisterPlayer(GetWorld(), PlayerState->SessionName, *UniqueId, bWasFromInvite);
		}
	}
}

void APlayerState::UnregisterPlayerWithSession()
{
	if (GetNetMode() == NM_Client && UniqueId.IsValid())
	{
		const APlayerState* PlayerState = GetDefault<APlayerState>();
		if (PlayerState->SessionName != NAME_None)
		{
			UOnlineEngineInterface::Get()->UnregisterPlayer(GetWorld(), PlayerState->SessionName, *UniqueId);
		}
	}
}

APlayerState* APlayerState::Duplicate()
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = Instigator;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.ObjectFlags |= RF_Transient;	// We never want to save player states into a map
	APlayerState* NewPlayerState = GetWorld()->SpawnActor<APlayerState>(GetClass(), SpawnInfo );
	// Can fail in case of multiplayer PIE teardown
	if (NewPlayerState)
	{
		DispatchCopyProperties(NewPlayerState);
	}
	return NewPlayerState;
}

void APlayerState::SeamlessTravelTo(APlayerState* NewPlayerState)
{
	DispatchCopyProperties(NewPlayerState);
	NewPlayerState->bOnlySpectator = bOnlySpectator;
}


bool APlayerState::IsPrimaryPlayer() const
{
	return true;
}

void APlayerState::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( APlayerState, Score );

	DOREPLIFETIME( APlayerState, PlayerName );
	DOREPLIFETIME( APlayerState, bIsSpectator );
	DOREPLIFETIME( APlayerState, bOnlySpectator );
	DOREPLIFETIME( APlayerState, bFromPreviousLevel );
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	DOREPLIFETIME( APlayerState, StartTime );
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	DOREPLIFETIME_CONDITION( APlayerState, Ping,		COND_SkipOwner );

	DOREPLIFETIME_CONDITION( APlayerState, PlayerId,	COND_InitialOnly );
	DOREPLIFETIME_CONDITION( APlayerState, bIsABot,		COND_InitialOnly );
	DOREPLIFETIME_CONDITION( APlayerState, bIsInactive, COND_InitialOnly );
	DOREPLIFETIME_CONDITION( APlayerState, UniqueId,	COND_InitialOnly );
}
