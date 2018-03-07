// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "AITypes.h"

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "DataProviders/AIDataProvider.h"
#include "BehaviorTree/Decorators/BTDecorator_BlackboardBase.h"
#include "BTDecorator_IsAtLocation.generated.h"

/**
 * Is At Location decorator node.
 * A decorator node that checks if AI controlled pawn is at given location.
 */
UCLASS()
class AIMODULE_API UBTDecorator_IsAtLocation : public UBTDecorator_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	/** distance threshold to accept as being at location */
	UPROPERTY(EditAnywhere, Category = Condition, meta = (ClampMin = "0.0", EditCondition = "!bUseParametrizedRadius"))
	float AcceptableRadius;

	UPROPERTY(EditAnywhere, Category = Condition, meta = (EditCondition = "bUseParametrizedRadius"))
	FAIDataProviderFloatValue ParametrizedAcceptableRadius;

	UPROPERTY(EditAnywhere, Category = Condition, meta = (EditCondition = "!bPathFindingBasedTest"))
	FAIDistanceType GeometricDistanceType;

	UPROPERTY()
	uint32 bUseParametrizedRadius : 1;

	/** if moving to an actor and this actor is a nav agent, then we will move to their nav agent location */
	UPROPERTY(EditAnywhere, Category = Condition, meta = (EditCondition = "bPathFindingBasedTest"))
	uint32 bUseNavAgentGoalLocation : 1;

	/** If true the result will be consistent with tests done while following paths.
	 *	Set to false to use geometric distance as configured with DistanceType */
	UPROPERTY(EditAnywhere, Category = Condition)
	uint32 bPathFindingBasedTest : 1;

	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;
	
	virtual FString GetStaticDescription() const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR

protected:
	float GetGeometricDistanceSquared(const FVector& A, const FVector& B) const;
};
