// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_SimpleParallel.generated.h"

namespace EBTParallelChild
{
	enum Type
	{
		MainTask,
		BackgroundTree,
	};
}

UENUM()
namespace EBTParallelMode
{
	// keep in sync with DescribeFinishMode

	enum Type
	{
		AbortBackground UMETA(DisplayName="Immediate" , ToolTip="When main task finishes, immediately abort background tree."),
		WaitForBackground UMETA(DisplayName="Delayed" , ToolTip="When main task finishes, wait for background tree to finish."),
	};
}

struct FBTParallelMemory : public FBTCompositeMemory
{
	/** last Id of search, detect infinite loops when there isn't any valid task in background tree */
	int32 LastSearchId;

	/** finish result of main task */
	TEnumAsByte<EBTNodeResult::Type> MainTaskResult;

	/** set when main task is running */
	uint8 bMainTaskIsActive : 1;

	/** try running background tree task even if main task has finished */
	uint8 bForceBackgroundTree : 1;

	/** set when main task needs to be repeated */
	uint8 bRepeatMainTask : 1;
};

/**
 * Simple Parallel composite node.
 * Allows for running two children: one which must be a single task node (with optional decorators), and the other of which can be a complete subtree.
 */
UCLASS()
class AIMODULE_API UBTComposite_SimpleParallel : public UBTCompositeNode
{
	GENERATED_UCLASS_BODY()

	/** how background tree should be handled when main task finishes execution */
	UPROPERTY(EditInstanceOnly, Category=Parallel)
	TEnumAsByte<EBTParallelMode::Type> FinishMode;

	/** handle child updates */
	int32 GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const;

	virtual void NotifyChildExecution(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, int32 ChildIdx, EBTNodeResult::Type& NodeResult) const override;
	virtual void NotifyNodeDeactivation(FBehaviorTreeSearchData& SearchData, EBTNodeResult::Type& NodeResult) const override;
	virtual bool CanNotifyDecoratorsOnDeactivation(FBehaviorTreeSearchData& SearchData, int32 ChildIdx, EBTNodeResult::Type& NodeResult) const override;
	virtual bool CanPushSubtree(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, int32 ChildIdx) const override;
	virtual void SetChildOverride(FBehaviorTreeSearchData& SearchData, int8 Index) const override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual FString GetStaticDescription() const override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;

	/** helper for showing values of EBTParallelMode enum */
	static FString DescribeFinishMode(EBTParallelMode::Type Mode);

#if WITH_EDITOR
	virtual bool CanAbortLowerPriority() const override;
	virtual bool CanAbortSelf() const override;
	virtual FName GetNodeIconName() const override;
#endif // WITH_EDITOR
};
