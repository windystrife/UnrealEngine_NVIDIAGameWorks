// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SFilePathBlock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "FilePathBlock"

void SFilePathBlock::Construct(const FArguments& InArgs)
{
	FText BrowseForFolderToolTipText;

	const bool bReadOnlyFolderPath = InArgs._ReadOnlyFolderPath;

	if (bReadOnlyFolderPath)
	{
		BrowseForFolderToolTipText = LOCTEXT("BrowseForFolderDisabled", "You cannot modify this location");
	}
	else
	{
		BrowseForFolderToolTipText = LOCTEXT("BrowseForFolder", "Browse for a folder");
	}

	ChildSlot
	[
		SNew(SGridPanel)
		.FillColumn(0, 2.f)
		.FillColumn(1, 1.f)

		// Folder input
		+ SGridPanel::Slot(0, 0)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SAssignNew(FolderPathTextBox, SEditableTextBox)
				.Text(InArgs._FolderPath)
				// Large right hand padding to make room for the browse button
				.Padding(FMargin(5.f, 3.f, 25.f, 3.f))
				.OnTextChanged(InArgs._OnFolderChanged)
				.OnTextCommitted(InArgs._OnFolderCommitted)
				.IsReadOnly(bReadOnlyFolderPath)
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "FilePath.FolderButton")
				.ContentPadding(FMargin(4.f, 0.f))
				.OnClicked(InArgs._OnBrowseForFolder)
				.ToolTipText(BrowseForFolderToolTipText)
				.Text(LOCTEXT("...", "..."))
				.IsEnabled( !bReadOnlyFolderPath )
			]
		]

		// Folder label
		+ SGridPanel::Slot(0, 1)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(3)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("FilePath.GroupIndicator"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.5f))
					.Padding(FMargin(150.f, 0.f))
				]
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.Padding(5.f)
				.BorderImage(InArgs._LabelBackgroundBrush)
				.BorderBackgroundColor(InArgs._LabelBackgroundColor)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Folder", "Folder"))
				]
			]
		]

		// Name input
		+ SGridPanel::Slot(1, 0)
		.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
		.VAlign(VAlign_Center)
		[
			SAssignNew(NameTextBox, SEditableTextBox)
			.Text(InArgs._Name)
			.Padding(FMargin(5.f, 3.f))
			.HintText(InArgs._NameHint)
			.OnTextChanged(InArgs._OnNameChanged)
			.OnTextCommitted(InArgs._OnNameCommitted)
		]

		// Name label
		+ SGridPanel::Slot(1, 1)
		.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(3)
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("FilePath.GroupIndicator"))
					.BorderBackgroundColor(FLinearColor(1.f, 1.f, 1.f, 0.5f))
					.Padding(FMargin(75.f, 0.f))
				]
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.Padding(5.f)
				.BorderImage(InArgs._LabelBackgroundBrush)
				.BorderBackgroundColor(InArgs._LabelBackgroundColor)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Name", "Name"))
				]
			]
		]
	];
}

void SFilePathBlock::SetFolderPathError(const FText& ErrorText)
{
	if (FolderPathTextBox.IsValid())
	{
		FolderPathTextBox->SetError(ErrorText);
	}
}

void SFilePathBlock::SetNameError(const FText& ErrorText)
{
	if (NameTextBox.IsValid())
	{
		NameTextBox->SetError(ErrorText);
	}
}

#undef LOCTEXT_NAMESPACE
