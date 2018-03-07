// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"

// ----------------------------------------------------------------

UAbilityTask_WaitGameplayEvent::UAbilityTask_WaitGameplayEvent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

UAbilityTask_WaitGameplayEvent* UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(UGameplayAbility* OwningAbility, FGameplayTag Tag, AActor* OptionalExternalTarget, bool OnlyTriggerOnce)
{
	UAbilityTask_WaitGameplayEvent* MyObj = NewAbilityTask<UAbilityTask_WaitGameplayEvent>(OwningAbility);
	MyObj->Tag = Tag;
	MyObj->SetExternalTarget(OptionalExternalTarget);
	MyObj->OnlyTriggerOnce = OnlyTriggerOnce;

	return MyObj;
}

void UAbilityTask_WaitGameplayEvent::Activate()
{
	UAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC)
	{	
		MyHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddUObject(this, &UAbilityTask_WaitGameplayEvent::GameplayEventCallback);
	}

	Super::Activate();
}

void UAbilityTask_WaitGameplayEvent::GameplayEventCallback(const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		EventReceived.Broadcast(*Payload);
	}
	if(OnlyTriggerOnce)
	{
		EndTask();
	}
}

void UAbilityTask_WaitGameplayEvent::SetExternalTarget(AActor* Actor)
{
	if (Actor)
	{
		UseExternalTarget = true;
		OptionalExternalTarget = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	}
}

UAbilitySystemComponent* UAbilityTask_WaitGameplayEvent::GetTargetASC()
{
	if (UseExternalTarget)
	{
		return OptionalExternalTarget;
	}

	return AbilitySystemComponent;
}

void UAbilityTask_WaitGameplayEvent::OnDestroy(bool AbilityEnding)
{
	UAbilitySystemComponent* ASC = GetTargetASC();
	if (ASC && MyHandle.IsValid())
	{	
		ASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).Remove(MyHandle);
	}

	Super::OnDestroy(AbilityEnding);
}
