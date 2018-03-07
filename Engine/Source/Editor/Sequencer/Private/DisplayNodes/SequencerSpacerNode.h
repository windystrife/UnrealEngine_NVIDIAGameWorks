// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"

class SSequencerTreeViewRow;

/**
 * A node that displays a category for other nodes
 */
class FSequencerSpacerNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 */
	FSequencerSpacerNode(float InSize, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
		: FSequencerDisplayNode(NAME_None, InParentNode, InParentTree)
		, Size(InSize)
	{ }

public:

	// FSequencerDisplayNode interface

	virtual bool CanRenameNode() const override { return false; }
	virtual FText GetDisplayName() const override { return FText(); }
	virtual float GetNodeHeight() const override { return Size; }
	virtual FNodePadding GetNodePadding() const override { return FNodePadding(0.f); }
	virtual ESequencerNode::Type GetType() const override { return ESequencerNode::Spacer; }
	virtual void SetDisplayName(const FText& NewDisplayName) override { }
	virtual TSharedRef<SWidget> GenerateContainerWidgetForOutliner(const TSharedRef<SSequencerTreeViewRow>& InRow) override { return SNew(SBox).HeightOverride(Size); }

private:

	/** The size of the spacer */
	float Size;
};
