// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/TileView.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UTileView

UTileView::UTileView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;

	ItemWidth = 128.0f;
	ItemHeight = 128.0f;
	SelectionMode = ESelectionMode::Single;
}

TSharedRef<SWidget> UTileView::RebuildWidget()
{
	MyTileView = SNew(STileView< UObject* >)
		.SelectionMode(SelectionMode)
		.ListItemsSource(&Items)
		.ItemWidth(ItemWidth)
		.ItemHeight(ItemHeight)
		.OnGenerateTile(BIND_UOBJECT_DELEGATE(STileView< UObject* >::FOnGenerateRow, HandleOnGenerateTile))
		//.OnSelectionChanged(this, &SSocketManager::SocketSelectionChanged_Execute)
		//.OnContextMenuOpening(this, &SSocketManager::OnContextMenuOpening)
		//.OnItemScrolledIntoView(this, &SSocketManager::OnItemScrolledIntoView)
		//	.HeaderRow
		//	(
		//		SNew(SHeaderRow)
		//		.Visibility(EVisibility::Collapsed)
		//		+ SHeaderRow::Column(TEXT("Socket"))
		//	);
		;

	return MyTileView.ToSharedRef();
}

TSharedRef<ITableRow> UTileView::HandleOnGenerateTile(UObject* Item, const TSharedRef< STableViewBase >& OwnerTable) const
{
	// Call the user's delegate to see if they want to generate a custom widget bound to the data source.
	if ( OnGenerateTileEvent.IsBound() )
	{
		UWidget* Widget = OnGenerateTileEvent.Execute(Item);
		if ( Widget != NULL )
		{
			return SNew(STableRow< UObject* >, OwnerTable)
			[
				Widget->TakeWidget()
			];
		}
	}

	// If a row wasn't generated just create the default one, a simple text block of the item's name.
	return SNew(STableRow< UObject* >, OwnerTable)
		[
			SNew(STextBlock).Text(Item ? FText::FromString(Item->GetName()) : LOCTEXT("null", "null"))
		];
}

void UTileView::SetItemWidth(float Width)
{
	MyTileView->SetItemWidth(Width);
}

void UTileView::SetItemHeight(float Height)
{
	MyTileView->SetItemHeight(Height);
}

void UTileView::RequestListRefresh()
{
	MyTileView->RequestListRefresh();
}

void UTileView::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyTileView.Reset();
}

#if WITH_EDITOR

const FText UTileView::GetPaletteCategory()
{
	return LOCTEXT("Misc", "Misc");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
