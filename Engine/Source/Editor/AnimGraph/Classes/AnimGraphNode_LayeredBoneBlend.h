// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_BlendListBase.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "AnimGraphNode_LayeredBoneBlend.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_LayeredBoneBlend : public UAnimGraphNode_BlendListBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_LayeredBoneBlend Node;

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	// Adds a new pose pin
	//@TODO: Generalize this behavior (returning a list of actions/delegates maybe?)
	ANIMGRAPH_API virtual void AddPinToBlendByFilter();
	ANIMGRAPH_API virtual void RemovePinFromBlendByFilter(UEdGraphPin* Pin);

	// UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UK2Node interface
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	// End of UK2Node interface

	// UAnimGraphNode_Base interface
	virtual FString GetNodeCategory() const override;
	// End of UAnimGraphNode_Base interface
};
