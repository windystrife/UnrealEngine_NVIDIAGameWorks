// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SRenameGameplayTagDialog.h"
#include "GameplayTagsEditorModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "RenameGameplayTag"

void SRenameGameplayTagDialog::Construct(const FArguments& InArgs)
{
	check(InArgs._GameplayTagNode.IsValid())

	GameplayTagNode = InArgs._GameplayTagNode;
	OnGameplayTagRenamed = InArgs._OnGameplayTagRenamed;

	ChildSlot
	[
		SNew( SBorder )
		.Padding(FMargin(15))
		[
			SNew( SVerticalBox )

			// Current name display
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(4.0f)
			[
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( STextBlock )
					.Text( LOCTEXT("CurrentTag", "Current Tag:"))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.Padding(8.0f, 0.0f)
				[
					SNew( STextBlock )
					.MinDesiredWidth(184.0f)
					.Text( FText::FromName(GameplayTagNode->GetCompleteTag().GetTagName() ) )
				]
			]

			
			// New name controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			.VAlign(VAlign_Top)
			[
				SNew( SHorizontalBox )

				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 4.0f)
				[
					SNew( STextBlock )
					.Text( LOCTEXT("NewTag", "New Tag:" ))
				]

				+SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.Padding(8.0f, 0.0f)
				[
					SAssignNew( NewTagNameTextBox, SEditableTextBox )
					.Text( FText::FromName(GameplayTagNode->GetCompleteTag().GetTagName() ))
					.Padding(4.0f)
					.MinDesiredWidth(180.0f)
					.OnTextCommitted( this, &SRenameGameplayTagDialog::OnRenameTextCommitted )
				]
			]

			// Dialog controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew( SHorizontalBox )

				// Rename
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 8.0f)
				[
					SNew( SButton )
					.IsFocusable( false )
					.IsEnabled( this, &SRenameGameplayTagDialog::IsRenameEnabled )
					.OnClicked( this, &SRenameGameplayTagDialog::OnRenameClicked )
					[
						SNew( STextBlock )
						.Text( LOCTEXT("RenameTagButtonText", "Rename" ) )
					]
				]

				// Cancel
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 8.0f)
				[
					SNew( SButton )
					.IsFocusable( false )
					.OnClicked( this, & SRenameGameplayTagDialog::OnCancelClicked )
					[
						SNew( STextBlock )
						.Text( LOCTEXT("CancelRenameButtonText", "Cancel"))
					]
				]
			]
		]
	];
}

bool SRenameGameplayTagDialog::IsRenameEnabled() const
{
	FString CurrentTagText;

	if (NewTagNameTextBox.IsValid())
	{
		CurrentTagText = NewTagNameTextBox->GetText().ToString();
	}

	return !CurrentTagText.IsEmpty() && CurrentTagText != GameplayTagNode->GetCompleteTag().GetTagName().ToString();
}

void SRenameGameplayTagDialog::RenameAndClose()
{
	IGameplayTagsEditorModule& Module = IGameplayTagsEditorModule::Get();

	FString TagToRename = GameplayTagNode->GetCompleteTag().GetTagName().ToString();
	FString NewTagName = NewTagNameTextBox->GetText().ToString();

	if (Module.RenameTagInINI(TagToRename, NewTagName))
	{
		OnGameplayTagRenamed.ExecuteIfBound(TagToRename, NewTagName);
	}

	CloseContainingWindow();
}

void SRenameGameplayTagDialog::OnRenameTextCommitted(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter && IsRenameEnabled())
	{
		RenameAndClose();
	}
}

FReply SRenameGameplayTagDialog::OnRenameClicked()
{
	RenameAndClose();

	return FReply::Handled();
}

FReply SRenameGameplayTagDialog::OnCancelClicked()
{
	CloseContainingWindow();

	return FReply::Handled();
}

void SRenameGameplayTagDialog::CloseContainingWindow()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> CurrentWindow = FSlateApplication::Get().FindWidgetWindow( AsShared(), WidgetPath );

	if (CurrentWindow.IsValid())
	{
		CurrentWindow->RequestDestroyWindow();
	}
}

#undef LOCTEXT_NAMESPACE
