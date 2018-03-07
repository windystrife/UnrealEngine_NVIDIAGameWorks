// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SEditableLabel;
class SSequencerTreeViewRow;
struct FSlateBrush;
struct FTableRowStyle;

/**
 * A widget for displaying a sequencer tree node in the animation outliner.
 */
class SAnimationOutlinerTreeNode
	: public SCompoundWidget
{
public:

	SAnimationOutlinerTreeNode(){}
	~SAnimationOutlinerTreeNode();
	
	SLATE_BEGIN_ARGS(SAnimationOutlinerTreeNode){}
		SLATE_ATTRIBUTE(const FSlateBrush*, IconBrush)
		SLATE_ATTRIBUTE(const FSlateBrush*, IconOverlayBrush)
		SLATE_ATTRIBUTE(FSlateColor, IconColor)
		SLATE_ATTRIBUTE(FText, IconToolTipText)
		SLATE_NAMED_SLOT(FArguments, CustomContent)
	SLATE_END_ARGS()

	void Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node, const TSharedRef<SSequencerTreeViewRow>& InTableRow );

public:

	/** Change the node's label text to edit mode. */
	void EnterRenameMode();

	/**
	 * @return The display node used by this widget.                                                           
	 */
	const TSharedPtr<FSequencerDisplayNode> GetDisplayNode() const
	{
		return DisplayNode;
	}

private:

	// SWidget interface

	void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	FSlateColor GetForegroundBasedOnSelection() const;

	/** Get the tint to apply to the color indicator based on this node's track */
	FSlateColor GetTrackColorTint() const;

	/**
	 * @return The border image to show in the tree node.
	 */
	const FSlateBrush* GetNodeBorderImage() const;
	
	/**
	 * @return The tint to apply to the border image
	 */
	FSlateColor GetNodeBackgroundTint() const;

	/**
	* @return The tint to apply to the border image for the inner portion of the node.
	*/
	FSlateColor GetNodeInnerBackgroundTint() const;

	/**
	 * @return The expander visibility of this node.
	 */
	EVisibility GetExpanderVisibility() const;

	/**
	 * @return The color used to draw the display name.
	 */
	FSlateColor GetDisplayNameColor() const;

	/**
	 * @return The text displayed for the tool tip for the diplay name label. 
	 */
	FText GetDisplayNameToolTipText() const;

	/**
	 * @return The display name for this node.
	 */
	FText GetDisplayName() const;

	/** Callback for checking whether the node label can be edited. */
	bool HandleNodeLabelCanEdit() const;

	/** Callback for when the node label text has changed. */
	void HandleNodeLabelTextChanged(const FText& NewLabel);

	/** Get all descendant nodes from the given root node. */
	void GetAllDescendantNodes(TSharedPtr<FSequencerDisplayNode> RootNode, TArray<TSharedRef<FSequencerDisplayNode> >& AllNodes);

	/** Called when the user clicks the track color */
	TSharedRef<SWidget> OnGetColorPicker() const;

private:

	/** Layout node the widget is visualizing. */
	TSharedPtr<FSequencerDisplayNode> DisplayNode;

	/** Holds the editable text label widget. */
	TSharedPtr<SEditableLabel> EditableLabel;

	/** True if this node is a top level node, at the root of the tree, false otherwise */
	bool bIsOuterTopLevelNode;

	/** True if this is a top level node inside or a folder, otherwise false. */
	bool bIsInnerTopLevelNode;

	/** Default background brush for this node when expanded */
	const FSlateBrush* ExpandedBackgroundBrush;

	/** Default background brush for this node when collapsed */
	const FSlateBrush* CollapsedBackgroundBrush;

	/** The brush to use when drawing the background for the inner portion of the node. */
	const FSlateBrush* InnerBackgroundBrush;

	/** The table row style used for nodes in the tree. This is required as we don't actually use the tree for selection. */
	const FTableRowStyle* TableRowStyle;
};
