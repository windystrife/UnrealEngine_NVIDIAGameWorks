// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerPublicTypes.h"
#include "ISceneOutlinerColumn.h"

template<typename ItemType> class STableRow;

namespace SceneOutliner
{

/** A column for the SceneOutliner that displays the item label */
class FItemLabelColumn : public ISceneOutlinerColumn
{

public:
	FItemLabelColumn(ISceneOutliner& SceneOutliner) : WeakSceneOutliner(StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared())) {}
	
	virtual ~FItemLabelColumn() {}

	static FName GetID() { return FBuiltInColumnTypes::Label(); }

	//////////////////////////////////////////////////////////////////////////
	// Begin ISceneOutlinerColumn Implementation

	virtual FName GetColumnID() override;

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override;
	
	virtual const TSharedRef< SWidget > ConstructRowWidget( FTreeItemRef TreeItem, const STableRow<FTreeItemPtr>& Row ) override;

	virtual void PopulateSearchStrings( const ITreeItem& Item, TArray< FString >& OutSearchStrings ) const override;

	virtual bool SupportsSorting() const override { return true; }

	virtual void SortItems(TArray<FTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override;
	
	// End ISceneOutlinerColumn Implementation
	//////////////////////////////////////////////////////////////////////////

private:
	TWeakPtr<ISceneOutliner> WeakSceneOutliner;

private:

	TSharedRef<SWidget> GenerateWidget( FActorTreeItem& TreeItem, const STableRow<FTreeItemPtr>& InRow );
	TSharedRef<SWidget> GenerateWidget( FWorldTreeItem& TreeItem, const STableRow<FTreeItemPtr>& InRow );
	TSharedRef<SWidget> GenerateWidget( FFolderTreeItem& TreeItem, const STableRow<FTreeItemPtr>& InRow );

	struct FColumnWidgetGenerator : FColumnGenerator
	{
		FItemLabelColumn& Column;
		const STableRow<FTreeItemPtr>& Row;

		FColumnWidgetGenerator(FItemLabelColumn& InColumn, const STableRow<FTreeItemPtr>& InRow) : Column(InColumn), Row(InRow) {}

		virtual TSharedRef<SWidget> GenerateWidget(FFolderTreeItem& Item) const override { return Column.GenerateWidget(Item, Row); }
		virtual TSharedRef<SWidget> GenerateWidget(FWorldTreeItem& Item) const override { return Column.GenerateWidget(Item, Row); }
		virtual TSharedRef<SWidget> GenerateWidget(FActorTreeItem& Item) const override { return Column.GenerateWidget(Item, Row); }
	};
	friend FColumnWidgetGenerator;
};

}	// namespace SceneOutliner
