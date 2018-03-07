// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "SceneOutlinerVisitorTypes.h"

class FMenuBuilder;

namespace SceneOutliner
{
	/** Interface used for validating movement (ie, drag/dropping) operations */
	struct IDropTarget
	{
		/** Called to test whether the specified payload can be dropped onto this tree item */
		virtual FDragValidationInfo ValidateDrop(FDragDropPayload& DraggedObjects, UWorld& World) const = 0;

		/** Called to drop the specified objects on this item. Only called if ValidateDrop() allows. */
		virtual void OnDrop(FDragDropPayload& DraggedObjects, UWorld& World, const FDragValidationInfo& ValidationInfo, TSharedRef<SWidget> DroppedOnWidget) = 0;
	};

	/** Base tree item interface  */
	struct ITreeItem : IDropTarget, TSharedFromThis<ITreeItem>
	{
		/** Friendship required for access to various internals */
		friend SSceneOutliner;

		/* Flags structure */
		struct FlagsType
		{
			/** Whether this item is expanded or not */
			bool bIsExpanded : 1;

			/* true if this item is filtered out */
			bool bIsFilteredOut : 1;

			/* true if this item can be interacted with as per the current outliner filters */
			bool bInteractive : 1;

			/** true if this item's children need to be sorted */
			bool bChildrenRequireSort : 1;

			/** Default constructor */		
			FlagsType() : bIsExpanded(1), bIsFilteredOut(0), bInteractive(1), bChildrenRequireSort(1) {}
		};

	public:
		
		/** Flags for this item */
		FlagsType Flags;

		/** Delegate for hooking up an inline editable text block to be notified that a rename is requested. */
		DECLARE_DELEGATE( FOnRenameRequest );

		/** Broadcasts whenever a rename is requested */
		FOnRenameRequest RenameRequestEvent;

	protected:

		/** Default constructor */
		ITreeItem() : SharedData(nullptr), Parent(nullptr) {}
		virtual ~ITreeItem() {}

		/** Data that is common between all outliner items - owned by the Outliner itself */
		TSharedPtr<FSharedOutlinerData> SharedData;

		/** This item's parent, if any. */
		mutable TWeakPtr<ITreeItem> Parent;

		/** Array of children contained underneath this item */
		mutable TArray<TWeakPtr<ITreeItem>> Children;

	public:

		const FSharedOutlinerData& GetSharedData() const
		{
			checkSlow(SharedData.IsValid());
			return *SharedData;
		}

	public:

		/** Get this item's parent. Can be nullptr. */
		FTreeItemPtr GetParent() const
		{
			return Parent.Pin();
		}

		/** Add a child to this item */
		void AddChild(FTreeItemRef Child)
		{
			Child->Parent = AsShared();
			Children.Add(MoveTemp(Child));
		}

		/** Remove a child from this item */
		void RemoveChild(const FTreeItemRef& Child)
		{
			if (Children.Remove(Child))
			{
				Child->Parent = nullptr;	
			}
		}

		/** Get this item's children, if any. Although we store as weak pointers, they are guaranteed to be valid. */
		FORCEINLINE const TArray<TWeakPtr<ITreeItem>>& GetChildren() const
		{
			return Children;
		}

		/** Find this item's parent in the specified map. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr FindParent(const FTreeItemMap& ExistingItems) const = 0;

		/** Create this item's parent. It is valid to return nullptr if this item has no parent */
		virtual FTreeItemPtr CreateParent() const = 0;

	public:

	 	/** Visit this tree item */
	 	virtual void Visit(const ITreeItemVisitor& Visitor) const = 0;
	 	virtual void Visit(const IMutableTreeItemVisitor& Visitor) = 0;

	 	/** Get some data from this tree item using a 'getter' visitor. */
	 	template<typename T>
	 	T Get(const TTreeItemGetter<T>& Getter) const
	 	{
	 		Visit(Getter);
	 		return Getter.Result();
	 	}

	public:

		/** Get the ID that represents this tree item. Used to reference this item in a map */
		virtual FTreeItemID GetID() const = 0;

		/** Get the raw string to display for this tree item - used for sorting */
		virtual FString GetDisplayString() const = 0;

		/** Get the sort priority given to this item's type */
		virtual int32 GetTypeSortPriority() const = 0;

		/** Check whether it should be possible to interact with this tree item */
		virtual bool CanInteract() const = 0;
		
		/** Called when this item is expanded or collapsed */
		virtual void OnExpansionChanged() {};

		/** Generate a context menu for this item. Only called if *only* this item is selected. */
		virtual void GenerateContextMenu(FMenuBuilder& MenuBuilder, SSceneOutliner& Outliner) {}

		/** Populate the specified drag/drop payload with any relevant information for this type */
		virtual void PopulateDragDropPayload(FDragDropPayload& Payload) const = 0;
	};

}	// namespace SceneOutliner
