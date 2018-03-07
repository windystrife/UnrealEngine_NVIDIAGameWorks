// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Styling/SlateColor.h"
#include "Layout/Margin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "Widgets/Input/SEditableLabel.h"

struct FSequencerLabelTreeNode;

/** Delegate that is executed whenever a label has been renamed. */
DECLARE_DELEGATE_TwoParams(FOnSequencerLabelRenamed, TSharedPtr<FSequencerLabelTreeNode> /*Node*/, const FString& /*NewLabel*/);


/**
 * Represents a node in the label tree.
 */
struct FSequencerLabelTreeNode
{
public:

	/** Holds the child label nodes. */
	TArray<TSharedPtr<FSequencerLabelTreeNode>> Children;

	/** Holds the display name text. */
	FText DisplayName;

	/** Holds the label. */
	FString Label;

public:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InLabel The node's label.
	 */
	FSequencerLabelTreeNode(const FString& InLabel, const FText& InDisplayName)
		: DisplayName(InDisplayName)
		, Label(InLabel)
	{ }
};


#define LOCTEXT_NAMESPACE "SSequencerLabelListRow"

/**
 * Implements a row widget for the label browser tree view.
 */
class SSequencerLabelListRow
	: public STableRow<TSharedPtr<FSequencerLabelTreeNode>>
{
public:

	SLATE_BEGIN_ARGS(SSequencerLabelListRow) { }
		/** The label tree node data visualized in this list row. */
		SLATE_ARGUMENT(TSharedPtr<FSequencerLabelTreeNode>, Node)

		/** Called whenever the folder has been renamed. */
		SLATE_EVENT(FOnSequencerLabelRenamed, OnLabelRenamed)
	SLATE_END_ARGS()

public:

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The arguments.
	 */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView )
	{
		Node = InArgs._Node;
		OnLabelRenamed = InArgs._OnLabelRenamed;

		STableRow<TSharedPtr<FSequencerLabelTreeNode>>::Construct(
			STableRow<TSharedPtr<FSequencerLabelTreeNode>>::FArguments()
				.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
				.Content()
				[
					SNew(SHorizontalBox)

					// folder icon
					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 2.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage) 
								.Image(this, &SSequencerLabelListRow::HandleFolderIconImage)
								.ColorAndOpacity(this, &SSequencerLabelListRow::HandleFolderIconColor)
						]

					// folder name
					+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(0.0f, 2.0f)
						.VAlign(VAlign_Center)
						[
							SAssignNew(EditableLabel, SEditableLabel)
								.CanEdit(this, &SSequencerLabelListRow::HandleFolderNameCanEdit)
								.OnTextChanged(this, &SSequencerLabelListRow::HandleFolderNameTextChanged)
								.Text(
									Node->Label.IsEmpty()
										? LOCTEXT("AllTracksLabel", "All Tracks")
										: FText::FromString(Node->Label)
								)
						]
				],
				InOwnerTableView
			);
	}

	/** Change the label text to edit mode. */
	void EnterRenameMode()
	{
		EditableLabel->EnterTextMode();
	}

private:

	EVisibility HandleEditIconVisibility() const
	{
		return IsHovered()
			? EVisibility::Visible
			: EVisibility::Collapsed;
	}

	const FSlateBrush* HandleFolderIconImage() const
	{
		static const FSlateBrush* FolderOpenBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
		static const FSlateBrush* FolderClosedBrush = FEditorStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");

		return IsItemExpanded()
			? FolderOpenBrush
			: FolderClosedBrush;
	}

	FSlateColor HandleFolderIconColor() const
	{
		// TODO sequencer: gmp: allow folder color customization
		return FLinearColor::Gray;
	}

	bool HandleFolderNameCanEdit() const
	{
		return !Node->Label.IsEmpty();
	}

	void HandleFolderNameTextChanged(const FText& NewLabel)
	{
		FString NewLabelString = NewLabel.ToString();

		if (NewLabelString != Node->Label)
		{
			OnLabelRenamed.ExecuteIfBound(Node.ToSharedRef(), NewLabelString);
		}
	}

private:

	/** Holds the editable text label widget. */
	TSharedPtr<SEditableLabel> EditableLabel;

	/** Holds the label node. */
	TSharedPtr<FSequencerLabelTreeNode> Node;

	/** A delegate that is executed whenever the label has been renamed. */
	FOnSequencerLabelRenamed OnLabelRenamed;
};


#undef LOCTEXT_NAMESPACE
