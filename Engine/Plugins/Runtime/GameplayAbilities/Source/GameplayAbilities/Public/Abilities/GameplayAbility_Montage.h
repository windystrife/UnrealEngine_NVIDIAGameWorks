// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbility_Montage.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;

/**
 *	A gameplay ability that plays a single montage and applies a GameplayEffect
 */
UCLASS()
class GAMEPLAYABILITIES_API UGameplayAbility_Montage : public UGameplayAbility
{
	GENERATED_UCLASS_BODY()

public:
	
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* OwnerInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	UAnimMontage *	MontageToPlay;

	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	float	PlayRate;

	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	FName	SectionName;

	/** GameplayEffects to apply and then remove while the animation is playing */
	UPROPERTY(EditDefaultsOnly, Category = MontageAbility)
	TArray<TSubclassOf<UGameplayEffect>> GameplayEffectClassesWhileAnimating;

	/** Deprecated. Use GameplayEffectClassesWhileAnimating instead. */
	UPROPERTY(VisibleDefaultsOnly, Category = Deprecated)
	TArray<const UGameplayEffect*>	GameplayEffectsWhileAnimating;

	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent, TArray<struct FActiveGameplayEffectHandle>	AppliedEffects);

	void GetGameplayEffectsWhileAnimating(TArray<const UGameplayEffect*>& OutEffects) const;
};
