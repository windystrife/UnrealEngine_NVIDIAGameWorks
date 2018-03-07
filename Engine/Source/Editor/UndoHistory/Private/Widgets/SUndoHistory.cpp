// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SUndoHistory.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/SUndoHistoryTableRow.h"


#define LOCTEXT_NAMESPACE "SUndoHistory"


/* SUndoHistory interface
 *****************************************************************************/

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SUndoHistory::Construct( const FArguments& InArgs )
{
	LastActiveTransactionIndex = INDEX_NONE;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(0.0f, 4.0f))
					[
						SAssignNew(UndoListView, SListView<TSharedPtr<FTransactionInfo> >)
							.ItemHeight(24.0f)
							.ListItemsSource(&UndoList)
							.SelectionMode(ESelectionMode::Single)
							.OnGenerateRow(this, &SUndoHistory::HandleUndoListGenerateRow)
							.OnSelectionChanged(this, &SUndoHistory::HandleUndoListSelectionChanged)
							.HeaderRow
							(
								SNew(SHeaderRow)
									.Visibility(EVisibility::Collapsed)

								+ SHeaderRow::Column("Title")
							)
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						// undo size
						SNew(STextBlock)
							.Text(this, &SUndoHistory::HandleUndoSizeTextBlockText)
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						// discard history button
						SNew(SButton)
							.ButtonStyle(FCoreStyle::Get(), "NoBorder")
							.OnClicked(this, &SUndoHistory::HandleDiscardHistoryButtonClicked)
							.ToolTipText(LOCTEXT("DiscardHistoryButtonToolTip", "Discard the Undo History."))
							[
								SNew(SImage)
									.Image(FEditorStyle::Get().GetBrush("TrashCan_Small"))
							]
					]
			]
	];

	ReloadUndoList();
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SUndoHistory::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// reload the transaction list if necessary
	if ((GEditor == nullptr) || (GEditor->Trans == nullptr) || (LastQueueLength != GEditor->Trans->GetQueueLength()) || (GEditor->Trans->GetUndoCount() != LastUndoCount))
	{
		ReloadUndoList();
	}

	// update the selected transaction if necessary
	if ((GEditor != nullptr) && (GEditor->Trans != nullptr))
	{
		int32 ActiveTransactionIndex = GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1;

		if (UndoList.IsValidIndex(ActiveTransactionIndex) && (ActiveTransactionIndex != LastActiveTransactionIndex))
		{
			UndoListView->SetSelection(UndoList[ActiveTransactionIndex]);
			LastActiveTransactionIndex = ActiveTransactionIndex;
		}
	}
}


/* SUndoHistory implementation
 *****************************************************************************/

void SUndoHistory::ReloadUndoList( )
{
	UndoList.Empty();

	if ((GEditor == nullptr) || (GEditor->Trans == nullptr))
	{
		return;
	}

	LastQueueLength = GEditor->Trans->GetQueueLength();
	LastUndoCount = GEditor->Trans->GetUndoCount();

	for (int32 QueueIndex = 0; QueueIndex < LastQueueLength; ++QueueIndex)
	{
		UndoList.Add(MakeShareable(new FTransactionInfo(QueueIndex, GEditor->Trans->GetTransaction(QueueIndex))));
	}

	UndoListView->RequestListRefresh();
}


/* SUndoHistory callbacks
 *****************************************************************************/

FReply SUndoHistory::HandleDiscardHistoryButtonClicked( )
{
	if ((GEditor != nullptr) && (GEditor->Trans != nullptr))
	{
		GEditor->Trans->Reset(LOCTEXT("DiscardHistoryReason", "Discard undo history."));
		ReloadUndoList();
	}

	return FReply::Handled();
}


TSharedRef<ITableRow> SUndoHistory::HandleUndoListGenerateRow( TSharedPtr<FTransactionInfo> TransactionInfo, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SUndoHistoryTableRow, OwnerTable)
		.IsApplied(this, &SUndoHistory::HandleUndoListRowIsApplied, TransactionInfo->QueueIndex)
		.QueueIndex(TransactionInfo->QueueIndex)
		.Transaction(TransactionInfo->Transaction);
}


bool SUndoHistory::HandleUndoListRowIsApplied( int32 QueueIndex ) const
{
	if ((GEditor == nullptr) || (GEditor->Trans == nullptr))
	{
		return false;
	}

	return (QueueIndex < (GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount()));
}


void SUndoHistory::HandleUndoListSelectionChanged( TSharedPtr<FTransactionInfo> InItem, ESelectInfo::Type SelectInfo )
{
	if (!InItem.IsValid() || (SelectInfo == ESelectInfo::Direct) || (GEditor == nullptr) || (GEditor->Trans == nullptr))
	{
		return;
	}

	LastActiveTransactionIndex = GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1;

	while (InItem->QueueIndex > LastActiveTransactionIndex)
	{
		if (!GEditor->Trans->Redo())
		{
			break;
		}

		++LastActiveTransactionIndex;
	}

	while (InItem->QueueIndex < LastActiveTransactionIndex)
	{
		if (!GEditor->Trans->Undo())
		{
			break;
		}

		--LastActiveTransactionIndex;
	}
}


FText SUndoHistory::HandleUndoSizeTextBlockText( ) const
{
	if ((GEditor == nullptr) || (GEditor->Trans == nullptr))
	{
		return FText::GetEmpty();
	}

	return FText::Format(LOCTEXT("TransactionCountF", "{0} Transactions ({1})"), FText::AsNumber(UndoList.Num()), FText::AsMemory(GEditor->Trans->GetUndoSize()));
}


#undef LOCTEXT_NAMESPACE
