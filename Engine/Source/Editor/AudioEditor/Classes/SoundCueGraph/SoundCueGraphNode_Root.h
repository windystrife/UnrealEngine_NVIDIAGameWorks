// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "SoundCueGraph/SoundCueGraphNode_Base.h"
#include "SoundCueGraphNode_Root.generated.h"

UCLASS(MinimalAPI)
class USoundCueGraphNode_Root : public USoundCueGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	// UEdGraphNode interface
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual void GetContextMenuActions(const FGraphNodeContextMenuBuilder& Context) const override;
	// End of UEdGraphNode interface

	// USoundCueGraphNode_Base interface
	virtual void CreateInputPins() override;
	virtual bool IsRootNode() const override {return true;}
	// End of USoundCueGraphNode_Base interface
};
