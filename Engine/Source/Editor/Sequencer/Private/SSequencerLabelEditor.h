// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"

class FSequencer;
class SEditableTextBox;

class SSequencerLabelEditor
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SSequencerLabelEditor)
	{ }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InObjectIds The unique identifier of the objects whose labels are being edited.
	 */
	void Construct(const FArguments& InArgs, FSequencer& InSequencer, const TArray<FGuid>& InObjectIds);

protected:

	/** Create a new label from the text in the filter box. */
	void CreateLabelFromFilterText();

	/**
	 * Reload the list of track labels.
	 *
	 * @param FullyReload Whether to fully reload entries, or only re-apply filtering.
	 */
	void ReloadLabelList(bool FullyReload);

private:

	/** Callback for clicking the 'Create new label' button. */
	FReply HandleCreateNewLabelButtonClicked();

	/** Callback for getting the enabled state of the 'Create new label' button. */
	bool HandleCreateNewLabelButtonIsEnabled() const;

	/** Callback for handling key down events in the filter text box. */
	FReply HandleFilterBoxKeyDown(const FGeometry&, const FKeyEvent& KeyEvent);

	/** Callback for text changes in the filter text box. */
	void HandleFilterBoxTextChanged(const FText& NewText);

	/** Callback for generating a row widget in the device service list view. */
	TSharedRef<ITableRow> HandleLabelListViewGenerateRow(TSharedPtr<FString> Label, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback for changing the state of a list view row's check box. */
	void HandleLabelListViewRowCheckedStateChanged(ECheckBoxState State, TSharedPtr<FString> Label);

	/** Callback for getting the highlight text in session rows. */
	FText HandleLabelListViewRowHighlightText() const;

	/** Callback for determining whether the check box of a list view row is checked. */
	ECheckBoxState HandleLabelListViewRowIsChecked(TSharedPtr<FString> Label) const;

private:

	/** The list of available track labels. */
	TArray<FString> AvailableLabels;

	/** The label filter text box. */
	TSharedPtr<SEditableTextBox> FilterBox;

	/** The filtered list of track labels. */
	TArray<TSharedPtr<FString>> LabelList;

	/** Holds the list view for filtered track labels. */
	TSharedPtr<SListView<TSharedPtr<FString>>> LabelListView;

	/** The identifiers of the objects being edited. */
	TArray<FGuid> ObjectIds;

	/** The sequencer object. */
	FSequencer* Sequencer;
};
