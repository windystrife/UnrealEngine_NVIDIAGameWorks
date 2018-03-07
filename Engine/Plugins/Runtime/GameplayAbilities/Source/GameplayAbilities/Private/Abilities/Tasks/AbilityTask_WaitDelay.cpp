// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "TimerManager.h"
#include "AbilitySystemGlobals.h"

UAbilityTask_WaitDelay::UAbilityTask_WaitDelay(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Time = 0.f;
	TimeStarted = 0.f;
}

UAbilityTask_WaitDelay* UAbilityTask_WaitDelay::WaitDelay(UGameplayAbility* OwningAbility, float Time)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(Time);

	UAbilityTask_WaitDelay* MyObj = NewAbilityTask<UAbilityTask_WaitDelay>(OwningAbility);
	MyObj->Time = Time;
	return MyObj;
}

void UAbilityTask_WaitDelay::Activate()
{
	UWorld* World = GetWorld();
	TimeStarted = World->GetTimeSeconds();

	// Use a dummy timer handle as we don't need to store it for later but we don't need to look for something to clear
	FTimerHandle TimerHandle;
	World->GetTimerManager().SetTimer(TimerHandle, this, &UAbilityTask_WaitDelay::OnTimeFinish, Time, false);
}

void UAbilityTask_WaitDelay::OnTimeFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}
	EndTask();
}

FString UAbilityTask_WaitDelay::GetDebugString() const
{
	float TimeLeft = Time - GetWorld()->TimeSince(TimeStarted);
	return FString::Printf(TEXT("WaitDelay. Time: %.2f. TimeLeft: %.2f"), Time, TimeLeft);
}