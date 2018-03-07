// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Sequencer.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

struct FSequencerLabelTreeNode;

/**
 * Implements a widget for browsing sequencer track labels.
 */
class SSequencerLabelBrowser
	: public SCompoundWidget
{
public:

	typedef TSlateDelegates<FString>::FOnSelectionChanged FOnSelectionChanged;

	SLATE_BEGIN_ARGS(SSequencerLabelBrowser)
		: _OnSelectionChanged()
	{ }

		SLATE_EVENT(FOnSelectionChanged, OnSelectionChanged)
	SLATE_END_ARGS()

public:

	/** Destructor. */
	~SSequencerLabelBrowser();

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The construction arguments.
	 * @param InSequencer The sequencer object being visualized.
	 */
	void Construct(const FArguments& InArgs, TSharedRef<FSequencer> InSequencer);

	/**
	 * Sets the selected label.
	 *
	 * @param Label The label to select.
	 */
	void SetSelectedLabel(const FString& Label);

protected:

	/** Reloads the list of processes. */
	void ReloadLabelList(bool FullyReload);

private:

	/** Callback for when the label manager's labels changed. */
	void HandleLabelManagerLabelsChanged();

	/** Callback for when a label has been renamed in the label tree view. */
	void HandleLabelListRowLabelRenamed(TSharedPtr<FSequencerLabelTreeNode> Node, const FString& NewLabel);

	/** Callback for generating the label tree view's context menu. */
	TSharedPtr<SWidget> HandleLabelTreeViewContextMenuOpening();

	/** Callback for generating a row widget in the label tree view. */
	TSharedRef<ITableRow> HandleLabelTreeViewGenerateRow(TSharedPtr<FSequencerLabelTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback for getting the children of a node in the label tree view. */
	void HandleLabelTreeViewGetChildren(TSharedPtr<FSequencerLabelTreeNode> Item, TArray<TSharedPtr<FSequencerLabelTreeNode>>& OutChildren);

	/** Callback for when the label tree view selection changed. */
	void HandleLabelTreeViewSelectionChanged(TSharedPtr<FSequencerLabelTreeNode> InItem, ESelectInfo::Type SelectInfo);

	/** Callback for executing the 'Set Remove label' context menu entry. */
	void HandleRemoveLabelMenuEntryExecute();

	/** Callback for checking whether 'Remove label' context menu entry can execute. */
	bool HandleRemoveLabelMenuEntryCanExecute() const;

	/** Callback for executing the 'Rename label' context menu entry. */
	void HandleRenameLabelMenuEntryExecute();

	/** Callback for checking whether 'Rename' context menu entry can execute. */
	bool HandleRenameLabelMenuEntryCanExecute() const;

private:

	/** Holds the collection of root labels to be displayed in the tree view. */
	TArray<TSharedPtr<FSequencerLabelTreeNode>> LabelList;

	/** Holds the label tree view. */
	TSharedPtr<STreeView<TSharedPtr<FSequencerLabelTreeNode>>> LabelTreeView;

	/** Delegate to invoke when the selected label changed. */
	FOnSelectionChanged OnSelectionChanged;

	/** The sequencer object being visualized. */
	TWeakPtr<FSequencer> Sequencer;
};
