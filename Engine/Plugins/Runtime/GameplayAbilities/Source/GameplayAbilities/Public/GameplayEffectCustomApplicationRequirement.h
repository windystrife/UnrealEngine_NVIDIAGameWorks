// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once


#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "GameplayEffect.h"
#include "GameplayEffectCustomApplicationRequirement.generated.h"

class UAbilitySystemComponent;

/** Class used to perform custom gameplay effect modifier calculations, either via blueprint or native code */ 
UCLASS(BlueprintType, Blueprintable, Abstract)
class GAMEPLAYABILITIES_API UGameplayEffectCustomApplicationRequirement : public UObject
{

public:
	GENERATED_UCLASS_BODY()
	
	/** Return whether the gameplay effect should be applied or not */
	UFUNCTION(BlueprintNativeEvent, Category="Calculation")
	bool CanApplyGameplayEffect(const UGameplayEffect* GameplayEffect, const FGameplayEffectSpec& Spec, UAbilitySystemComponent* ASC) const;
};
