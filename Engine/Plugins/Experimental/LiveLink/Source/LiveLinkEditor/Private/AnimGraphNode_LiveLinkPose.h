// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "AnimNode_LiveLinkPose.h"

#include "AnimGraphNode_LiveLinkPose.generated.h"
UCLASS()
class UAnimGraphNode_LiveLinkPose : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_LiveLinkPose Node;

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const;
	// End of UEdGraphNode
};