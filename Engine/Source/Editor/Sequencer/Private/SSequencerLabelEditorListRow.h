// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "SSequencerLabelListRow"

/**
 * Implements a row widget for the label browser tree view.
 */
class SSequencerLabelEditorListRow
	: public STableRow<TSharedPtr<FString>>
{
public:

	SLATE_BEGIN_ARGS(SSequencerLabelEditorListRow) { }
		SLATE_ATTRIBUTE(FText, HighlightText)
		SLATE_ATTRIBUTE(ECheckBoxState, IsChecked)
		SLATE_ARGUMENT(TSharedPtr<FString>, Label)
		SLATE_EVENT(FOnCheckStateChanged, OnCheckStateChanged)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The arguments.
	 */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SCheckBox)
						.IsChecked(InArgs._IsChecked)
						.OnCheckStateChanged(InArgs._OnCheckStateChanged)
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
						.HighlightText(InArgs._HighlightText)
						.Text(FText::FromString(*InArgs._Label))
				]
		];

		STableRow<TSharedPtr<FString>>::ConstructInternal(
			STableRow<TSharedPtr<FString>>::FArguments()
				.ShowSelection(false)
				.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow"),
			InOwnerTableView
		);
	}
};


#undef LOCTEXT_NAMESPACE
