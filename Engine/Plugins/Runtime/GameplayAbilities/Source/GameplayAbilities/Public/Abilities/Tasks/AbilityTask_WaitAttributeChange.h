// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_WaitAttributeChange.generated.h"

struct FGameplayEffectModCallbackData;

UENUM()
namespace EWaitAttributeChangeComparison
{
	enum Type
	{
		None,
		GreaterThan,
		LessThan,
		GreaterThanOrEqualTo,
		LessThanOrEqualTo,
		NotEqualTo,
		ExactlyEqualTo,
		MAX UMETA(Hidden)
	};
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitAttributeChangeDelegate);

/**
 *	Waits for the actor to activate another ability
 */
UCLASS()
class GAMEPLAYABILITIES_API UAbilityTask_WaitAttributeChange : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeDelegate	OnChange;

	virtual void Activate() override;

	void OnAttributeChange(const FOnAttributeChangeData& CallbackData);

	/** Wait until an attribute changes. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitAttributeChange* WaitForAttributeChange(UGameplayAbility* OwningAbility, FGameplayAttribute Attribute, FGameplayTag WithSrcTag, FGameplayTag WithoutSrcTag, bool TriggerOnce=true);

	/** Wait until an attribute changes to pass a given test. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitAttributeChange* WaitForAttributeChangeWithComparison(UGameplayAbility* OwningAbility, FGameplayAttribute InAttribute, FGameplayTag InWithTag, FGameplayTag InWithoutTag, TEnumAsByte<EWaitAttributeChangeComparison::Type> InComparisonType, float InComparisonValue, bool TriggerOnce=true);

	FGameplayTag WithTag;
	FGameplayTag WithoutTag;
	FGameplayAttribute	Attribute;
	TEnumAsByte<EWaitAttributeChangeComparison::Type> ComparisonType;
	float ComparisonValue;
	bool bTriggerOnce;
	FDelegateHandle OnAttributeChangeDelegateHandle;

protected:

	virtual void OnDestroy(bool AbilityEnded) override;
};
