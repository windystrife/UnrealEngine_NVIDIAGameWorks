// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ITreeItem.h"

class FMenuBuilder;

namespace SceneOutliner
{
	/** Helper class to manage moving abritrary data onto a folder */
	struct FFolderDropTarget : IDropTarget
	{
		/** The path that we are dropping on */
		FName DestinationPath;

		/** Constructor that takes a path to this folder (including leaf-name) */
		FFolderDropTarget(FName InDestinationPath) : DestinationPath(InDestinationPath) {}

	public:

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;
	};

	/** A tree item that represents a folder in the world */
	struct FFolderTreeItem : ITreeItem
	{
		/** The path of this folder. / separated. */
		FName Path;

		/** The leaf name of this folder */
		FName LeafName;

		/** Constructor that takes a path to this folder (including leaf-name) */
		FFolderTreeItem(FName InPath);

		/** Get this item's parent item. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

		/** Create this item's parent. It is valid to return nullptr if this item has no parent */
		FTreeItemPtr CreateParent() const override;

 		/** Visit this tree item */
 		virtual void Visit(const ITreeItemVisitor& Visitor) const override;
 		virtual void Visit(const IMutableTreeItemVisitor& Visitor) override;
 		
	public:

		/** Called when the expansion changes on this folder */
		virtual void OnExpansionChanged() override;

		/** Get the ID that represents this tree item. Used to reference this item in a map */
		virtual FTreeItemID GetID() const override;

		/** Get the raw string to display for this tree item - used for sorting */
		virtual FString GetDisplayString() const override;

		/** Get the sort priority given to this item's type */
		virtual int32 GetTypeSortPriority() const override;
		
		/** Check whether it should be possible to interact with this tree item */
		virtual bool CanInteract() const override;
		
		/** Generate a context menu for this item. Only called if *only* this item is selected. */
		virtual void GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner) override;

		/** Populate the specified drag/drop payload with any relevant information for this type */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override;

	public:

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

		/** Delete this folder */
		void Delete();

	private:

		/** Create a new folder as a child of this one */
		void CreateSubFolder(TWeakPtr<SSceneOutliner> WeakOutliner);
	};

}		// namespace SceneOutliner
