// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayEffectCustomApplicationRequirement.h"

UGameplayEffectCustomApplicationRequirement::UGameplayEffectCustomApplicationRequirement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UGameplayEffectCustomApplicationRequirement::CanApplyGameplayEffect_Implementation(const UGameplayEffect* GameplayEffect, const FGameplayEffectSpec& Spec, UAbilitySystemComponent* ASC) const
{
	return true;
}
