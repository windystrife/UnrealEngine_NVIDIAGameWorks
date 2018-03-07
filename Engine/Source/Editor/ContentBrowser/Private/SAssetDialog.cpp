// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAssetDialog.h"
#include "Misc/MessageDialog.h"
#include "Widgets/SBoxPanel.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSplitter.h"
#include "EditorStyleSet.h"
#include "AssetRegistryModule.h"
#include "ContentBrowserSingleton.h"
#include "ContentBrowserUtils.h"

#include "MultiBoxBuilder.h"
#include "ContentBrowserCommands.h"
#include "GenericCommands.h"
#include "SPathPicker.h"
#include "SAssetPicker.h"
#include "AssetViewTypes.h"
#include "ObjectTools.h"

#include "SPathView.h"
#include "SAssetView.h"
#include "SNotificationList.h"
#include "NotificationManager.h"
#include "Editor/EditorEngine.h"
#include "FileManager.h"
#include "NativeClassHierarchy.h"
#include "ISizeMapModule.h"
#include "CoreMinimal.h"
#include "SourceCodeNavigation.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

SAssetDialog::SAssetDialog()
	: DialogType(EAssetDialogType::Open)
	, ExistingAssetPolicy(ESaveAssetDialogExistingAssetPolicy::Disallow)
	, bLastInputValidityCheckSuccessful(false)
	, bPendingFocusNextFrame(true)
	, bValidAssetsChosen(false)
	, OpenedContextMenuWidget(EOpenedContextMenuWidget::None)
{
}

SAssetDialog::~SAssetDialog()
{
	if (!bValidAssetsChosen)
	{
		OnAssetDialogCancelled.ExecuteIfBound();
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAssetDialog::Construct(const FArguments& InArgs, const FSharedAssetDialogConfig& InConfig)
{
	DialogType = InConfig.GetDialogType();

	AssetClassNames = InConfig.AssetClassNames;

	const FString DefaultPath = InConfig.DefaultPath;

	RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SAssetDialog::SetFocusPostConstruct ) );

	FPathPickerConfig PathPickerConfig;
	PathPickerConfig.DefaultPath = DefaultPath;
	PathPickerConfig.bFocusSearchBoxWhenOpened = false;
	PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &SAssetDialog::HandlePathSelected);
	PathPickerConfig.SetPathsDelegates.Add(&SetPathsDelegate);
	PathPickerConfig.OnGetFolderContextMenu = FOnGetFolderContextMenu::CreateSP(this, &SAssetDialog::OnGetFolderContextMenu);	

	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassNames.Append(AssetClassNames);
	AssetPickerConfig.Filter.PackagePaths.Add(FName(*DefaultPath));
	AssetPickerConfig.bAllowDragging = false;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SAssetDialog::OnAssetSelected);
	AssetPickerConfig.OnAssetsActivated = FOnAssetsActivated::CreateSP(this, &SAssetDialog::OnAssetsActivated);
	AssetPickerConfig.SetFilterDelegates.Add(&SetFilterDelegate);
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.SaveSettingsName = TEXT("AssetDialog");
	AssetPickerConfig.bCanShowFolders = true;
	AssetPickerConfig.bCanShowDevelopersFolder = true;
	AssetPickerConfig.OnFolderEntered = FOnPathSelected::CreateSP(this, &SAssetDialog::HandleAssetViewFolderEntered);
	AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SAssetDialog::OnGetAssetContextMenu);
	AssetPickerConfig.OnGetFolderContextMenu = FOnGetFolderContextMenu::CreateSP(this, &SAssetDialog::OnGetFolderContextMenu);

	SetCurrentlySelectedPath(DefaultPath);

	// Open and save specific configuration
	FText ConfirmButtonText;
	bool bIncludeNameBox = false;
	if ( DialogType == EAssetDialogType::Open )
	{
		const FOpenAssetDialogConfig& OpenAssetConfig = static_cast<const FOpenAssetDialogConfig&>(InConfig);
		PathPickerConfig.bAllowContextMenu = true;
		ConfirmButtonText = LOCTEXT("AssetDialogOpenButton", "Open");
		AssetPickerConfig.SelectionMode = OpenAssetConfig.bAllowMultipleSelection ? ESelectionMode::Multi : ESelectionMode::Single;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = true;
		bIncludeNameBox = false;
	}
	else if ( DialogType == EAssetDialogType::Save )
	{
		const FSaveAssetDialogConfig& SaveAssetConfig = static_cast<const FSaveAssetDialogConfig&>(InConfig);
		PathPickerConfig.bAllowContextMenu = true;
		ConfirmButtonText = LOCTEXT("AssetDialogSaveButton", "Save");
		AssetPickerConfig.SelectionMode = ESelectionMode::Single;
		AssetPickerConfig.bFocusSearchBoxWhenOpened = false;
		bIncludeNameBox = true;
		ExistingAssetPolicy = SaveAssetConfig.ExistingAssetPolicy;
		SetCurrentlyEnteredAssetName(SaveAssetConfig.DefaultAssetName);
	}
	else
	{
		ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
	}

	PathPicker = StaticCastSharedRef<SPathPicker>(FContentBrowserSingleton::Get().CreatePathPicker(PathPickerConfig));
	AssetPicker = StaticCastSharedRef<SAssetPicker>(FContentBrowserSingleton::Get().CreateAssetPicker(AssetPickerConfig));

	FContentBrowserCommands::Register();
	BindCommands();

	// The root widget in this dialog.
	TSharedRef<SVerticalBox> MainVerticalBox = SNew(SVerticalBox);

	// Path/Asset view
	MainVerticalBox->AddSlot()
		.FillHeight(1)
		.Padding(0, 0, 0, 4)
		[
			SNew(SSplitter)
		
			+SSplitter::Slot()
			.Value(0.25f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					PathPicker.ToSharedRef()
				]
			]

			+SSplitter::Slot()
			.Value(0.75f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					AssetPicker.ToSharedRef()
				]
			]
		];

	// Input error strip, if we are using a name box
	if (bIncludeNameBox)
	{
		// Name Error label
		MainVerticalBox->AddSlot()
		.AutoHeight()
		[
			// Constant height, whether the label is visible or not
			SNew(SBox).HeightOverride(18)
			[
				SNew(SBorder)
				.Visibility( this, &SAssetDialog::GetNameErrorLabelVisibility )
				.BorderImage( FEditorStyle::GetBrush("AssetDialog.ErrorLabelBorder") )
				.Content()
				[
					SNew(STextBlock)
					.Text( this, &SAssetDialog::GetNameErrorLabelText )
					.TextStyle( FEditorStyle::Get(), "AssetDialog.ErrorLabelFont" )
				]
			]
		];
	}

	TSharedRef<SVerticalBox> LabelsBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.VAlign(VAlign_Center)
		.Padding(0, 2, 0, 2)
		[
			SNew(STextBlock).Text(LOCTEXT("PathBoxLabel", "Path:"))
		];

	TSharedRef<SVerticalBox> ContentBox = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1)
		.VAlign(VAlign_Center)
		.Padding(0, 2, 0, 2)
		[
			SAssignNew(PathText, STextBlock)
			.Text(this, &SAssetDialog::GetPathNameText)
		];

	if (bIncludeNameBox)
	{
		LabelsBox->AddSlot()
			.FillHeight(1)
			.VAlign(VAlign_Center)
			.Padding(0, 2, 0, 2)
			[
				SNew(STextBlock).Text(LOCTEXT("NameBoxLabel", "Name:"))
			];

		ContentBox->AddSlot()
			.FillHeight(1)
			.VAlign(VAlign_Center)
			.Padding(0, 2, 0, 2)
			[
				SAssignNew(NameEditableText, SEditableTextBox)
				.Text(this, &SAssetDialog::GetAssetNameText)
				.OnTextCommitted(this, &SAssetDialog::OnAssetNameTextCommited)
				.OnTextChanged(this, &SAssetDialog::OnAssetNameTextCommited, ETextCommit::Default)
				.SelectAllTextWhenFocused(true)
			];
	}

	// Buttons and asset name
	TSharedRef<SHorizontalBox> ButtonsAndNameBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(bIncludeNameBox ? 80 : 4, 20, 4, 3)
		[
			LabelsBox
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1)
		.VAlign(VAlign_Bottom)
		.Padding(4, 3)
		[
			ContentBox
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		.Padding(4, 3)
		[
			SNew(SButton)
			.Text(ConfirmButtonText)
			.ContentPadding(FMargin(8, 2, 8, 2))
			.IsEnabled(this, &SAssetDialog::IsConfirmButtonEnabled)
			.OnClicked(this, &SAssetDialog::OnConfirmClicked)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Bottom)
		.Padding(4, 3)
		[
			SNew(SButton)
			.ContentPadding(FMargin(8, 2, 8, 2))
			.Text(LOCTEXT("AssetDialogCancelButton", "Cancel"))
			.OnClicked(this, &SAssetDialog::OnCancelClicked)
		];

	MainVerticalBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(0)
		[
			ButtonsAndNameBox
		];

	ChildSlot
	[
		MainVerticalBox
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FReply SAssetDialog::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if ( InKeyEvent.GetKey() == EKeys::Escape )
	{
		CloseDialog();
		return FReply::Handled();
	}
	else if (Commands->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SAssetDialog::BindCommands()
{
	Commands = TSharedPtr< FUICommandList >(new FUICommandList);

	Commands->MapAction(FGenericCommands::Get().Rename, FUIAction(
		FExecuteAction::CreateSP(this, &SAssetDialog::ExecuteRename),
		FCanExecuteAction::CreateSP(this, &SAssetDialog::CanExecuteRename)
	));

	Commands->MapAction(FGenericCommands::Get().Delete, FUIAction(
		FExecuteAction::CreateSP(this, &SAssetDialog::ExecuteDelete),
		FCanExecuteAction::CreateSP(this, &SAssetDialog::CanExecuteDelete)
	));

	Commands->MapAction(FContentBrowserCommands::Get().CreateNewFolder, FUIAction(
		FExecuteAction::CreateSP(this, &SAssetDialog::ExecuteCreateNewFolder),
		FCanExecuteAction::CreateSP(this, &SAssetDialog::CanExecuteCreateNewFolder)
	));
}

bool SAssetDialog::CanExecuteRename() const
{
	switch (OpenedContextMenuWidget)
	{
		case EOpenedContextMenuWidget::AssetView: return ContentBrowserUtils::CanRenameFromAssetView(AssetPicker->GetAssetView());
		case EOpenedContextMenuWidget::PathView: return ContentBrowserUtils::CanRenameFromPathView(PathPicker->GetPaths());
	}

	return false;
}

void SAssetDialog::ExecuteRename()
{
	TArray< FAssetData > AssetViewSelectedAssets = AssetPicker->GetAssetView()->GetSelectedAssets();
	TArray< FString > SelectedFolders = AssetPicker->GetAssetView()->GetSelectedFolders();

	if (SelectedFolders.Num() > 0 || AssetViewSelectedAssets.Num() > 0)
	{
		if (AssetViewSelectedAssets.Num() == 1 && SelectedFolders.Num() == 0)
		{
			// Don't operate on Redirectors
			if (AssetViewSelectedAssets[0].AssetClass != UObjectRedirector::StaticClass()->GetFName())
			{
				AssetPicker->GetAssetView()->RenameAsset(AssetViewSelectedAssets[0]);
			}
		}
		else if (AssetViewSelectedAssets.Num() == 0 && SelectedFolders.Num() == 1)
		{
			AssetPicker->GetAssetView()->RenameFolder(SelectedFolders[0]);
		}
	}
	else
	{		
		const TArray<FString>& SelectedPaths = PathPicker->GetPathView()->GetSelectedPaths();

		if (SelectedPaths.Num() == 1)
		{
			PathPicker->GetPathView()->RenameFolder(SelectedPaths[0]);
		}
	}
}

bool SAssetDialog::CanExecuteDelete() const
{
	switch (OpenedContextMenuWidget)
	{
		case EOpenedContextMenuWidget::AssetView: return ContentBrowserUtils::CanDeleteFromAssetView(AssetPicker->GetAssetView());
		case EOpenedContextMenuWidget::PathView: return ContentBrowserUtils::CanDeleteFromPathView(PathPicker->GetPaths());
	}

	return false;
}

void SAssetDialog::ExecuteDelete()
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

	TArray<FString> SelectedFolders = AssetPicker->GetAssetView()->GetSelectedFolders();
	TArray<FAssetData> SelectedAssets = AssetPicker->GetAssetView()->GetSelectedAssets();

	if (SelectedFolders.Num() == 0)
	{
		SelectedFolders = PathPicker->GetPaths();
	}

	if (SelectedAssets.Num() > 0 && OpenedContextMenuWidget == EOpenedContextMenuWidget::AssetView)
	{
		TArray<FAssetData> AssetsToDelete;
		AssetsToDelete.Reserve(SelectedAssets.Num());

		for (auto AssetData : SelectedAssets)
		{
			// Don't operate on Redirectors
			if (AssetData.AssetClass != UObjectRedirector::StaticClass()->GetFName())
			{
				AssetsToDelete.Add(AssetData);
			}
		}

		if (AssetsToDelete.Num() > 0)
		{
			ObjectTools::DeleteAssets(AssetsToDelete);
		}
	}

	if (SelectedFolders.Num() > 0)
	{
		FText Prompt;
		if (SelectedFolders.Num() == 1)
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Single", "Delete folder '{0}'?"), FText::FromString(SelectedFolders[0]));
		}
		else
		{
			Prompt = FText::Format(LOCTEXT("FolderDeleteConfirm_Multiple", "Delete {0} folders?"), FText::AsNumber(SelectedFolders.Num()));
		}

		// Spawn a confirmation dialog since this is potentially a highly destructive operation
		FOnClicked OnYesClicked = FOnClicked::CreateSP(this, &SAssetDialog::ExecuteDeleteFolderConfirmed);
		ContentBrowserUtils::DisplayConfirmationPopup(Prompt, LOCTEXT("FolderDeleteConfirm_Yes", "Delete"), LOCTEXT("FolderDeleteConfirm_No", "Cancel"), AssetPicker->GetAssetView().ToSharedRef(), OnYesClicked);
	}
}

FReply SAssetDialog::ExecuteDeleteFolderConfirmed()
{
	TArray< FString > SelectedFolders = AssetPicker->GetAssetView()->GetSelectedFolders();

	if (SelectedFolders.Num() > 0)
	{
		ContentBrowserUtils::DeleteFolders(SelectedFolders);
	}
	else
	{
		const TArray<FString>& SelectedPaths = PathPicker->GetPaths();

		if (SelectedPaths.Num() > 0)
		{
			if (ContentBrowserUtils::DeleteFolders(SelectedPaths))
			{
				// Since the contents of the asset view have just been deleted, set the selected path to the default "/Game"
				TArray<FString> DefaultSelectedPaths;
				DefaultSelectedPaths.Add(TEXT("/Game"));
				PathPicker->GetPathView()->SetSelectedPaths(DefaultSelectedPaths);

				FSourcesData DefaultSourcesData(FName("/Game"));
				AssetPicker->GetAssetView()->SetSourcesData(DefaultSourcesData);
			}
		}
	}

	return FReply::Handled();
}

void SAssetDialog::ExecuteExplore()
{
	TArray<FString> SelectedFolders = AssetPicker->GetAssetView()->GetSelectedFolders();
	TArray<FAssetData> SelectedAssets = AssetPicker->GetAssetView()->GetSelectedAssets();

	if (SelectedFolders.Num() == 0 && SelectedAssets.Num() == 0)
	{
		SelectedFolders = PathPicker->GetPaths();
	}

	FString PathToExplore;

	if (SelectedFolders.Num() > 0 && SelectedAssets.Num() == 0)
	{
		for (int32 PathIdx = 0; PathIdx < SelectedFolders.Num(); ++PathIdx)
		{
			const FString& Path = SelectedFolders[PathIdx];

			FString FilePath;
			if (ContentBrowserUtils::IsClassPath(Path))
			{
				TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
				if (NativeClassHierarchy->GetFileSystemPath(Path, FilePath))
				{
					FilePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*FilePath);
				}
			}
			else
			{
				FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(Path + TEXT("/")));
			}

			if (!FilePath.IsEmpty())
			{
				// If the folder has not yet been created, make is right before we try to explore to it
				if (!IFileManager::Get().DirectoryExists(*FilePath))
				{
					IFileManager::Get().MakeDirectory(*FilePath, /*Tree=*/true);
				}

				PathToExplore = FilePath;
			}
		}
	}
	else
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
							PathToExplore = FPaths::ConvertRelativePathToFull(ModulePath / (*RelativePath));
						}
					}
				}
				else
				{
					const bool bIsWorldAsset = (AssetData.AssetClass == UWorld::StaticClass()->GetFName());
					const FString Extension = bIsWorldAsset ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
					const FString FilePath = FPackageName::LongPackageNameToFilename(PackageName, Extension);

					PathToExplore = FPaths::ConvertRelativePathToFull(FilePath);
				}
			}
		}
	}

	if (!PathToExplore.IsEmpty())
	{
		FPlatformProcess::ExploreFolder(*PathToExplore);
	}
}

void SAssetDialog::ExecuteSizeMap()
{
	TArray<FString> SelectedPaths = AssetPicker->GetAssetView()->GetSelectedFolders();
	TArray<FAssetData> SelectedAssets = AssetPicker->GetAssetView()->GetSelectedAssets();

	TArray<FName> PackageNames;

	if (SelectedPaths.Num() == 0 && SelectedAssets.Num() == 0)
	{
		SelectedPaths = PathPicker->GetPaths();
	}

	if (SelectedPaths.Num() > 0)
	{
		// Form a filter from the paths
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		for (int32 PathIdx = 0; PathIdx < SelectedPaths.Num(); ++PathIdx)
		{
			const FString& Path = SelectedPaths[PathIdx];
			new (Filter.PackagePaths) FName(*Path);
		}

		// Load the asset registry module
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		// Query for a list of assets in the selected paths
		TArray<FAssetData> AssetList;
		AssetRegistryModule.Get().GetAssets(Filter, AssetList);

		// Form a list of unique package names from the assets
		TSet<FName> UniquePackageNames;
		for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); ++AssetIdx)
		{
			UniquePackageNames.Add(AssetList[AssetIdx].PackageName);
		}

		// Add all unique package names to the output
		PackageNames.Reserve(UniquePackageNames.Num());

		for (auto PackageIt = UniquePackageNames.CreateConstIterator(); PackageIt; ++PackageIt)
		{
			PackageNames.Add((*PackageIt));
		}
	}	
	else
	{
		PackageNames.Reserve(SelectedAssets.Num());

		for (auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
		{
			PackageNames.Add(AssetIt->PackageName);
		}
	}

	if (PackageNames.Num() > 0)
	{
		ISizeMapModule::Get().InvokeSizeMapModalDialog(PackageNames, FSlateApplication::Get().GetActiveModalWindow());
	}
}

bool SAssetDialog::CanExecuteCreateNewFolder() const
{	
	// We can only create folders when we have a single path selected
	return ContentBrowserUtils::IsValidPathToCreateNewFolder(CurrentlySelectedPath);
}

void SAssetDialog::ExecuteCreateNewFolder()
{
	PathPicker->CreateNewFolder(CurrentlySelectedPath, CurrentContextMenuCreateNewFolderDelegate);
}

TSharedPtr<SWidget> SAssetDialog::OnGetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder)
{
	if (FSlateApplication::Get().HasFocusedDescendants(PathPicker.ToSharedRef()))
	{
		OpenedContextMenuWidget = EOpenedContextMenuWidget::PathView;
	}
	else if (FSlateApplication::Get().HasFocusedDescendants(AssetPicker.ToSharedRef()))
	{
		OpenedContextMenuWidget = EOpenedContextMenuWidget::AssetView;
	}

	TSharedPtr<FExtender> Extender;
	if (InMenuExtender.IsBound())
	{
		Extender = InMenuExtender.Execute(SelectedPaths);
	}

	if (FSlateApplication::Get().HasFocusedDescendants(PathPicker.ToSharedRef()))
	{
		PathPicker->SetPaths(SelectedPaths);
	}

	CurrentContextMenuCreateNewFolderDelegate = InOnCreateNewFolder;

	FMenuBuilder MenuBuilder(true /*bInShouldCloseWindowAfterMenuSelection*/, Commands, Extender);
	SetupContextMenuContent(MenuBuilder, SelectedPaths);

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SAssetDialog::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	OpenedContextMenuWidget = EOpenedContextMenuWidget::AssetView;

	FMenuBuilder MenuBuilder(true /*bInShouldCloseWindowAfterMenuSelection*/, Commands);

	CurrentContextMenuCreateNewFolderDelegate = FOnCreateNewFolder::CreateSP(AssetPicker->GetAssetView().Get(), &SAssetView::OnCreateNewFolder);

	TArray<FString> Paths;
	SetupContextMenuContent(MenuBuilder, Paths);

	return MenuBuilder.MakeWidget();
}

void SAssetDialog::SetupContextMenuContent(FMenuBuilder& MenuBuilder, const TArray<FString>& SelectedPaths)
{
	FText NewFolderToolTip;

	if (SelectedPaths.Num() > 0)
	{
		if (CanExecuteCreateNewFolder())
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
		NewFolderToolTip = FText(LOCTEXT("NewFolderTooltip_InvalidAction", "Cannot create new folders when an asset is selected."));
	}

	MenuBuilder.BeginSection("AssetDialogOptions", LOCTEXT("AssetDialogMenuHeading", "Options"));

	MenuBuilder.AddMenuEntry(FContentBrowserCommands::Get().CreateNewFolder, NAME_None, LOCTEXT("NewFolder", "New Folder"), NewFolderToolTip, FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.NewFolderIcon"));
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameFolder", "Rename"), LOCTEXT("RenameFolderTooltip", "Rename the selected folder."), FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Rename"));
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("DeleteFolder", "Delete"), LOCTEXT("DeleteFolderTooltip", "Removes this folder and all assets it contains."));

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AssetDialogExplore", LOCTEXT("AssetDialogExploreHeading", "Explore"));

	MenuBuilder.AddMenuEntry(ContentBrowserUtils::GetExploreFolderText(), 
							 LOCTEXT("ExploreTooltip", "Finds this folder on disk."), 
							 FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"), 
							 FUIAction(FExecuteAction::CreateSP(this, &SAssetDialog::ExecuteExplore)));

	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AssetDialogReferences", LOCTEXT("AssetDialogReferencesHeading", "References"));

	MenuBuilder.AddMenuEntry(LOCTEXT("SizeMap", "Size Map..."), 
							 LOCTEXT("SizeMapOnFolderTooltip", "Shows an interactive map of the approximate memory used by the assets in this folder and everything they reference."), 
							 FSlateIcon(), 
							 FUIAction(FExecuteAction::CreateSP(this, &SAssetDialog::ExecuteSizeMap)));

	MenuBuilder.EndSection();
}

EActiveTimerReturnType SAssetDialog::SetFocusPostConstruct( double InCurrentTime, float InDeltaTime )
{
	FocusNameBox();
	return EActiveTimerReturnType::Stop;
}

void SAssetDialog::SetOnAssetsChosenForOpen(const FOnAssetsChosenForOpen& InOnAssetsChosenForOpen)
{
	OnAssetsChosenForOpen = InOnAssetsChosenForOpen;
}

void SAssetDialog::SetOnObjectPathChosenForSave(const FOnObjectPathChosenForSave& InOnObjectPathChosenForSave)
{
	OnObjectPathChosenForSave = InOnObjectPathChosenForSave;
}

void SAssetDialog::SetOnAssetDialogCancelled(const FOnAssetDialogCancelled& InOnAssetDialogCancelled)
{
	OnAssetDialogCancelled = InOnAssetDialogCancelled;
}

void SAssetDialog::FocusNameBox()
{
	if ( NameEditableText.IsValid() )
	{
		FSlateApplication::Get().SetKeyboardFocus(NameEditableText.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

FText SAssetDialog::GetAssetNameText() const
{
	return FText::FromString(CurrentlyEnteredAssetName);
}

FText SAssetDialog::GetPathNameText() const
{
	return FText::FromString(CurrentlySelectedPath);
}

void SAssetDialog::OnAssetNameTextCommited(const FText& InText, ETextCommit::Type InCommitType)
{
	SetCurrentlyEnteredAssetName(InText.ToString());

	if ( InCommitType == ETextCommit::OnEnter )
	{
		CommitObjectPathForSave();
	}
}

EVisibility SAssetDialog::GetNameErrorLabelVisibility() const
{
	return GetNameErrorLabelText().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}

FText SAssetDialog::GetNameErrorLabelText() const
{
	if (!bLastInputValidityCheckSuccessful)
	{
		return LastInputValidityErrorText;
	}

	return FText::GetEmpty();
}

void SAssetDialog::HandlePathSelected(const FString& NewPath)
{
	FARFilter NewFilter;

	NewFilter.ClassNames.Append(AssetClassNames);
	NewFilter.PackagePaths.Add(FName(*NewPath));

	SetCurrentlySelectedPath(NewPath);

	SetFilterDelegate.ExecuteIfBound(NewFilter);
}

void SAssetDialog::HandleAssetViewFolderEntered(const FString& NewPath)
{
	SetCurrentlySelectedPath(NewPath);

	TArray<FString> NewPaths;
	NewPaths.Add(NewPath);
	SetPathsDelegate.Execute(NewPaths);
}

bool SAssetDialog::IsConfirmButtonEnabled() const
{
	if ( DialogType == EAssetDialogType::Open )
	{
		return CurrentlySelectedAssets.Num() > 0;
	}
	else if ( DialogType == EAssetDialogType::Save )
	{
		return bLastInputValidityCheckSuccessful;
	}
	else
	{
		ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
	}

	return false;
}

FReply SAssetDialog::OnConfirmClicked()
{
	if ( DialogType == EAssetDialogType::Open )
	{
		TArray<FAssetData> SelectedAssets = GetCurrentSelectionDelegate.Execute();
		if (SelectedAssets.Num() > 0)
		{
			ChooseAssetsForOpen(SelectedAssets);
		}
	}
	else if ( DialogType == EAssetDialogType::Save )
	{
		// @todo save asset validation (e.g. "asset already exists" check)
		CommitObjectPathForSave();
	}
	else
	{
		ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
	}

	return FReply::Handled();
}

FReply SAssetDialog::OnCancelClicked()
{
	CloseDialog();

	return FReply::Handled();
}

void SAssetDialog::OnAssetSelected(const FAssetData& AssetData)
{
	CurrentlySelectedAssets = GetCurrentSelectionDelegate.Execute();
	
	if ( AssetData.IsValid() )
	{
		SetCurrentlySelectedPath(AssetData.PackagePath.ToString());
		SetCurrentlyEnteredAssetName(AssetData.AssetName.ToString());
	}
}

void SAssetDialog::OnAssetsActivated(const TArray<FAssetData>& SelectedAssets, EAssetTypeActivationMethod::Type ActivationType)
{
	const bool bCorrectActivationMethod = (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened);
	if (SelectedAssets.Num() > 0 && bCorrectActivationMethod)
	{
		if ( DialogType == EAssetDialogType::Open )
		{
			ChooseAssetsForOpen(SelectedAssets);
		}
		else if ( DialogType == EAssetDialogType::Save )
		{
			const FAssetData& AssetData = SelectedAssets[0];
			SetCurrentlySelectedPath(AssetData.PackagePath.ToString());
			SetCurrentlyEnteredAssetName(AssetData.AssetName.ToString());
			CommitObjectPathForSave();
		}
		else
		{
			ensureMsgf(0, TEXT("AssetDialog type %d is not supported."), DialogType);
		}
	}
}

void SAssetDialog::CloseDialog()
{
	FWidgetPath WidgetPath;
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared(), WidgetPath);

	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}
}

void SAssetDialog::SetCurrentlySelectedPath(const FString& NewPath)
{
	CurrentlySelectedPath = NewPath;
	UpdateInputValidity();
}

void SAssetDialog::SetCurrentlyEnteredAssetName(const FString& NewName)
{
	CurrentlyEnteredAssetName = NewName;
	UpdateInputValidity();
}

void SAssetDialog::UpdateInputValidity()
{
	bLastInputValidityCheckSuccessful = true;

	if ( CurrentlyEnteredAssetName.IsEmpty() )
	{
		// No error text for an empty name. Just fail validity.
		LastInputValidityErrorText = FText::GetEmpty();
		bLastInputValidityCheckSuccessful = false;
	}

	if (bLastInputValidityCheckSuccessful)
	{
		if ( CurrentlySelectedPath.IsEmpty() )
		{
			LastInputValidityErrorText = LOCTEXT("AssetDialog_NoPathSelected", "You must select a path.");
			bLastInputValidityCheckSuccessful = false;
		}
	}

	if ( DialogType == EAssetDialogType::Save )
	{
		if ( bLastInputValidityCheckSuccessful )
		{
			const FString ObjectPath = GetObjectPathForSave();
			FText ErrorMessage;
			const bool bAllowExistingAsset = (ExistingAssetPolicy == ESaveAssetDialogExistingAssetPolicy::AllowButWarn);
			if ( !ContentBrowserUtils::IsValidObjectPathForCreate(ObjectPath, ErrorMessage, bAllowExistingAsset) )
			{
				LastInputValidityErrorText = ErrorMessage;
				bLastInputValidityCheckSuccessful = false;
			}
		}
	}
}

void SAssetDialog::ChooseAssetsForOpen(const TArray<FAssetData>& SelectedAssets)
{
	if ( ensure(DialogType == EAssetDialogType::Open) )
	{
		if (SelectedAssets.Num() > 0)
		{
			bValidAssetsChosen = true;
			OnAssetsChosenForOpen.ExecuteIfBound(SelectedAssets);
			CloseDialog();
		}
	}
}

FString SAssetDialog::GetObjectPathForSave() const
{
	return CurrentlySelectedPath / CurrentlyEnteredAssetName + TEXT(".") + CurrentlyEnteredAssetName;
}

void SAssetDialog::CommitObjectPathForSave()
{
	if ( ensure(DialogType == EAssetDialogType::Save) )
	{
		if ( bLastInputValidityCheckSuccessful )
		{
			const FString ObjectPath = GetObjectPathForSave();

			bool bProceedWithSave = true;

			// If we were asked to warn on existing assets, do it now
			if ( ExistingAssetPolicy == ESaveAssetDialogExistingAssetPolicy::AllowButWarn )
			{
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				FAssetData ExistingAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FName(*ObjectPath));
				if ( ExistingAsset.IsValid() && AssetClassNames.Contains(ExistingAsset.AssetClass) )
				{
					EAppReturnType::Type ShouldReplace = FMessageDialog::Open( EAppMsgType::YesNo, FText::Format(LOCTEXT("ReplaceAssetMessage", "{ExistingAsset} already exists. Do you want to replace it?"), FText::FromString(CurrentlyEnteredAssetName)) );
					bProceedWithSave = (ShouldReplace == EAppReturnType::Yes);
				}
			}

			if ( bProceedWithSave )
			{
				bValidAssetsChosen = true;
				OnObjectPathChosenForSave.ExecuteIfBound(ObjectPath);
				CloseDialog();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
