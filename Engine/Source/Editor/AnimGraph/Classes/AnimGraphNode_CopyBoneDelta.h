// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "BoneControllers/AnimNode_CopyBoneDelta.h"
#include "AnimGraphNode_CopyBoneDelta.generated.h"

UCLASS(MinimalAPI)
class UAnimGraphNode_CopyBoneDelta : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_CopyBoneDelta Node;

public:

	// UEdGraphNode interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	// End of UEdGraphNode interface

protected:

	// UAnimGraphNode_SkeletalControlBase interface
	virtual FText GetControllerDescription() const override;

	virtual const FAnimNode_SkeletalControlBase* GetNode() const override
	{
		return &Node;
	}
	// End of UAnimGraphNode_SkeletalControlBase interface
};
