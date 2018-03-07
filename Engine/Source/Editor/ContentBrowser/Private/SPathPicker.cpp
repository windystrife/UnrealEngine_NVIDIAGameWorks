// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SPathPicker.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "ContentBrowserUtils.h"
#include "SPathView.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"


void SPathPicker::Construct( const FArguments& InArgs )
{
	for (auto DelegateIt = InArgs._PathPickerConfig.SetPathsDelegates.CreateConstIterator(); DelegateIt; ++DelegateIt)
	{
		if ((*DelegateIt) != NULL)
		{
			(**DelegateIt) = FSetPathPickerPathsDelegate::CreateSP(this, &SPathPicker::SetPaths);
		}
	}

	FOnGetFolderContextMenu OnGetFolderContextMenuDelegate = InArgs._PathPickerConfig.OnGetFolderContextMenu.IsBound() ? InArgs._PathPickerConfig.OnGetFolderContextMenu : FOnGetFolderContextMenu::CreateSP(this, &SPathPicker::GetFolderContextMenu);

	ChildSlot
	[
		SAssignNew(PathViewPtr, SPathView)
		.OnPathSelected(InArgs._PathPickerConfig.OnPathSelected)
		.OnGetFolderContextMenu(OnGetFolderContextMenuDelegate)
		.OnGetPathContextMenuExtender(InArgs._PathPickerConfig.OnGetPathContextMenuExtender)
		.FocusSearchBoxWhenOpened(InArgs._PathPickerConfig.bFocusSearchBoxWhenOpened)
		.AllowContextMenu(InArgs._PathPickerConfig.bAllowContextMenu)
		.AllowClassesFolder(InArgs._PathPickerConfig.bAllowClassesFolder)
		.SelectionMode(ESelectionMode::Single)
	];

	const FString& DefaultPath = InArgs._PathPickerConfig.DefaultPath;
	if ( !DefaultPath.IsEmpty() )
	{
		if (InArgs._PathPickerConfig.bAddDefaultPath)
		{
			PathViewPtr->AddPath(DefaultPath, false);
		}

		TArray<FString> SelectedPaths;
		SelectedPaths.Add(DefaultPath);
		PathViewPtr->SetSelectedPaths(SelectedPaths);
	}
}

TSharedPtr<SWidget> SPathPicker::GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder)
{
	TSharedPtr<FExtender> Extender;
	if (InMenuExtender.IsBound())
	{
		Extender = InMenuExtender.Execute(SelectedPaths);
	}

	const bool bInShouldCloseWindowAfterSelection = true;
	const bool bCloseSelfOnly = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterSelection, nullptr, Extender, bCloseSelfOnly);

	// We can only create folders when we have a single path selected
	const bool bCanCreateNewFolder = SelectedPaths.Num() == 1 && ContentBrowserUtils::IsValidPathToCreateNewFolder(SelectedPaths[0]);

	FText NewFolderToolTip;
	if(SelectedPaths.Num() == 1)
	{
		if(bCanCreateNewFolder)
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_CreateIn", "Create a new folder in {0}."), FText::FromString(SelectedPaths[0]));
		}
		else
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_InvalidPath", "Cannot create new folders in {0}."), FText::FromString(SelectedPaths[0]));
		}
	}
	else
	{
		NewFolderToolTip = LOCTEXT("NewFolderTooltip_InvalidNumberOfPaths", "Can only create folders when there is a single path selected.");
	}

	// New Folder
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewFolder", "New Folder"),
		NewFolderToolTip,
		FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPathPicker::CreateNewFolder, SelectedPaths.Num() > 0 ? SelectedPaths[0] : FString(), InOnCreateNewFolder),
			FCanExecuteAction::CreateLambda( [bCanCreateNewFolder] { return bCanCreateNewFolder; } )
			),
		"NewFolder"
		);

	return MenuBuilder.MakeWidget();
}

void SPathPicker::CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder)
{
	// Create a valid base name for this folder
	FText DefaultFolderBaseName = LOCTEXT("DefaultFolderName", "NewFolder");
	FText DefaultFolderName = DefaultFolderBaseName;
	int32 NewFolderPostfix = 1;
	while (ContentBrowserUtils::DoesFolderExist(FolderPath / DefaultFolderName.ToString()))
	{
		DefaultFolderName = FText::Format(LOCTEXT("DefaultFolderNamePattern", "{0}{1}"), DefaultFolderBaseName, FText::AsNumber(NewFolderPostfix));
		NewFolderPostfix++;
	}

	InOnCreateNewFolder.ExecuteIfBound(DefaultFolderName.ToString(), FolderPath);
}

void SPathPicker::SetPaths(const TArray<FString>& NewPaths)
{
	PathViewPtr->SetSelectedPaths(NewPaths);
}

TArray<FString> SPathPicker::GetPaths() const
{
	return PathViewPtr->GetSelectedPaths();
}

const TSharedPtr<SPathView>& SPathPicker::GetPathView() const
{
	return PathViewPtr;
}

#undef LOCTEXT_NAMESPACE
