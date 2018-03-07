// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "SceneOutlinerPublicTypes.h"
#include "ISceneOutlinerColumn.h"

class ISceneOutliner;
template<typename ItemType> class STableRow;

namespace SceneOutliner
{

/** A 'getter' visitor that gets, and caches the visibility of a tree item */
struct FGetVisibilityVisitor : TTreeItemGetter<bool>
{
	/** Map of tree item to visibility */
	mutable TMap<const ITreeItem*, bool> VisibilityInfo;

	/** Get an item's visibility based on its children */
	bool RecurseChildren(const ITreeItem& Item) const;

	/** Get an actor's visibility */
	virtual bool Get(const FActorTreeItem& ActorItem) const override;

	/** Get a World's visibility */
	virtual bool Get(const FWorldTreeItem& WorldItem) const override;

	/** Get a folder's visibility */
	virtual bool Get(const FFolderTreeItem& FolderItem) const override;
};

/**
 * A gutter for the SceneOutliner which is capable of displaying a variety of Actor details
 */
class FSceneOutlinerGutter : public ISceneOutlinerColumn
{

public:

	/**	Constructor */
	FSceneOutlinerGutter(ISceneOutliner& Outliner);

	virtual ~FSceneOutlinerGutter() {}

	static FName GetID() { return FBuiltInColumnTypes::Gutter(); }
	
	// -----------------------------------------
	// ISceneOutlinerColumn Implementation
	virtual FName GetColumnID() override;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;

	virtual const TSharedRef< SWidget > ConstructRowWidget( FTreeItemRef TreeItem, const STableRow<FTreeItemPtr>& Row ) override;
	
	virtual void Tick(double InCurrentTime, float InDeltaTime) override;

	virtual bool SupportsSorting() const override { return true; }

	virtual void SortItems(TArray<FTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override;
	// -----------------------------------------

	/** Check whether the specified item is visible */
	FORCEINLINE bool IsItemVisible(const ITreeItem& Item)
	{
		return Item.Get(VisibilityCache);
	}

private:

	/** Weak pointer back to the scene outliner - required for setting visibility on current selection. */
	TWeakPtr<ISceneOutliner> WeakOutliner;

	/** Visitor used to get (and cache) visibilty for items. Cahced per-frame to avoid expensive recursion. */
	FGetVisibilityVisitor VisibilityCache;
};

}	// namespace SceneOutliner
