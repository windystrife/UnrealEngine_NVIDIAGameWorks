// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitVelocityChange.h"
#include "GameFramework/MovementComponent.h"

UAbilityTask_WaitVelocityChange::UAbilityTask_WaitVelocityChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

void UAbilityTask_WaitVelocityChange::TickTask(float DeltaTime)
{
	if (CachedMovementComponent)
	{
		float dot = FVector::DotProduct(Direction, CachedMovementComponent->Velocity);

		if (dot > MinimumMagnitude)
		{
			if (ShouldBroadcastAbilityTaskDelegates())
			{
				OnVelocityChage.Broadcast();
			}
			EndTask();
		}
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("UAbilityTask_WaitVelocityChange ticked without a valid movement component. ending."));
		EndTask();
	}
}

UAbilityTask_WaitVelocityChange* UAbilityTask_WaitVelocityChange::CreateWaitVelocityChange(UGameplayAbility* OwningAbility, FVector InDirection, float InMinimumMagnitude)
{
	UAbilityTask_WaitVelocityChange* MyObj = NewAbilityTask<UAbilityTask_WaitVelocityChange>(OwningAbility);

	MyObj->MinimumMagnitude = InMinimumMagnitude;
	MyObj->Direction = InDirection.GetSafeNormal();
	

	return MyObj;
}

void UAbilityTask_WaitVelocityChange::Activate()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	CachedMovementComponent = ActorInfo->MovementComponent.Get();
	SetWaitingOnAvatar();
}
