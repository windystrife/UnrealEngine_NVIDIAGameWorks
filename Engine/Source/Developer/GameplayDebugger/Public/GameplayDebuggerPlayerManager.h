// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "GameplayDebuggerPlayerManager.generated.h"

class AGameplayDebuggerCategoryReplicator;
class APlayerController;
class UGameplayDebuggerLocalController;
class UInputComponent;

USTRUCT()
struct FGameplayDebuggerPlayerData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UGameplayDebuggerLocalController* Controller;

	UPROPERTY()
	UInputComponent* InputComponent;

	UPROPERTY()
	AGameplayDebuggerCategoryReplicator* Replicator;
};

UCLASS(NotBlueprintable, NotBlueprintType, notplaceable, noteditinlinenew, hidedropdown, Transient)
class AGameplayDebuggerPlayerManager : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
		
	void UpdateAuthReplicators();
	void RegisterReplicator(AGameplayDebuggerCategoryReplicator& Replicator);
	void RefreshInputBindings(AGameplayDebuggerCategoryReplicator& Replicator);

	AGameplayDebuggerCategoryReplicator* GetReplicator(const APlayerController& OwnerPC) const;
	UInputComponent* GetInputComponent(const APlayerController& OwnerPC) const;
	UGameplayDebuggerLocalController* GetLocalController(const APlayerController& OwnerPC) const;
	
	const FGameplayDebuggerPlayerData* GetPlayerData(const APlayerController& OwnerPC) const;

	static AGameplayDebuggerPlayerManager& GetCurrent(UWorld* World);

protected:

	UPROPERTY()
	TArray<FGameplayDebuggerPlayerData> PlayerData;

	UPROPERTY()
	TArray<AGameplayDebuggerCategoryReplicator*> PendingRegistrations;

	uint32 bHasAuthority : 1;
	uint32 bIsLocal : 1;
	uint32 bInitialized : 1;
};
