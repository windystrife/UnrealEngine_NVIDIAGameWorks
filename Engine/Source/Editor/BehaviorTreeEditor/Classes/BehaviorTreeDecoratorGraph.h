// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "BehaviorTreeDecoratorGraph.generated.h"

class UBehaviorTreeDecoratorGraphNode;
class UEdGraphPin;

UCLASS()
class UBehaviorTreeDecoratorGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	void CollectDecoratorData(TArray<class UBTDecorator*>& DecoratorInstances, TArray<struct FBTDecoratorLogic>& DecoratorOperations) const;
	int32 SpawnMissingNodes(const TArray<class UBTDecorator*>& NodeInstances, const TArray<struct FBTDecoratorLogic>& Operations, int32 StartIndex);

protected:

	const class UBehaviorTreeDecoratorGraphNode* FindRootNode() const;
	void CollectDecoratorDataWorker(const class UBehaviorTreeDecoratorGraphNode* Node, TArray<class UBTDecorator*>& DecoratorInstances, TArray<struct FBTDecoratorLogic>& DecoratorOperations) const;

	UEdGraphPin* FindFreePin(UEdGraphNode* Node, EEdGraphPinDirection Direction);
	UBehaviorTreeDecoratorGraphNode* SpawnMissingNodeWorker(const TArray<class UBTDecorator*>& NodeInstances, const TArray<struct FBTDecoratorLogic>& Operations, int32& Index,
		UEdGraphNode* ParentGraphNode, int32 ChildIdx);

};
