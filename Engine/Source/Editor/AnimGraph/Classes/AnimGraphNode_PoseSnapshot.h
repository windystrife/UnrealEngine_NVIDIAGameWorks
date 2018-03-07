// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_RefPoseBase.h"
#include "AnimNodes/AnimNode_PoseSnapshot.h"
#include "AnimGraphNode_PoseSnapshot.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_PoseSnapshot : public UAnimGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_PoseSnapshot Node;

	// UEdGraphNode interface
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	// End of UEdGraphNode interface
};
