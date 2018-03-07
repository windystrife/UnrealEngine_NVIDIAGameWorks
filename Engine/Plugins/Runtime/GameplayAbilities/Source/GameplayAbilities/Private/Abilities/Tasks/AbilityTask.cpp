// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemStats.h"

int32 UAbilityTask::GlobalAbilityTaskCount = 0;

UAbilityTask::UAbilityTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WaitStateBitMask = (uint8)EAbilityTaskWaitState::WaitingOnGame;
	bWasSuccessfullyDestroyed = false;
	SET_DWORD_STAT(STAT_AbilitySystem_TaskCount, ++GlobalAbilityTaskCount);
	if (!ensure(GlobalAbilityTaskCount < 1000))
	{
		ABILITY_LOG(Warning, TEXT("Way too many AbilityTasks are currently active! %d. %s"), GlobalAbilityTaskCount, *GetClass()->GetName());
	}
}

void UAbilityTask::OnDestroy(bool bInOwnerFinished)
{
	SET_DWORD_STAT(STAT_AbilitySystem_TaskCount, --GlobalAbilityTaskCount);
	bWasSuccessfullyDestroyed = true;

	Super::OnDestroy(bInOwnerFinished);
}

void UAbilityTask::BeginDestroy()
{
	Super::BeginDestroy();
	
	if (!bWasSuccessfullyDestroyed)
	{
		// this shouldn't happen, it means that ability was destroyed while being active, but we need to keep GlobalAbilityTaskCount in sync anyway

		SET_DWORD_STAT(STAT_AbilitySystem_TaskCount, --GlobalAbilityTaskCount);
		bWasSuccessfullyDestroyed = true;
	}
}

FGameplayAbilitySpecHandle UAbilityTask::GetAbilitySpecHandle() const
{
	return Ability ? Ability->GetCurrentAbilitySpecHandle() : FGameplayAbilitySpecHandle();
}

void UAbilityTask::SetAbilitySystemComponent(UAbilitySystemComponent* InAbilitySystemComponent)
{
	AbilitySystemComponent = InAbilitySystemComponent;
}

void UAbilityTask::InitSimulatedTask(UGameplayTasksComponent& InGameplayTasksComponent)
{
	UGameplayTask::InitSimulatedTask(InGameplayTasksComponent);

	SetAbilitySystemComponent(Cast<UAbilitySystemComponent>(TasksComponent.Get()));
}

FPredictionKey UAbilityTask::GetActivationPredictionKey() const
{
	return Ability ? Ability->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
}

int32 AbilityTaskWarnIfBroadcastSuppress = 0;
static FAutoConsoleVariableRef CVarAbilityTaskWarnIfBroadcastSuppress(TEXT("AbilitySystem.AbilityTaskWarnIfBroadcastSuppress"), AbilityTaskWarnIfBroadcastSuppress, TEXT("Print warning if an ability task broadcast is suppressed because the ability has ended"), ECVF_Default );

bool UAbilityTask::ShouldBroadcastAbilityTaskDelegates() const
{
	bool ShouldBroadcast = (Ability && Ability->IsActive());

	if (!ShouldBroadcast && AbilityTaskWarnIfBroadcastSuppress)
	{
		ABILITY_LOG(Warning, TEXT("Suppressing ability task %s broadcsat"), *GetDebugString());
	}

	return ShouldBroadcast;
}

bool UAbilityTask::IsPredictingClient() const
{
	return Ability && Ability->IsPredictingClient();
}

bool UAbilityTask::IsForRemoteClient() const
{
	return Ability && Ability->IsForRemoteClient();
}

bool UAbilityTask::IsLocallyControlled() const
{
	return Ability && Ability->IsLocallyControlled();
}

bool UAbilityTask::CallOrAddReplicatedDelegate(EAbilityGenericReplicatedEvent::Type Event, FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!AbilitySystemComponent->CallOrAddReplicatedDelegate(Event, GetAbilitySpecHandle(), GetActivationPredictionKey(), Delegate))
	{
		SetWaitingOnRemotePlayerData();
		return false;
	}
	return true;
}

void UAbilityTask::SetWaitingOnRemotePlayerData()
{
	if (Ability && IsPendingKill() == false && AbilitySystemComponent)
	{
		WaitStateBitMask |= (uint8)EAbilityTaskWaitState::WaitingOnUser;
		Ability->NotifyAbilityTaskWaitingOnPlayerData(this);
	}
}

void UAbilityTask::ClearWaitingOnRemotePlayerData()
{
	WaitStateBitMask &= ~((uint8)EAbilityTaskWaitState::WaitingOnUser);
}

bool UAbilityTask::IsWaitingOnRemotePlayerdata() const
{
	return (WaitStateBitMask & (uint8)EAbilityTaskWaitState::WaitingOnUser) != 0;
}

void UAbilityTask::SetWaitingOnAvatar()
{
	if (Ability && IsPendingKill() == false && AbilitySystemComponent)
	{
		WaitStateBitMask |= (uint8)EAbilityTaskWaitState::WaitingOnAvatar;
		Ability->NotifyAbilityTaskWaitingOnAvatar(this);
	}
}

void UAbilityTask::ClearWaitingOnAvatar()
{
	WaitStateBitMask &= ~((uint8)EAbilityTaskWaitState::WaitingOnAvatar);
}

bool UAbilityTask::IsWaitingOnAvatar() const
{
	return (WaitStateBitMask & (uint8)EAbilityTaskWaitState::WaitingOnAvatar) != 0;
}
