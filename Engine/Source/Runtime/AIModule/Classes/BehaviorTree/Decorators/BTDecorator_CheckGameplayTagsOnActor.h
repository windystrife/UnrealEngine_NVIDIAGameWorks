// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayTagContainer.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CheckGameplayTagsOnActor.generated.h"

class UBehaviorTree;

/**
 * GameplayTag decorator node.
 * A decorator node that bases its condition on whether the specified Actor (in the blackboard) has a Gameplay Tag or
 * Tags specified.
 */
UCLASS()
class AIMODULE_API UBTDecorator_CheckGameplayTagsOnActor : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

protected:

	UPROPERTY(EditAnywhere, Category=GameplayTagCheck,
		Meta=(ToolTips="Which Actor (from the blackboard) should be checked for these gameplay tags?"))
	struct FBlackboardKeySelector ActorToCheck;

	UPROPERTY(EditAnywhere, Category=GameplayTagCheck)
	EGameplayContainerMatchType TagsToMatch;

	UPROPERTY(EditAnywhere, Category=GameplayTagCheck)
	FGameplayTagContainer GameplayTags;

	/** cached description */
	UPROPERTY()
	FString CachedDescription;

#if WITH_EDITOR
	/** describe decorator and cache it */
	virtual void BuildDescription();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
};
