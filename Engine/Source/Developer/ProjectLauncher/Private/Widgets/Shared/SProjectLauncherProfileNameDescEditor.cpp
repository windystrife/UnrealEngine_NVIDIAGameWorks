// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SProjectLauncherProfileNameDescEditor.h"

#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SProjectLauncherProfileNameDescEditor"


void SProjectLauncherProfileNameDescEditor::Construct(const FArguments& InArgs, const TSharedRef<FProjectLauncherModel>& InModel, bool InShowAddDescriptionText)
{
	EnterTextDescription = FText(LOCTEXT("LaunchProfileEnterDescription", "Enter a description here."));

	Model = InModel;
	LaunchProfileAttr = InArgs._LaunchProfile;
	bShowAddDescriptionText = InShowAddDescriptionText;

	ChildSlot
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(40)
				.HeightOverride(40)
				[
					SNew(SImage)
					.Image(this, &SProjectLauncherProfileNameDescEditor::HandleProfileImage)
				]
			]

			+ SHorizontalBox::Slot()
				.FillWidth(1)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2, 4, 2, 4)
					[
						SAssignNew(NameEditableTextBlock, SInlineEditableTextBlock)
						.Text(this, &SProjectLauncherProfileNameDescEditor::OnGetNameText)
						.OnTextCommitted(this, &SProjectLauncherProfileNameDescEditor::OnNameTextCommitted)
						.Cursor(EMouseCursor::TextEditBeam)
					]

					+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2, 4, 2, 4)
						[
							SNew(SInlineEditableTextBlock)
							.Text(this, &SProjectLauncherProfileNameDescEditor::OnGetDescriptionText)
							.Style(FCoreStyle::Get(), "InlineEditableTextBlockSmallStyle")
							.OnTextCommitted(this, &SProjectLauncherProfileNameDescEditor::OnDescriptionTextCommitted)
							.Cursor(EMouseCursor::TextEditBeam)
						]
				]
		];
}


void SProjectLauncherProfileNameDescEditor::TriggerNameEdit()
{
	if (NameEditableTextBlock.IsValid())
	{
		NameEditableTextBlock->EnterEditingMode();
	}
}


#undef LOCTEXT_NAMESPACE
