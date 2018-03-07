// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "DisplayNodes/SequencerDisplayNode.h"
#include "Widgets/SWindow.h"

class FMenuBuilder;
class FSequencerDisplayNodeDragDropOp;
class UMovieSceneFolder;
enum class EItemDropZone;

/** A sequencer display node representing folders in the outliner. */
class FSequencerFolderNode : public FSequencerDisplayNode
{
public:
	FSequencerFolderNode( UMovieSceneFolder& InMovieSceneFolder, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree );

	// FSequencerDisplayNode interface
	virtual ESequencerNode::Type GetType() const override;
	virtual float GetNodeHeight() const override;
	virtual FNodePadding GetNodePadding() const override;
	virtual bool CanRenameNode() const override;
	virtual FText GetDisplayName() const override;
	virtual void SetDisplayName( const FText& NewDisplayName ) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual FSlateColor GetIconColor() const override;
	virtual bool CanDrag() const override;
	virtual TOptional<EItemDropZone> CanDrop( FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone ) const override;
	virtual void Drop( const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone ) override;
	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder);

	/** Adds a child node to this folder node. */
	void AddChildNode( TSharedRef<FSequencerDisplayNode> ChildNode );

	/** Gets the folder data for this display node. */
	UMovieSceneFolder& GetFolder() const;

	/** Callback to set the folder color */
	void SetFolderColor();

	/** Callback for the color being picked from the color picker */
	void OnColorPickerPicked(FLinearColor NewFolderColor);

	/** Callback for the color picker being closed */
	void OnColorPickerClosed(const TSharedRef<SWindow>& Window);

	/** Callback for the color picker being cancelled */
	void OnColorPickerCancelled(FLinearColor NewFolderColor);

private:

	/** The brush used to draw the icon when this folder is open .*/
	const FSlateBrush* FolderOpenBrush;

	/** The brush used to draw the icon when this folder is closed. */
	const FSlateBrush* FolderClosedBrush;

	/** The movie scene folder data which this node represents. */
	UMovieSceneFolder& MovieSceneFolder;
};
