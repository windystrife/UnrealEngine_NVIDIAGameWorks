// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GameState.cpp: GameState C++ code.
=============================================================================*/

#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY(LogGameState);

AGameStateBase::AGameStateBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
		.DoNotCreateDefaultSubobject(TEXT("Sprite")) )
{
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	bReplicateMovement = false;

	// Note: this is very important to set to false. Though all replication infos are spawned at run time, during seamless travel
	// they are held on to and brought over into the new world. In ULevel::InitializeNetworkActors, these PlayerStates may be treated as map/startup actors
	// and given static NetGUIDs. This also causes their deletions to be recorded and sent to new clients, which if unlucky due to name conflicts,
	// may end up deleting the new PlayerStates they had just spawned.
	bNetLoadOnClient = false;

	// Default to every few seconds.
	ServerWorldTimeSecondsUpdateFrequency = 5.f;
}

const AGameModeBase* AGameStateBase::GetDefaultGameMode() const
{
	if ( GameModeClass )
	{
		AGameModeBase* const DefaultGameActor = GameModeClass->GetDefaultObject<AGameModeBase>();
		return DefaultGameActor;
	}
	return nullptr;
}

void AGameStateBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	UWorld* World = GetWorld();
	World->SetGameState(this);

	FTimerManager& TimerManager = GetWorldTimerManager();
	if (World->IsGameWorld() && Role == ROLE_Authority)
	{
		UpdateServerTimeSeconds();
		if (ServerWorldTimeSecondsUpdateFrequency > 0.f)
		{
			TimerManager.SetTimer(TimerHandle_UpdateServerTimeSeconds, this, &AGameStateBase::UpdateServerTimeSeconds, ServerWorldTimeSecondsUpdateFrequency, true);
		}
	}

	for (TActorIterator<APlayerState> It(World); It; ++It)
	{
		AddPlayerState(*It);
	}
}

void AGameStateBase::OnRep_GameModeClass()
{
	ReceivedGameModeClass();
}

void AGameStateBase::OnRep_SpectatorClass()
{
	ReceivedSpectatorClass();
}

void AGameStateBase::ReceivedGameModeClass()
{
	// Tell each PlayerController that the Game class is here
	for( FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator )
	{
		APlayerController* const PlayerController = Iterator->Get();
		if (PlayerController)
		{
			PlayerController->ReceivedGameModeClass(GameModeClass);
		}
	}
}

void AGameStateBase::ReceivedSpectatorClass()
{
	// Tell each PlayerController that the Spectator class is here
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* const PlayerController = Iterator->Get();
		if (PlayerController && PlayerController->IsLocalController())
		{
			PlayerController->ReceivedSpectatorClass(SpectatorClass);
		}
	}
}

void AGameStateBase::SeamlessTravelTransitionCheckpoint(bool bToTransitionMap)
{
	// mark all existing player states as from previous level for various bookkeeping
	for (int32 i = 0; i < PlayerArray.Num(); i++)
	{
		PlayerArray[i]->bFromPreviousLevel = true;
	}
}

void AGameStateBase::AddPlayerState(APlayerState* PlayerState)
{
	// Determine whether it should go in the active or inactive list
	if (!PlayerState->bIsInactive)
	{
		// make sure no duplicates
		PlayerArray.AddUnique(PlayerState);
	}
}

void AGameStateBase::RemovePlayerState(APlayerState* PlayerState)
{
	for (int32 i=0; i<PlayerArray.Num(); i++)
	{
		if (PlayerArray[i] == PlayerState)
		{
			PlayerArray.RemoveAt(i,1);
			return;
		}
	}
}

float AGameStateBase::GetServerWorldTimeSeconds() const
{
	UWorld* World = GetWorld();
	if (World)
	{
		return World->GetTimeSeconds() + ServerWorldTimeSecondsDelta;
	}

	return 0.f;
}

void AGameStateBase::UpdateServerTimeSeconds()
{
	UWorld* World = GetWorld();
	if (World)
	{
		ReplicatedWorldTimeSeconds = World->GetTimeSeconds();
	}
}

void AGameStateBase::OnRep_ReplicatedWorldTimeSeconds()
{
	UWorld* World = GetWorld();
	if (World)
	{
		ServerWorldTimeSecondsDelta = ReplicatedWorldTimeSeconds - World->GetTimeSeconds();
	}
}

void AGameStateBase::OnRep_ReplicatedHasBegunPlay()
{
	if (bReplicatedHasBegunPlay && Role != ROLE_Authority)
	{
		GetWorldSettings()->NotifyBeginPlay();
		GetWorldSettings()->NotifyMatchStarted();
	}
}

void AGameStateBase::HandleBeginPlay()
{
	bReplicatedHasBegunPlay = true;

	GetWorldSettings()->NotifyBeginPlay();
	GetWorldSettings()->NotifyMatchStarted();
}

bool AGameStateBase::HasBegunPlay() const
{
	UWorld* World = GetWorld();
	if (World)
	{
		return World->bBegunPlay;
	}

	return false;
}

bool AGameStateBase::HasMatchStarted() const
{
	UWorld* World = GetWorld();
	if (World)
	{
		return World->bMatchStarted;
	}

	return false;
}

float AGameStateBase::GetPlayerStartTime(AController* Controller) const
{
	return GetServerWorldTimeSeconds();
}

float AGameStateBase::GetPlayerRespawnDelay(AController* Controller) const
{
	return 1.0f;
}

void AGameStateBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AGameStateBase, SpectatorClass );

	DOREPLIFETIME_CONDITION( AGameStateBase, GameModeClass,	COND_InitialOnly );

	DOREPLIFETIME( AGameStateBase, ReplicatedWorldTimeSeconds );
	DOREPLIFETIME( AGameStateBase, bReplicatedHasBegunPlay );
}
