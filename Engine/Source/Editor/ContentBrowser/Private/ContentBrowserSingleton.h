// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"
#include "IContentBrowserSingleton.h"

class FCollectionAssetRegistryBridge;
class FEmptyFolderVisibilityManager;
class FNativeClassHierarchy;
class FSpawnTabArgs;
class FTabManager;
class FViewport;
class SContentBrowser;
class UFactory;

#define MAX_CONTENT_BROWSERS 4

/**
 * Content browser module singleton implementation class
 */
class FContentBrowserSingleton : public IContentBrowserSingleton
{
public:
	/** Constructor, Destructor */
	FContentBrowserSingleton();
	virtual ~FContentBrowserSingleton();

	// IContentBrowserSingleton interface
	virtual TSharedRef<class SWidget> CreateContentBrowser( const FName InstanceName, TSharedPtr<SDockTab> ContainingTab, const FContentBrowserConfig* ContentBrowserConfig ) override;
	virtual TSharedRef<class SWidget> CreateAssetPicker(const FAssetPickerConfig& AssetPickerConfig) override;
	virtual TSharedRef<class SWidget> CreatePathPicker(const FPathPickerConfig& PathPickerConfig) override;
	virtual TSharedRef<class SWidget> CreateCollectionPicker(const FCollectionPickerConfig& CollectionPickerConfig) override;
	virtual void CreateOpenAssetDialog(const FOpenAssetDialogConfig& OpenAssetConfig, const FOnAssetsChosenForOpen& OnAssetsChosenForOpen, const FOnAssetDialogCancelled& OnAssetDialogCancelled) override;
	virtual TArray<FAssetData> CreateModalOpenAssetDialog(const FOpenAssetDialogConfig& InConfig) override;
	virtual void CreateSaveAssetDialog(const FSaveAssetDialogConfig& SaveAssetConfig, const FOnObjectPathChosenForSave& OnAssetNameChosenForSave, const FOnAssetDialogCancelled& OnAssetDialogCancelled) override;
	virtual FString CreateModalSaveAssetDialog(const FSaveAssetDialogConfig& SaveAssetConfig) override;
	virtual bool HasPrimaryContentBrowser() const override;
	virtual void FocusPrimaryContentBrowser(bool bFocusSearch) override;
	virtual void CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory) override;
	virtual void SyncBrowserToAssets(const TArray<struct FAssetData>& AssetDataList, bool bAllowLockedBrowsers = false, bool bFocusContentBrowser = true) override;
	virtual void SyncBrowserToAssets(const TArray<UObject*>& AssetList, bool bAllowLockedBrowsers = false, bool bFocusContentBrowser = true) override;
	virtual void SyncBrowserToFolders(const TArray<FString>& FolderList, bool bAllowLockedBrowsers = false, bool bFocusContentBrowser = true) override;
	virtual void SyncBrowserTo(const FContentBrowserSelection& ItemSelection, bool bAllowLockedBrowsers = false, bool bFocusContentBrowser = true) override;
	virtual void GetSelectedAssets(TArray<FAssetData>& SelectedAssets) override;
	virtual void CaptureThumbnailFromViewport(FViewport* InViewport, TArray<FAssetData>& SelectedAssets) override;
	virtual void SetSelectedPaths(const TArray<FString>& FolderPaths, bool bNeedsRefresh = false) override;
	virtual void ForceShowPluginContent(bool bEnginePlugin) override;


	/** Gets the content browser singleton as a FContentBrowserSingleton */
	static FContentBrowserSingleton& Get();
	
	/** Sets the current primary content browser. */
	void SetPrimaryContentBrowser(const TSharedRef<SContentBrowser>& NewPrimaryBrowser);

	/** Notifies the singleton that a browser was closed */
	void ContentBrowserClosed(const TSharedRef<SContentBrowser>& ClosedBrowser);

	TSharedRef<FNativeClassHierarchy> GetNativeClassHierarchy();

	TSharedRef<FEmptyFolderVisibilityManager> GetEmptyFolderVisibilityManager();

private:

	/** Util to get or create the content browser that should be used by the various Sync functions */
	TSharedPtr<SContentBrowser> FindContentBrowserToSync(bool bAllowLockedBrowsers);

	/** Shared code to open an asset dialog window with a config */
	void SharedCreateAssetDialogWindow(const TSharedRef<class SAssetDialog>& AssetDialog, const FSharedAssetDialogConfig& InConfig, bool bModal) const;

	/** 
	 * Delegate handlers
	 **/
	void OnEditorLoadSelectedAssetsIfNeeded();

	/** Sets the primary content browser to the next valid browser in the list of all browsers */
	void ChooseNewPrimaryBrowser();

	/** Gives focus to the specified content browser */
	void FocusContentBrowser(const TSharedPtr<SContentBrowser>& BrowserToFocus);

	/** Summons a new content browser */
	void SummonNewBrowser(bool bAllowLockedBrowsers = false);

	/** Handler for a request to spawn a new content browser tab */
	TSharedRef<SDockTab> SpawnContentBrowserTab( const FSpawnTabArgs& SpawnTabArgs, int32 BrowserIdx );

	/** Handler for a request to spawn a new content browser tab */
	FText GetContentBrowserTabLabel(int32 BrowserIdx);

	/** Returns true if this content browser is locked (can be used even when closed) */
	bool IsLocked(const FName& InstanceName) const;

	/** Returns a localized name for the tab/menu entry with index */
	static FText GetContentBrowserLabelWithIndex( int32 BrowserIdx );

public:
	/** The tab identifier/instance name for content browser tabs */
	FName ContentBrowserTabIDs[MAX_CONTENT_BROWSERS];

private:
	TArray<TWeakPtr<SContentBrowser>> AllContentBrowsers;

	TMap<FName, TWeakPtr<FTabManager>> BrowserToLastKnownTabManagerMap;

	TWeakPtr<SContentBrowser> PrimaryContentBrowser;

	TSharedPtr<FNativeClassHierarchy> NativeClassHierarchy;

	TSharedRef<FEmptyFolderVisibilityManager> EmptyFolderVisibilityManager;

	TSharedRef<FCollectionAssetRegistryBridge> CollectionAssetRegistryBridge;

	/** An incrementing int32 which is used when making unique settings strings */
	int32 SettingsStringID;
};
