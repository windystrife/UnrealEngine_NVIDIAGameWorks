// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerPlayerManager.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "GameplayDebuggerLocalController.h"
#include "Engine/DebugCameraController.h"

AGameplayDebuggerPlayerManager::AGameplayDebuggerPlayerManager(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = 0.5f;

#if WITH_EDITOR
	SetIsTemporarilyHiddenInEditor(true);
#endif

#if WITH_EDITORONLY_DATA
	bHiddenEdLevel = true;
	bHiddenEdLayer = true;
	bHiddenEd = true;
	bEditable = false;
#endif

	bIsLocal = false;
	bInitialized = false;
}

void AGameplayDebuggerPlayerManager::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	const ENetMode NetMode = World->GetNetMode();
	
	bHasAuthority = (NetMode != NM_Client);
	bIsLocal = (NetMode != NM_DedicatedServer);
	bInitialized = true;

	if (bHasAuthority)
	{
		UpdateAuthReplicators();
		SetActorTickEnabled(true);
	}
	
	for (int32 Idx = 0; Idx < PendingRegistrations.Num(); Idx++)
	{
		RegisterReplicator(*PendingRegistrations[Idx]);
	}

	PendingRegistrations.Empty();
}

void AGameplayDebuggerPlayerManager::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);

	for (int32 Idx = 0; Idx < PlayerData.Num(); Idx++)
	{
		FGameplayDebuggerPlayerData& TestData = PlayerData[Idx];
		if (IsValid(TestData.Controller))
		{
			TestData.Controller->Cleanup();
			TestData.Controller = nullptr;
		}
	}
}

void AGameplayDebuggerPlayerManager::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	UpdateAuthReplicators();
};

void AGameplayDebuggerPlayerManager::UpdateAuthReplicators()
{
	UWorld* World = GetWorld();
	for (int32 Idx = PlayerData.Num() - 1; Idx >= 0; Idx--)
	{
		FGameplayDebuggerPlayerData& TestData = PlayerData[Idx];

		if (!IsValid(TestData.Replicator) || !IsValid(TestData.Replicator->GetReplicationOwner()))
		{
			if (IsValid(TestData.Replicator))
			{
				World->DestroyActor(TestData.Replicator);
			}

			if (IsValid(TestData.Controller))
			{
				TestData.Controller->Cleanup();
			}

			PlayerData.RemoveAt(Idx, 1, false);
		}
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; It++)
	{
		APlayerController* TestPC = It->Get();
		if (TestPC && !TestPC->IsA<ADebugCameraController>())
		{
			const bool bNeedsReplicator = (GetReplicator(*TestPC) == nullptr);
			if (bNeedsReplicator)
			{
				AGameplayDebuggerCategoryReplicator* Replicator = World->SpawnActorDeferred<AGameplayDebuggerCategoryReplicator>(AGameplayDebuggerCategoryReplicator::StaticClass(), FTransform::Identity);
				Replicator->SetReplicatorOwner(TestPC);
				Replicator->FinishSpawning(FTransform::Identity, true);
			}
		}
	}

	PrimaryActorTick.TickInterval = PlayerData.Num() ? 5.0f : 0.5f;
}

void AGameplayDebuggerPlayerManager::RegisterReplicator(AGameplayDebuggerCategoryReplicator& Replicator)
{
	APlayerController* OwnerPC = Replicator.GetReplicationOwner();
	if (OwnerPC == nullptr)
	{
		return;
	}

	if (!bInitialized)
	{
		PendingRegistrations.Add(&Replicator);
		return;
	}

	// keep all player related objects together for easy access and GC
	FGameplayDebuggerPlayerData NewData;
	NewData.Replicator = &Replicator;
	
	if (bIsLocal)
	{
		NewData.InputComponent = NewObject<UInputComponent>(OwnerPC, TEXT("GameplayDebug_Input"));
		NewData.InputComponent->Priority = -1;

		NewData.Controller = NewObject<UGameplayDebuggerLocalController>(OwnerPC, TEXT("GameplayDebug_Controller"));
		NewData.Controller->Initialize(Replicator, *this);
		NewData.Controller->BindInput(*NewData.InputComponent);

		OwnerPC->PushInputComponent(NewData.InputComponent);
	}
	else
	{
		NewData.Controller = nullptr;
		NewData.InputComponent = nullptr;
	}

	PlayerData.Add(NewData);
}

void AGameplayDebuggerPlayerManager::RefreshInputBindings(AGameplayDebuggerCategoryReplicator& Replicator)
{
	for (int32 Idx = 0; Idx < PlayerData.Num(); Idx++)
	{
		FGameplayDebuggerPlayerData& TestData = PlayerData[Idx];
		if (TestData.Replicator == &Replicator)
		{
			TestData.InputComponent->ClearActionBindings();
			TestData.InputComponent->ClearBindingValues();
			TestData.InputComponent->KeyBindings.Empty();

			TestData.Controller->BindInput(*TestData.InputComponent);
		}
	}
}

AGameplayDebuggerCategoryReplicator* AGameplayDebuggerPlayerManager::GetReplicator(const APlayerController& OwnerPC) const
{
	const FGameplayDebuggerPlayerData* DataPtr = GetPlayerData(OwnerPC);
	return DataPtr ? DataPtr->Replicator : nullptr;
}

UInputComponent* AGameplayDebuggerPlayerManager::GetInputComponent(const APlayerController& OwnerPC) const
{
	const FGameplayDebuggerPlayerData* DataPtr = GetPlayerData(OwnerPC);
	return DataPtr ? DataPtr->InputComponent : nullptr;
}

UGameplayDebuggerLocalController* AGameplayDebuggerPlayerManager::GetLocalController(const APlayerController& OwnerPC) const
{
	const FGameplayDebuggerPlayerData* DataPtr = GetPlayerData(OwnerPC);
	return DataPtr ? DataPtr->Controller : nullptr;
}

const FGameplayDebuggerPlayerData* AGameplayDebuggerPlayerManager::GetPlayerData(const APlayerController& OwnerPC) const
{
	for (int32 Idx = 0; Idx < PlayerData.Num(); Idx++)
	{
		const FGameplayDebuggerPlayerData& TestData = PlayerData[Idx];
		if (TestData.Replicator && TestData.Replicator->GetReplicationOwner() == &OwnerPC)
		{
			return &TestData;
		}
	}

	return nullptr;
}
