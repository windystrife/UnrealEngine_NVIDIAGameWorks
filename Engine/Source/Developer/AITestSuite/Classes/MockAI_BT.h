// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "MockAI.h"
#include "MockAI_BT.generated.h"

class UBehaviorTree;
class UBehaviorTreeComponent;

UCLASS()
class UMockAI_BT : public UMockAI
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UBehaviorTreeComponent* BTComp;

	static TArray<int32> ExecutionLog;
	TArray<int32> ExpectedResult;

	bool IsRunning() const;
	void RunBT(UBehaviorTree& BTAsset, EBTExecutionMode::Type RunType = EBTExecutionMode::SingleRun);
};
