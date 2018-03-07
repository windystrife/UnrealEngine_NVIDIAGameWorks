// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneOutlinerFwd.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"

template<typename ItemType> class STableRow;

/**
 *	Interface for a scene outliner column
 */
class ISceneOutlinerColumn : public TSharedFromThis< ISceneOutlinerColumn >
{
public:
	virtual ~ISceneOutlinerColumn() {}

	virtual FName GetColumnID() = 0;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() = 0;

	virtual const TSharedRef< SWidget > ConstructRowWidget( SceneOutliner::FTreeItemRef TreeItem, const STableRow<SceneOutliner::FTreeItemPtr>& Row ) = 0;

	virtual void Tick(double InCurrentTime, float InDeltaTime) {}

public:
	/** Optionally overridden interface methods */
	virtual void PopulateSearchStrings( const SceneOutliner::ITreeItem& Item, TArray< FString >& OutSearchStrings ) const {}

	virtual bool SupportsSorting() const { return false; }

	virtual void SortItems(TArray<SceneOutliner::FTreeItemPtr>& OutItems, const EColumnSortMode::Type SortMode) const {}
};

