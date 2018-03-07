// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_Base.h"
#include "Animation/AnimNodeSpaceConversions.h"
#include "AnimGraphNode_ComponentToLocalSpace.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_ComponentToLocalSpace : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Settings)
	FAnimNode_ConvertComponentToLocalSpace Node;

	// UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface

	// UAnimGraphNode_Base interface
	virtual FString GetNodeCategory() const override;
	virtual void PostProcessPinName(const UEdGraphPin* Pin, FString& DisplayName) const override;
	// End of UAnimGraphNode_Base interface
};
