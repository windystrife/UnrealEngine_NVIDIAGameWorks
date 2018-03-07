// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "ITreeItem.h"
#include "UObject/ObjectKey.h"

namespace SceneOutliner
{
	/** Helper class to manage moving abritrary data onto an actor */
	struct FActorDropTarget : IDropTarget
	{
		/** The actor this tree item is associated with. */
		TWeakObjectPtr<AActor> Actor;
		
		/** Construct this object out of an Actor */
		FActorDropTarget(AActor* InActor) : Actor(InActor) {}

	public:

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

	protected:

		/** Attach our actor to the specified parent and socket */
		static void PerformAttachment(FName SocketName, TWeakObjectPtr<AActor> Parent, const FActorArray NewAttachments);

		/** Detach the specified actor from this actor */
		void DetachActorFromParent(AActor* ChildActor);

	};

	/** A tree item that represents an actor in the world */
	struct FActorTreeItem : ITreeItem
	{
		/** The actor this tree item is associated with. */
		mutable TWeakObjectPtr<AActor> Actor;

		/** Constant identifier for this tree item */
		const FObjectKey ID;

		/** Construct this item from an actor */
		FActorTreeItem(AActor* InActor);

		/** Get this item's parent item. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const override;

		/** Create this item's parent. It is valid to return nullptr if this item has no parent */
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

	public:

		/** Populate the specified drag/drop payload with any relevant information for this type */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const override;

		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const override;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) override;

	public:

		/** true if this item exists in both the current world and PIE. */
		bool bExistsInCurrentWorldAndPIE;

	};

}		// namespace SceneOutliner
