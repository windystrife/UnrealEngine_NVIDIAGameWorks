// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayAbilitySet.h"
#include "AbilitySystemComponent.h"


// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	UGameplayAbilitySet
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------


UGameplayAbilitySet::UGameplayAbilitySet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UGameplayAbilitySet::GiveAbilities(UAbilitySystemComponent* AbilitySystemComponent) const
{
	for (const FGameplayAbilityBindInfo& BindInfo : Abilities)
	{
		if (BindInfo.GameplayAbilityClass)
		{
			AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(BindInfo.GameplayAbilityClass->GetDefaultObject<UGameplayAbility>(), 1, (int32)BindInfo.Command));
		}
	}
}
