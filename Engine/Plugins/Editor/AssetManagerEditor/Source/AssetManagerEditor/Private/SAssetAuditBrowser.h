// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SWidget.h"
#include "AssetData.h"
#include "Widgets/SToolTip.h"
#include "Editor/ContentBrowser/Public/ContentBrowserDelegates.h"
#include "AssetManagerEditorModule.h"

class FUICommandList;
class SMenuAnchor;

//////////////////////////////////////////////////////////////////////////
// SAssetAuditBrowser

class SAssetAuditBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetAuditBrowser)
	{}

	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs);

	virtual ~SAssetAuditBrowser();

	/** Adds assets to current management view */
	void AddAssetsToList(const TArray<FAssetData>& AssetsToView, bool bReplaceExisting);
	void AddAssetsToList(const TArray<FSoftObjectPath>& AssetsToView, bool bReplaceExisting);
	void AddAssetsToList(const TArray<FName>& PackageNamesToView, bool bReplaceExisting);

protected:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	/** Delegate that handles creation of context menu */
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets);

	/** Delegate to handle "Find in Content Browser" context menu option */
	void FindInContentBrowser() const;
	bool IsAnythingSelected() const;
	void OnRequestOpenAsset(const FAssetData& AssetData) const;

	void EditSelectedAssets(TArray<FAssetData> Assets) const;
	void SaveSelectedAssets(TArray<FAssetData> Assets) const;
	bool CanSaveSelectedAssets(TArray<FAssetData> Assets) const;
	void FindReferencesForSelectedAssets(TArray<FAssetData> Assets) const;
	void LoadSelectedAssets(TArray<FAssetData> Assets) const;

	bool CanShowColumnForAssetRegistryTag(FName AssetType, FName TagName) const;
	FString GetValueForCustomColumn(FAssetData& AssetData, FName ColumnName) const;
	
	/** Populate supplied OutPackages with the packages for the supplied Assets array */
	void GetSelectedPackages(const TArray<FAssetData>& Assets, TArray<UPackage*>& OutPackages) const;

	/** Single step forward in history */
	FReply OnGoForwardInHistory();

	/** Single step back in history */
	FReply OnGoBackInHistory();

	/** Jumps immediately to an index in the history if valid */
	void GoToHistoryIndex(int32 InHistoryIdx);

	/** Returns TRUE if stepping backward in history is allowed */
	bool CanStepBackwardInHistory() const;

	/** Returns TRUE if stepping forward in history is allowed */
	bool CanStepForwardInHistory() const;

	/**
	 * Mouse down callback to display a history menu
	 *
	 * @param InMenuAnchor		This is the anchor the menu will use for positioning
	 */
	FReply OnMouseDownHistory( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TWeakPtr< SMenuAnchor > InMenuAnchor );

	/** 
	 * Callback to create the history menu.
	 *
	 * @param bInBackHistory		TRUE if the back history is requested, FALSE if the forward history is
	 *
	 * @return						The menu widget displaying all available history
	 */
	TSharedRef<SWidget> CreateHistoryMenu(bool bInBackHistory) const;

	/** Returns visible when not in a Blueprint mode */
	EVisibility GetHistoryVisibility() const;

	/** Perform additional filtering */
	bool HandleFilterAsset(const FAssetData& InAssetData) const;

	/** Button callbacks */
	FReply ClearAssets();
	FReply RefreshAssets();

	/** Adds all assets of type */
	void AddAssetsOfType(FPrimaryAssetType AssetType);
	
	/** Adds all managed assets by an id */
	void AddManagedAssets(FPrimaryAssetId AssetId);

	/** Adds all assets of a class */
	void AddAssetsOfClass(UClass* AssetClass);

	/** Generates a class picker combo button content */
	TSharedRef<SWidget> CreateClassPicker();

	/** Platform combo box */
	TSharedRef<SWidget> GeneratePlatformComboItem(TSharedPtr<FString> InItem);
	void HandlePlatformComboChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	FText GetPlatformComboText() const;

	/** Refresh the asset view with a new filter */
	void RefreshAssetView();

	/** Commands handled by this widget */
	TSharedPtr<FUICommandList> Commands;

	/** Set of tags to prevent creating details view columns for (infrequently used) */
	TSet<FName> AssetRegistryTagsToIgnore;

	/** List of asset sets to display in browser, current index sets what to display */
	TArray<TSet<FName>> AssetHistory;

	/** Current position in the asset history */
	int32 CurrentAssetHistoryIndex;

	/** List of valid platforms */
	TArray<TSharedPtr<FString>> PlatformComboList;
	TArray<ITargetPlatform*> PlatformList;

	/** Current platform string */
	FString CurrentPlatformString;

	/** Current TargetPlatform and registry state, may be null! */
	ITargetPlatform* CurrentTargetPlatform;
	FAssetRegistryState* CurrentPlatformState;

	/** Delegates to interact with asset view */
	FSyncToAssetsDelegate SyncToAssetsDelegate;
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
	FSetARFilterDelegate SetFilterDelegate;

	/** Max asset sets to save in history */
	static const int32 MaxAssetsHistory;

	/** The section of EditorPerProjectUserSettings in which to save settings */
	static const FString SettingsIniSection;

	/** Cached interfaces */
	class IAssetRegistry* AssetRegistry;
	class UAssetManager* AssetManager;
	class IAssetManagerEditorModule* EditorModule;
};
