// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.generated.h"

UCLASS()
class UBehaviorTreeGraphNode_Composite : public UBehaviorTreeGraphNode
{
	GENERATED_UCLASS_BODY()
	
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool RefreshNodeClass() override{ return false; }

	/** Gets a list of actions that can be done to this particular node */
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;

	/** check if node can accept breakpoints */
	virtual bool CanPlaceBreakpoints() const override { return true; }
};
