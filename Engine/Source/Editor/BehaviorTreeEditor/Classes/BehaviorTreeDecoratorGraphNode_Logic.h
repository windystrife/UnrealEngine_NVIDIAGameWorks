// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeDecoratorGraphNode.h"
#include "BehaviorTreeDecoratorGraphNode_Logic.generated.h"

UENUM()
namespace EDecoratorLogicMode
{
	enum Type
	{
		Sink,
		And,
		Or,
		Not,
	};
}

UCLASS()
class UBehaviorTreeDecoratorGraphNode_Logic : public UBehaviorTreeDecoratorGraphNode
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TEnumAsByte<EDecoratorLogicMode::Type> LogicMode;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;
	virtual bool CanUserDeleteNode() const override;
	virtual EBTDecoratorLogic::Type GetOperationType() const override;

	virtual void GetMenuEntries(struct FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	bool CanRemovePins() const;
	bool CanAddPins() const;

	UEdGraphPin* AddInputPin();
	void RemoveInputPin(class UEdGraphPin* Pin);

	EDecoratorLogicMode::Type GetLogicMode(EBTDecoratorLogic::Type Op) const;
};
