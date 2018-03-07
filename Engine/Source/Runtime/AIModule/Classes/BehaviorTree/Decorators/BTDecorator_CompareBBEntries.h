// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_CompareBBEntries.generated.h"

class UBehaviorTree;
class UBlackboardComponent;

UENUM()
namespace EBlackBoardEntryComparison
{
	enum Type
	{
		Equal			UMETA(DisplayName="Is Equal To"),
		NotEqual		UMETA(DisplayName="Is Not Equal To")
	};
}

/**
 * Blackboard comparison decorator node.
 * A decorator node that bases its condition on a comparison between two Blackboard keys.
 */
UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_CompareBBEntries : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

protected:

	/** operation type */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	TEnumAsByte<EBlackBoardEntryComparison::Type> Operator;

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector BlackboardKeyA;

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector BlackboardKeyB;

public:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual FString GetStaticDescription() const override;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;

	virtual EBlackboardNotificationResult OnBlackboardKeyValueChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
