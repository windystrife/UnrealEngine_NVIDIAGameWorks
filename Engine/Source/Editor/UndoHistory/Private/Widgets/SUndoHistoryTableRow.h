// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Widgets/SWidget.h"
#include "Layout/Margin.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STableRow.h"
#include "SlateOptMacros.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor/Transactor.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "SUndoHistoryTableRow"

/**
 * Implements a row widget for the undo history list.
 */
class SUndoHistoryTableRow
	: public SMultiColumnTableRow<TSharedPtr<int32> >
{
public:

	SLATE_BEGIN_ARGS(SUndoHistoryTableRow) { }
		SLATE_ATTRIBUTE(bool, IsApplied)
		SLATE_ARGUMENT(int32, QueueIndex)
		SLATE_ARGUMENT(const FTransaction*, Transaction)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The construction arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		IsApplied = InArgs._IsApplied;
		QueueIndex = InArgs._QueueIndex;

		UObject* ContextObject = InArgs._Transaction->GetContext().PrimaryObject;

		if (ContextObject != nullptr)
		{
			Title = FText::Format(LOCTEXT("UndoHistoryTableRowTitleF", "{0} [{1}]"), InArgs._Transaction->GetTitle(), FText::FromString(ContextObject->GetFName().ToString()));
		}
		else
		{
			Title = InArgs._Transaction->GetTitle();
		}

		SMultiColumnTableRow<TSharedPtr<int32> >::Construct(FSuperRowType::FArguments(), InOwnerTableView);
	}

public:

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override
	{
		if (ColumnName == "Title")
		{
			return SNew(SBox)
				.Padding(FMargin(4.0f, 0.0f))
				[
					SNew(STextBlock)
						.ColorAndOpacity(this, &SUndoHistoryTableRow::HandleTitleTextColorAndOpacity)
						.Text(Title)
				];
		}

		return SNullWidget::NullWidget;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

private:

	// Callback for getting the color of the 'Title' text.
	FSlateColor HandleTitleTextColorAndOpacity( ) const
	{
		if (IsApplied.Get())
		{
			return FSlateColor::UseForeground();
		}

		return FSlateColor::UseSubduedForeground();
	}

private:

	// Holds an attribute that determines whether the transaction in this row is applied.
	TAttribute<bool> IsApplied;

	// Holds the transaction's index in the transaction queue.
	int32 QueueIndex;

	// Holds the transaction's title text.
	FText Title;
};


#undef LOCTEXT_NAMESPACE
