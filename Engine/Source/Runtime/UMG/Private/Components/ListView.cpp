// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/ListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "UMG"

/////////////////////////////////////////////////////
// UListView

UListView::UListView(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsVariable = true;

	ItemHeight = 16.0f;
	SelectionMode = ESelectionMode::Single;
}

TSharedRef<SWidget> UListView::RebuildWidget()
{
	MyListView = SNew(SListView< UObject* >)
		.SelectionMode(SelectionMode)
		.ListItemsSource(&Items)
		.ItemHeight(ItemHeight)
		.OnGenerateRow(BIND_UOBJECT_DELEGATE(SListView< UObject* >::FOnGenerateRow, HandleOnGenerateRow))
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

	return MyListView.ToSharedRef();
}

TSharedRef<ITableRow> UListView::HandleOnGenerateRow(UObject* Item, const TSharedRef< STableViewBase >& OwnerTable) const
{
	// Call the user's delegate to see if they want to generate a custom widget bound to the data source.
	if ( OnGenerateRowEvent.IsBound() )
	{
		UWidget* Widget = OnGenerateRowEvent.Execute(Item);
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


#if WITH_EDITOR

const FText UListView::GetPaletteCategory()
{
	return LOCTEXT("Misc", "Misc");
}

#endif

void UListView::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyListView.Reset();
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
