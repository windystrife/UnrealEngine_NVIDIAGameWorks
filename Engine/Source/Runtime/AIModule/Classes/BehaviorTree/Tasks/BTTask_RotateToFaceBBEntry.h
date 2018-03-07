// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "BTTask_RotateToFaceBBEntry.generated.h"

class AAIController;

/**
 * 
 */
UCLASS(config = Game)
class AIMODULE_API UBTTask_RotateToFaceBBEntry : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

protected:
	/** Success condition precision in degrees */
	UPROPERTY(config, Category = Node, EditAnywhere, meta = (ClampMin = "0.0"))
	float Precision;

private:
	/** cached Precision tangent value */
	float PrecisionDot;

public:

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;
	
	virtual uint16 GetInstanceMemorySize() const override { return sizeof(FBTFocusMemory); }

protected:

	float GetPrecisionDot() const { return PrecisionDot; }
	void CleanUp(AAIController& AIController, uint8* NodeMemory);
};
