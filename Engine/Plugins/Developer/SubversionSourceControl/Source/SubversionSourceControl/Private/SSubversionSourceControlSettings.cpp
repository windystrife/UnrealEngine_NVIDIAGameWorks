// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSubversionSourceControlSettings.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "EditorStyleSet.h"
#include "SubversionSourceControlModule.h"

TWeakPtr<SEditableTextBox> SSubversionSourceControlSettings::PasswordTextBox;

#define LOCTEXT_NAMESPACE "SSubversionSourceControlSettings"

void SSubversionSourceControlSettings::Construct(const FArguments& InArgs)
{
	FSlateFontInfo Font = FEditorStyle::GetFontStyle(TEXT("SourceControl.LoginWindow.Font"));

	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( FEditorStyle::GetBrush("DetailsView.CategoryBottom") )
		.Padding( FMargin( 0.0f, 3.0f, 0.0f, 0.0f ) )
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RepositoryLabel", "Repository"))
					.ToolTipText(LOCTEXT("RepositoryLabel_Tooltip", "Address of SVN repository"))
					.Font(Font)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("UserNameLabel", "User Name"))
					.ToolTipText(LOCTEXT("UserNameLabel_Tooltip", "SVN username"))
					.Font(Font)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("LabelsDirectoryLabel", "Labels Directory"))
					.ToolTipText(LOCTEXT("LabelsDirectoryLabel_Tooltip", "Relative path to repository root where labels/tags are stored. For example, if the labels/tags were to be stored in 'http://repo-name/tags/', then the path here would be 'tags/'"))
					.Font(Font)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("PasswordLabel", "Password"))
					.ToolTipText(LOCTEXT("Password_Tooltip", "Enter your password here if your repository requires it.\nYour credentials will be stored by Subversion once you have successfully logged on, so you won't have to enter it again."))
					.Font(Font)
				]
			]
			+SHorizontalBox::Slot()
			.FillWidth(2.0f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SSubversionSourceControlSettings::GetRepositoryText)
					.ToolTipText(LOCTEXT("RepositoryLabel_Tooltip", "Address of SVN repository"))
					.OnTextCommitted(this, &SSubversionSourceControlSettings::OnRepositoryTextCommited)
					.OnTextChanged(this, &SSubversionSourceControlSettings::OnRepositoryTextCommited, ETextCommit::Default)
					.Font(Font)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SSubversionSourceControlSettings::GetUserNameText)
					.ToolTipText(LOCTEXT("UserNameLabel_Tooltip", "SVN username"))
					.OnTextCommitted(this, &SSubversionSourceControlSettings::OnUserNameTextCommited)
					.OnTextChanged(this, &SSubversionSourceControlSettings::OnUserNameTextCommited, ETextCommit::Default)
					.Font(Font)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				[
					SNew(SEditableTextBox)
					.Text(this, &SSubversionSourceControlSettings::GetLabelsRootText)
					.ToolTipText(LOCTEXT("LabelsDirectoryLabel_Tooltip", "Relative path to repository root where labels/tags are stored. For example, if the labels/tags were to be stored in 'http://repo-name/tags/', then the path here would be 'tags/'"))
					.OnTextCommitted(this, &SSubversionSourceControlSettings::OnLabelsRootTextCommited)
					.OnTextChanged(this, &SSubversionSourceControlSettings::OnLabelsRootTextCommited, ETextCommit::Default)
					.Font(Font)
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(2.0f)
				[
					SAssignNew(PasswordTextBox, SEditableTextBox)
					.IsPassword(true)
					.HintText(LOCTEXT("PasswordHint", "Enter password here if required"))
					.ToolTipText(LOCTEXT("Password_Tooltip", "Enter your password here if your repository requires it.\nYour credentials will be stored by Subversion once you have successfully logged on, so you won't have to enter it again."))
					.Font(Font)
				]				
			]
		]
	];
}

FText SSubversionSourceControlSettings::GetRepositoryText() const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	return FText::FromString(SubversionSourceControl.AccessSettings().GetRepository());
}

void SSubversionSourceControlSettings::OnRepositoryTextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	SubversionSourceControl.AccessSettings().SetRepository(InText.ToString());
	SubversionSourceControl.SaveSettings();
}

FText SSubversionSourceControlSettings::GetUserNameText() const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	return FText::FromString(SubversionSourceControl.AccessSettings().GetUserName());
}

void SSubversionSourceControlSettings::OnUserNameTextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	SubversionSourceControl.AccessSettings().SetUserName(InText.ToString());
	SubversionSourceControl.SaveSettings();
}

FText SSubversionSourceControlSettings::GetLabelsRootText() const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	return FText::FromString(SubversionSourceControl.AccessSettings().GetLabelsRoot());
}

void SSubversionSourceControlSettings::OnLabelsRootTextCommited(const FText& InText, ETextCommit::Type InCommitType) const
{
	FSubversionSourceControlModule& SubversionSourceControl = FModuleManager::LoadModuleChecked<FSubversionSourceControlModule>( "SubversionSourceControl" );
	SubversionSourceControl.AccessSettings().SetLabelsRoot(InText.ToString());
	SubversionSourceControl.SaveSettings();
}

FString SSubversionSourceControlSettings::GetPassword()
{
	if(PasswordTextBox.IsValid())
	{
		return PasswordTextBox.Pin()->GetText().ToString();
	}
	return FString();
}

#undef LOCTEXT_NAMESPACE
