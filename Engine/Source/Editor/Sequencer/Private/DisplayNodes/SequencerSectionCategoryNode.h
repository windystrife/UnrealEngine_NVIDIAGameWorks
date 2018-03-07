// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"

/**
 * A node that displays a category for other nodes
 */
class FSequencerSectionCategoryNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InNodeName The name identifier of then node.
	 * @param InDisplayName Display name of the category.
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 */
	FSequencerSectionCategoryNode(FName NodeName, const FText& InDisplayName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
		: FSequencerDisplayNode(NodeName, InParentNode, InParentTree)
		, DisplayName(InDisplayName)
	{ }

public:

	// FSequencerDisplayNode interface

	virtual bool CanRenameNode() const override;
	virtual TSharedRef<SWidget> GetCustomOutlinerContent() override;
	virtual FText GetDisplayName() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual ESequencerNode::Type GetType() const override;
	virtual void SetDisplayName(const FText& NewDisplayName) override;

private:

	/** The display name of the category */
	FText DisplayName;
};
