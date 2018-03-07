// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetContextMenu.h"
#include "Templates/SubclassOf.h"
#include "Styling/SlateTypes.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Engine/Blueprint.h"
#include "Misc/MessageDialog.h"
#include "HAL/FileManager.h"
#include "Misc/ScopedSlowTask.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UnrealClient.h"
#include "Materials/MaterialFunction.h"
#include "Materials/Material.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlModule.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Materials/MaterialInstanceConstant.h"
#include "FileHelpers.h"
#include "AssetRegistryModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "ContentBrowserUtils.h"
#include "SAssetView.h"
#include "ContentBrowserModule.h"

#include "ObjectTools.h"
#include "PackageTools.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "PropertyEditorModule.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "ConsolidateWindow.h"
#include "ReferenceViewer.h"
#include "ISizeMapModule.h"

#include "ReferencedAssetsUtils.h"
#include "Internationalization/PackageLocalizationUtil.h"

#include "SourceControlWindows.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "CollectionAssetManagement.h"
#include "ComponentAssetBroker.h"
#include "Widgets/Input/SNumericEntryBox.h"

#include "SourceCodeNavigation.h"
#include "IDocumentation.h"
#include "EditorClassUtils.h"

#include "Internationalization/Culture.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/LevelStreaming.h"
#include "ContentBrowserCommands.h"


#define LOCTEXT_NAMESPACE "ContentBrowser"

FAssetContextMenu::FAssetContextMenu(const TWeakPtr<SAssetView>& InAssetView)
	: AssetView(InAssetView)
	, bAtLeastOneNonRedirectorSelected(false)
	, bAtLeastOneClassSelected(false)
	, bCanExecuteSCCMerge(false)
	, bCanExecuteSCCCheckOut(false)
	, bCanExecuteSCCOpenForAdd(false)
	, bCanExecuteSCCCheckIn(false)
	, bCanExecuteSCCHistory(false)
	, bCanExecuteSCCRevert(false)
	, bCanExecuteSCCSync(false)
{
	
}

void FAssetContextMenu::BindCommands(TSharedPtr< FUICommandList >& Commands)
{
	Commands->MapAction(FGenericCommands::Get().Duplicate, FUIAction(
		FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteDuplicate),
		FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteDuplicate),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &FAssetContextMenu::CanExecuteDuplicate)
		));

	Commands->MapAction(FGlobalEditorCommonCommands::Get().FindInContentBrowser, FUIAction(
		FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteSyncToAssetTree),
		FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteSyncToAssetTree)
		));
}

TSharedRef<SWidget> FAssetContextMenu::MakeContextMenu(const TArray<FAssetData>& InSelectedAssets, const FSourcesData& InSourcesData, TSharedPtr< FUICommandList > InCommandList)
{
	SetSelectedAssets(InSelectedAssets);
	SourcesData = InSourcesData;

	// Cache any vars that are used in determining if you can execute any actions.
	// Useful for actions whose "CanExecute" will not change or is expensive to calculate.
	CacheCanExecuteVars();

	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender_SelectedAssets> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute(SelectedAssets));
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, InCommandList, MenuExtender);

	// Only add something if at least one asset is selected
	if ( SelectedAssets.Num() )
	{
		// Add any type-specific context menu options
		AddAssetTypeMenuOptions(MenuBuilder);

		// Add imported asset context menu options
		AddImportedAssetMenuOptions(MenuBuilder);

		// Add quick access to common commands.
		AddCommonMenuOptions(MenuBuilder);

		// Add quick access to view commands
		AddExploreMenuOptions(MenuBuilder);

		// Add reference options
		AddReferenceMenuOptions(MenuBuilder);

		// Add collection options
		AddCollectionMenuOptions(MenuBuilder);

		// Add documentation options
		AddDocumentationMenuOptions(MenuBuilder);

		// Add source control options
		AddSourceControlMenuOptions(MenuBuilder);
	}
	
	return MenuBuilder.MakeWidget();
}

void FAssetContextMenu::SetSelectedAssets(const TArray<FAssetData>& InSelectedAssets)
{
	SelectedAssets = InSelectedAssets;
}

void FAssetContextMenu::SetOnFindInAssetTreeRequested(const FOnFindInAssetTreeRequested& InOnFindInAssetTreeRequested)
{
	OnFindInAssetTreeRequested = InOnFindInAssetTreeRequested;
}

void FAssetContextMenu::SetOnRenameRequested(const FOnRenameRequested& InOnRenameRequested)
{
	OnRenameRequested = InOnRenameRequested;
}

void FAssetContextMenu::SetOnRenameFolderRequested(const FOnRenameFolderRequested& InOnRenameFolderRequested)
{
	OnRenameFolderRequested = InOnRenameFolderRequested;
}

void FAssetContextMenu::SetOnDuplicateRequested(const FOnDuplicateRequested& InOnDuplicateRequested)
{
	OnDuplicateRequested = InOnDuplicateRequested;
}

void FAssetContextMenu::SetOnAssetViewRefreshRequested(const FOnAssetViewRefreshRequested& InOnAssetViewRefreshRequested)
{
	OnAssetViewRefreshRequested = InOnAssetViewRefreshRequested;
}

bool FAssetContextMenu::AddImportedAssetMenuOptions(FMenuBuilder& MenuBuilder)
{
	if (AreImportedAssetActionsVisible())
	{
		TArray<FString> ResolvedFilePaths;
		GetSelectedAssetSourceFilePaths(ResolvedFilePaths);

		MenuBuilder.BeginSection("ImportedAssetActions", LOCTEXT("ImportedAssetActionsMenuHeading", "Imported Asset"));
		{
			// Reimport
			MenuBuilder.AddMenuEntry(
				LOCTEXT("Reimport", "Reimport"),
				LOCTEXT("ReimportTooltip", "Reimport the selected asset(s) from the source file on disk."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.ReimportAsset"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteReimport),
					FCanExecuteAction()
					)
				);

			// Show Source In Explorer
			MenuBuilder.AddMenuEntry(
				LOCTEXT("FindSourceFile", "Open Source Location"),
				LOCTEXT("FindSourceFileTooltip", "Opens the folder containing the source of the selected asset(s)."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.OpenSourceLocation"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteFindSourceInExplorer, ResolvedFilePaths),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteImportedAssetActions, ResolvedFilePaths)
					)
				);

			// Open In External Editor
			MenuBuilder.AddMenuEntry(
				LOCTEXT("OpenInExternalEditor", "Open In External Editor"),
				LOCTEXT("OpenInExternalEditorTooltip", "Open the selected asset(s) in the default external editor."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.OpenInExternalEditor"),
				FUIAction(
					FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteOpenInExternalEditor, ResolvedFilePaths),
					FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteImportedAssetActions, ResolvedFilePaths)
					)
				);
		}
		MenuBuilder.EndSection();

		return true;
	}
	

	return false;
}

bool FAssetContextMenu::AddCommonMenuOptions(FMenuBuilder& MenuBuilder)
{
	int32 NumAssetItems, NumClassItems;
	ContentBrowserUtils::CountItemTypes(SelectedAssets, NumAssetItems, NumClassItems);

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// Can any of the selected assets be localized?
	bool bAnyLocalizableAssetsSelected = false;
	for (const FAssetData& Asset : SelectedAssets)
	{
		TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Asset.GetClass()).Pin();
		if (AssetTypeActions.IsValid())
		{
			bAnyLocalizableAssetsSelected = AssetTypeActions->CanLocalize();
		}

		if (bAnyLocalizableAssetsSelected)
		{
			break;
		}
	}

	MenuBuilder.BeginSection("CommonAssetActions", LOCTEXT("CommonAssetActionsMenuHeading", "Common"));
	{
		// Edit
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EditAsset", "Edit..."),
			LOCTEXT("EditAssetTooltip", "Opens the selected asset(s) for edit."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Edit"),
			FUIAction( FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteEditAsset) )
			);
	
		// Only add these options if assets are selected
		if (NumAssetItems > 0)
		{
			// Rename
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None,
				LOCTEXT("Rename", "Rename"),
				LOCTEXT("RenameTooltip", "Rename the selected asset."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Rename")
				);

			// Duplicate
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Duplicate, NAME_None,
				LOCTEXT("Duplicate", "Duplicate"),
				LOCTEXT("DuplicateTooltip", "Create a copy of the selected asset(s)."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Duplicate")
				);

			// Save
			MenuBuilder.AddMenuEntry(FContentBrowserCommands::Get().SaveSelectedAsset, NAME_None,
				LOCTEXT("SaveAsset", "Save"),
				LOCTEXT("SaveAssetTooltip", "Saves the asset to file."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "Level.SaveIcon16x")
				);

			// Delete
			MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None,
				LOCTEXT("Delete", "Delete"),
				LOCTEXT("DeleteTooltip", "Delete the selected assets."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Delete")
				);

			// Asset Actions sub-menu
			MenuBuilder.AddSubMenu(
				LOCTEXT("AssetActionsSubMenuLabel", "Asset Actions"),
				LOCTEXT("AssetActionsSubMenuToolTip", "Other asset actions"),
				FNewMenuDelegate::CreateSP(this, &FAssetContextMenu::MakeAssetActionsSubMenu),
				FUIAction(
					FExecuteAction(),
					FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteAssetActions )
					),
				NAME_None,
				EUserInterfaceActionType::Button,
				false, 
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions")
				);

			if (bAnyLocalizableAssetsSelected && NumClassItems == 0)
			{
				// Asset Localization sub-menu
				MenuBuilder.AddSubMenu(
					LOCTEXT("LocalizationSubMenuLabel", "Asset Localization"),
					LOCTEXT("LocalizationSubMenuToolTip", "View or create localized variants of this asset"),
					FNewMenuDelegate::CreateSP(this, &FAssetContextMenu::MakeAssetLocalizationSubMenu),
					FUIAction(),
					NAME_None,
					EUserInterfaceActionType::Button,
					false,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetLocalization")
					);
			}
		}
	}
	MenuBuilder.EndSection();

	return true;
}

void FAssetContextMenu::AddExploreMenuOptions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("AssetContextExploreMenuOptions", LOCTEXT("AssetContextExploreMenuOptionsHeading", "Explore"));
	{
		// Find in Content Browser
		MenuBuilder.AddMenuEntry(
			FGlobalEditorCommonCommands::Get().FindInContentBrowser, 
			NAME_None, 
			LOCTEXT("ShowInFolderView", "Show in Folder View"),
			LOCTEXT("ShowInFolderViewTooltip", "Selects the folder that contains this asset in the Content Browser Sources Panel.")
			);

		// Find in Explorer
		MenuBuilder.AddMenuEntry(
			ContentBrowserUtils::GetExploreFolderText(),
			LOCTEXT("FindInExplorerTooltip", "Finds this asset on disk"),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteFindInExplorer ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteFindInExplorer )
				)
			);
	}
	MenuBuilder.EndSection();
}

void FAssetContextMenu::MakeAssetActionsSubMenu(FMenuBuilder& MenuBuilder)
{	
	// Create BP Using This
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CreateBlueprintUsing", "Create Blueprint Using This..."),
		LOCTEXT("CreateBlueprintUsingTooltip", "Create a new Blueprint and add this asset to it"),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.CreateClassBlueprint"),
		FUIAction(
		FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteCreateBlueprintUsing),
		FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteCreateBlueprintUsing)
			)
		);

	// Capture Thumbnail
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	if (SelectedAssets.Num() == 1 && AssetToolsModule.Get().AssetUsesGenericThumbnail(SelectedAssets[0]))
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CaptureThumbnail", "Capture Thumbnail"),
			LOCTEXT("CaptureThumbnailTooltip", "Captures a thumbnail from the active viewport."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.CreateThumbnail"),
			FUIAction(
			FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteCaptureThumbnail),
			FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteCaptureThumbnail)
				)
			);
	}

	// Clear Thumbnail
	if (CanClearCustomThumbnails())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ClearCustomThumbnail", "Clear Thumbnail"),
			LOCTEXT("ClearCustomThumbnailTooltip", "Clears all custom thumbnails for selected assets."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.DeleteThumbnail"),
			FUIAction( FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteClearThumbnail) ) 
			);
	}

	// FIND ACTIONS
	MenuBuilder.BeginSection("AssetContextFindActions", LOCTEXT("AssetContextFindActionsMenuHeading", "Find"));
	{
		// Select Actors Using This Asset
		MenuBuilder.AddMenuEntry(
			LOCTEXT("FindAssetInWorld", "Select Actors Using This Asset"),
			LOCTEXT("FindAssetInWorldTooltip", "Selects all actors referencing this asset."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteFindAssetInWorld),
				FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteFindAssetInWorld)
				)
			);
	}
	MenuBuilder.EndSection();

	// MOVE ACTIONS
	MenuBuilder.BeginSection("AssetContextMoveActions", LOCTEXT("AssetContextMoveActionsMenuHeading", "Move"));
	{
		// Export
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Export", "Export..."),
			LOCTEXT("ExportTooltip", "Export the selected assets to file."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteExport ) )
			);

		// Bulk Export
		if (SelectedAssets.Num() > 1)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("BulkExport", "Bulk Export..."),
				LOCTEXT("BulkExportTooltip", "Export the selected assets to file in the selected directory"),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteBulkExport ) )
				);
		}

		// Migrate
		MenuBuilder.AddMenuEntry(
			LOCTEXT("MigrateAsset", "Migrate..."),
			LOCTEXT("MigrateAssetTooltip", "Copies all selected assets and their dependencies to another project"),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteMigrateAsset ) )
			);
	}
	MenuBuilder.EndSection();

	// ADVANCED ACTIONS
	MenuBuilder.BeginSection("AssetContextAdvancedActions", LOCTEXT("AssetContextAdvancedActionsMenuHeading", "Advanced"));
	{
		// Reload
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Reload", "Reload"),
			LOCTEXT("ReloadTooltip", "Reload the selected assets from their file on disk."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteReload),
				FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteReload)
			)
		);

		// Replace References
		if (CanExecuteConsolidate())
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ReplaceReferences", "Replace References"),
				LOCTEXT("ConsolidateTooltip", "Replace references to the selected assets."),
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteConsolidate)
				)
				);
		}

		// Property Matrix
		bool bCanUsePropertyMatrix = true;
		// Materials can't be bulk edited currently as they require very special handling because of their dependencies with the rendering thread, and we'd have to hack the property matrix too much.
		for (auto& Asset : SelectedAssets)
		{
			if (Asset.AssetClass == UMaterial::StaticClass()->GetFName() || Asset.AssetClass == UMaterialInstanceConstant::StaticClass()->GetFName() || Asset.AssetClass == UMaterialFunction::StaticClass()->GetFName())
			{
				bCanUsePropertyMatrix = false;
				break;
			}
		}

		if (bCanUsePropertyMatrix)
		{
			TAttribute<FText>::FGetter DynamicTooltipGetter;
			DynamicTooltipGetter.BindSP(this, &FAssetContextMenu::GetExecutePropertyMatrixTooltip);
			TAttribute<FText> DynamicTooltipAttribute = TAttribute<FText>::Create(DynamicTooltipGetter);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("PropertyMatrix", "Bulk Edit via Property Matrix..."),
				DynamicTooltipAttribute,
				FSlateIcon(),
				FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecutePropertyMatrix),
				FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecutePropertyMatrix)
				)
				);
		}

		// Chunk actions
		if (GetDefault<UEditorExperimentalSettings>()->bContextMenuChunkAssignments)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AssignAssetChunk", "Assign to Chunk..."),
				LOCTEXT("AssignAssetChunkTooltip", "Assign this asset to a specific Chunk"),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteAssignChunkID) )
				);

			MenuBuilder.AddSubMenu(
				LOCTEXT("RemoveAssetFromChunk", "Remove from Chunk..."),
				LOCTEXT("RemoveAssetFromChunkTooltip", "Removed an asset from a Chunk it's assigned to."),
				FNewMenuDelegate::CreateRaw(this, &FAssetContextMenu::MakeChunkIDListMenu)
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("RemoveAllChunkAssignments", "Remove from all Chunks"),
				LOCTEXT("RemoveAllChunkAssignmentsTooltip", "Removed an asset from all Chunks it's assigned to."),
				FSlateIcon(),
				FUIAction( FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteRemoveAllChunkID) )
				);
		}
	}
	MenuBuilder.EndSection();
}

bool FAssetContextMenu::CanExecuteAssetActions() const
{
	return !bAtLeastOneClassSelected;
}

void FAssetContextMenu::MakeAssetLocalizationSubMenu(FMenuBuilder& MenuBuilder)
{
	TArray<FCultureRef> CurrentCultures;

	// Build up the list of cultures already used
	{
		TSet<FString> CulturePaths;

		bool bIncludeEngineCultures = false;
		bool bIncludeProjectCultures = false;

		for (const FAssetData& Asset : SelectedAssets)
		{
			const FString AssetPath = Asset.ObjectPath.ToString();

			if (ContentBrowserUtils::IsEngineFolder(AssetPath))
			{
				bIncludeEngineCultures = true;
			}
			else
			{
				bIncludeProjectCultures = true;
			}

			{
				FString AssetLocalizationRoot;
				if (FPackageLocalizationUtil::GetLocalizedRoot(AssetPath, FString(), AssetLocalizationRoot))
				{
					FString AssetLocalizationFileRoot;
					if (FPackageName::TryConvertLongPackageNameToFilename(AssetLocalizationRoot, AssetLocalizationFileRoot))
					{
						CulturePaths.Add(MoveTemp(AssetLocalizationFileRoot));
					}
				}
			}
		}

		if (bIncludeEngineCultures)
		{
			CulturePaths.Append(FPaths::GetEngineLocalizationPaths());
		}

		if (bIncludeProjectCultures)
		{
			CulturePaths.Append(FPaths::GetGameLocalizationPaths());
		}

		FInternationalization::Get().GetCulturesWithAvailableLocalization(CulturePaths.Array(), CurrentCultures, false);

		if (CurrentCultures.Num() == 0)
		{
			CurrentCultures.Add(FInternationalization::Get().GetCurrentCulture());
		}
	}

	// Sort by display name for the UI
	CurrentCultures.Sort([](const FCultureRef& FirstCulture, const FCultureRef& SecondCulture) -> bool
	{
		const FText FirstDisplayName = FText::FromString(FirstCulture->GetDisplayName());
		const FText SecondDisplayName = FText::FromString(SecondCulture->GetDisplayName());
		return FirstDisplayName.CompareTo(SecondDisplayName) < 0;
	});

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// Now build up the list of available localized or source assets based upon the current selection and current cultures
	FSourceAssetsState SourceAssetsState;
	TArray<FLocalizedAssetsState> LocalizedAssetsState;
	for (const FCultureRef& CurrentCulture : CurrentCultures)
	{
		FLocalizedAssetsState& LocalizedAssetsStateForCulture = LocalizedAssetsState[LocalizedAssetsState.AddDefaulted()];
		LocalizedAssetsStateForCulture.Culture = CurrentCulture;

		for (const FAssetData& Asset : SelectedAssets)
		{
			// Can this type of asset be localized?
			bool bCanLocalizeAsset = false;
			{
				TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(Asset.GetClass()).Pin();
				if (AssetTypeActions.IsValid())
				{
					bCanLocalizeAsset = AssetTypeActions->CanLocalize();
				}
			}

			if (!bCanLocalizeAsset)
			{
				continue;
			}

			const FString ObjectPath = Asset.ObjectPath.ToString();
			if (FPackageName::IsLocalizedPackage(ObjectPath))
			{
				// Get the source path for this asset
				FString SourceObjectPath;
				if (FPackageLocalizationUtil::ConvertLocalizedToSource(ObjectPath, SourceObjectPath))
				{
					SourceAssetsState.CurrentAssets.Add(*SourceObjectPath);
				}
			}
			else
			{
				SourceAssetsState.SelectedAssets.Add(Asset.ObjectPath);

				// Get the localized path for this asset and culture
				FString LocalizedObjectPath;
				if (FPackageLocalizationUtil::ConvertSourceToLocalized(ObjectPath, CurrentCulture->GetName(), LocalizedObjectPath))
				{
					// Does this localized asset already exist?
					FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
					FAssetData LocalizedAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*LocalizedObjectPath);

					if (LocalizedAssetData.IsValid())
					{
						LocalizedAssetsStateForCulture.CurrentAssets.Add(*LocalizedObjectPath);
					}
					else
					{
						LocalizedAssetsStateForCulture.NewAssets.Add(*LocalizedObjectPath);
					}
				}
			}
		}
	}

	// If we found source assets for localized assets, then we can show the Source Asset options
	if (SourceAssetsState.CurrentAssets.Num() > 0)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ManageSourceAssetHeading", "Manage Source Asset"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ShowSourceAsset", "Show Source Asset"),
				LOCTEXT("ShowSourceAssetTooltip", "Show the source asset in the Content Browser."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
				FUIAction(FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteFindInAssetTree, SourceAssetsState.CurrentAssets.Array()))
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditSourceAsset", "Edit Source Asset"),
				LOCTEXT("EditSourceAssetTooltip", "Edit the source asset."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Edit"),
				FUIAction(FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteOpenEditorsForAssets, SourceAssetsState.CurrentAssets.Array()))
				);
		}
		MenuBuilder.EndSection();
	}

	// If we currently have source assets selected, then we can show the Localized Asset options
	if (SourceAssetsState.SelectedAssets.Num() > 0)
	{
		MenuBuilder.BeginSection(NAME_None, LOCTEXT("ManageLocalizedAssetHeading", "Manage Localized Asset"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("CreateLocalizedAsset", "Create Localized Asset"),
				LOCTEXT("CreateLocalizedAssetTooltip", "Create a new localized asset."),
				FNewMenuDelegate::CreateSP(this, &FAssetContextMenu::MakeCreateLocalizedAssetSubMenu, SourceAssetsState.SelectedAssets, LocalizedAssetsState),
				FUIAction(),
				NAME_None,
				EUserInterfaceActionType::Button,
				false,
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Duplicate")
				);

			int32 NumLocalizedAssets = 0;
			for (const FLocalizedAssetsState& LocalizedAssetsStateForCulture : LocalizedAssetsState)
			{
				NumLocalizedAssets += LocalizedAssetsStateForCulture.CurrentAssets.Num();
			}

			if (NumLocalizedAssets > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("ShowLocalizedAsset", "Show Localized Asset"),
					LOCTEXT("ShowLocalizedAssetTooltip", "Show the localized asset in the Content Browser."),
					FNewMenuDelegate::CreateSP(this, &FAssetContextMenu::MakeShowLocalizedAssetSubMenu, LocalizedAssetsState),
					FUIAction(),
					NAME_None,
					EUserInterfaceActionType::Button,
					false,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser")
					);

				MenuBuilder.AddSubMenu(
					LOCTEXT("EditLocalizedAsset", "Edit Localized Asset"),
					LOCTEXT("EditLocalizedAssetTooltip", "Edit the localized asset."),
					FNewMenuDelegate::CreateSP(this, &FAssetContextMenu::MakeEditLocalizedAssetSubMenu, LocalizedAssetsState),
					FUIAction(),
					NAME_None,
					EUserInterfaceActionType::Button,
					false,
					FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Edit")
					);
			}
		}
		MenuBuilder.EndSection();
	}
}

void FAssetContextMenu::MakeCreateLocalizedAssetSubMenu(FMenuBuilder& MenuBuilder, TSet<FName> InSelectedSourceAssets, TArray<FLocalizedAssetsState> InLocalizedAssetsState)
{
	for (const FLocalizedAssetsState& LocalizedAssetsStateForCulture : InLocalizedAssetsState)
	{
		// If we have less localized assets than we have selected source assets, then we'll have some assets to create localized variants of
		if (LocalizedAssetsStateForCulture.CurrentAssets.Num() < InSelectedSourceAssets.Num())
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(LocalizedAssetsStateForCulture.Culture->GetDisplayName()),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteCreateLocalizedAsset, InSelectedSourceAssets, LocalizedAssetsStateForCulture))
				);
		}
	}
}

void FAssetContextMenu::MakeShowLocalizedAssetSubMenu(FMenuBuilder& MenuBuilder, TArray<FLocalizedAssetsState> InLocalizedAssetsState)
{
	for (const FLocalizedAssetsState& LocalizedAssetsStateForCulture : InLocalizedAssetsState)
	{
		if (LocalizedAssetsStateForCulture.CurrentAssets.Num() > 0)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(LocalizedAssetsStateForCulture.Culture->GetDisplayName()),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteFindInAssetTree, LocalizedAssetsStateForCulture.CurrentAssets.Array()))
				);
		}
	}
}

void FAssetContextMenu::MakeEditLocalizedAssetSubMenu(FMenuBuilder& MenuBuilder, TArray<FLocalizedAssetsState> InLocalizedAssetsState)
{
	for (const FLocalizedAssetsState& LocalizedAssetsStateForCulture : InLocalizedAssetsState)
	{
		if (LocalizedAssetsStateForCulture.CurrentAssets.Num() > 0)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromString(LocalizedAssetsStateForCulture.Culture->GetDisplayName()),
				FText::GetEmpty(),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteOpenEditorsForAssets, LocalizedAssetsStateForCulture.CurrentAssets.Array()))
				);
		}
	}
}

void FAssetContextMenu::ExecuteCreateLocalizedAsset(TSet<FName> InSelectedSourceAssets, FLocalizedAssetsState InLocalizedAssetsStateForCulture)
{
	TArray<UPackage*> PackagesToSave;
	TArray<FAssetData> NewObjects;

	for (const FName SourceAssetName : InSelectedSourceAssets)
	{
		if (InLocalizedAssetsStateForCulture.CurrentAssets.Contains(SourceAssetName))
		{
			// Asset is already localized
			continue;
		}

		UObject* SourceAssetObject = LoadObject<UObject>(nullptr, *SourceAssetName.ToString());
		if (!SourceAssetObject)
		{
			// Source object cannot be loaded
			continue;
		}

		FString LocalizedPackageName;
		if (!FPackageLocalizationUtil::ConvertSourceToLocalized(SourceAssetObject->GetOutermost()->GetPathName(), InLocalizedAssetsStateForCulture.Culture->GetName(), LocalizedPackageName))
		{
			continue;
		}

		ObjectTools::FPackageGroupName NewAssetName;
		NewAssetName.PackageName = LocalizedPackageName;
		NewAssetName.ObjectName = SourceAssetObject->GetName();

		TSet<UPackage*> PackagesNotDuplicated;
		UObject* NewObject = ObjectTools::DuplicateSingleObject(SourceAssetObject, NewAssetName, PackagesNotDuplicated);
		if (NewObject)
		{
			PackagesToSave.Add(NewObject->GetOutermost());
			NewObjects.Add(FAssetData(NewObject));
		}
	}

	if (PackagesToSave.Num() > 0)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty*/false, /*bPromptToSave*/false);
	}

	OnFindInAssetTreeRequested.ExecuteIfBound(NewObjects);
}

void FAssetContextMenu::ExecuteFindInAssetTree(TArray<FName> InAssets)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	FARFilter ARFilter;
	ARFilter.ObjectPaths = MoveTemp(InAssets);
	
	TArray<FAssetData> FoundLocalizedAssetData;
	AssetRegistryModule.Get().GetAssets(ARFilter, FoundLocalizedAssetData);

	OnFindInAssetTreeRequested.ExecuteIfBound(FoundLocalizedAssetData);
}

void FAssetContextMenu::ExecuteOpenEditorsForAssets(TArray<FName> InAssets)
{
	FAssetEditorManager::Get().OpenEditorsForAssets(InAssets);
}

bool FAssetContextMenu::AddReferenceMenuOptions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("AssetContextReferences", LOCTEXT("ReferencesMenuHeading", "References"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("CopyReference", "Copy Reference"),
			LOCTEXT("CopyReferenceTooltip", "Copies reference paths for the selected assets to the clipboard."),
			FSlateIcon(),
			FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteCopyReference ) )
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ReferenceViewer", "Reference Viewer..."),
			LOCTEXT("ReferenceViewerTooltip", "Shows a graph of references for this asset."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteShowReferenceViewer )
				)
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SizeMap", "Size Map..."),
			LOCTEXT("SizeMapTooltip", "Shows an interactive map of the approximate memory used by this asset and everything it references."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteShowSizeMap )
				)
			);
	}
	MenuBuilder.EndSection();

	return true;
}

bool FAssetContextMenu::AddDocumentationMenuOptions(FMenuBuilder& MenuBuilder)
{
	bool bAddedOption = false;

	// Objects must be loaded for this operation... for now
	UClass* SelectedClass = (SelectedAssets.Num() > 0 ? SelectedAssets[0].GetClass() : nullptr);
	for (const FAssetData& AssetData : SelectedAssets)
	{
		if (SelectedClass != AssetData.GetClass())
		{
			SelectedClass = nullptr;
			break;
		}
	}

	// Go to C++ Code
	if( SelectedClass != nullptr )
	{
		// Blueprints are special.  We won't link to C++ and for documentation we'll use the class it is generated from
		const bool bIsBlueprint = SelectedClass->IsChildOf<UBlueprint>();
		if (bIsBlueprint)
		{
			const FString ParentClassPath = SelectedAssets[0].GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UBlueprint,ParentClass));
			if (!ParentClassPath.IsEmpty())
			{
				SelectedClass = FindObject<UClass>(nullptr,*ParentClassPath);
			}
		}

		if ( !bIsBlueprint && FSourceCodeNavigation::IsCompilerAvailable() )
		{
			FString ClassHeaderPath;
			if( FSourceCodeNavigation::FindClassHeaderPath( SelectedClass, ClassHeaderPath ) && IFileManager::Get().FileSize( *ClassHeaderPath ) != INDEX_NONE )
			{
				bAddedOption = true;

				const FString CodeFileName = FPaths::GetCleanFilename( *ClassHeaderPath );

				MenuBuilder.BeginSection( "AssetCode"/*, LOCTEXT("AssetCodeHeading", "C++")*/ );
				{
					MenuBuilder.AddMenuEntry(
						FText::Format( LOCTEXT("GoToCodeForAsset", "Open {0}"), FText::FromString( CodeFileName ) ),
						FText::Format( LOCTEXT("GoToCodeForAsset_ToolTip", "Opens the header file for this asset ({0}) in a code editing program"), FText::FromString( CodeFileName ) ),
						FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.GoToCodeForAsset"),
						FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToCodeForAsset, SelectedClass ) )
						);
				}
				MenuBuilder.EndSection();
			}
		}

		const FString DocumentationLink = FEditorClassUtils::GetDocumentationLink(SelectedClass);
		if (bIsBlueprint || !DocumentationLink.IsEmpty())
		{
			bAddedOption = true;

			MenuBuilder.BeginSection( "AssetDocumentation"/*, LOCTEXT("AseetDocsHeading", "Documentation")*/ );
			{
					if (bIsBlueprint)
					{
						if (!DocumentationLink.IsEmpty())
						{
							MenuBuilder.AddMenuEntry(
								FText::Format( LOCTEXT("GoToDocsForAssetWithClass", "View Documentation - {0}"), SelectedClass->GetDisplayNameText() ),
								FText::Format( LOCTEXT("GoToDocsForAssetWithClass_ToolTip", "Click to open documentation for {0}"), SelectedClass->GetDisplayNameText() ),
								FSlateIcon(FEditorStyle::GetStyleSetName(), "HelpIcon.Hovered" ),
								FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToDocsForAsset, SelectedClass ) )
								);
						}

						UEnum* BlueprintTypeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EBlueprintType"), true);
						const FString EnumString = SelectedAssets[0].GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UBlueprint,BlueprintType));
						EBlueprintType BlueprintType = (!EnumString.IsEmpty() ? (EBlueprintType)BlueprintTypeEnum->GetValueByName(*EnumString) : BPTYPE_Normal);

						switch (BlueprintType)
						{
						case BPTYPE_FunctionLibrary:
							MenuBuilder.AddMenuEntry(
								LOCTEXT("GoToDocsForMacroBlueprint", "View Documentation - Function Library"),
								LOCTEXT("GoToDocsForMacroBlueprint_ToolTip", "Click to open documentation on blueprint function libraries"),
								FSlateIcon(FEditorStyle::GetStyleSetName(), "HelpIcon.Hovered" ),
								FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToDocsForAsset, UBlueprint::StaticClass(), FString(TEXT("UBlueprint_FunctionLibrary")) ) )
								);
							break;
						case BPTYPE_Interface:
							MenuBuilder.AddMenuEntry(
								LOCTEXT("GoToDocsForInterfaceBlueprint", "View Documentation - Interface"),
								LOCTEXT("GoToDocsForInterfaceBlueprint_ToolTip", "Click to open documentation on blueprint interfaces"),
								FSlateIcon(FEditorStyle::GetStyleSetName(), "HelpIcon.Hovered" ),
								FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToDocsForAsset, UBlueprint::StaticClass(), FString(TEXT("UBlueprint_Interface")) ) )
								);
							break;
						case BPTYPE_MacroLibrary:
							MenuBuilder.AddMenuEntry(
								LOCTEXT("GoToDocsForMacroLibrary", "View Documentation - Macro"),
								LOCTEXT("GoToDocsForMacroLibrary_ToolTip", "Click to open documentation on blueprint macros"),
								FSlateIcon(FEditorStyle::GetStyleSetName(), "HelpIcon.Hovered" ),
								FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToDocsForAsset, UBlueprint::StaticClass(), FString(TEXT("UBlueprint_Macro")) ) )
								);
							break;
						default:
							MenuBuilder.AddMenuEntry(
								LOCTEXT("GoToDocsForBlueprint", "View Documentation - Blueprint"),
								LOCTEXT("GoToDocsForBlueprint_ToolTip", "Click to open documentation on blueprints"),
								FSlateIcon(FEditorStyle::GetStyleSetName(), "HelpIcon.Hovered" ),
								FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToDocsForAsset, UBlueprint::StaticClass(), FString(TEXT("UBlueprint")) ) )
								);
						}
					}
					else
					{
						MenuBuilder.AddMenuEntry(
							LOCTEXT("GoToDocsForAsset", "View Documentation"),
							LOCTEXT("GoToDocsForAsset_ToolTip", "Click to open documentation"),
							FSlateIcon(FEditorStyle::GetStyleSetName(), "HelpIcon.Hovered" ),
							FUIAction( FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteGoToDocsForAsset, SelectedClass ) )
							);
					}
			}
			MenuBuilder.EndSection();
		}
	}

	return bAddedOption;
}

bool FAssetContextMenu::AddAssetTypeMenuOptions(FMenuBuilder& MenuBuilder)
{
	bool bAnyTypeOptions = false;

	// Objects must be loaded for this operation... for now
	TArray<FString> ObjectPaths;
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		ObjectPaths.Add(SelectedAssets[AssetIdx].ObjectPath.ToString());
	}

	TArray<UObject*> SelectedObjects;
	if ( ContentBrowserUtils::LoadAssetsIfNeeded(ObjectPaths, SelectedObjects) )
	{
		// Load the asset tools module
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		bAnyTypeOptions = AssetToolsModule.Get().GetAssetActions(SelectedObjects, MenuBuilder, /*bIncludeHeading=*/true);
	}

	return bAnyTypeOptions;
}

bool FAssetContextMenu::AddSourceControlMenuOptions(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuSeparator();
	
	if ( ISourceControlModule::Get().IsEnabled() )
	{
		// SCC sub menu
		MenuBuilder.AddSubMenu(
			LOCTEXT("SourceControlSubMenuLabel", "Source Control"),
			LOCTEXT("SourceControlSubMenuToolTip", "Source control actions."),
			FNewMenuDelegate::CreateSP(this, &FAssetContextMenu::FillSourceControlSubMenu),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSourceControlActions )
				),
			NAME_None,
			EUserInterfaceActionType::Button,
			false,
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.StatusIcon.On")
			);
	}
	else
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCConnectToSourceControl", "Connect To Source Control..."),
			LOCTEXT("SCCConnectToSourceControlTooltip", "Connect to source control to allow source control operations to be performed on content and levels."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Connect"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteEnableSourceControl ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSourceControlActions )
				)
			);
	}

	// Diff selected
	if (CanExecuteDiffSelected())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("DiffSelected", "Diff Selected"),
			LOCTEXT("DiffSelectedTooltip", "Diff the two assets that you have selected."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Diff"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteDiffSelected)
			)
		);
	}

	return true;
}

void FAssetContextMenu::FillSourceControlSubMenu(FMenuBuilder& MenuBuilder)
{
	if( CanExecuteSCCMerge() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCMerge", "Merge"),
			LOCTEXT("SCCMergeTooltip", "Opens the blueprint editor with the merge tool open."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteSCCMerge),
				FCanExecuteAction::CreateSP(this, &FAssetContextMenu::CanExecuteSCCMerge)
			)
		);
	}

	if( CanExecuteSCCSync() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCSync", "Sync"),
			LOCTEXT("SCCSyncTooltip", "Updates the item to the latest version in source control."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Sync"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCSync ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCSync )
			)
		);
	}

	if ( CanExecuteSCCCheckOut() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCCheckOut", "Check Out"),
			LOCTEXT("SCCCheckOutTooltip", "Checks out the selected asset from source control."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.CheckOut"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCCheckOut ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCCheckOut )
			)
		);
	}

	if ( CanExecuteSCCOpenForAdd() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCOpenForAdd", "Mark For Add"),
			LOCTEXT("SCCOpenForAddTooltip", "Adds the selected asset to source control."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Add"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCOpenForAdd ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCOpenForAdd )
			)
		);
	}

	if ( CanExecuteSCCCheckIn() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCCheckIn", "Check In"),
			LOCTEXT("SCCCheckInTooltip", "Checks in the selected asset to source control."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Submit"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCCheckIn ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCCheckIn )
			)
		);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("SCCRefresh", "Refresh"),
		LOCTEXT("SCCRefreshTooltip", "Updates the source control status of the asset."),
		FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Refresh"),
		FUIAction(
			FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCRefresh ),
			FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCRefresh )
			)
		);

	if( CanExecuteSCCHistory() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCHistory", "History"),
			LOCTEXT("SCCHistoryTooltip", "Displays the source control revision history of the selected asset."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.History"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCHistory ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCHistory )
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCDiffAgainstDepot", "Diff Against Depot"),
			LOCTEXT("SCCDiffAgainstDepotTooltip", "Look at differences between your version of the asset and that in source control."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Diff"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCDiffAgainstDepot ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCDiffAgainstDepot )
			)
		);	
	}

	if( CanExecuteSCCRevert() )
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SCCRevert", "Revert"),
			LOCTEXT("SCCRevertTooltip", "Reverts the asset to the state it was before it was checked out."),
			FSlateIcon(FEditorStyle::GetStyleSetName(), "SourceControl.Actions.Revert"),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteSCCRevert ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteSCCRevert )
			)
		);
	}
}

bool FAssetContextMenu::CanExecuteSourceControlActions() const
{
	return !bAtLeastOneClassSelected;
}

bool FAssetContextMenu::AddCollectionMenuOptions(FMenuBuilder& MenuBuilder)
{
	class FManageCollectionsContextMenu
	{
	public:
		static void CreateManageCollectionsSubMenu(FMenuBuilder& SubMenuBuilder, TSharedRef<FCollectionAssetManagement> QuickAssetManagement)
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			TArray<FCollectionNameType> AvailableCollections;
			CollectionManagerModule.Get().GetRootCollections(AvailableCollections);

			CreateManageCollectionsSubMenu(SubMenuBuilder, QuickAssetManagement, MoveTemp(AvailableCollections));
		}

		static void CreateManageCollectionsSubMenu(FMenuBuilder& SubMenuBuilder, TSharedRef<FCollectionAssetManagement> QuickAssetManagement, TArray<FCollectionNameType> AvailableCollections)
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			AvailableCollections.Sort([](const FCollectionNameType& One, const FCollectionNameType& Two) -> bool
			{
				return One.Name < Two.Name;
			});

			for (const FCollectionNameType& AvailableCollection : AvailableCollections)
			{
				// Never display system collections
				if (AvailableCollection.Type == ECollectionShareType::CST_System)
				{
					continue;
				}

				// Can only manage assets for static collections
				ECollectionStorageMode::Type StorageMode = ECollectionStorageMode::Static;
				CollectionManagerModule.Get().GetCollectionStorageMode(AvailableCollection.Name, AvailableCollection.Type, StorageMode);
				if (StorageMode != ECollectionStorageMode::Static)
				{
					continue;
				}

				TArray<FCollectionNameType> AvailableChildCollections;
				CollectionManagerModule.Get().GetChildCollections(AvailableCollection.Name, AvailableCollection.Type, AvailableChildCollections);

				if (AvailableChildCollections.Num() > 0)
				{
					SubMenuBuilder.AddSubMenu(
						FText::FromName(AvailableCollection.Name), 
						FText::GetEmpty(), 
						FNewMenuDelegate::CreateStatic(&FManageCollectionsContextMenu::CreateManageCollectionsSubMenu, QuickAssetManagement, AvailableChildCollections),
						FUIAction(
							FExecuteAction::CreateStatic(&FManageCollectionsContextMenu::OnCollectionClicked, QuickAssetManagement, AvailableCollection),
							FCanExecuteAction::CreateStatic(&FManageCollectionsContextMenu::IsCollectionEnabled, QuickAssetManagement, AvailableCollection),
							FGetActionCheckState::CreateStatic(&FManageCollectionsContextMenu::GetCollectionCheckState, QuickAssetManagement, AvailableCollection)
							), 
						NAME_None, 
						EUserInterfaceActionType::ToggleButton,
						false,
						FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(AvailableCollection.Type))
						);
				}
				else
				{
					SubMenuBuilder.AddMenuEntry(
						FText::FromName(AvailableCollection.Name), 
						FText::GetEmpty(), 
						FSlateIcon(FEditorStyle::GetStyleSetName(), ECollectionShareType::GetIconStyleName(AvailableCollection.Type)), 
						FUIAction(
							FExecuteAction::CreateStatic(&FManageCollectionsContextMenu::OnCollectionClicked, QuickAssetManagement, AvailableCollection),
							FCanExecuteAction::CreateStatic(&FManageCollectionsContextMenu::IsCollectionEnabled, QuickAssetManagement, AvailableCollection),
							FGetActionCheckState::CreateStatic(&FManageCollectionsContextMenu::GetCollectionCheckState, QuickAssetManagement, AvailableCollection)
							), 
						NAME_None, 
						EUserInterfaceActionType::ToggleButton
						);
				}
			}
		}

	private:
		static bool IsCollectionEnabled(TSharedRef<FCollectionAssetManagement> QuickAssetManagement, FCollectionNameType InCollectionKey)
		{
			return QuickAssetManagement->IsCollectionEnabled(InCollectionKey);
		}

		static ECheckBoxState GetCollectionCheckState(TSharedRef<FCollectionAssetManagement> QuickAssetManagement, FCollectionNameType InCollectionKey)
		{
			return QuickAssetManagement->GetCollectionCheckState(InCollectionKey);
		}

		static void OnCollectionClicked(TSharedRef<FCollectionAssetManagement> QuickAssetManagement, FCollectionNameType InCollectionKey)
		{
			// The UI actions don't give you the new check state, so we need to emulate the behavior of SCheckBox
			// Basically, unchecked will transition to checked (adding items), and anything else will transition to unchecked (removing items)
			if (GetCollectionCheckState(QuickAssetManagement, InCollectionKey) == ECheckBoxState::Unchecked)
			{
				QuickAssetManagement->AddCurrentAssetsToCollection(InCollectionKey);
			}
			else
			{
				QuickAssetManagement->RemoveCurrentAssetsFromCollection(InCollectionKey);
			}
		}
	};

	bool bHasAddedItems = false;

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

	MenuBuilder.BeginSection("AssetContextCollections", LOCTEXT("AssetCollectionOptionsMenuHeading", "Collections"));

	// Show a sub-menu that allows you to quickly add or remove the current asset selection from the available collections
	if (CollectionManagerModule.Get().HasCollections())
	{
		TSharedRef<FCollectionAssetManagement> QuickAssetManagement = MakeShareable(new FCollectionAssetManagement());
		QuickAssetManagement->SetCurrentAssets(SelectedAssets);

		MenuBuilder.AddSubMenu(
			LOCTEXT("ManageCollections", "Manage Collections"),
			LOCTEXT("ManageCollections_ToolTip", "Manage the collections that the selected asset(s) belong to."),
			FNewMenuDelegate::CreateStatic(&FManageCollectionsContextMenu::CreateManageCollectionsSubMenu, QuickAssetManagement)
			);

		bHasAddedItems = true;
	}

	// "Remove from collection" (only display option if exactly one collection is selected)
	if ( SourcesData.Collections.Num() == 1 && !SourcesData.IsDynamicCollection() )
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("RemoveFromCollectionFmt", "Remove From {0}"), FText::FromName(SourcesData.Collections[0].Name)),
			LOCTEXT("RemoveFromCollection_ToolTip", "Removes the selected asset from the current collection."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &FAssetContextMenu::ExecuteRemoveFromCollection ),
				FCanExecuteAction::CreateSP( this, &FAssetContextMenu::CanExecuteRemoveFromCollection )
				)
			);

		bHasAddedItems = true;
	}

	MenuBuilder.EndSection();

	return bHasAddedItems;
}

bool FAssetContextMenu::AreImportedAssetActionsVisible() const
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	
	// Check that all of the selected assets are imported
	for (auto& SelectedAsset : SelectedAssets)
	{
		auto AssetClass = SelectedAsset.GetClass();
		if (AssetClass)
		{
			auto AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetClass).Pin();
			if (!AssetTypeActions.IsValid() || !AssetTypeActions->IsImportedAsset())
			{
				return false;
			}
		}
	}

	return true;
}

bool FAssetContextMenu::CanExecuteImportedAssetActions(const TArray<FString> ResolvedFilePaths) const
{
	// Verify that all the file paths are legitimate
	for (const auto& SourceFilePath : ResolvedFilePaths)
	{
		if (!SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE)
		{
			return false;
		}
	}

	return true;
}

void FAssetContextMenu::ExecuteReimport()
{
	// Reimport all selected assets
	TArray<UObject *> CopyOfSelectedAssets;
	for (const FAssetData &SelectedAsset : SelectedAssets)
		{
		UObject *Asset = SelectedAsset.GetAsset();
		CopyOfSelectedAssets.Add(Asset);
	}
	FReimportManager::Instance()->ValidateAllSourceFileAndReimport(CopyOfSelectedAssets);
}

void FAssetContextMenu::ExecuteFindSourceInExplorer(const TArray<FString> ResolvedFilePaths)
{
	// Open all files in the explorer
	for (const auto& SourceFilePath : ResolvedFilePaths)
	{
		FPlatformProcess::ExploreFolder(*FPaths::GetPath(SourceFilePath));
	}
}

void FAssetContextMenu::ExecuteOpenInExternalEditor(const TArray<FString> ResolvedFilePaths)
{
	// Open all files in their respective editor
	for (const auto& SourceFilePath : ResolvedFilePaths)
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*SourceFilePath, NULL, ELaunchVerb::Edit);
	}
}

void FAssetContextMenu::GetSelectedAssetsByClass(TMap<UClass*, TArray<UObject*> >& OutSelectedAssetsByClass) const
{
	// Sort all selected assets by class
	for (const auto& SelectedAsset : SelectedAssets)
	{
		auto Asset = SelectedAsset.GetAsset();
		auto AssetClass = Asset->GetClass();

		if ( !OutSelectedAssetsByClass.Contains(AssetClass) )
		{
			OutSelectedAssetsByClass.Add(AssetClass);
		}
		
		OutSelectedAssetsByClass[AssetClass].Add(Asset);
	}
}

void FAssetContextMenu::GetSelectedAssetSourceFilePaths(TArray<FString>& OutFilePaths) const
{
	OutFilePaths.Empty();
	
	TMap<UClass*, TArray<UObject*> > SelectedAssetsByClass;
	GetSelectedAssetsByClass(SelectedAssetsByClass);
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// Get the source file paths for the assets of each type
	for (const auto& AssetsByClassPair : SelectedAssetsByClass)
	{
		const auto AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetsByClassPair.Key);
		if (AssetTypeActions.IsValid())
		{
			const auto& TypeAssets = AssetsByClassPair.Value;
			TArray<FString> AssetSourcePaths;
			AssetTypeActions.Pin()->GetResolvedSourceFilePaths(TypeAssets, AssetSourcePaths);

			OutFilePaths.Append(AssetSourcePaths);
		}
	}
}

void FAssetContextMenu::ExecuteSyncToAssetTree()
{
	// Copy this as the sync may adjust our selected assets array
	const TArray<FAssetData> SelectedAssetsCopy = SelectedAssets;
	OnFindInAssetTreeRequested.ExecuteIfBound(SelectedAssetsCopy);
}

void FAssetContextMenu::ExecuteFindInExplorer()
{
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		const UObject* Asset = SelectedAssets[AssetIdx].GetAsset();
		if (Asset)
		{
			FAssetData AssetData(Asset);

			const FString PackageName = AssetData.PackageName.ToString();

			static const TCHAR* ScriptString = TEXT("/Script/");
			if (PackageName.StartsWith(ScriptString))
			{
				// Handle C++ classes specially, as FPackageName::LongPackageNameToFilename won't return the correct path in this case
				const FString ModuleName = PackageName.RightChop(FCString::Strlen(ScriptString));
				FString ModulePath;
				if (FSourceCodeNavigation::FindModulePath(ModuleName, ModulePath))
				{
					FString RelativePath;
					if (AssetData.GetTagValue("ModuleRelativePath", RelativePath))
					{
						const FString FullFilePath = FPaths::ConvertRelativePathToFull(ModulePath / (*RelativePath));
						FPlatformProcess::ExploreFolder(*FullFilePath);
					}
				}

				return;
			}

			const bool bIsWorldAsset = (AssetData.AssetClass == UWorld::StaticClass()->GetFName());
			const FString Extension = bIsWorldAsset ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
			const FString FilePath = FPackageName::LongPackageNameToFilename(PackageName, Extension);
			const FString FullFilePath = FPaths::ConvertRelativePathToFull(FilePath);
			FPlatformProcess::ExploreFolder(*FullFilePath);
		}
	}
}

void FAssetContextMenu::ExecuteCreateBlueprintUsing()
{
	if(SelectedAssets.Num() == 1)
	{
		UObject* Asset = SelectedAssets[0].GetAsset();
		FKismetEditorUtilities::CreateBlueprintUsingAsset(Asset, true);
	}
}

void FAssetContextMenu::GetSelectedAssets(TArray<UObject*>& Assets, bool SkipRedirectors) const
{
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		if (SkipRedirectors && (SelectedAssets[AssetIdx].AssetClass == UObjectRedirector::StaticClass()->GetFName()))
		{
			// Don't operate on Redirectors
			continue;
		}

		UObject* Object = SelectedAssets[AssetIdx].GetAsset();

		if (Object)
		{
			Assets.Add(Object);
		}
	}
}

/** Generates a reference graph of the world and can then find actors referencing specified objects */
struct WorldReferenceGenerator : public FFindReferencedAssets
{
	void BuildReferencingData()
	{
		MarkAllObjects();

		const int32 MaxRecursionDepth = 0;
		const bool bIncludeClasses = true;
		const bool bIncludeDefaults = false;
		const bool bReverseReferenceGraph = true;


		UWorld* World = GWorld;

		// Generate the reference graph for the world
		FReferencedAssets* WorldReferencer = new(Referencers)FReferencedAssets(World);
		FFindAssetsArchive(World, WorldReferencer->AssetList, &ReferenceGraph, MaxRecursionDepth, bIncludeClasses, bIncludeDefaults, bReverseReferenceGraph);

		// Also include all the streaming levels in the results
		for (int32 LevelIndex = 0; LevelIndex < World->StreamingLevels.Num(); ++LevelIndex)
		{
			ULevelStreaming* StreamingLevel = World->StreamingLevels[LevelIndex];
			if( StreamingLevel != NULL )
			{
				ULevel* Level = StreamingLevel->GetLoadedLevel();
				if( Level != NULL )
				{
					// Generate the reference graph for each streamed in level
					FReferencedAssets* LevelReferencer = new(Referencers) FReferencedAssets(Level);			
					FFindAssetsArchive(Level, LevelReferencer->AssetList, &ReferenceGraph, MaxRecursionDepth, bIncludeClasses, bIncludeDefaults, bReverseReferenceGraph);
				}
			}
		}
	}

	void MarkAllObjects()
	{
		// Mark all objects so we don't get into an endless recursion
		for (FObjectIterator It; It; ++It)
		{
			It->Mark(OBJECTMARK_TagExp);
		}
	}

	void Generate( const UObject* AssetToFind, TArray< TWeakObjectPtr<UObject> >& OutObjects )
	{
		// Don't examine visited objects
		if (!AssetToFind->HasAnyMarks(OBJECTMARK_TagExp))
		{
			return;
		}

		AssetToFind->UnMark(OBJECTMARK_TagExp);

		// Return once we find a parent object that is an actor
		if (AssetToFind->IsA(AActor::StaticClass()))
		{
			OutObjects.Add(AssetToFind);
			return;
		}

		// Transverse the reference graph looking for actor objects
		TSet<UObject*>* ReferencingObjects = ReferenceGraph.Find(AssetToFind);
		if (ReferencingObjects)
		{
			for(TSet<UObject*>::TConstIterator SetIt(*ReferencingObjects); SetIt; ++SetIt)
			{
				Generate(*SetIt, OutObjects);
			}
		}
	}
};

void FAssetContextMenu::ExecuteFindAssetInWorld()
{
	TArray<UObject*> AssetsToFind;
	const bool SkipRedirectors = true;
	GetSelectedAssets(AssetsToFind, SkipRedirectors);

	const bool NoteSelectionChange = true;
	const bool DeselectBSPSurfs = true;
	const bool WarnAboutManyActors = false;
	GEditor->SelectNone(NoteSelectionChange, DeselectBSPSurfs, WarnAboutManyActors);

	if (AssetsToFind.Num() > 0)
	{
		FScopedSlowTask SlowTask(2 + AssetsToFind.Num(), NSLOCTEXT("AssetContextMenu", "FindAssetInWorld", "Finding actors that use this asset..."));
		SlowTask.MakeDialog();

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		TArray< TWeakObjectPtr<UObject> > OutObjects;
		WorldReferenceGenerator ObjRefGenerator;

		SlowTask.EnterProgressFrame();
		ObjRefGenerator.BuildReferencingData();

		for (int32 AssetIdx = 0; AssetIdx < AssetsToFind.Num(); ++AssetIdx)
		{
			SlowTask.EnterProgressFrame();
			ObjRefGenerator.MarkAllObjects();
			ObjRefGenerator.Generate(AssetsToFind[AssetIdx], OutObjects);
		}

		SlowTask.EnterProgressFrame();

		if (OutObjects.Num() > 0)
		{
			const bool InSelected = true;
			const bool Notify = false;

			// Select referencing actors
			for (int32 ActorIdx = 0; ActorIdx < OutObjects.Num(); ++ActorIdx)
			{
				GEditor->SelectActor(CastChecked<AActor>(OutObjects[ActorIdx].Get()), InSelected, Notify);
			}

			GEditor->NoteSelectionChange();
		}
		else
		{
			FNotificationInfo Info(LOCTEXT("NoReferencingActorsFound", "No actors found."));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

void FAssetContextMenu::ExecutePropertyMatrix()
{
	TArray<UObject*> ObjectsForPropertiesMenu;
	const bool SkipRedirectors = true;
	GetSelectedAssets(ObjectsForPropertiesMenu, SkipRedirectors);

	if ( ObjectsForPropertiesMenu.Num() > 0 )
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
		PropertyEditorModule.CreatePropertyEditorToolkit( EToolkitMode::Standalone, TSharedPtr<IToolkitHost>(), ObjectsForPropertiesMenu );
	}
}

void FAssetContextMenu::ExecuteEditAsset()
{
	TMap<UClass*, TArray<UObject*> > SelectedAssetsByClass;
	GetSelectedAssetsByClass(SelectedAssetsByClass);

	// Open 
	for (const auto& AssetsByClassPair : SelectedAssetsByClass)
	{
		const auto& TypeAssets = AssetsByClassPair.Value;
		FAssetEditorManager::Get().OpenEditorForAssets(TypeAssets);
	}
}

void FAssetContextMenu::ExecuteSaveAsset()
{
	TArray<UPackage*> PackagesToSave;
	GetSelectedPackages(PackagesToSave);

	TArray< UPackage* > PackagesWithExternalRefs;
	FString PackageNames;
	if( PackageTools::CheckForReferencesToExternalPackages( &PackagesToSave, &PackagesWithExternalRefs ) )
	{
		for(int32 PkgIdx = 0; PkgIdx < PackagesWithExternalRefs.Num(); ++PkgIdx)
		{
			PackageNames += FString::Printf(TEXT("%s\n"), *PackagesWithExternalRefs[ PkgIdx ]->GetName());
		}
		bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, FText::Format( NSLOCTEXT("UnrealEd", "Warning_ExternalPackageRef", "The following assets have references to external assets: \n{0}\nExternal assets won't be found when in a game and all references will be broken.  Proceed?"), FText::FromString(PackageNames) ) );
		if(!bProceed)
		{
			return;
		}
	}

	const bool bCheckDirty = false;
	const bool bPromptToSave = false;
	const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);

	//return Return == FEditorFileUtils::EPromptReturnCode::PR_Success;
}

void FAssetContextMenu::ExecuteDiffSelected() const
{
	if (SelectedAssets.Num() >= 2)
	{
		UObject* FirstObjectSelected = SelectedAssets[0].GetAsset();
		UObject* SecondObjectSelected = SelectedAssets[1].GetAsset();

		if ((FirstObjectSelected != NULL) && (SecondObjectSelected != NULL))
		{
			// Load the asset registry module
			FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

			FRevisionInfo CurrentRevision; 
			CurrentRevision.Revision = TEXT("");

			AssetToolsModule.Get().DiffAssets(FirstObjectSelected, SecondObjectSelected, CurrentRevision, CurrentRevision);
		}
	}
}

void FAssetContextMenu::ExecuteDuplicate() 
{
	TArray<UObject*> ObjectsToDuplicate;
	const bool SkipRedirectors = true;
	GetSelectedAssets(ObjectsToDuplicate, SkipRedirectors);

	if ( ObjectsToDuplicate.Num() == 1 )
	{
		OnDuplicateRequested.ExecuteIfBound(ObjectsToDuplicate[0]);
	}
	else if ( ObjectsToDuplicate.Num() > 1 )
	{
		TArray<UObject*> NewObjects;
		ObjectTools::DuplicateObjects(ObjectsToDuplicate, TEXT(""), TEXT(""), /*bOpenDialog=*/false, &NewObjects);

		TArray<FAssetData> AssetsToSync;
		for ( auto ObjIt = NewObjects.CreateConstIterator(); ObjIt; ++ObjIt )
		{
			new(AssetsToSync) FAssetData(*ObjIt);
		}

		// Sync to asset tree
		if ( NewObjects.Num() > 0 )
		{
			OnFindInAssetTreeRequested.ExecuteIfBound(AssetsToSync);
		}
	}
}

void FAssetContextMenu::ExecuteRename()
{
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	TArray< FString > SelectedFolders = AssetView.Pin()->GetSelectedFolders();

	if ( AssetViewSelectedAssets.Num() == 1 && SelectedFolders.Num() == 0 )
	{
		// Don't operate on Redirectors
		if ( AssetViewSelectedAssets[0].AssetClass != UObjectRedirector::StaticClass()->GetFName() )
		{
			OnRenameRequested.ExecuteIfBound(AssetViewSelectedAssets[0]);
		}
	}

	if ( AssetViewSelectedAssets.Num() == 0 && SelectedFolders.Num() == 1 )
	{
		OnRenameFolderRequested.ExecuteIfBound(SelectedFolders[0]);
	}
}

void FAssetContextMenu::ExecuteDelete()
{
	// Don't allow asset deletion during PIE
	if (GIsEditor)
	{
		UEditorEngine* Editor = GEditor;
		FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		if (PIEWorldContext)
		{
			FNotificationInfo Notification(LOCTEXT("CannotDeleteAssetInPIE", "Assets cannot be deleted while in PIE."));
			Notification.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Notification);
			return;
		}
	}

	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	if(AssetViewSelectedAssets.Num() > 0)
	{
		TArray<FAssetData> AssetsToDelete;

		for( auto AssetIt = AssetViewSelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			const FAssetData& AssetData = *AssetIt;

			if( AssetData.AssetClass == UObjectRedirector::StaticClass()->GetFName() )
			{
				// Don't operate on Redirectors
				continue;
			}

			AssetsToDelete.Add( AssetData );
		}

		if ( AssetsToDelete.Num() > 0 )
		{
			ObjectTools::DeleteAssets( AssetsToDelete );
		}
	}

	TArray< FString > SelectedFolders = AssetView.Pin()->GetSelectedFolders();
	if(SelectedFolders.Num() > 0)
	{
		FText Prompt;
		if ( SelectedFolders.Num() == 1 )
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Single", "Delete folder '{0}'?"), FText::FromString(SelectedFolders[0]));
		}
		else
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Multiple", "Delete {0} folders?"), FText::AsNumber(SelectedFolders.Num()));
		}

		// Spawn a confirmation dialog since this is potentially a highly destructive operation
		ContentBrowserUtils::DisplayConfirmationPopup(
			Prompt,
			LOCTEXT("FolderDeleteConfirm_Yes", "Delete"),
			LOCTEXT("FolderDeleteConfirm_No", "Cancel"),
			AssetView.Pin().ToSharedRef(),
			FOnClicked::CreateSP( this, &FAssetContextMenu::ExecuteDeleteFolderConfirmed ));
	}
}

bool FAssetContextMenu::CanExecuteReload() const
{
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	TArray< FString > SelectedFolders = AssetView.Pin()->GetSelectedFolders();

	int32 NumAssetItems, NumClassItems;
	ContentBrowserUtils::CountItemTypes(AssetViewSelectedAssets, NumAssetItems, NumClassItems);

	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SelectedFolders, NumAssetPaths, NumClassPaths);

	bool bHasSelectedCollections = false;
	for (const FString& SelectedFolder : SelectedFolders)
	{
		if (ContentBrowserUtils::IsCollectionPath(SelectedFolder))
		{
			bHasSelectedCollections = true;
			break;
		}
	}

	// We can't reload classes, or folders containing classes, or any collection folders
	return ((NumAssetItems > 0 && NumClassItems == 0) || (NumAssetPaths > 0 && NumClassPaths == 0)) && !bHasSelectedCollections;
}

void FAssetContextMenu::ExecuteReload()
{
	// Don't allow asset reload during PIE
	if (GIsEditor)
	{
		UEditorEngine* Editor = GEditor;
		FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
		if (PIEWorldContext)
		{
			FNotificationInfo Notification(LOCTEXT("CannotReloadAssetInPIE", "Assets cannot be reloaded while in PIE."));
			Notification.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Notification);
			return;
		}
	}

	TArray<FAssetData> AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	if (AssetViewSelectedAssets.Num() > 0)
	{
		TArray<UPackage*> PackagesToReload;

		for (auto AssetIt = AssetViewSelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			const FAssetData& AssetData = *AssetIt;

			if (AssetData.AssetClass == UObjectRedirector::StaticClass()->GetFName())
			{
				// Don't operate on Redirectors
				continue;
			}

			PackagesToReload.AddUnique(AssetData.GetPackage());
		}

		if (PackagesToReload.Num() > 0)
		{
			PackageTools::ReloadPackages(PackagesToReload);
		}
	}
}

FReply FAssetContextMenu::ExecuteDeleteFolderConfirmed()
{
	TArray< FString > SelectedFolders = AssetView.Pin()->GetSelectedFolders();
	if(SelectedFolders.Num() > 0)
	{
		ContentBrowserUtils::DeleteFolders(SelectedFolders);
	}

	return FReply::Handled();
}

void FAssetContextMenu::ExecuteConsolidate()
{
	TArray<UObject*> ObjectsToConsolidate;
	const bool SkipRedirectors = true;
	GetSelectedAssets(ObjectsToConsolidate, SkipRedirectors);

	if ( ObjectsToConsolidate.Num() >  0 )
	{
		FConsolidateToolWindow::AddConsolidationObjects( ObjectsToConsolidate );
	}
}

void FAssetContextMenu::ExecuteCaptureThumbnail()
{
	FViewport* Viewport = GEditor->GetActiveViewport();

	if ( ensure(GCurrentLevelEditingViewportClient) && ensure(Viewport) )
	{
		//have to re-render the requested viewport
		FLevelEditorViewportClient* OldViewportClient = GCurrentLevelEditingViewportClient;
		//remove selection box around client during render
		GCurrentLevelEditingViewportClient = NULL;
		Viewport->Draw();

		ContentBrowserUtils::CaptureThumbnailFromViewport(Viewport, SelectedAssets);

		//redraw viewport to have the yellow highlight again
		GCurrentLevelEditingViewportClient = OldViewportClient;
		Viewport->Draw();
	}
}

void FAssetContextMenu::ExecuteClearThumbnail()
{
	ContentBrowserUtils::ClearCustomThumbnails(SelectedAssets);
}

void FAssetContextMenu::ExecuteMigrateAsset()
{
	// Get a list of package names for input into MigratePackages
	TArray<FName> PackageNames;
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		PackageNames.Add(SelectedAssets[AssetIdx].PackageName);
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().MigratePackages( PackageNames );
}

void FAssetContextMenu::ExecuteShowReferenceViewer()
{
	TArray<FName> PackageNames;
	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		PackageNames.Add(AssetIt->PackageName);
	}

	if ( PackageNames.Num() > 0 )
	{
		IReferenceViewerModule::Get().InvokeReferenceViewerTab(PackageNames);
	}
}

void FAssetContextMenu::ExecuteShowSizeMap()
{
	TArray<FName> PackageNames;
	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		PackageNames.Add(AssetIt->PackageName);
	}

	if ( PackageNames.Num() > 0 )
	{
		ISizeMapModule::Get().InvokeSizeMapTab(PackageNames);
	}
}

void FAssetContextMenu::ExecuteGoToCodeForAsset(UClass* SelectedClass)
{
	if (SelectedClass)
	{
		FString ClassHeaderPath;
		if( FSourceCodeNavigation::FindClassHeaderPath( SelectedClass, ClassHeaderPath ) && IFileManager::Get().FileSize( *ClassHeaderPath ) != INDEX_NONE )
		{
			const FString AbsoluteHeaderPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ClassHeaderPath);
			FSourceCodeNavigation::OpenSourceFile( AbsoluteHeaderPath );
		}
	}
}

void FAssetContextMenu::ExecuteGoToDocsForAsset(UClass* SelectedClass)
{
	ExecuteGoToDocsForAsset(SelectedClass, FString());
}

void FAssetContextMenu::ExecuteGoToDocsForAsset(UClass* SelectedClass, const FString ExcerptSection)
{
	if (SelectedClass)
	{
		FString DocumentationLink = FEditorClassUtils::GetDocumentationLink(SelectedClass, ExcerptSection);
		if (!DocumentationLink.IsEmpty())
		{
			IDocumentation::Get()->Open(DocumentationLink, FDocumentationSourceInfo(TEXT("cb_docs")));
		}
	}
}

void FAssetContextMenu::ExecuteCopyReference()
{
	ContentBrowserUtils::CopyAssetReferencesToClipboard(SelectedAssets);
}

void FAssetContextMenu::ExecuteExport()
{
	TArray<UObject*> ObjectsToExport;
	const bool SkipRedirectors = false;
	GetSelectedAssets(ObjectsToExport, SkipRedirectors);

	if ( ObjectsToExport.Num() > 0 )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

		AssetToolsModule.Get().ExportAssetsWithDialog(ObjectsToExport, true);
	}
}

void FAssetContextMenu::ExecuteBulkExport()
{
	TArray<UObject*> ObjectsToExport;
	const bool SkipRedirectors = false;
	GetSelectedAssets(ObjectsToExport, SkipRedirectors);

	if ( ObjectsToExport.Num() > 0 )
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

		AssetToolsModule.Get().ExportAssetsWithDialog(ObjectsToExport, false);
	}
}

void FAssetContextMenu::ExecuteRemoveFromCollection()
{
	if ( ensure(SourcesData.Collections.Num() == 1) )
	{
		TArray<FName> AssetsToRemove;
		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			AssetsToRemove.Add((*AssetIt).ObjectPath);
		}

		if ( AssetsToRemove.Num() > 0 )
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			const FCollectionNameType& Collection = SourcesData.Collections[0];
			CollectionManagerModule.Get().RemoveFromCollection(Collection.Name, Collection.Type, AssetsToRemove);
			OnAssetViewRefreshRequested.ExecuteIfBound();
		}
	}
}

void FAssetContextMenu::ExecuteSCCRefresh()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);

	ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), SourceControlHelpers::PackageFilenames(PackageNames), EConcurrency::Asynchronous);
}

void FAssetContextMenu::ExecuteSCCMerge()
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); AssetIdx++)
	{
		// Get the actual asset (will load it)
		const FAssetData& AssetData = SelectedAssets[AssetIdx];

		UObject* CurrentObject = AssetData.GetAsset();
		if (CurrentObject)
		{
			const FString PackagePath = AssetData.PackageName.ToString();
			const FString PackageName = AssetData.AssetName.ToString();
			auto AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( CurrentObject->GetClass() ).Pin();
			if( AssetTypeActions.IsValid() )
			{
				AssetTypeActions->Merge(CurrentObject);
			}
		}
	}
}

void FAssetContextMenu::ExecuteSCCCheckOut()
{
	TArray<UPackage*> PackagesToCheckOut;
	GetSelectedPackages(PackagesToCheckOut);

	if ( PackagesToCheckOut.Num() > 0 )
	{
		// Update the source control status of all potentially relevant packages
		if (ISourceControlModule::Get().GetProvider().Execute(ISourceControlOperation::Create<FUpdateStatus>(), PackagesToCheckOut) == ECommandResult::Succeeded)
		{
			// Now check them out
			FEditorFileUtils::CheckoutPackages(PackagesToCheckOut);
		}
	}
}

void FAssetContextMenu::ExecuteSCCOpenForAdd()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);

	ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

	TArray<FString> PackagesToAdd;
	TArray<UPackage*> PackagesToSave;
	for ( auto PackageIt = PackageNames.CreateConstIterator(); PackageIt; ++PackageIt )
	{
		FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(*PackageIt), EStateCacheUsage::Use);
		if ( SourceControlState.IsValid() && !SourceControlState->IsSourceControlled() )
		{
			PackagesToAdd.Add(*PackageIt);

			// Make sure the file actually exists on disk before adding it
			FString Filename;
			if ( !FPackageName::DoesPackageExist(*PackageIt, NULL, &Filename) )
			{
				UPackage* Package = FindPackage(NULL, **PackageIt);
				if ( Package )
				{
					PackagesToSave.Add(Package);
				}
			}
		}
	}

	if ( PackagesToAdd.Num() > 0 )
	{
		// If any of the packages are new, save them now
		if ( PackagesToSave.Num() > 0 )
		{
			const bool bCheckDirty = false;
			const bool bPromptToSave = false;
			TArray<UPackage*> FailedPackages;
			const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave, &FailedPackages);
			if(FailedPackages.Num() > 0)
			{
				// don't try and add files that failed to save - remove them from the list
				for(auto FailedPackageIt = FailedPackages.CreateConstIterator(); FailedPackageIt; FailedPackageIt++)
				{
					PackagesToAdd.Remove((*FailedPackageIt)->GetName());
				}
			}
		}

		SourceControlProvider.Execute(ISourceControlOperation::Create<FMarkForAdd>(), SourceControlHelpers::PackageFilenames(PackagesToAdd));
	}
}

void FAssetContextMenu::ExecuteSCCCheckIn()
{
	TArray<UPackage*> Packages;
	GetSelectedPackages(Packages);

	// Prompt the user to ask if they would like to first save any dirty packages they are trying to check-in
	const FEditorFileUtils::EPromptReturnCode UserResponse = FEditorFileUtils::PromptForCheckoutAndSave( Packages, true, true );

	// If the user elected to save dirty packages, but one or more of the packages failed to save properly OR if the user
	// canceled out of the prompt, don't follow through on the check-in process
	const bool bShouldProceed = ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Success || UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Declined );
	if ( bShouldProceed )
	{
		TArray<FString> PackageNames;
		GetSelectedPackageNames(PackageNames);

		const bool bUseSourceControlStateCache = true;
		FSourceControlWindows::PromptForCheckin(bUseSourceControlStateCache, PackageNames);
	}
	else
	{
		// If a failure occurred, alert the user that the check-in was aborted. This warning shouldn't be necessary if the user cancelled
		// from the dialog, because they obviously intended to cancel the whole operation.
		if ( UserResponse == FEditorFileUtils::EPromptReturnCode::PR_Failure )
		{
			FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "SCC_Checkin_Aborted", "Check-in aborted as a result of save failure.") );
		}
	}
}

void FAssetContextMenu::ExecuteSCCHistory()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);
	FSourceControlWindows::DisplayRevisionHistory(SourceControlHelpers::PackageFilenames(PackageNames));
}

void FAssetContextMenu::ExecuteSCCDiffAgainstDepot() const
{
	// Load the asset registry module
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	// Iterate over each selected asset
	for(int32 AssetIdx=0; AssetIdx<SelectedAssets.Num(); AssetIdx++)
	{
		// Get the actual asset (will load it)
		const FAssetData& AssetData = SelectedAssets[AssetIdx];

		UObject* CurrentObject = AssetData.GetAsset();
		if( CurrentObject )
		{
			const FString PackagePath = AssetData.PackageName.ToString();
			const FString PackageName = AssetData.AssetName.ToString();
			AssetToolsModule.Get().DiffAgainstDepot( CurrentObject, PackagePath, PackageName );
		}
	}
}

void FAssetContextMenu::ExecuteSCCRevert()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);
	FSourceControlWindows::PromptForRevert(PackageNames);
}

void FAssetContextMenu::ExecuteSCCSync()
{
	TArray<FString> PackageNames;
	GetSelectedPackageNames(PackageNames);
	ContentBrowserUtils::SyncPackagesFromSourceControl(PackageNames);
}

void FAssetContextMenu::ExecuteEnableSourceControl()
{
	ISourceControlModule::Get().ShowLoginDialog(FSourceControlLoginClosed(), ELoginWindowMode::Modeless);
}

bool FAssetContextMenu::CanExecuteSyncToAssetTree() const
{
	return SelectedAssets.Num() > 0;
}

bool FAssetContextMenu::CanExecuteFindInExplorer() const
{
	// selection must contain at least one asset that has already been saved to disk
	for (const FAssetData& Asset : SelectedAssets)
	{
		if ((Asset.PackageFlags & PKG_NewlyCreated) == 0)
		{
			return true;
		}
	}

	return false;
}

bool FAssetContextMenu::CanExecuteCreateBlueprintUsing() const
{
	// Only work if you have a single asset selected
	if(SelectedAssets.Num() == 1)
	{
		UObject* Asset = SelectedAssets[0].GetAsset();
		// See if we know how to make a component from this asset
		TArray< TSubclassOf<UActorComponent> > ComponentClassList = FComponentAssetBrokerage::GetComponentsForAsset(Asset);
		return (ComponentClassList.Num() > 0);
	}

	return false;
}

bool FAssetContextMenu::CanExecuteFindAssetInWorld() const
{
	return bAtLeastOneNonRedirectorSelected;
}

bool FAssetContextMenu::CanExecuteProperties() const
{
	return bAtLeastOneNonRedirectorSelected;
}

bool FAssetContextMenu::CanExecutePropertyMatrix(FText& OutErrorMessage) const
{
	bool bResult = bAtLeastOneNonRedirectorSelected;
	if (bAtLeastOneNonRedirectorSelected)
	{
		TArray<UObject*> ObjectsForPropertiesMenu;
		const bool SkipRedirectors = true;
		GetSelectedAssets(ObjectsForPropertiesMenu, SkipRedirectors);

		// Ensure all Blueprints are valid.
		for (UObject* Object : ObjectsForPropertiesMenu)
		{
			if (UBlueprint* BlueprintObj = Cast<UBlueprint>(Object))
			{
				if (BlueprintObj->GeneratedClass == nullptr)
				{
					OutErrorMessage = LOCTEXT("InvalidBlueprint", "A selected Blueprint is invalid.");
					bResult = false;
					break;
				}
			}
		}
	}
	return bResult;
}

bool FAssetContextMenu::CanExecutePropertyMatrix() const
{
	FText ErrorMessageDummy;
	return CanExecutePropertyMatrix(ErrorMessageDummy);
}

FText FAssetContextMenu::GetExecutePropertyMatrixTooltip() const
{
	FText ResultTooltip;
	if (CanExecutePropertyMatrix(ResultTooltip))
	{
		ResultTooltip = LOCTEXT("PropertyMatrixTooltip", "Opens the property matrix editor for the selected assets.");
	}
	return ResultTooltip;
}

bool FAssetContextMenu::CanExecuteDuplicate() const
{
	const TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	uint32 NumNonRedirectors = 0;

	for (const FAssetData& AssetData : AssetViewSelectedAssets)
	{
		if (!AssetData.IsValid())
		{
			continue;
		}

		if (AssetData.AssetClass == NAME_Class)
		{
			return false;
		}

		if (AssetData.AssetClass != UObjectRedirector::StaticClass()->GetFName())
		{
			++NumNonRedirectors;
		}
	}

	return (NumNonRedirectors > 0);
}

bool FAssetContextMenu::CanExecuteRename() const
{
	return ContentBrowserUtils::CanRenameFromAssetView(AssetView);
}

bool FAssetContextMenu::CanExecuteDelete() const
{
	return ContentBrowserUtils::CanDeleteFromAssetView(AssetView);
}

bool FAssetContextMenu::CanExecuteRemoveFromCollection() const 
{
	return SourcesData.Collections.Num() == 1 && !SourcesData.IsDynamicCollection();
}

bool FAssetContextMenu::CanExecuteSCCRefresh() const
{
	return ISourceControlModule::Get().IsEnabled();
}

bool FAssetContextMenu::CanExecuteSCCMerge() const
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

	bool bCanExecuteMerge = bCanExecuteSCCMerge;
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num() && bCanExecuteMerge; AssetIdx++)
	{
		// Get the actual asset (will load it)
		const FAssetData& AssetData = SelectedAssets[AssetIdx];
		UObject* CurrentObject = AssetData.GetAsset();
		if (CurrentObject)
		{
			auto AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(CurrentObject->GetClass()).Pin();
			if (AssetTypeActions.IsValid())
			{
				bCanExecuteMerge = AssetTypeActions->CanMerge();
			}
		}
		else
		{
			bCanExecuteMerge = false;
		}
	}

	return bCanExecuteMerge;
}

bool FAssetContextMenu::CanExecuteSCCCheckOut() const
{
	return bCanExecuteSCCCheckOut;
}

bool FAssetContextMenu::CanExecuteSCCOpenForAdd() const
{
	return bCanExecuteSCCOpenForAdd;
}

bool FAssetContextMenu::CanExecuteSCCCheckIn() const
{
	return bCanExecuteSCCCheckIn;
}

bool FAssetContextMenu::CanExecuteSCCHistory() const
{
	return bCanExecuteSCCHistory;
}

bool FAssetContextMenu::CanExecuteSCCDiffAgainstDepot() const
{
	return bCanExecuteSCCHistory;
}

bool FAssetContextMenu::CanExecuteSCCRevert() const
{
	return bCanExecuteSCCRevert;
}

bool FAssetContextMenu::CanExecuteSCCSync() const
{
	return bCanExecuteSCCSync;
}

bool FAssetContextMenu::CanExecuteConsolidate() const
{
	TArray<UObject*> ProposedObjects;
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		// Don't load assets here. Only operate on already loaded assets.
		if ( SelectedAssets[AssetIdx].IsAssetLoaded() )
		{
			UObject* Object = SelectedAssets[AssetIdx].GetAsset();

			if ( Object )
			{
				ProposedObjects.Add(Object);
			}
		}
	}
	
	if ( ProposedObjects.Num() > 0 )
	{
		TArray<UObject*> CompatibleObjects;
		return FConsolidateToolWindow::DetermineAssetCompatibility(ProposedObjects, CompatibleObjects);
	}

	return false;
}

bool FAssetContextMenu::CanExecuteSaveAsset() const
{
	if ( bAtLeastOneClassSelected )
	{
		return false;
	}

	TArray<UPackage*> Packages;
	GetSelectedPackages(Packages);

	// only enabled if at least one selected package is loaded at all
	for (int32 PackageIdx = 0; PackageIdx < Packages.Num(); ++PackageIdx)
	{
		if ( Packages[PackageIdx] != NULL )
		{
			return true;
		}
	}

	return false;
}

bool FAssetContextMenu::CanExecuteDiffSelected() const
{
	bool bCanDiffSelected = false;
	if (SelectedAssets.Num() == 2 && !bAtLeastOneClassSelected)
	{
		FAssetData const& FirstSelection = SelectedAssets[0];
		FAssetData const& SecondSelection = SelectedAssets[1];

		bCanDiffSelected = FirstSelection.AssetClass == SecondSelection.AssetClass;
	}

	return bCanDiffSelected;
}

bool FAssetContextMenu::CanExecuteCaptureThumbnail() const
{
	return GCurrentLevelEditingViewportClient != NULL;
}

bool FAssetContextMenu::CanClearCustomThumbnails() const
{
	for ( auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt )
	{
		if ( ContentBrowserUtils::AssetHasCustomThumbnail(*AssetIt) )
		{
			return true;
		}
	}

	return false;
}

void FAssetContextMenu::CacheCanExecuteVars()
{
	bAtLeastOneNonRedirectorSelected = false;
	bAtLeastOneClassSelected = false;
	bCanExecuteSCCMerge = false;
	bCanExecuteSCCCheckOut = false;
	bCanExecuteSCCOpenForAdd = false;
	bCanExecuteSCCCheckIn = false;
	bCanExecuteSCCHistory = false;
	bCanExecuteSCCRevert = false;
	bCanExecuteSCCSync = false;

	for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		const FAssetData& AssetData = *AssetIt;
		if ( !AssetData.IsValid() )
		{
			continue;
		}

		if ( !bAtLeastOneNonRedirectorSelected && AssetData.AssetClass != UObjectRedirector::StaticClass()->GetFName() )
		{
			bAtLeastOneNonRedirectorSelected = true;
		}

		bAtLeastOneClassSelected |= AssetData.AssetClass == NAME_Class;

		ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();
		if ( ISourceControlModule::Get().IsEnabled() )
		{
			// Check the SCC state for each package in the selected paths
			FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(SourceControlHelpers::PackageFilename(AssetData.PackageName.ToString()), EStateCacheUsage::Use);
			if(SourceControlState.IsValid())
			{
				if (SourceControlState->IsConflicted() )
				{
					bCanExecuteSCCMerge = true;
				}

				if ( SourceControlState->CanCheckout() )
				{
					bCanExecuteSCCCheckOut = true;
				}

				if ( !SourceControlState->IsSourceControlled() && SourceControlState->CanAdd() )
				{
					bCanExecuteSCCOpenForAdd = true;
				}
				else if( SourceControlState->IsSourceControlled() && !SourceControlState->IsAdded() )
				{
					bCanExecuteSCCHistory = true;
				}

				if(!SourceControlState->IsCurrent())
				{
					bCanExecuteSCCSync = true;
				}

				if ( SourceControlState->CanCheckIn() )
				{
					bCanExecuteSCCCheckIn = true;
					bCanExecuteSCCRevert = true;
				}
			}
		}

		if ( bAtLeastOneNonRedirectorSelected
			&& bAtLeastOneClassSelected
			&& bCanExecuteSCCMerge
			&& bCanExecuteSCCCheckOut
			&& bCanExecuteSCCOpenForAdd
			&& bCanExecuteSCCCheckIn
			&& bCanExecuteSCCHistory
			&& bCanExecuteSCCRevert
			&& bCanExecuteSCCSync
			)
		{
			// All options are available, no need to keep iterating
			break;
		}
	}
}

void FAssetContextMenu::GetSelectedPackageNames(TArray<FString>& OutPackageNames) const
{
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		OutPackageNames.Add(SelectedAssets[AssetIdx].PackageName.ToString());
	}
}

void FAssetContextMenu::GetSelectedPackages(TArray<UPackage*>& OutPackages) const
{
	for (int32 AssetIdx = 0; AssetIdx < SelectedAssets.Num(); ++AssetIdx)
	{
		UPackage* Package = FindPackage(NULL, *SelectedAssets[AssetIdx].PackageName.ToString());

		if ( Package )
		{
			OutPackages.Add(Package);
		}
	}
}

void FAssetContextMenu::MakeChunkIDListMenu(FMenuBuilder& MenuBuilder)
{
	TArray<int32> FoundChunks;
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	for (const auto& SelectedAsset : AssetViewSelectedAssets)
	{
		UPackage* Package = FindPackage(NULL, *SelectedAsset.PackageName.ToString());

		if (Package)
		{
			for (auto ChunkID : Package->GetChunkIDs())
			{
				FoundChunks.AddUnique(ChunkID);
			}
		}
	}

	for (auto ChunkID : FoundChunks)
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("PackageChunk", "Chunk {0}"), FText::AsNumber(ChunkID)),
			FText(),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetContextMenu::ExecuteRemoveChunkID, ChunkID)
			)
		);
	}
}

void FAssetContextMenu::ExecuteAssignChunkID()
{
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	auto AssetViewPtr = AssetView.Pin();
	if (AssetViewSelectedAssets.Num() > 0 && AssetViewPtr.IsValid())
	{
		// Determine the position of the window so that it will spawn near the mouse, but not go off the screen.
		const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();
		FSlateRect Anchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);

		FVector2D AdjustedSummonLocation = FSlateApplication::Get().CalculatePopupWindowPosition(Anchor, SColorPicker::DEFAULT_WINDOW_SIZE, true, FVector2D::ZeroVector, Orient_Horizontal);

		TSharedPtr<SWindow> Window = SNew(SWindow)
			.AutoCenter(EAutoCenter::None)
			.ScreenPosition(AdjustedSummonLocation)
			.SupportsMaximize(false)
			.SupportsMinimize(false)
			.SizingRule(ESizingRule::Autosized)
			.Title(LOCTEXT("WindowHeader", "Enter Chunk ID"));

		Window->SetContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MeshPaint_LabelStrength", "Chunk ID"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(2.0f)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SNumericEntryBox<int32>)
					.AllowSpin(true)
					.MinSliderValue(0)
					.MaxSliderValue(300)
					.MinValue(0)
					.MaxValue(300)
					.Value(this, &FAssetContextMenu::GetChunkIDSelection)
					.OnValueChanged(this, &FAssetContextMenu::OnChunkIDAssignChanged)
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("ChunkIDAssign_Yes", "OK"))
					.OnClicked(this, &FAssetContextMenu::OnChunkIDAssignCommit, Window)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.Text(LOCTEXT("ChunkIDAssign_No", "Cancel"))
					.OnClicked(this, &FAssetContextMenu::OnChunkIDAssignCancel, Window)
				]
			]
		);

		ChunkIDSelected = 0;
		FSlateApplication::Get().AddModalWindow(Window.ToSharedRef(), AssetViewPtr);
	}
}

void FAssetContextMenu::ExecuteRemoveAllChunkID()
{
	TArray<int32> EmptyChunks;
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	for (const auto& SelectedAsset : AssetViewSelectedAssets)
	{
		UPackage* Package = FindPackage(NULL, *SelectedAsset.PackageName.ToString());

		if (Package)
		{
			Package->SetChunkIDs(EmptyChunks);
			Package->SetDirtyFlag(true);
		}
	}
}

TOptional<int32> FAssetContextMenu::GetChunkIDSelection() const
{
	return ChunkIDSelected;
}

void FAssetContextMenu::OnChunkIDAssignChanged(int32 NewChunkID)
{
	ChunkIDSelected = NewChunkID;
}

FReply FAssetContextMenu::OnChunkIDAssignCommit(TSharedPtr<SWindow> Window)
{
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	for (const auto& SelectedAsset : AssetViewSelectedAssets)
	{
		UPackage* Package = FindPackage(NULL, *SelectedAsset.PackageName.ToString());

		if (Package)
		{
			TArray<int32> CurrentChunks = Package->GetChunkIDs();
			CurrentChunks.AddUnique(ChunkIDSelected);
			Package->SetChunkIDs(CurrentChunks);
			Package->SetDirtyFlag(true);
		}
	}

	Window->RequestDestroyWindow();

	return FReply::Handled();
}

FReply FAssetContextMenu::OnChunkIDAssignCancel(TSharedPtr<SWindow> Window)
{
	Window->RequestDestroyWindow();

	return FReply::Handled();
}

void FAssetContextMenu::ExecuteRemoveChunkID(int32 ChunkID)
{
	TArray< FAssetData > AssetViewSelectedAssets = AssetView.Pin()->GetSelectedAssets();
	for (const auto& SelectedAsset : AssetViewSelectedAssets)
	{
		UPackage* Package = FindPackage(NULL, *SelectedAsset.PackageName.ToString());

		if (Package)
		{
			int32 FoundIndex;
			TArray<int32> CurrentChunks = Package->GetChunkIDs();
			CurrentChunks.Find(ChunkID, FoundIndex);
			if (FoundIndex != INDEX_NONE)
			{
				CurrentChunks.RemoveAt(FoundIndex);
				Package->SetChunkIDs(CurrentChunks);
				Package->SetDirtyFlag(true);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
