// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectStackChange.h"

#include "AbilitySystemComponent.h"


UAbilityTask_WaitGameplayEffectStackChange::UAbilityTask_WaitGameplayEffectStackChange(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Registered = false;
}

UAbilityTask_WaitGameplayEffectStackChange* UAbilityTask_WaitGameplayEffectStackChange::WaitForGameplayEffectStackChange(UGameplayAbility* OwningAbility, FActiveGameplayEffectHandle InHandle)
{
	UAbilityTask_WaitGameplayEffectStackChange* MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEffectStackChange>(OwningAbility);
	MyObj->Handle = InHandle;
	return MyObj;
}

void UAbilityTask_WaitGameplayEffectStackChange::Activate()
{
	if (Handle.IsValid() == false)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			InvalidHandle.Broadcast(Handle, 0, 0);
		}
		EndTask();
		return;;
	}

	UAbilitySystemComponent* EffectOwningAbilitySystemComponent = Handle.GetOwningAbilitySystemComponent();

	if (EffectOwningAbilitySystemComponent)
	{
		FOnActiveGameplayEffectStackChange* DelPtr = EffectOwningAbilitySystemComponent->OnGameplayEffectStackChangeDelegate(Handle);
		if (DelPtr)
		{
			OnGameplayEffectStackChangeDelegateHandle = DelPtr->AddUObject(this, &UAbilityTask_WaitGameplayEffectStackChange::OnGameplayEffectStackChange);
			Registered = true;
		}
	}
}

void UAbilityTask_WaitGameplayEffectStackChange::OnDestroy(bool AbilityIsEnding)
{
	UAbilitySystemComponent* EffectOwningAbilitySystemComponent = Handle.GetOwningAbilitySystemComponent();
	if (EffectOwningAbilitySystemComponent && OnGameplayEffectStackChangeDelegateHandle.IsValid())
	{
		FOnActiveGameplayEffectRemoved_Info* DelPtr = EffectOwningAbilitySystemComponent->OnGameplayEffectRemoved_InfoDelegate(Handle);
		if (DelPtr)
		{
			DelPtr->Remove(OnGameplayEffectStackChangeDelegateHandle);
		}
	}

	Super::OnDestroy(AbilityIsEnding);
}

void UAbilityTask_WaitGameplayEffectStackChange::OnGameplayEffectStackChange(FActiveGameplayEffectHandle InHandle, int32 NewCount, int32 OldCount)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnChange.Broadcast(InHandle, NewCount, OldCount);
	}
}
