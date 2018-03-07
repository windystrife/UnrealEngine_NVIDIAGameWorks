// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

class FDebugLineItem;
class FTraceStackParentItem;
class UBlueprint;
class UBlueprintGeneratedClass;

//////////////////////////////////////////////////////////////////////////
// FDebugLineItem

// Shared pointer to a debugging tree line entry
typedef TSharedPtr<class FDebugLineItem> FDebugTreeItemPtr;

// The base class for a line entry in the debugging tree view
class FDebugLineItem : public TSharedFromThis<FDebugLineItem>
{
public:
	enum EDebugLineType
	{
		DLT_Message,
		DLT_TraceStackParent,
		DLT_TraceStackChild,
		DLT_Parent,
		DLT_Watch,
		DLT_LatentAction,
		DLT_Breakpoint
	};

	virtual ~FDebugLineItem() {}
	
	// Create the widget for the name column
	virtual TSharedRef<SWidget> GenerateNameWidget();

	// Create the widget for the value column
	virtual TSharedRef<SWidget> GenerateValueWidget();

	// Add any context menu items that can act on this node
	virtual void MakeMenu(class FMenuBuilder& MenuBuilder) { }

	// Gather all of the children
	virtual void GatherChildren(TArray<FDebugTreeItemPtr>& OutChildren) {}

	// @return The object that will act as a parent to more items in the tree, or NULL if this is a leaf node
	virtual UObject* GetParentObject() { return NULL; }

	// Helper function to try to get the blueprint for a given object;
	//   Returns the blueprint that was used to create the instance if there was one
	//   Returns the object itself if it is already a blueprint
	//   Otherwise returns NULL
	static UBlueprint* GetBlueprintForObject(UObject* ParentObject);

	static UBlueprintGeneratedClass* GetClassForObject(UObject* ParentObject);
protected:
	// Adds either Item or an identical node that was previously created (present in ChildrenMirrors) as a child to OutChildren
	void EnsureChildIsAdded(TArray<FDebugTreeItemPtr>& ChildrenMirrors, TArray<FDebugTreeItemPtr>& OutChildren, const FDebugLineItem& Item);

	// Cannot create an instance of this class, it's just for use as a base class
	FDebugLineItem(EDebugLineType InType)
		: Type(InType)
	{
	}

	// Duplicate this item
	virtual FDebugLineItem* Duplicate() const=0;

	// Compare this item to another of the same type
	virtual bool Compare(const FDebugLineItem* Other) const=0;

	// @return The text to display in the name column, unless GenerateNameWidget is overridden
	virtual FText GetDisplayName() const;

	// @return The text to display in the value column, unless GenerateValueWidget is overridden
	virtual FText GetDescription() const;
private:
	// Type of action (poor mans RTTI for the tree, really only used to accelerate Compare checks)
	EDebugLineType Type;
};

//////////////////////////////////////////////////////////////////////////
// SKismetDebuggingView

class SKismetDebuggingView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SKismetDebuggingView )
		: _BlueprintToWatch()
		{}

		SLATE_ARGUMENT( TWeakObjectPtr<UBlueprint>, BlueprintToWatch )
	SLATE_END_ARGS()
public:
	void Construct( const FArguments& InArgs );

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	// End of SWidget interface
protected:
	FText GetTopText() const;

	TSharedRef<ITableRow> OnGenerateRowForWatchTree(FDebugTreeItemPtr InItem, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildrenForWatchTree(FDebugTreeItemPtr InParent, TArray<FDebugTreeItemPtr>& OutChildren);
	EVisibility IsDebuggerVisible() const;

	TSharedPtr<SWidget> OnMakeContextMenu();
protected:
	TSharedPtr< STreeView<FDebugTreeItemPtr> > DebugTreeView;
	TMap<UObject*, FDebugTreeItemPtr> ObjectToTreeItemMap;
	TArray<FDebugTreeItemPtr> RootTreeItems;

	// The trace stack parent item
	TSharedPtr< class FTraceStackParentItem > TraceStackItem;

	/** Pointer to the blueprint to observe when not in PIE/SIE; can be NULL for a free-floating watch window */
	TWeakObjectPtr<UBlueprint> BlueprintToWatchPtr;
};
