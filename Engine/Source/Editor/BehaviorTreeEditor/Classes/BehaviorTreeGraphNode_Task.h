// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Task.generated.h"

UCLASS()
class UBehaviorTreeGraphNode_Task : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	/** Gets a list of actions that can be done to this particular node */
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;

	virtual bool CanPlaceBreakpoints() const override { return true; }
};
