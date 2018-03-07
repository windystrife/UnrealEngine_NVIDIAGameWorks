// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "DisplayNodes/SequencerSectionKeyAreaNode.h"
#include "ISequencerSection.h"
#include "IKeyArea.h"
#include "SequencerHotspots.h"

class FMenuBuilder;
class ISequencerTrackEditor;
class UMovieSceneTrack;
struct FSlateBrush;

struct FSequencerOverlapRange
{
	/** The range for the overlap */
	TRange<float> Range;
	/** The sections that occupy this range, sorted by overlap priority */
	TArray<FSectionHandle> Sections;
};

/**
 * Represents an area to display Sequencer sections (possibly on multiple lines).
 */
class FSequencerTrackNode
	: public FSequencerDisplayNode
{
public:

	/**
	 * Create and initialize a new instance.
	 * 
	 * @param InAssociatedType The track that this node represents.
	 * @param InAssociatedEditor The track editor for the track that this node represents.
	 * @param bInCanBeDragged Whether or not this node can be dragged and dropped.
	 * @param InParentNode The parent of this node, or nullptr if this is a root node.
	 * @param InParentTree The tree this node is in.
	 */
	FSequencerTrackNode(UMovieSceneTrack& InAssociatedTrack, ISequencerTrackEditor& InAssociatedEditor, bool bInCanBeDragged, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree);

public:
	/** Defines interaction modes when using sub-tracks for sections on multiple rows. */
	enum class ESubTrackMode
	{
		/** This track node isn't part of a sub-track set. */
		None,
		/** This track node is the parent and has child sub tracks. */
		ParentTrack,
		/** This track node is a sub-track of another track node. */
		SubTrack
	};

public:

	/**
	 * Adds a section to this node
	 *
	 * @param SequencerSection	The section to add
	 */
	void AddSection(TSharedRef<ISequencerSection>& SequencerSection)
	{
		Sections.Add(SequencerSection);
	}

	void AddChildTrack(TSharedRef<FSequencerTrackNode> TrackNode)
	{
		AddChildAndSetParent(TrackNode);
	}

	/**
	 * Makes the section itself a key area without taking up extra space
	 *
	 * @param KeyArea	Interface for the key area
	 */
	void SetSectionAsKeyArea(TSharedRef<IKeyArea>& KeyArea);
	
	/**
	 * Adds a key to the track
	 *
	 */
	void AddKey(const FGuid& ObjectGuid);

	/**
	 * @return All sections in this node
	 */
	const TArray<TSharedRef<ISequencerSection>>& GetSections() const
	{
		return Sections;
	}

	TArray<TSharedRef<ISequencerSection>>& GetSections()
	{
		return Sections;
	}

	/** @return Returns the top level key node for the section area if it exists */
	TSharedPtr<FSequencerSectionKeyAreaNode> GetTopLevelKeyNode() const
	{
		return TopLevelKeyNode;
	}

	/** @return the track associated with this section */
	UMovieSceneTrack* GetTrack() const
	{
		return AssociatedTrack.Get();
	}

	/** Gets the track editor associated with this track node. */
	ISequencerTrackEditor& GetTrackEditor() const
	{
		return AssociatedEditor;
	}

	/** Gets the sub track mode for this track node, used when the track supports multiple rows. */
	ESubTrackMode GetSubTrackMode() const;

	/** Sets the sub track mode for this track node, used when the track supports multiple rows. */
	void SetSubTrackMode(ESubTrackMode InSubTrackMode);

	/** Gets the row index for this track node.  This is only relevant when this track node is a sub-track node. */
	int32 GetRowIndex() const;

	/**  Gets the row index for this track node when this track node is a sub-track. */
	void SetRowIndex(int32 InRowIndex);

	/** Gets an array of sections that underlap the specified section */
	TArray<FSequencerOverlapRange> GetUnderlappingSections(UMovieSceneSection* InSection);

	/** Gets an array of sections whose easing bounds underlap the specified section */
	TArray<FSequencerOverlapRange> GetEasingSegmentsForSection(UMovieSceneSection* InSection);

public:

	// FSequencerDisplayNode interface

	virtual void BuildContextMenu( FMenuBuilder& MenuBuilder );
	virtual bool CanRenameNode() const override;
	virtual TSharedRef<SWidget> GetCustomOutlinerContent() override;
	virtual void GetChildKeyAreaNodesRecursively(TArray<TSharedRef<FSequencerSectionKeyAreaNode>>& OutNodes) const override;
	virtual FText GetDisplayName() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual ESequencerNode::Type GetType() const override;
	virtual void SetDisplayName(const FText& NewDisplayName) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool CanDrag() const override;
	virtual bool IsResizable() const override;
	virtual void Resize(float NewSize) override;
	
private:

	FReply CreateNewSection() const;

private:

	/** The track editor for the track associated with this node. */
	ISequencerTrackEditor& AssociatedEditor;

	/** The type associated with the sections in this node */
	TWeakObjectPtr<UMovieSceneTrack> AssociatedTrack;

	/** All of the sequencer sections in this node */
	TArray<TSharedRef<ISequencerSection>> Sections;

	/** If the section area is a key area itself, this represents the node for the keys */
	TSharedPtr<FSequencerSectionKeyAreaNode> TopLevelKeyNode;

	/** Whether or not this track node can be dragged. */
	bool bCanBeDragged;

	/** The current sub-track mode this node is using. */
	ESubTrackMode SubTrackMode;

	/** The row index when this track node is a sub-track node. */
	int32 RowIndex;
};
