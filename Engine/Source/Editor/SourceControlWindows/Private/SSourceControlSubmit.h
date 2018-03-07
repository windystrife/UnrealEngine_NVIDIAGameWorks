// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "ISourceControlState.h"

class SMultiLineEditableTextBox;
class SWindow;

//-------------------------------------
// Source Control Window Constants
//-------------------------------------
namespace ESubmitResults
{
	enum Type
	{
		SUBMIT_ACCEPTED,
		SUBMIT_CANCELED
	};
}

//-----------------------------------------------
// Source Control Check in Helper Struct
//-----------------------------------------------
class FChangeListDescription
{
public:
	TArray<FString> FilesForAdd;
	TArray<FString> FilesForSubmit;
	FText Description;
};

class FSubmitItem : public TSharedFromThis<FSubmitItem>
{
public:
	/** Constructor */
	explicit FSubmitItem(const FSourceControlStateRef& InItem);

	/** Returns the full path of the item in source control */
	FString GetFilename() const { return Item->GetFilename(); }

	/** Returns the name of the item as displayed in the widget */
	FText GetDisplayName() const { return DisplayName; }

	/** Returns the name of the icon to be used in the list item widget */
	FName GetIconName() const { return Item->GetSmallIconName(); }

	/** Returns the tooltip text for the icon */
	FText GetIconTooltip() const { return Item->GetDisplayTooltip(); }

	/** Returns the checkbox state of this item */
	ECheckBoxState GetCheckBoxState() const { return CheckBoxState; }

	/** Sets the checkbox state of this item */
	void SetCheckBoxState(ECheckBoxState NewState) { CheckBoxState = NewState; }

	/** true if the item is not in source control and needs to be added prior to checkin */
	bool NeedsAdding() const { return !Item->IsSourceControlled(); }

	/** true if the item is in source control and is able to be checked in */
	bool CanCheckIn() const { return Item->CanCheckIn() || Item->IsDeleted(); }

	/** true if the item is enabled in the list */
	bool IsEnabled() const { return !Item->IsConflicted() && Item->IsCurrent(); }

private:
	/** Shared pointer to the source control state object itself */
	FSourceControlStateRef Item;

	/** Checkbox state */
	ECheckBoxState CheckBoxState;

	/** Cached name to display in the listview */
	FText DisplayName;
};

class SSourceControlSubmitWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSourceControlSubmitWidget)
		: _ParentWindow()
		, _Items()
	{}

		SLATE_ATTRIBUTE(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ATTRIBUTE(TArray<FSourceControlStateRef>, Items)

	SLATE_END_ARGS()

	/** Constructor */
	SSourceControlSubmitWidget()
	{
	}

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

	/** Get dialog result */
	ESubmitResults::Type GetResult() { return DialogResult; }

	/** Returns a widget representing the item and column supplied */
	TSharedRef<SWidget> GenerateWidgetForItemAndColumn(TSharedPtr<FSubmitItem> Item, const FName ColumnID) const;

	/** Gets the requested files and the change list description*/
	void FillChangeListDescription(FChangeListDescription& OutDesc);

	/** Does the user want to keep the files checked out */
	bool WantToKeepCheckedOut();

private:
	/**
	 * @return the desired toggle state for the ToggleSelectedCheckBox.
	 * Returns Unchecked, unless all of the selected items are Checked.
	 */
	ECheckBoxState GetToggleSelectedState() const;

	/**
	 * Toggles the highlighted items.
	 * If no items are explicitly highlighted, toggles all items in the list.
	 */
	void OnToggleSelectedCheckBox(ECheckBoxState InNewState);

	/** Called when the settings of the dialog are to be accepted*/
	FReply OKClicked();

	/** Called when the settings of the dialog are to be ignored*/
	FReply CancelClicked();

	/** Called to check if the OK button is enabled or not. */
	bool IsOKEnabled() const;

	/** Check if the warning panel should be visible. */
	EVisibility IsWarningPanelVisible() const;

	/** Called when the Keep checked out Checkbox is changed */
	void OnCheckStateChanged_KeepCheckedOut(ECheckBoxState InState);

	/** Get the current state of the Keep Checked Out checkbox  */
	ECheckBoxState GetKeepCheckedOut() const;

	/** Check if Provider can checkout files */
	bool CanCheckOut() const;

	/** Called by SListView to get a widget corresponding to the supplied item */
	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FSubmitItem> SubmitItemData, const TSharedRef<STableViewBase>& OwnerTable);

	/**
	 * Returns the current column sort mode (ascending or descending) if the ColumnId parameter matches the current
	 * column to be sorted by, otherwise returns EColumnSortMode_None.
	 *
	 * @param	ColumnId	Column ID to query sort mode for.
	 *
	 * @return	The sort mode for the column, or EColumnSortMode_None if it is not known.
	 */
	EColumnSortMode::Type GetColumnSortMode(const FName ColumnId) const;

	/**
	 * Callback for SHeaderRow::Column::OnSort, called when the column to sort by is changed.
	 *
	 * @param	ColumnId	The new column to sort by
	 * @param	InSortMode	The sort mode (ascending or descending)
	 */
	void OnColumnSortModeChanged(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type InSortMode);

	/**
	 * Requests that the source list data be sorted according to the current sort column and mode,
	 * and refreshes the list view.
	 */
	void RequestSort();

	/**
	 * Sorts the source list data according to the current sort column and mode.
	 */
	void SortTree();

private:
	ESubmitResults::Type DialogResult;

	/** ListBox for selecting which object to consolidate */
	TSharedPtr<SListView<TSharedPtr<FSubmitItem>>> ListView;

	/** Collection of objects (Widgets) to display in the List View. */
	TArray<TSharedPtr<FSubmitItem>> ListViewItems;

	/** Pointer to the parent modal window */
	TWeakPtr<SWindow> ParentFrame;

	/** Internal widgets to save having to get in multiple places*/
	TSharedPtr<SMultiLineEditableTextBox> ChangeListDescriptionTextCtrl;

	/** State of the "Keep checked out" checkbox */
	ECheckBoxState	KeepCheckedOut;

	/** Specify which column to sort with */
	FName SortByColumn;

	/** Currently selected sorting mode */
	EColumnSortMode::Type SortMode;
};

class SSourceControlSubmitListRow : public SMultiColumnTableRow<TSharedPtr<FSubmitItem>>
{
public:

	SLATE_BEGIN_ARGS(SSourceControlSubmitListRow) {}

	/** The SSourceControlSubmitWidget that owns the tree.  We'll only keep a weak reference to it. */
	SLATE_ARGUMENT(TSharedPtr<SSourceControlSubmitWidget>, SourceControlSubmitWidget)

		/** The list item for this row */
		SLATE_ARGUMENT(TSharedPtr<FSubmitItem>, Item)

	SLATE_END_ARGS()

	/** Construct function for this widget */
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the list row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

private:

	/** Weak reference to the SSourceControlSubmitWidget that owns our list */
	TWeakPtr<SSourceControlSubmitWidget> SourceControlSubmitWidgetPtr;

	/** The item associated with this row of data */
	TSharedPtr<FSubmitItem> Item;
};
