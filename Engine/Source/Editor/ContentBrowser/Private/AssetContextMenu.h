// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "AssetData.h"
#include "ContentBrowserDelegates.h"
#include "SourcesData.h"

class FMenuBuilder;
class FUICommandList;
class SAssetView;
class SWindow;

class FAssetContextMenu : public TSharedFromThis<FAssetContextMenu>
{
public:
	/** Constructor */
	FAssetContextMenu(const TWeakPtr<SAssetView>& InAssetView);

	/** Binds the commands used by the asset view context menu to the content browser command list */
	void BindCommands(TSharedPtr< FUICommandList >& Commands);

	/** Makes the context menu widget */
	TSharedRef<SWidget> MakeContextMenu(const TArray<FAssetData>& SelectedAssets, const FSourcesData& InSourcesData, TSharedPtr< FUICommandList > InCommandList);

	/** Updates the list of currently selected assets to those passed in */
	void SetSelectedAssets(const TArray<FAssetData>& InSelectedAssets);

	/** Delegate for when the context menu requests a rename */
	void SetOnFindInAssetTreeRequested(const FOnFindInAssetTreeRequested& InOnFindInAssetTreeRequested);

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE_OneParam(FOnRenameRequested, const FAssetData& /*AssetToRename*/);
	void SetOnRenameRequested(const FOnRenameRequested& InOnRenameRequested);

	/** Delegate for when the context menu requests a rename of a folder */
	DECLARE_DELEGATE_OneParam(FOnRenameFolderRequested, const FString& /*FolderToRename*/);
	void SetOnRenameFolderRequested(const FOnRenameFolderRequested& InOnRenameFolderRequested);

	/** Delegate for when the context menu requests a rename */
	DECLARE_DELEGATE_OneParam(FOnDuplicateRequested, const TWeakObjectPtr<UObject>& /*OriginalObject*/);
	void SetOnDuplicateRequested(const FOnDuplicateRequested& InOnDuplicateRequested);

	/** Delegate for when the context menu requests an asset view refresh */
	DECLARE_DELEGATE(FOnAssetViewRefreshRequested);
	void SetOnAssetViewRefreshRequested(const FOnAssetViewRefreshRequested& InOnAssetViewRefreshRequested);

	/** Handler to check to see if a rename command is allowed */
	bool CanExecuteRename() const;

	/** Handler for Rename */
	void ExecuteRename();

	/** Handler to check to see if a delete command is allowed */
	bool CanExecuteDelete() const;

	/** Handler for Delete */
	void ExecuteDelete();

	/** Handler to check to see if a reload command is allowed */
	bool CanExecuteReload() const;

	/** Handler for Reload */
	void ExecuteReload();

	/** Handler to check to see if "Save Asset" can be executed */
	bool CanExecuteSaveAsset() const;

	/** Handler for when "Save Asset" is selected */
	void ExecuteSaveAsset();

private:
	struct FSourceAssetsState
	{
		TSet<FName> SelectedAssets;
		TSet<FName> CurrentAssets;
	};

	struct FLocalizedAssetsState
	{
		FCulturePtr Culture;
		TSet<FName> NewAssets;
		TSet<FName> CurrentAssets;
	};

private:
	/** Helper to load selected assets and sort them by UClass */
	void GetSelectedAssetsByClass(TMap<UClass*, TArray<UObject*> >& OutSelectedAssetsByClass) const;

	/** Helper to collect resolved filepaths for all selected assets */
	void GetSelectedAssetSourceFilePaths(TArray<FString>& OutFilePaths) const;

	/** Handler to check to see if a imported asset actions should be visible in the menu */
	bool AreImportedAssetActionsVisible() const;

	/** Handler to check to see if imported asset actions are allowed */
	bool CanExecuteImportedAssetActions(const TArray<FString> ResolvedFilePaths) const;

	/** Handler for Reimport */
	void ExecuteReimport();

	/** Handler for FindInExplorer */
	void ExecuteFindSourceInExplorer(const TArray<FString> ResolvedFilePaths);

	/** Handler for OpenInExternalEditor */
	void ExecuteOpenInExternalEditor(const TArray<FString> ResolvedFilePaths);

	/** Handler to check to see if a duplicate command is allowed */
	bool CanExecuteDuplicate() const;

	/** Handler for Duplicate */
	void ExecuteDuplicate();

private:
	/** Adds asset type-specific menu options to a menu builder. Returns true if any options were added. */
	bool AddAssetTypeMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds asset type-specific menu options to a menu builder. Returns true if any options were added. */
	bool AddImportedAssetMenuOptions(FMenuBuilder& MenuBuilder);
	
	/** Adds common menu options to a menu builder. Returns true if any options were added. */
	bool AddCommonMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds explore menu options to a menu builder. */
	void AddExploreMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds Asset Actions sub-menu to a menu builder. */
	void MakeAssetActionsSubMenu(FMenuBuilder& MenuBuilder);

	/** Handler to check to see if "Asset Actions" are allowed */
	bool CanExecuteAssetActions() const;

	/** Adds Asset Localization sub-menu to a menu builder. */
	void MakeAssetLocalizationSubMenu(FMenuBuilder& MenuBuilder);

	/** Adds the Create Localized Asset sub-menu to a menu builder. */
	void MakeCreateLocalizedAssetSubMenu(FMenuBuilder& MenuBuilder, TSet<FName> InSelectedSourceAssets, TArray<FLocalizedAssetsState> InLocalizedAssetsState);

	/** Adds the Show Localized Assets sub-menu to a menu builder. */
	void MakeShowLocalizedAssetSubMenu(FMenuBuilder& MenuBuilder, TArray<FLocalizedAssetsState> InLocalizedAssetsState);

	/** Adds the Edit Localized Assets sub-menu to a menu builder. */
	void MakeEditLocalizedAssetSubMenu(FMenuBuilder& MenuBuilder, TArray<FLocalizedAssetsState> InLocalizedAssetsState);

	/** Create new localized assets for the given culture */
	void ExecuteCreateLocalizedAsset(TSet<FName> InSelectedSourceAssets, FLocalizedAssetsState InLocalizedAssetsStateForCulture);

	/** Find the given assets in the Content Browser */
	void ExecuteFindInAssetTree(TArray<FName> InAssets);

	/** Open the given assets in their respective editors */
	void ExecuteOpenEditorsForAssets(TArray<FName> InAssets);

	/** Adds asset reference menu options to a menu builder. Returns true if any options were added. */
	bool AddReferenceMenuOptions(FMenuBuilder& MenuBuilder);

	/** Adds asset documentation menu options to a menu builder. Returns true if any options were added. */
	bool AddDocumentationMenuOptions(FMenuBuilder& MenuBuilder);
	
	/** Adds source control menu options to a menu builder. */
	bool AddSourceControlMenuOptions(FMenuBuilder& MenuBuilder);

	/** Fills the source control sub-menu */
	void FillSourceControlSubMenu(FMenuBuilder& MenuBuilder);

	/** Handler to check to see if SCC actions are allowed */
	bool CanExecuteSourceControlActions() const;

	/** Adds menu options related to working with collections */
	bool AddCollectionMenuOptions(FMenuBuilder& MenuBuilder);

	/** Creates a sub-menu of Chunk IDs that are are assigned to all selected assets */
	void MakeChunkIDListMenu(FMenuBuilder& MenuBuilder);

	/** Handler for when sync to asset tree is selected */
	void ExecuteSyncToAssetTree();

	/** Handler for when find in explorer is selected */
	void ExecuteFindInExplorer();

	/** Handler for when create using asset is selected */
	void ExecuteCreateBlueprintUsing();

	/** Handler for when "Find in World" is selected */
	void ExecuteFindAssetInWorld();

	/** Handler for when "Property Matrix..." is selected */
	void ExecutePropertyMatrix();

	/** Handler for when "Edit Asset" is selected */
	void ExecuteEditAsset();

	/** Handler for when "Diff Selected" is selected */
	void ExecuteDiffSelected() const;

	/** Handler for confirmation of folder deletion */
	FReply ExecuteDeleteFolderConfirmed();

	/** Handler for Consolidate */
	void ExecuteConsolidate();

	/** Handler for Capture Thumbnail */
	void ExecuteCaptureThumbnail();

	/** Handler for Clear Thumbnail */
	void ExecuteClearThumbnail();

	/** Handler for when "Migrate Asset" is selected */
	void ExecuteMigrateAsset();

	/** Handler for ShowReferenceViewer */
	void ExecuteShowReferenceViewer();

	/** Handler for ShowSizeMap */
	void ExecuteShowSizeMap();

	/** Handler for GoToAssetCode */
	void ExecuteGoToCodeForAsset(UClass* SelectedClass);

	/** Handler for GoToAssetDocs */
	void ExecuteGoToDocsForAsset(UClass* SelectedClass);

	/** Handler for GoToAssetDocs */
	void ExecuteGoToDocsForAsset(UClass* SelectedClass, const FString ExcerptSection);

	/** Handler for CopyReference */
	void ExecuteCopyReference();

	/** Handler for Export */
	void ExecuteExport();

	/** Handler for Bulk Export */
	void ExecuteBulkExport();

	/** Handler for when "Remove from collection" is selected */
	void ExecuteRemoveFromCollection();

	/** Handler for when "Refresh source control" is selected */
	void ExecuteSCCRefresh();

	/** Handler for when "Merge" is selected */
	void ExecuteSCCMerge();

	/** Handler for when "Checkout from source control" is selected */
	void ExecuteSCCCheckOut();

	/** Handler for when "Open for add to source control" is selected */
	void ExecuteSCCOpenForAdd();

	/** Handler for when "Checkin to source control" is selected */
	void ExecuteSCCCheckIn();

	/** Handler for when "Source Control History" is selected */
	void ExecuteSCCHistory();

	/** Handler for when "Diff Against Depot" is selected */
	void ExecuteSCCDiffAgainstDepot() const;

	/** Handler for when "Source Control Revert" is selected */
	void ExecuteSCCRevert();

	/** Handler for when "Source Control Sync" is selected */
	void ExecuteSCCSync();

	/** Handler for when source control is disabled */
	void ExecuteEnableSourceControl();

	/** Handler to assign ChunkID to a selection of assets */
	void ExecuteAssignChunkID();

	/** Handler to remove all ChunkID assignments from a selection of assets */
	void ExecuteRemoveAllChunkID();

	/** Handler to remove a single ChunkID assignment from a selection of assets */
	void ExecuteRemoveChunkID(int32 ChunkID);

	/** Handler to check to see if a sync to asset tree command is allowed */
	bool CanExecuteSyncToAssetTree() const;

	/** Handler to check to see if a find in explorer command is allowed */
	bool CanExecuteFindInExplorer() const;	

	/** Handler to check if we can create blueprint using selected asset */
	bool CanExecuteCreateBlueprintUsing() const;

	/** Handler to check to see if a find in world command is allowed */
	bool CanExecuteFindAssetInWorld() const;

	/** Handler to check to see if a properties command is allowed */
	bool CanExecuteProperties() const;

	/** Handler to check to see if a property matrix command is allowed */
	bool CanExecutePropertyMatrix(FText& OutErrorMessage) const;
	bool CanExecutePropertyMatrix() const;

	/** Handler to check to see if a "Remove from collection" command is allowed */
	bool CanExecuteRemoveFromCollection() const;

	/** Handler to check to see if "Refresh source control" can be executed */
	bool CanExecuteSCCRefresh() const;

	/** Handler to check to see if "Merge" can be executed */
	bool CanExecuteSCCMerge() const;

	/** Handler to check to see if "Checkout from source control" can be executed */
	bool CanExecuteSCCCheckOut() const;

	/** Handler to check to see if "Open for add to source control" can be executed */
	bool CanExecuteSCCOpenForAdd() const;

	/** Handler to check to see if "Checkin to source control" can be executed */
	bool CanExecuteSCCCheckIn() const;

	/** Handler to check to see if "Source Control History" can be executed */
	bool CanExecuteSCCHistory() const;

	/** Handler to check to see if "Source Control Revert" can be executed */
	bool CanExecuteSCCRevert() const;

	/** Handler to check to see if "Source Control Sync" can be executed */
	bool CanExecuteSCCSync() const;

	/** Handler to check to see if "Diff Against Depot" can be executed */
	bool CanExecuteSCCDiffAgainstDepot() const;

	/** Handler to check to see if "Enable source control" can be executed */
	bool CanExecuteSCCEnable() const;

	/** Handler to check to see if "Consolidate" can be executed */
	bool CanExecuteConsolidate() const;

	/** Handler to check to see if "Diff Selected" can be executed */
	bool CanExecuteDiffSelected() const;

	/** Handler to check to see if "Capture Thumbnail" can be executed */
	bool CanExecuteCaptureThumbnail() const;

	/** Handler to check to see if "Clear Thumbnail" can be executed */
	bool CanExecuteClearThumbnail() const;

	/** Handler to check to see if "Clear Thumbnail" should be visible */
	bool CanClearCustomThumbnails() const;

	/** Initializes some variable used to in "CanExecute" checks that won't change at runtime or are too expensive to check every frame. */
	void CacheCanExecuteVars();

	/** Helper function to gather the package names of all selected assets */
	void GetSelectedPackageNames(TArray<FString>& OutPackageNames) const;

	/** Helper function to gather the packages containing all selected assets */
	void GetSelectedPackages(TArray<UPackage*>& OutPackages) const;

	/** Update interanl state logic */
	void OnChunkIDAssignChanged(int32 ChunkID);

	/** Gets the current value of the ChunkID entry box */
	TOptional<int32> GetChunkIDSelection() const;

	/** Handles when the Assign chunkID dialog OK button is clicked */
	FReply OnChunkIDAssignCommit(TSharedPtr<SWindow> Window);

	/** Handles when the Assign chunkID dialog Cancel button is clicked */
	FReply OnChunkIDAssignCancel(TSharedPtr<SWindow> Window);

	/** Generates tooltip for the Property Matrix menu option */
	FText GetExecutePropertyMatrixTooltip() const;

private:
	/** Generates a list of selected assets in the content browser */
	void GetSelectedAssets(TArray<UObject*>& Assets, bool SkipRedirectors) const;

	TArray<FAssetData> SelectedAssets;
	FSourcesData SourcesData;

	/** The asset view this context menu is a part of */
	TWeakPtr<SAssetView> AssetView;

	FOnFindInAssetTreeRequested OnFindInAssetTreeRequested;
	FOnRenameRequested OnRenameRequested;
	FOnRenameFolderRequested OnRenameFolderRequested;
	FOnDuplicateRequested OnDuplicateRequested;
	FOnAssetViewRefreshRequested OnAssetViewRefreshRequested;

	/** Cached CanExecute vars */
	bool bAtLeastOneNonRedirectorSelected;
	bool bAtLeastOneClassSelected;
	bool bCanExecuteSCCMerge;
	bool bCanExecuteSCCCheckOut;
	bool bCanExecuteSCCOpenForAdd;
	bool bCanExecuteSCCCheckIn;
	bool bCanExecuteSCCHistory;
	bool bCanExecuteSCCRevert;
	bool bCanExecuteSCCSync;
	/** */
	int32 ChunkIDSelected;
};
