// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SGraphNode.h"

class SHorizontalBox;

class SPhysicsAssetGraphNode : public SGraphNode
{
public:
	SLATE_BEGIN_ARGS(SPhysicsAssetGraphNode){}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, class UPhysicsAssetGraphNode* InNode);

	void AddSubWidget(const TSharedRef<SWidget>& InWidget);

protected:
	// SGraphNode interface
	virtual void CreatePinWidgets() override;
	virtual void AddPin(const TSharedRef<SGraphPin>& PinToAdd) override;
	virtual void UpdateGraphNode() override;
	virtual const FSlateBrush* GetShadowBrush(bool bSelected) const override;

	FSlateColor GetNodeColor() const;
	FText GetNodeTitle() const;

protected:
	/** The content widget for this node - derived classes can insert what they want */
	TSharedPtr<SWidget> ContentWidget;

	/** Any sub-nodes are inserted here */
	TSharedPtr<SVerticalBox> SubNodeContent;
};
