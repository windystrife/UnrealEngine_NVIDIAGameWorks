// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSequencerLabelEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "MovieScene.h"
#include "Sequencer.h"
#include "Widgets/Views/SListView.h"
#include "SSequencerLabelEditorListRow.h"


#define LOCTEXT_NAMESPACE "SSequencerLabelEditor"


/* SSequencerLabelEditor interface
 *****************************************************************************/

void SSequencerLabelEditor::Construct(const FArguments& InArgs, FSequencer& InSequencer, const TArray<FGuid>& InObjectIds)
{
	Sequencer = &InSequencer;
	ObjectIds = InObjectIds;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
					.Text(LOCTEXT("LabelAs", "Label as:"))
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SAssignNew(FilterBox, SEditableTextBox)
							.ClearKeyboardFocusOnCommit(false)
							.MinDesiredWidth(144.0f)
							.OnKeyDownHandler(this, &SSequencerLabelEditor::HandleFilterBoxKeyDown)
							.OnTextChanged(this, &SSequencerLabelEditor::HandleFilterBoxTextChanged)
							.SelectAllTextWhenFocused(true)
							.ToolTipText(LOCTEXT("FilterBoxToolTip", "Type one or more strings to filter by. New label names may not contain whitespace. Use the `.` symbol to filter or create hierarchical labels"))
					]

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
							.IsEnabled(this, &SSequencerLabelEditor::HandleCreateNewLabelButtonIsEnabled)
							.OnClicked(this, &SSequencerLabelEditor::HandleCreateNewLabelButtonClicked)
							.Content()
							[
								SNew(STextBlock)
									.Text(LOCTEXT("CreateNewLabelButton", "Create New"))
							]
					]
			]

		+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SAssignNew(LabelListView, SListView<TSharedPtr<FString>>)
					.ItemHeight(20.0f)
					.ListItemsSource(&LabelList)
					.OnGenerateRow(this, &SSequencerLabelEditor::HandleLabelListViewGenerateRow)
					.SelectionMode(ESelectionMode::None)
			]
	];

	ReloadLabelList(true);
}


/* SSequencerLabelEditor implementation
 *****************************************************************************/

void SSequencerLabelEditor::CreateLabelFromFilterText()
{
	for (auto ObjectId : ObjectIds)
	{
		Sequencer->GetLabelManager().AddObjectLabel(ObjectId, FilterBox->GetText().ToString());
	}
	FilterBox->SetText(FText::GetEmpty());
	ReloadLabelList(true);
}


void SSequencerLabelEditor::ReloadLabelList(bool FullyReload)
{
	// reload label list
	if (FullyReload)
	{
		AvailableLabels.Reset();
		Sequencer->GetLabelManager().GetAllLabels(AvailableLabels);
	}

	// filter label list
	TArray<FString> Filters;
	FilterBox->GetText().ToString().ParseIntoArray(Filters, TEXT(" "));

	LabelList.Reset();

	for (const auto& Label : AvailableLabels)
	{
		bool Matched = true;

		for (const auto& Filter : Filters)
		{
			if (!Label.Contains(Filter))
			{
				Matched = false;
				break;
			}
		}

		if (Matched)
		{
			LabelList.Add(MakeShareable(new FString(Label)));
		}
	}

	// refresh list view
	LabelListView->RequestListRefresh();
}


/* SSequencerLabelEditor callbacks
 *****************************************************************************/

FReply SSequencerLabelEditor::HandleCreateNewLabelButtonClicked()
{
	CreateLabelFromFilterText();
	return FReply::Handled();
}


bool SSequencerLabelEditor::HandleCreateNewLabelButtonIsEnabled() const
{
	FString FilterString = FilterBox->GetText().ToString();
	FilterString.TrimStartInline();

	return !FilterString.IsEmpty() && !FilterString.Contains(TEXT(" ")) && !Sequencer->GetLabelManager().LabelExists(FilterString);
}


FReply SSequencerLabelEditor::HandleFilterBoxKeyDown(const FGeometry& /*Geometry*/, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Enter)
	{
		CreateLabelFromFilterText();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}


void SSequencerLabelEditor::HandleFilterBoxTextChanged(const FText& NewText)
{
	ReloadLabelList(false);
}


TSharedRef<ITableRow> SSequencerLabelEditor::HandleLabelListViewGenerateRow(TSharedPtr<FString> Label, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SSequencerLabelEditorListRow, OwnerTable)
		.HighlightText(this, &SSequencerLabelEditor::HandleLabelListViewRowHighlightText)
		.IsChecked(this, &SSequencerLabelEditor::HandleLabelListViewRowIsChecked, Label)
		.Label(Label)
		.OnCheckStateChanged(this, &SSequencerLabelEditor::HandleLabelListViewRowCheckedStateChanged, Label);
}


void SSequencerLabelEditor::HandleLabelListViewRowCheckedStateChanged(ECheckBoxState State, TSharedPtr<FString> Label)
{
	FSequencerLabelManager& LabelManager = Sequencer->GetLabelManager();

	if (State == ECheckBoxState::Checked)
	{
		for (auto ObjectId : ObjectIds)
		{
			LabelManager.AddObjectLabel(ObjectId, *Label);
		}
	}
	else
	{
		for (auto ObjectId : ObjectIds)
		{
			LabelManager.RemoveObjectLabel(ObjectId, *Label);
		}
	}
}


FText SSequencerLabelEditor::HandleLabelListViewRowHighlightText() const
{
	return FilterBox->GetText();
}


ECheckBoxState SSequencerLabelEditor::HandleLabelListViewRowIsChecked(TSharedPtr<FString> Label) const
{
	int32 NumChecked = 0;
	for (auto ObjectId : ObjectIds)
	{
		const FMovieSceneTrackLabels* Labels = Sequencer->GetLabelManager().GetObjectLabels(ObjectId);
		if (Labels != nullptr)
		{
			if (Labels->Strings.Contains(*Label))
			{
				++NumChecked;
			}
		}
	}

	if (NumChecked == ObjectIds.Num())
	{
		return ECheckBoxState::Checked;
	}

	return ECheckBoxState::Unchecked;
}


#undef LOCTEXT_NAMESPACE
