// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialGraph/MaterialGraphNode_Base.h"
#include "MaterialGraphNode_Root.generated.h"

class UEdGraphPin;

UCLASS(MinimalAPI)
class UMaterialGraphNode_Root : public UMaterialGraphNode_Base
{
	GENERATED_UCLASS_BODY()

	/** Material whose inputs this root node represents */
	UPROPERTY()
	class UMaterial* Material;

	//~ Begin UEdGraphNode Interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }
	virtual void PostPlacedNewNode() override;
	//~ End UEdGraphNode Interface.

	//~ Begin UMaterialGraphNode_Base Interface
	virtual void CreateInputPins() override;
	virtual bool IsRootNode() const override {return true;}
	virtual int32 GetInputIndex(const UEdGraphPin* InputPin) const override;
	virtual uint32 GetInputType(const UEdGraphPin* InputPin) const override;
	//~ End UMaterialGraphNode_Base Interface
};
