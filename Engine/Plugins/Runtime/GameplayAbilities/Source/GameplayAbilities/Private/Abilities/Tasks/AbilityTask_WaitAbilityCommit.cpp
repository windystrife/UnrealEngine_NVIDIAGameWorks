// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitAbilityCommit.h"

#include "AbilitySystemComponent.h"



UAbilityTask_WaitAbilityCommit::UAbilityTask_WaitAbilityCommit(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitAbilityCommit* UAbilityTask_WaitAbilityCommit::WaitForAbilityCommit(UGameplayAbility* OwningAbility, FGameplayTag InWithTag, FGameplayTag InWithoutTag, bool InTriggerOnce)
{
	UAbilityTask_WaitAbilityCommit* MyObj = NewAbilityTask<UAbilityTask_WaitAbilityCommit>(OwningAbility);
	MyObj->WithTag = InWithTag;
	MyObj->WithoutTag = InWithoutTag;
	MyObj->TriggerOnce = InTriggerOnce;

	return MyObj;
}

UAbilityTask_WaitAbilityCommit* UAbilityTask_WaitAbilityCommit::WaitForAbilityCommit_Query(UGameplayAbility* OwningAbility, FGameplayTagQuery Query, bool InTriggerOnce)
{
	UAbilityTask_WaitAbilityCommit* MyObj = NewAbilityTask<UAbilityTask_WaitAbilityCommit>(OwningAbility);
	MyObj->Query = Query;
	MyObj->TriggerOnce = InTriggerOnce;
	return MyObj;
}

void UAbilityTask_WaitAbilityCommit::Activate()
{
	if (AbilitySystemComponent)	
	{		
		OnAbilityCommitDelegateHandle = AbilitySystemComponent->AbilityCommittedCallbacks.AddUObject(this, &UAbilityTask_WaitAbilityCommit::OnAbilityCommit);
	}
}

void UAbilityTask_WaitAbilityCommit::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->AbilityCommittedCallbacks.Remove(OnAbilityCommitDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitAbilityCommit::OnAbilityCommit(UGameplayAbility *ActivatedAbility)
{
	if ( (WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag)) ||
		 (WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag)))
	{
		// Failed tag check
		return;
	}

	if (Query.IsEmpty() == false)
	{
		if (Query.Matches(ActivatedAbility->AbilityTags) == false)
		{
			// Failed query
			return;
		}
	}

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnCommit.Broadcast(ActivatedAbility);
	}

	if (TriggerOnce)
	{
		EndTask();
	}
}
