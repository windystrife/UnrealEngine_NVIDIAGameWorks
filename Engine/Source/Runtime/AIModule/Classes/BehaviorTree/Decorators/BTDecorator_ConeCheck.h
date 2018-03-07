// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "BehaviorTree/BTDecorator.h"
#include "BTDecorator_ConeCheck.generated.h"

class UBehaviorTree;
class UBlackboardComponent;

struct FBTConeCheckDecoratorMemory
{
	bool bLastRawResult;
};

/**
 * Cone check decorator node.
 * A decorator node that bases its condition on a cone check, using Blackboard entries to form the parameters of the check.
 */
UCLASS()
class AIMODULE_API UBTDecorator_ConeCheck : public UBTDecorator
{
	GENERATED_UCLASS_BODY()

	typedef FBTConeCheckDecoratorMemory TNodeInstanceMemory;

	/** Angle between cone direction and code cone edge, or a half of the total cone angle */
	UPROPERTY(Category=Decorator, EditAnywhere)
	float ConeHalfAngle;
	
	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector ConeOrigin;

	/** "None" means "use ConeOrigin's direction" */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector ConeDirection;

	/** blackboard key selector */
	UPROPERTY(EditAnywhere, Category=Blackboard)
	FBlackboardKeySelector Observed;
	
	float ConeHalfAngleDot;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	virtual void OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	void OnBlackboardChange(const UBlackboardComponent& Blackboard, FBlackboard::FKey ChangedKeyID);

	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	
	bool CalculateDirection(const UBlackboardComponent* BlackboardComp, const FBlackboardKeySelector& Origin, const FBlackboardKeySelector& End, FVector& Direction) const;

private:
	bool CalcConditionImpl(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const;
};
