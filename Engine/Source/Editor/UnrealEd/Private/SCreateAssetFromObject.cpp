// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCreateAssetFromObject.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "Misc/FileHelper.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "SCreateAssetFromActor"

void SCreateAssetFromObject::Construct(const FArguments& InArgs, TSharedPtr<SWindow> InParentWindow)
{
	AssetFilenameSuffix = InArgs._AssetFilenameSuffix;
	HeadingText = InArgs._HeadingText;
	CreateButtonText = InArgs._CreateButtonText;
	OnCreateAssetAction = InArgs._OnCreateAssetAction;

	bIsReportingError = false;
	AssetPath = FString("/Game");
	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = AssetPath;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateRaw(this, &SCreateAssetFromObject::OnSelectAssetPath);

	USelection::SelectionChangedEvent.AddRaw(this, &SCreateAssetFromObject::OnLevelSelectionChanged);

	// Set up PathPickerConfig.
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	ParentWindow = InParentWindow;

	FString PackageName;
	ActorInstanceLabel.Empty();

	if( InArgs._DefaultNameOverride.IsEmpty() )
	{
		USelection* SelectedActors = GEditor->GetSelectedActors();
		for(FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
		{
			AActor* Actor = Cast<AActor>(*Iter);
			if(Actor)
			{
				ActorInstanceLabel += Actor->GetActorLabel();
				ActorInstanceLabel += TEXT("_");
				break;
			}
		}
	}
	else
	{
		ActorInstanceLabel = InArgs._DefaultNameOverride.ToString();
	}

	ActorInstanceLabel = PackageTools::SanitizePackageName(ActorInstanceLabel + AssetFilenameSuffix);

	FString AssetName = ActorInstanceLabel;
	FString BasePath = AssetPath / AssetName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(HeadingText)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(FileNameWidget, SEditableTextBox)
				.Text(FText::FromString(AssetName))
				.OnTextChanged(this, &SCreateAssetFromObject::OnFilenameChanged)
			]
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.Padding(0, 20, 0, 0)
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0, 2, 6, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Bottom)
				.ContentPadding(FMargin(8, 2, 8, 2))
				.OnClicked(this, &SCreateAssetFromObject::OnCreateAssetFromActorClicked)
				.IsEnabled(this, &SCreateAssetFromObject::IsCreateAssetFromActorEnabled)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
				.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
				.Text(CreateButtonText)
			]
			+ SHorizontalBox::Slot()
			.Padding(0, 2, 0, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.VAlign(VAlign_Bottom)
				.ContentPadding(FMargin(8, 2, 8, 2))
				.OnClicked(this, &SCreateAssetFromObject::OnCancelCreateAssetFromActor)
				.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
				.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
				.Text(LOCTEXT("CancelButtonText", "Cancel"))
			]
		]
	];

	OnFilenameChanged(FText::FromString(AssetName));
}

FReply SCreateAssetFromObject::OnCreateAssetFromActorClicked()
{
	ParentWindow->RequestDestroyWindow();
	OnCreateAssetAction.ExecuteIfBound(AssetPath / FileNameWidget->GetText().ToString());
	return FReply::Handled();
}

FReply SCreateAssetFromObject::OnCancelCreateAssetFromActor()
{
	ParentWindow->RequestDestroyWindow();
	return FReply::Handled();
}

void SCreateAssetFromObject::OnSelectAssetPath(const FString& Path)
{
	AssetPath = Path;
	OnFilenameChanged(FileNameWidget->GetText());
}

void SCreateAssetFromObject::OnLevelSelectionChanged(UObject* InObjectSelected)
{
	// When actor selection changes, this window should be destroyed.
	ParentWindow->RequestDestroyWindow();
}

void SCreateAssetFromObject::OnFilenameChanged(const FText& InNewName)
{
	TArray<FAssetData> AssetData;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssetsByPath(FName(*AssetPath), AssetData);

	FText ErrorText;
	if (!FFileHelper::IsFilenameValidForSaving(InNewName.ToString(), ErrorText) || !FName(*InNewName.ToString()).IsValidObjectName(ErrorText))
	{
		FileNameWidget->SetError(ErrorText);
		bIsReportingError = true;
		return;
	}
	else
	{
		// Check to see if the name conflicts
		for (auto Iter = AssetData.CreateConstIterator(); Iter; ++Iter)
		{
			if (Iter->AssetName.ToString() == InNewName.ToString())
			{
				FileNameWidget->SetError(LOCTEXT("AssetInUseError", "Asset name already in use!"));
				bIsReportingError = true;
				return;
			}
		}
	}

	FileNameWidget->SetError(FText::FromString(TEXT("")));
	bIsReportingError = false;
}

bool SCreateAssetFromObject::IsCreateAssetFromActorEnabled() const
{
	return !bIsReportingError;
}

#undef LOCTEXT_NAMESPACE
