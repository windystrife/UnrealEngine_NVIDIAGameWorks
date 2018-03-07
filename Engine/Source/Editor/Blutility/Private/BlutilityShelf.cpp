// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlutilityShelf.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "EditorUtilityBlueprint.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"

#include "Toolkits/AssetEditorManager.h"
#include "CollectionManagerTypes.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "ICollectionManager.h"
#include "CollectionManagerModule.h"

#define LOCTEXT_NAMESPACE "BlutilityShelf"

/////////////////////////////////////////////////////

namespace BlutilityModule
{
	static const FName BlutilityShelfCollectionName = FName(TEXT("MyBlutilityShelf"));
}

/////////////////////////////////////////////////////
// SBlutilityShelf

void SBlutilityShelf::BuildShelf()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateStatic(&SBlutilityShelf::OnBlutilityDoubleClicked);
	AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SBlutilityShelf::OnBlutilityGetContextMenu);
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.bShowBottomToolbar = false;
	AssetPickerConfig.bAutohideSearchBar = bInFavoritesMode ? true : false;

	AssetPickerConfig.Filter.ClassNames.Add(UEditorUtilityBlueprint::StaticClass()->GetFName());
	if (bInFavoritesMode)
	{
		new (AssetPickerConfig.Collections) FCollectionNameType(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local);
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		]
	];
}

void SBlutilityShelf::Construct(const FArguments& InArgs)
{
	bInFavoritesMode = false;

	// Check the shelf collection
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	if (CollectionManagerModule.Get().CollectionExists(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local))
	{
		// Start up in favorites mode if it contains something
		TArray<FName> AssetPaths;
		CollectionManagerModule.Get().GetAssetsInCollection(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local, /*out*/ AssetPaths);

		bInFavoritesMode = AssetPaths.Num() > 0;
	}
	else
	{
		// Create the collection
		CollectionManagerModule.Get().CreateCollection(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local, ECollectionStorageMode::Static);
	}

	// Build the shelf
	BuildShelf();
}

void SBlutilityShelf::OnBlutilityDoubleClicked(const struct FAssetData& AssetData)
{
	UEditorUtilityBlueprint* Blueprint = NULL;
	if (!AssetData.IsAssetLoaded())
	{
		//@TODO: Slow progress dialog here
		Blueprint = Cast<UEditorUtilityBlueprint>(AssetData.GetAsset());
	}
	else
	{ //-V523 Remove it when todo will be implemented
		Blueprint = Cast<UEditorUtilityBlueprint>(AssetData.GetAsset());
	}
		
	if (Blueprint != NULL)
	{
		FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);
	}
}

TSharedPtr<SWidget> SBlutilityShelf::OnBlutilityGetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	//@TODO: Perform code like in ContentBrowserUtils::LoadAssetsIfNeeded
	TArray<UObject*> SelectedObjects;
	for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		const FAssetData& AssetInfo = *AssetIt;

		if (UObject* Asset = AssetInfo.GetAsset())
		{
			SelectedObjects.Add(Asset);
		}
	}

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/ true, NULL);

	// Only add something if at least one asset is selected
	bool bAddedAnything = false;
	if (SelectedObjects.Num())
	{
		// Add any type-specific context menu options
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		bAddedAnything = AssetToolsModule.Get().GetAssetActions(SelectedObjects, MenuBuilder, /*bIncludeHeading=*/true);
	}

	MenuBuilder.BeginSection("ShelfManagement", LOCTEXT("BlutilityShelfMenuItemsHeading", "Shelf Management"));
	{
		if (bAddedAnything)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("FavoriteTool", "Show on shelf"),
				LOCTEXT("FavoriteTool_Tooltip", "Should this blueprint be shown as a favorite on the compact shelf"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "IconName"),
				FUIAction(
				FExecuteAction::CreateStatic(&SBlutilityShelf::ToggleFavoriteStatusOnSelection, SelectedAssets, true),
				FCanExecuteAction(),
				FIsActionChecked::CreateStatic(&SBlutilityShelf::GetFavoriteStatusOnSelection, SelectedAssets)
				),
				NAME_None,
				EUserInterfaceActionType::Check
				);
		}

		if (bInFavoritesMode)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditShelfButton", "Edit Shelf"),
				LOCTEXT("EditShelfButton_Tooltip", "Edit the shelf"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "IconName"),
				FUIAction(FExecuteAction::CreateSP(this, &SBlutilityShelf::ToggleShelfMode)));
		}
		else
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("FinishEditShelfButton", "Finish Editing Shelf"),
				LOCTEXT("FinishEditShelfButton_Tooltip", "Switch back to the compact shelf mode"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "IconName"),
				FUIAction(FExecuteAction::CreateSP(this, &SBlutilityShelf::ToggleShelfMode)));
		}
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SBlutilityShelf::ToggleFavoriteStatusOnSelection(TArray<FAssetData> AssetList, bool bIsNewFavorite)
{
	if (AssetList.Num() > 0)
	{
		// Get the list to add/remove
		TArray<FName> AllPaths;
		for (auto AssetIt = AssetList.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			AllPaths.Add(AssetData.ObjectPath);
		}

		// Determine the current status
		bool bAreAnySelected;
		bool bAreAllSelected;
		GetFavoriteStatus(AssetList, /*out*/ bAreAnySelected, /*out*/ bAreAllSelected);

		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		if (bAreAllSelected)
		{
			// Remove them
			CollectionManagerModule.Get().RemoveFromCollection(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local, AllPaths);
		}
		else
		{
			// Add them
			CollectionManagerModule.Get().AddToCollection(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local, AllPaths);
		}
	}
}

bool SBlutilityShelf::GetFavoriteStatusOnSelection(TArray<FAssetData> AssetList)
{
	bool bAreAnySelected;
	bool bAreAllSelected;
	GetFavoriteStatus(AssetList, /*out*/ bAreAnySelected, /*out*/ bAreAllSelected);
	return bAreAnySelected;
}

void SBlutilityShelf::GetFavoriteStatus(const TArray<FAssetData>& AssetList, /*out*/ bool& bAreAnySelected, /*out*/ bool& bAreAllSelected)
{
	TArray<FName> AssetPaths;
	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	CollectionManagerModule.Get().GetAssetsInCollection(BlutilityModule::BlutilityShelfCollectionName, ECollectionShareType::CST_Local, /*out*/ AssetPaths);

	bAreAnySelected = false;
	bAreAllSelected = false;

	if (AssetList.Num() > 0)
	{
		bAreAllSelected = true;
		for (auto AssetIt = AssetList.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;
			if (AssetPaths.Contains(AssetData.ObjectPath))
			{
				bAreAnySelected = true;
			}
			else
			{
				bAreAllSelected = false;
			}
		}
	}
}

void SBlutilityShelf::ToggleShelfMode()
{
	bInFavoritesMode = !bInFavoritesMode;
	BuildShelf();
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
