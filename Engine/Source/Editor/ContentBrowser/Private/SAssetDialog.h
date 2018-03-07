// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetData.h"
#include "IContentBrowserSingleton.h"

class SEditableTextBox;
class STextBlock;
class SPathPicker;
class SAssetPicker;

enum class EOpenedContextMenuWidget : uint8
{
	AssetView,
	PathView,
	None
};

class SAssetDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetDialog){}

	SLATE_END_ARGS()

	SAssetDialog();
	virtual ~SAssetDialog();

	virtual void Construct(const FArguments& InArgs, const FSharedAssetDialogConfig& InConfig);

	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	/** Sets the delegate handler for when an open operation is committed */
	void SetOnAssetsChosenForOpen(const FOnAssetsChosenForOpen& InOnAssetsChosenForOpen);

	/** Sets the delegate handler for when a save operation is committed */
	void SetOnObjectPathChosenForSave(const FOnObjectPathChosenForSave& InOnObjectPathChosenForSave);

	/** Sets the delegate handler for when the dialog is closed or cancelled */
	void SetOnAssetDialogCancelled(const FOnAssetDialogCancelled& InOnAssetDialogCancelled);

private:

	/** Used to focus the name box immediately following construction */
	EActiveTimerReturnType SetFocusPostConstruct( double InCurrentTime, float InDeltaTime );

	/** Moves keyboard focus to the name box if this is a save dialog */
	void FocusNameBox();

	/** Gets the name to display in the asset name box */
	FText GetAssetNameText() const;

	/** Gets the name to display in the path text block */
	FText GetPathNameText() const;

	/** Fired when the asset name box text is commited */
	void OnAssetNameTextCommited(const FText& InText, ETextCommit::Type InCommitType);

	/** Gets the visibility of the name error label */
	EVisibility GetNameErrorLabelVisibility() const;

	/** Gets the text to display in the name error label */
	FText GetNameErrorLabelText() const;

	/** Handler for when a path is selected in the path view */
	void HandlePathSelected(const FString& NewPath);

	/** Handler for when a folder is entered in the asset view */
	void HandleAssetViewFolderEntered(const FString& NewPath);

	/** Determines if the confirm button (e.g. Open/Save) is enabled. */
	bool IsConfirmButtonEnabled() const;

	/** Handler for when the confirm button (e.g. Open/Save) is clicked */
	FReply OnConfirmClicked();

	/** Handler for when the cancel button is clicked */
	FReply OnCancelClicked();

	/** Handler for when an asset was selected in the asset picker */
	void OnAssetSelected(const FAssetData& AssetData);

	/* Handler for when an asset was double clicked in the asset picker */
	void OnAssetsActivated(const TArray<FAssetData>& SelectedAssets, EAssetTypeActivationMethod::Type ActivationType);


	/** Will generate the context menu if an asset or a folder is selected, either from the PathView or AssetView */
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets);
	TSharedPtr<SWidget> OnGetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder);

	/** Handler to check to see if a rename command is allowed */
	bool CanExecuteRename() const;

	/** Handler for Rename */
	void ExecuteRename();

	/** Handler to check to see if a delete command is allowed */
	bool CanExecuteDelete() const;

	/** Handler for Delete */
	void ExecuteDelete();
	FReply ExecuteDeleteFolderConfirmed();

	/** Handler to check to see if a create new folder command is allowed */
	bool CanExecuteCreateNewFolder() const;

	/** Handler for creating new folder */
	void ExecuteCreateNewFolder();

	/** Handler for show in explorer */
	void ExecuteExplore();

	/** Handler for size map tool */
	void ExecuteSizeMap();

	/** Setup function for the context menu creation of folder and assets */
	void SetupContextMenuContent(FMenuBuilder& MenuBuilder, const TArray<FString>& SelectedPaths);
	
	void BindCommands();

	/** Closes this dialog */
	void CloseDialog();

	void SetCurrentlySelectedPath(const FString& NewPath);

	void SetCurrentlyEnteredAssetName(const FString& NewName);

	void UpdateInputValidity();

	/** Used to commit the assets that were selected for open in this dialog */
	void ChooseAssetsForOpen(const TArray<FAssetData>& SelectedAssets);

	FString GetObjectPathForSave() const;

	/** Used to commit the object path used for saving in this dialog */
	void CommitObjectPathForSave();

private:

	/** Whether this is an open or save dialog */
	EAssetDialogType::Type DialogType;

	/** Used to update the path view after it has been created */
	FSetPathPickerPathsDelegate SetPathsDelegate;

	/** Used to update the asset view after it has been created */
	FSetARFilterDelegate SetFilterDelegate;

	/** Used to get the currently selected assets */
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;

	/** Only assets of these classes will show up */
	TArray<FName> AssetClassNames;

	/** Fired when assets are chosen for open. Only fired in open dialogs. */
	FOnAssetsChosenForOpen OnAssetsChosenForOpen;

	/** Fired when an object path was chosen for save. Only fired in save dialogs. */
	FOnObjectPathChosenForSave OnObjectPathChosenForSave;

	/** Fired when the asset dialog is cancelled or closed */
	FOnAssetDialogCancelled OnAssetDialogCancelled;

	/** The assets that are currently selected in the asset picker */
	TArray<FAssetData> CurrentlySelectedAssets;

	/** The name box. Only used in save dialogs. */
	TSharedPtr<SEditableTextBox> NameEditableText;

	/** The path box. */
	TSharedPtr<STextBlock> PathText;

	/** The object path of the asset to save. Only used in save dialogs. */
	FString CurrentlySelectedPath;

	/** The object name of the asset to save. Only used in save dialogs. */
	FString CurrentlyEnteredAssetName;

	/** The behavior when the user chooses an existing asset. Only used in save dialogs. */
	ESaveAssetDialogExistingAssetPolicy::Type ExistingAssetPolicy;

	/** The error text from the last validity check */
	FText LastInputValidityErrorText;

	/** True if the last validity check returned that the class name/path is valid for creation */
	bool bLastInputValidityCheckSuccessful;

	/** Used to focus the name box after opening the dialog */
	bool bPendingFocusNextFrame;

	/** Used to specify that valid assets were chosen */
	bool bValidAssetsChosen;

	/** Commands handled by this widget */
	TSharedPtr< FUICommandList > Commands;

	/** Path Picker used by the dialog */
	TSharedPtr<SPathPicker> PathPicker;

	/** Asset Picker used by the dialog */
	TSharedPtr<SAssetPicker> AssetPicker;

	/** CreateNewFolder delegate used when user select create new folder from the context menu */
	FOnCreateNewFolder CurrentContextMenuCreateNewFolderDelegate;

	/** Utility member to know if the context menu was opened on the asset view or the path view */
	EOpenedContextMenuWidget OpenedContextMenuWidget;
};
