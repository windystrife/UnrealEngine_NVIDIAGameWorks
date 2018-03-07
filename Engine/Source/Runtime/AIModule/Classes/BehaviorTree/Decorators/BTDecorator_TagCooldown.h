// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_TagCooldown.generated.h"

struct FBTTagCooldownDecoratorMemory
{	
	uint8 bRequestedRestart : 1;
};

/**
 * Cooldown decorator node.
 * A decorator node that bases its condition on whether a cooldown timer based on a gameplay tag has expired.
 */
UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_TagCooldown : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	/** Gameplay tag that will be used for the cooldown. */
	UPROPERTY(Category = Decorator, EditAnywhere)
	FGameplayTag CooldownTag;

	/** Value we will add or set to the Cooldown tag when this node is deactivated. */
	UPROPERTY(Category = Decorator, EditAnywhere, meta = (EditCondition = "bActivatesCooldown"))
	float CooldownDuration;

	/** True if we are adding to any existing duration, false if we are setting the duration (potentially invalidating an existing end time). */
	UPROPERTY(Category = Decorator, EditAnywhere, meta = (EditCondition = "bActivatesCooldown"))
	bool bAddToExistingDuration;

	/** Whether or not we are adding/setting to the cooldown tag's value when the decorator deactivates. */
	UPROPERTY(Category = Decorator, EditAnywhere, meta = (DisplayName = "Adds/Sets Cooldown on Deactivation"))
	bool bActivatesCooldown;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTMemoryInit::Type InitType) const override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:

	virtual void OnNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type NodeResult) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	bool HasCooldownFinished(const UBehaviorTreeComponent& OwnerComp) const;
};
