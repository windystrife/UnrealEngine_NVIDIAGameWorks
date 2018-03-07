// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_KeepInCone.generated.h"

class UBehaviorTree;

struct FBTKeepInConeDecoratorMemory
{
	FVector InitialDirection;
};

/**
 * Cooldown decorator node.
 * A decorator node that bases its condition on whether the observed position is still inside a cone. The cone's direction is calculated when the node first becomes relevant.
 */
UCLASS(HideCategories=(Condition))
class AIMODULE_API UBTDecorator_KeepInCone : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	typedef FBTKeepInConeDecoratorMemory TNodeInstanceMemory;

	/** max allowed time for execution of underlying node */
	UPROPERTY(Category=Decorator, EditAnywhere)
	float ConeHalfAngle;
	
	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector ConeOrigin;

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector Observed;

	// deprecated, set value of ConeOrigin on initialization
	UPROPERTY()
	uint32 bUseSelfAsOrigin:1;

	// deprecated, set value of Observed on initialization
	UPROPERTY()
	uint32 bUseSelfAsObserved:1;
	
	float ConeHalfAngleDot;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:

	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	bool CalculateCurrentDirection(const UBehaviorTreeComponent& OwnerComp, FVector& Direction) const;
};
