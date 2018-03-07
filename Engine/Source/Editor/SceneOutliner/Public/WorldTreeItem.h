// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ITreeItem.h"
#include "UObject/ObjectKey.h"

class FMenuBuilder;

namespace SceneOutliner
{
	/** A tree item that represents an entire world */
	struct FWorldTreeItem : ITreeItem
	{
		/** The world this tree item is associated with. */
		mutable TWeakObjectPtr<UWorld> World;
		
		/** Constant identifier for this tree item */
		const FObjectKey ID;

		/** Construct this item from a world */
		FWorldTreeItem(UWorld* InWorld);

		/** Get this item's parent item. Always returns nullptr. */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

		/** Create this item's parent. Always returns nullptr. */
		virtual FTreeItemPtr CreateParent() const override;

 		/** Visit this tree item */
 		virtual void Visit(const ITreeItemVisitor& Visitor) const override;
 		virtual void Visit(const IMutableTreeItemVisitor& Visitor) override;

	public:

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
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override
		{
		}

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

		/** Get just the name of the world, for tooltip use */
		FString GetWorldName() const;

	private:

		/** Create a new folder at the root of this world */
		void CreateFolder(TWeakPtr<SSceneOutliner> WeakOutliner);
		
	public:

		/** Open the world settings for the contained world */
		void OpenWorldSettings() const;
	};

}		// namespace SceneOutliner
