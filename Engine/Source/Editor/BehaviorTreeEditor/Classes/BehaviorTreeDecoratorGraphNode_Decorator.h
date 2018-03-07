// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeDecoratorGraphNode.h"
#include "AIGraphTypes.h"
#include "BehaviorTreeDecoratorGraphNode_Decorator.generated.h"

UCLASS()
class UBehaviorTreeDecoratorGraphNode_Decorator : public UBehaviorTreeDecoratorGraphNode 
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	UObject* NodeInstance;

	UPROPERTY()
	struct FGraphNodeClassData ClassData;

	virtual void PostPlacedNewNode() override;
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	virtual EBTDecoratorLogic::Type GetOperationType() const override;

	virtual void PostEditImport() override;
	virtual void PrepareForCopying() override;
	void PostCopyNode();
	bool RefreshNodeClass();
	void UpdateNodeClassData();

private:

	void ResetNodeOwner();
};
