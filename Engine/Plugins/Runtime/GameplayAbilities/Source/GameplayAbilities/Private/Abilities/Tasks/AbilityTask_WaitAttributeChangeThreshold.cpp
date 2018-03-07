// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"

#include "AbilitySystemComponent.h"


UAbilityTask_WaitAttributeChangeThreshold::UAbilityTask_WaitAttributeChangeThreshold(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTriggerOnce = false;
	bMatchedComparisonLastAttributeChange = false;
}

UAbilityTask_WaitAttributeChangeThreshold* UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(UGameplayAbility* OwningAbility, FGameplayAttribute Attribute, TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType, float ComparisonValue, bool bTriggerOnce)
{
	UAbilityTask_WaitAttributeChangeThreshold* MyTask = NewAbilityTask<UAbilityTask_WaitAttributeChangeThreshold>(OwningAbility);
	MyTask->Attribute = Attribute;
	MyTask->ComparisonType = ComparisonType;
	MyTask->ComparisonValue = ComparisonValue;
	MyTask->bTriggerOnce = bTriggerOnce;

	return MyTask;
}

void UAbilityTask_WaitAttributeChangeThreshold::Activate()
{
	if (AbilitySystemComponent)
	{
		const float CurrentValue = AbilitySystemComponent->GetNumericAttribute(Attribute);
		bMatchedComparisonLastAttributeChange = DoesValuePassComparison(CurrentValue);

		// Broadcast OnChange immediately with current value
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnChange.Broadcast(bMatchedComparisonLastAttributeChange, CurrentValue);
		}

		OnAttributeChangeDelegateHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UAbilityTask_WaitAttributeChangeThreshold::OnAttributeChange);
	}
}

void UAbilityTask_WaitAttributeChangeThreshold::OnAttributeChange(const FOnAttributeChangeData& CallbackData)
{
	float NewValue = CallbackData.NewValue;

	bool bPassedComparison = DoesValuePassComparison(NewValue);
	if (bPassedComparison != bMatchedComparisonLastAttributeChange)
	{
		bMatchedComparisonLastAttributeChange = bPassedComparison;
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnChange.Broadcast(bPassedComparison, NewValue);
		}
		if (bTriggerOnce)
		{
			EndTask();
		}
	}
}

bool UAbilityTask_WaitAttributeChangeThreshold::DoesValuePassComparison(float Value) const
{
	bool bPassedComparison = true;
	switch (ComparisonType)
	{
	case EWaitAttributeChangeComparison::ExactlyEqualTo:
		bPassedComparison = (Value == ComparisonValue);
		break;		
	case EWaitAttributeChangeComparison::GreaterThan:
		bPassedComparison = (Value > ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::GreaterThanOrEqualTo:
		bPassedComparison = (Value >= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThan:
		bPassedComparison = (Value < ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::LessThanOrEqualTo:
		bPassedComparison = (Value <= ComparisonValue);
		break;
	case EWaitAttributeChangeComparison::NotEqualTo:
		bPassedComparison = (Value != ComparisonValue);
		break;
	default:
		break;
	}
	return bPassedComparison;
}

void UAbilityTask_WaitAttributeChangeThreshold::OnDestroy(bool AbilityEnded)
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
	}

	Super::OnDestroy(AbilityEnded);
}
