// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"

class IKeyArea;
class SKeyAreaEditorSwitcher;
class UMovieSceneSection;

/**
 * Represents an area inside a section where keys are displayed.
 *
 * There is one key area per section that defines that key area.
 */
class FSequencerSectionKeyAreaNode
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
	 * @param bInTopLevel If true the node is part of the section itself instead of taking up extra height in the section.
	 */
	FSequencerSectionKeyAreaNode(FName NodeName, const FText& InDisplayName, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree, bool bInTopLevel = false);

public:

	/**
	 * Adds a key area to this node.
	 *
	 * @param KeyArea The key area interface to add.
	 */
	void AddKeyArea(TSharedRef<IKeyArea> KeyArea);

	/**
	 * Returns a key area that corresponds to the specified section
	 * 
	 * @param Section	The section to find a key area for
	 * @return the key area at the index.
	 */
	TSharedPtr<IKeyArea> GetKeyArea(UMovieSceneSection* Section) const;

	/**
	 * Returns all key area for this node
	 * 
	 * @return All key areas
	 */
	const TArray<TSharedRef<IKeyArea>>& GetAllKeyAreas() const
	{
		return KeyAreas;
	}

	/** @return Whether the node is top level.  (I.E., is part of the section itself instead of taking up extra height in the section) */
	bool IsTopLevel() const
	{
		return bTopLevel;
	}

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

	/** The display name of the key area. */
	FText DisplayName;

	/** All key areas on this node (one per section). */
	TArray<TSharedRef<IKeyArea>> KeyAreas;

	/** The outliner key editor switcher widget. */
	TSharedPtr<SKeyAreaEditorSwitcher> KeyEditorSwitcher;

	/** If true the node is part of the section itself instead of taking up extra height in the section. */
	bool bTopLevel;
};
