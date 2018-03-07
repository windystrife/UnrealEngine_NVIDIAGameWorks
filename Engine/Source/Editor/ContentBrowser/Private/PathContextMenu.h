// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Editor/ContentBrowser/Private/NewAssetOrClassContextMenu.h"

class FExtender;
class FMenuBuilder;
class SWidget;
class SWindow;

class FPathContextMenu : public TSharedFromThis<FPathContextMenu>
{
public:
	/** Constructor */
	FPathContextMenu(const TWeakPtr<SWidget>& InParentContent);

	/** Sets the handler for when new assets are requested */
	void SetOnNewAssetRequested(const FNewAssetOrClassContextMenu::FOnNewAssetRequested& InOnNewAssetRequested);

	/** Sets the handler for when new classes are requested */
	void SetOnNewClassRequested(const FNewAssetOrClassContextMenu::FOnNewClassRequested& InOnNewClassRequested);

	/** Sets the handler for when importing an asset is requested */
	void SetOnImportAssetRequested( const FNewAssetOrClassContextMenu::FOnImportAssetRequested& InOnImportAssetRequested );

	/** Delegate for when the context menu requests a rename of a folder */
	DECLARE_DELEGATE_OneParam(FOnRenameFolderRequested, const FString& /*FolderToRename*/);
	void SetOnRenameFolderRequested(const FOnRenameFolderRequested& InOnRenameFolderRequested);

	/** Delegate for when the context menu has successfully deleted a folder */
	DECLARE_DELEGATE(FOnFolderDeleted)
	void SetOnFolderDeleted(const FOnFolderDeleted& InOnFolderDeleted);

	/** Sets the currently selected paths */
	void SetSelectedPaths(const TArray<FString>& InSelectedPaths);

	/** Makes the asset tree context menu extender */
	TSharedRef<FExtender> MakePathViewContextMenuExtender(const TArray<FString>& InSelectedPaths);

	/** Makes the asset tree context menu widget */
	void MakePathViewContextMenu(FMenuBuilder& MenuBuilder);

	/** Handler to check to see if creating a new asset is allowed */
	bool CanCreateAsset() const;

	/** Makes the new asset submenu */
	void MakeNewAssetSubMenu(FMenuBuilder& MenuBuilder);

	/** Handler for when "New Class" is selected */
	void ExecuteCreateClass();

	/** Handler to check to see if creating a new class is allowed */
	bool CanCreateClass() const;

	/** Makes the set color submenu */
	void MakeSetColorSubMenu(FMenuBuilder& MenuBuilder);

	/** Handler for when "Migrate Folder" is selected */
	void ExecuteMigrateFolder();

	/** Handler for when "Explore" is selected */
	void ExecuteExplore();

	/** Handler to check to see if a rename command is allowed */
	bool CanExecuteRename() const;

	/** Handler for Rename */
	void ExecuteRename();

	/** Handler to check to see if a delete command is allowed */
	bool CanExecuteDelete() const;

	/** Handler for Delete */
	void ExecuteDelete();

	/** Handler for when reset color is selected */
	void ExecuteResetColor();

	/** Handler for when new or set color is selected */
	void ExecutePickColor();

	/** Handler for when "Save" is selected */
	void ExecuteSaveFolder();

	/** Handler for when "Resave" is selected */
	void ExecuteResaveFolder();

	/** Handler for when "ReferenceViewer" is selected */
	void ExecuteReferenceViewer();

	/** Handler for when "SizeMap" is selected */
	void ExecuteSizeMap();

	/** Handler for when "Fix up Redirectors in Folder" is selected */
	void ExecuteFixUpRedirectorsInFolder();

	/** Handler for when "Delete" is selected and the delete was confirmed */
	FReply ExecuteDeleteFolderConfirmed();

	/** Handler for when "Checkout from source control" is selected */
	void ExecuteSCCCheckOut();

	/** Handler for when "Open for Add to source control" is selected */
	void ExecuteSCCOpenForAdd();

	/** Handler for when "Checkin to source control" is selected */
	void ExecuteSCCCheckIn();

	/** Handler for when "Sync from source control" is selected */
	void ExecuteSCCSync() const;

	/** Handler for when "Connect to source control" is selected */
	void ExecuteSCCConnect() const;

	/** Handler to check to see if "Checkout from source control" can be executed */
	bool CanExecuteSCCCheckOut() const;

	/** Handler to check to see if "Open for Add to source control" can be executed */
	bool CanExecuteSCCOpenForAdd() const;

	/** Handler to check to see if "Checkin to source control" can be executed */
	bool CanExecuteSCCCheckIn() const;

	/** Handler to check to see if "Sync" can be executed */
	bool CanExecuteSCCSync() const;

	/** Handler to check to see if "Connect to source control" can be executed */
	bool CanExecuteSCCConnect() const;	

private:
	/** Initializes some variable used to in "CanExecute" checks that won't change at runtime or are too expensive to check every frame. */
	void CacheCanExecuteVars();

	/** Returns a list of names of packages in all selected paths in the sources view */
	void GetPackageNamesInSelectedPaths(TArray<FString>& OutPackageNames) const;

	/** Gets the first selected path, if it exists */
	FString GetFirstSelectedPath() const;

	/** Checks to see if any of the selected paths use custom colors */
	bool SelectedHasCustomColors() const;

	/** Callback when the color picker dialog has been closed */
	void NewColorComplete(const TSharedRef<SWindow>& Window);

	/** Callback when the color is picked from the set color submenu */
	FReply OnColorClicked( const FLinearColor InColor );

	/** Resets the colors of the selected paths */
	void ResetColors();

private:
	TArray<FString> SelectedPaths;
	TWeakPtr<SWidget> ParentContent;
	FNewAssetOrClassContextMenu::FOnNewAssetRequested OnNewAssetRequested;
	FNewAssetOrClassContextMenu::FOnNewClassRequested OnNewClassRequested;
	FNewAssetOrClassContextMenu::FOnImportAssetRequested OnImportAssetRequested;
	FOnRenameFolderRequested OnRenameFolderRequested;
	FOnFolderDeleted OnFolderDeleted;

	/** Cached SCC CanExecute vars */
	bool bCanExecuteSCCCheckOut;
	bool bCanExecuteSCCOpenForAdd;
	bool bCanExecuteSCCCheckIn;
};
