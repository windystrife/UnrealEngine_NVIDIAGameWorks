// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphNode.h"
#include "PhysicsAssetGraphNode.generated.h"

UCLASS()
class UPhysicsAssetGraphNode : public UEdGraphNode
{
	GENERATED_UCLASS_BODY()

	virtual void SetupPhysicsAssetNode();
	virtual UObject* GetDetailsObject();

	class UPhysicsAssetGraph* GetPhysicsAssetGraph() const;

	// UEdGraphNode implementation
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual void AllocateDefaultPins() override;

	UEdGraphPin& GetInputPin() const;

	UEdGraphPin& GetOutputPin() const;

	void SetDimensions(const FVector2D& InDimensions) { Dimensions = InDimensions; }
	const FVector2D& GetDimensions() const { return Dimensions; }


protected:
	/** Cached title for the node */
	FText NodeTitle;

	/** Our one input pin */
	UEdGraphPin* InputPin;

	/** Our one output pin */
	UEdGraphPin* OutputPin;

	/** Cached dimensions of this node (used for layout) */
	FVector2D Dimensions;
};


