// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.generated.h"

/** Root node of this behavior tree, holds Blackboard data */
UCLASS()
class UBehaviorTreeGraphNode_Root : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=BehaviorTree)
	class UBlackboardData* BlackboardAsset;

	virtual void PostPlacedNewNode() override;
	virtual void AllocateDefaultPins() override;
	virtual bool CanDuplicateNode() const override { return false; }
	virtual bool CanUserDeleteNode() const override{ return false; }
	virtual bool HasErrors() const override { return false; }
	virtual bool RefreshNodeClass() override { return false; }
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	/** gets icon resource name for title bar */
	virtual FName GetNameIcon() const override;
	virtual FText GetTooltipText() const override;

	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual FText GetDescription() const override;

	/** notify behavior tree about blackboard change */
	void UpdateBlackboard();
};
