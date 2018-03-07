// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "SequencerObjectBindingDragDropOp.h"

struct FMovieSceneObjectBindingID;

/** A decorated drag drop operation object for dragging sequencer display nodes. */
class FSequencerDisplayNodeDragDropOp : public FSequencerObjectBindingDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE( FSequencerDisplayNodeDragDropOp, FSequencerObjectBindingDragDropOp )

	/**
	 * Construct a new drag/drop operation for dragging a selection of display nodes
	 */
	static TSharedRef<FSequencerDisplayNodeDragDropOp> New(TArray<TSharedRef<FSequencerDisplayNode>>& InDraggedNodes, FText InDefaultText, const FSlateBrush* InDefaultIcon);

public:

	//~ FSequencerObjectBindingDragDropOp interface
	virtual TArray<FMovieSceneObjectBindingID> GetDraggedBindings() const override;

	//~ FGraphEditorDragDropAction interface
	virtual void HoverTargetChanged() override;
	virtual FReply DroppedOnPanel( const TSharedRef< class SWidget >& Panel, FVector2D ScreenPosition, FVector2D GraphPosition, UEdGraph& Graph) override;

	//~ FDragDropOperation interface
	virtual void Construct() override;

public:

	/**
	 * Reset the tooltip decorator back to its original state
	 */
	void ResetToDefaultToolTip();

	/**
	 * Gets the nodes which are currently being dragged.
	 */
	TArray<TSharedRef<FSequencerDisplayNode>>& GetDraggedNodes();

public:

	/**
	 * Current string to show as the decorator text
	 */
	FText CurrentHoverText;

	/**
	 * Current icon to be displayed on the decorator
	 */
	const FSlateBrush* CurrentIconBrush;

private:
	/** Private construction - use New() to create a new operation of this type */
	FSequencerDisplayNodeDragDropOp(){}

	/**
	 * Get the current decorator text
	 */
	FText GetDecoratorText() const
	{
		return CurrentHoverText;
	}

	/**
	 * Get the current decorator icon
	 */
	const FSlateBrush* GetDecoratorIcon() const
	{
		return CurrentIconBrush;
	}

	/**
	 * Attempt to extract a sequencer ptr from the dragged nodes
	 */
	FSequencer* GetSequencer() const;

private:

	/** The nodes currently being dragged. */
	TArray<TSharedRef<FSequencerDisplayNode>> DraggedNodes;
	
	/** Default string to show as hover text */
	FText DefaultHoverText;

	/** Default icon to be displayed */
	const FSlateBrush* DefaultHoverIcon;
};