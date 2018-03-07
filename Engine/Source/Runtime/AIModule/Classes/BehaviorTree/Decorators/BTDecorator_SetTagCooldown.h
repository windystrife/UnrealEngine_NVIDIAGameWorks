// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_SetTagCooldown.generated.h"

/**
 * Set tag cooldown decorator node.
 * A decorator node that sets a gameplay tag cooldown.
 */
UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_SetTagCooldown : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	/** Gameplay tag that will be used for the cooldown. */
	UPROPERTY(Category = Decorator, EditAnywhere)
	FGameplayTag CooldownTag;

	/** Value we will add or set to the Cooldown tag when this task runs. */
	UPROPERTY(Category = Decorator, EditAnywhere)
	float CooldownDuration;

	/** True if we are adding to any existing duration, false if we are setting the duration (potentially invalidating an existing end time). */
	UPROPERTY(Category = Decorator, EditAnywhere)
	bool bAddToExistingDuration;

	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:
	virtual void OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult) override;
};
