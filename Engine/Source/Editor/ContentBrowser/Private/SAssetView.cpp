// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAssetView.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/SOverlay.h"
#include "Engine/GameViewportClient.h"
#include "Factories/Factory.h"
#include "Framework/Commands/UIAction.h"
#include "Textures/SlateIcon.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSlider.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "Settings/ContentBrowserSettings.h"
#include "Engine/Blueprint.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "AssetSelection.h"
#include "AssetRegistryModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "ContentBrowserLog.h"
#include "FrontendFilterBase.h"
#include "ContentBrowserSingleton.h"
#include "EditorWidgetsModule.h"
#include "AssetViewTypes.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragDropHandler.h"
#include "AssetViewWidgets.h"
#include "ContentBrowserModule.h"
#include "ObjectTools.h"
#include "NativeClassHierarchy.h"
#include "EmptyFolderVisibilityManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "SSplitter.h"
#include "HAL/PlatformApplicationMisc.h"
#include "DesktopPlatformModule.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

namespace
{
	/** Time delay between recently added items being added to the filtered asset items list */
	const double TimeBetweenAddingNewAssets = 4.0;

	/** Time delay between performing the last jump, and the jump term being reset */
	const double JumpDelaySeconds = 2.0;
}

#define MAX_THUMBNAIL_SIZE 4096

SAssetView::~SAssetView()
{
	// Load the asset registry module to unregister delegates
	if ( FModuleManager::Get().IsModuleLoaded("AssetRegistry") )
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnAssetAdded().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRemoved().RemoveAll( this );
		AssetRegistryModule.Get().OnAssetRenamed().RemoveAll( this );
		AssetRegistryModule.Get().OnPathAdded().RemoveAll( this );
		AssetRegistryModule.Get().OnPathRemoved().RemoveAll( this );
	}

	// Unregister listener for asset loading and object property changes
	FCoreUObjectDelegates::OnAssetLoaded.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);

	// Unsubscribe from folder population events
	{
		TSharedRef<FEmptyFolderVisibilityManager> EmptyFolderVisibilityManager = FContentBrowserSingleton::Get().GetEmptyFolderVisibilityManager();
		EmptyFolderVisibilityManager->OnFolderPopulated().RemoveAll(this);
	}

	// Unsubscribe from class events
	if ( bCanShowClasses )
	{
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
		NativeClassHierarchy->OnClassHierarchyUpdated().RemoveAll( this );
	}

	// Remove the listener for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().RemoveAll(this);

	if ( FrontendFilters.IsValid() )
	{
		// Clear the frontend filter changed delegate
		FrontendFilters->OnChanged().RemoveAll( this );
	}

	// Release all rendering resources being held onto
	AssetThumbnailPool->ReleaseResources();
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAssetView::Construct( const FArguments& InArgs )
{
	bIsWorking = false;
	TotalAmortizeTime = 0;
	AmortizeStartTime = 0;
	MaxSecondsPerFrame = 0.015;

	bFillEmptySpaceInTileView = InArgs._FillEmptySpaceInTileView;
	FillScale = 1.0f;

	ThumbnailHintFadeInSequence.JumpToStart();
	ThumbnailHintFadeInSequence.AddCurve(0, 0.5f, ECurveEaseFunction::Linear);

	// Load the asset registry module to listen for updates
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetAdded().AddSP( this, &SAssetView::OnAssetAdded );
	AssetRegistryModule.Get().OnAssetRemoved().AddSP( this, &SAssetView::OnAssetRemoved );
	AssetRegistryModule.Get().OnAssetRenamed().AddSP( this, &SAssetView::OnAssetRenamed );
	AssetRegistryModule.Get().OnPathAdded().AddSP( this, &SAssetView::OnAssetRegistryPathAdded );
	AssetRegistryModule.Get().OnPathRemoved().AddSP( this, &SAssetView::OnAssetRegistryPathRemoved );

	FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
	CollectionManagerModule.Get().OnAssetsAdded().AddSP( this, &SAssetView::OnAssetsAddedToCollection );
	CollectionManagerModule.Get().OnAssetsRemoved().AddSP( this, &SAssetView::OnAssetsRemovedFromCollection );
	CollectionManagerModule.Get().OnCollectionRenamed().AddSP( this, &SAssetView::OnCollectionRenamed );
	CollectionManagerModule.Get().OnCollectionUpdated().AddSP( this, &SAssetView::OnCollectionUpdated );

	// Listen for when assets are loaded or changed to update item data
	FCoreUObjectDelegates::OnAssetLoaded.AddSP(this, &SAssetView::OnAssetLoaded);
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &SAssetView::OnObjectPropertyChanged);

	// Listen to find out when the available classes are changed, so that we can refresh our paths
	if ( bCanShowClasses )
	{
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();
		NativeClassHierarchy->OnClassHierarchyUpdated().AddSP( this, &SAssetView::OnClassHierarchyUpdated );
	}

	// Listen to find out when previously empty paths are populated with content
	{
		TSharedRef<FEmptyFolderVisibilityManager> EmptyFolderVisibilityManager = FContentBrowserSingleton::Get().GetEmptyFolderVisibilityManager();
		EmptyFolderVisibilityManager->OnFolderPopulated().AddSP(this, &SAssetView::OnFolderPopulated);
	}

	// Listen for when view settings are changed
	UContentBrowserSettings::OnSettingChanged().AddSP(this, &SAssetView::HandleSettingChanged);

	// Get desktop metrics
	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetDisplayMetrics( DisplayMetrics );

	const FVector2D DisplaySize(
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Right - DisplayMetrics.PrimaryDisplayWorkAreaRect.Left,
		DisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom - DisplayMetrics.PrimaryDisplayWorkAreaRect.Top );

	const float ThumbnailScaleRangeScalar = ( DisplaySize.Y / 1080 );

	// Create a thumbnail pool for rendering thumbnails	
	AssetThumbnailPool = MakeShareable( new FAssetThumbnailPool(1024, InArgs._AreRealTimeThumbnailsAllowed) );
	NumOffscreenThumbnails = 64;
	ListViewThumbnailResolution = 128;
	ListViewThumbnailSize = 64;
	ListViewThumbnailPadding = 4;
	TileViewThumbnailResolution = 256;
	TileViewThumbnailSize = 128;
	TileViewThumbnailPadding = 5;

	TileViewNameHeight = 36;
	ThumbnailScaleSliderValue = InArgs._ThumbnailScale; 

	if ( !ThumbnailScaleSliderValue.IsBound() )
	{
		ThumbnailScaleSliderValue = FMath::Clamp<float>(ThumbnailScaleSliderValue.Get(), 0.0f, 1.0f);
	}

	MinThumbnailScale = 0.2f * ThumbnailScaleRangeScalar;
	MaxThumbnailScale = 2.0f * ThumbnailScaleRangeScalar;

	bCanShowClasses = InArgs._CanShowClasses;

	bCanShowFolders = InArgs._CanShowFolders;

	bFilterRecursivelyWithBackendFilter = InArgs._FilterRecursivelyWithBackendFilter;
		
	bCanShowRealTimeThumbnails = InArgs._CanShowRealTimeThumbnails;

	bCanShowDevelopersFolder = InArgs._CanShowDevelopersFolder;

	bCanShowCollections = InArgs._CanShowCollections;

	bPreloadAssetsForContextMenu = InArgs._PreloadAssetsForContextMenu;

	SelectionMode = InArgs._SelectionMode;

	bShowPathInColumnView = InArgs._ShowPathInColumnView;
	bShowTypeInColumnView = InArgs._ShowTypeInColumnView;
	bSortByPathInColumnView = bShowPathInColumnView & InArgs._SortByPathInColumnView;

	bPendingUpdateThumbnails = false;
	bShouldNotifyNextAssetSync = true;
	CurrentThumbnailSize = TileViewThumbnailSize;

	SourcesData = InArgs._InitialSourcesData;
	BackendFilter = InArgs._InitialBackendFilter;

	FrontendFilters = InArgs._FrontendFilters;
	if ( FrontendFilters.IsValid() )
	{
		FrontendFilters->OnChanged().AddSP( this, &SAssetView::OnFrontendFiltersChanged );
	}

	OnShouldFilterAsset = InArgs._OnShouldFilterAsset;
	OnAssetSelected = InArgs._OnAssetSelected;
	OnAssetsActivated = InArgs._OnAssetsActivated;
	OnGetAssetContextMenu = InArgs._OnGetAssetContextMenu;
	OnGetFolderContextMenu = InArgs._OnGetFolderContextMenu;
	OnGetPathContextMenuExtender = InArgs._OnGetPathContextMenuExtender;
	OnFindInAssetTreeRequested = InArgs._OnFindInAssetTreeRequested;
	OnAssetRenameCommitted = InArgs._OnAssetRenameCommitted;
	OnAssetTagWantsToBeDisplayed = InArgs._OnAssetTagWantsToBeDisplayed;
	OnGetCustomAssetToolTip = InArgs._OnGetCustomAssetToolTip;
	OnVisualizeAssetToolTip = InArgs._OnVisualizeAssetToolTip;
	OnAssetToolTipClosing = InArgs._OnAssetToolTipClosing;
	HighlightedText = InArgs._HighlightedText;
	ThumbnailLabel = InArgs._ThumbnailLabel;
	AllowThumbnailHintLabel = InArgs._AllowThumbnailHintLabel;
	AssetShowWarningText = InArgs._AssetShowWarningText;
	bAllowDragging = InArgs._AllowDragging;
	bAllowFocusOnSync = InArgs._AllowFocusOnSync;
	OnPathSelected = InArgs._OnPathSelected;
	HiddenColumnNames = DefaultHiddenColumnNames = InArgs._HiddenColumnNames;
	CustomColumns = InArgs._CustomColumns;

	if ( InArgs._InitialViewType >= 0 && InArgs._InitialViewType < EAssetViewType::MAX )
	{
		CurrentViewType = InArgs._InitialViewType;
	}
	else
	{
		CurrentViewType = EAssetViewType::Tile;
	}

	bPendingSortFilteredItems = false;
	bQuickFrontendListRefreshRequested = false;
	bSlowFullListRefreshRequested = false;
	LastSortTime = 0;
	SortDelaySeconds = 8;

	LastProcessAddsTime = 0;

	bBulkSelecting = false;
	bAllowThumbnailEditMode = InArgs._AllowThumbnailEditMode;
	bThumbnailEditMode = false;
	bUserSearching = false;
	bPendingFocusOnSync = false;
	bWereItemsRecursivelyFiltered = false;

	NumVisibleColumns = 0;

	FEditorWidgetsModule& EditorWidgetsModule = FModuleManager::LoadModuleChecked<FEditorWidgetsModule>("EditorWidgets");
	TSharedRef<SWidget> AssetDiscoveryIndicator = EditorWidgetsModule.CreateAssetDiscoveryIndicator(EAssetDiscoveryIndicatorScaleMode::Scale_Vertical);

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);

	ChildSlot
	[
		VerticalBox
	];

	// Assets area
	VerticalBox->AddSlot()
	.FillHeight(1.f)
	[
		SNew( SVerticalBox ) 

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew( SBox )
			.Visibility_Lambda([this] { return bIsWorking ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
			.HeightOverride( 2 )
			[
				SNew( SProgressBar )
				.Percent( this, &SAssetView::GetIsWorkingProgressBarState )
				.Style( FEditorStyle::Get(), "WorkingBar" )
				.BorderPadding( FVector2D(0,0) )
			]
		]
		
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				// Container for the view types
				SAssignNew(ViewContainer, SBorder)
				.Padding(0)
				.BorderImage(FEditorStyle::GetBrush("NoBorder"))
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.Padding(FMargin(0, 14, 0, 0))
			[
				// A warning to display when there are no assets to show
				SNew( STextBlock )
				.Justification( ETextJustify::Center )
				.Text( this, &SAssetView::GetAssetShowWarningText )
				.Visibility( this, &SAssetView::IsAssetShowWarningTextVisible )
				.AutoWrapText( true )
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(24, 0, 24, 0))
			[
				// Asset discovery indicator
				AssetDiscoveryIndicator
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(FMargin(8, 0))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("ErrorReporting.EmptyBox"))
				.BorderBackgroundColor(this, &SAssetView::GetQuickJumpColor)
				.Visibility(this, &SAssetView::IsQuickJumpVisible)
				[
					SNew(STextBlock)
					.Text(this, &SAssetView::GetQuickJumpTerm)
				]
			]
		]
	];

	// Thumbnail edit mode banner
	VerticalBox->AddSlot()
	.AutoHeight()
	.Padding(0, 4)
	[
		SNew(SBorder)
		.Visibility( this, &SAssetView::GetEditModeLabelVisibility )
		.BorderImage( FEditorStyle::GetBrush("ContentBrowser.EditModeLabelBorder") )
		.Content()
		[
			SNew( SHorizontalBox )

			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4, 0, 0, 0)
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ThumbnailEditModeLabel", "Editing Thumbnails. Drag a thumbnail to rotate it if there is a 3D environment."))
				.TextStyle( FEditorStyle::Get(), "ContentBrowser.EditModeLabelFont" )
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.Text( LOCTEXT("EndThumbnailEditModeButton", "Done Editing") )
				.OnClicked( this, &SAssetView::EndThumbnailEditModeClicked )
			]
		]
	];

	if (InArgs._ShowBottomToolbar)
	{
		// Bottom panel
		VerticalBox->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// Asset count
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(8, 0)
			[
				SNew(STextBlock)
				.Text(this, &SAssetView::GetAssetCountText)
			]

			// View mode combo button
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew( ViewOptionsComboButton, SComboButton )
				.ContentPadding(0)
				.ForegroundColor( this, &SAssetView::GetViewButtonForegroundColor )
				.ButtonStyle( FEditorStyle::Get(), "ToggleButton" ) // Use the tool bar item style for this button
				.OnGetMenuContent( this, &SAssetView::GetViewButtonContent )
				.ButtonContent()
				[
					SNew(SHorizontalBox)
 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SImage).Image( FEditorStyle::GetBrush("GenericViewButton") )
					]
 
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2, 0, 0, 0)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text( LOCTEXT("ViewButton", "View Options") )
					]
				]
			]
		];
	}

	CreateCurrentView();

	if( InArgs._InitialAssetSelection.IsValid() )
	{
		// sync to the initial item without notifying of selection
		bShouldNotifyNextAssetSync = false;
		TArray<FAssetData> AssetsToSync;
		AssetsToSync.Add( InArgs._InitialAssetSelection );
		SyncToAssets( AssetsToSync );
	}

	// If currently looking at column, and you could choose to sort by path in column first and then name
	// Generalizing this is a bit difficult because the column ID is not accessible or is not known
	// Currently I assume this won't work, if this view mode is not column. Otherwise, I don't think sorting by path
	// is a good idea. 
	if (CurrentViewType == EAssetViewType::Column && bSortByPathInColumnView)
	{
		SortManager.SetSortColumnId(EColumnSortPriority::Primary, SortManager.PathColumnId);
		SortManager.SetSortColumnId(EColumnSortPriority::Secondary, SortManager.NameColumnId);
		SortManager.SetSortMode(EColumnSortPriority::Primary, EColumnSortMode::Ascending);
		SortManager.SetSortMode(EColumnSortPriority::Secondary, EColumnSortMode::Ascending);
		SortList();
	}
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TOptional< float > SAssetView::GetIsWorkingProgressBarState() const
{
	return bIsWorking ? TOptional< float >() : 0.0f; 
}

void SAssetView::SetSourcesData(const FSourcesData& InSourcesData)
{
	// Update the path and collection lists
	SourcesData = InSourcesData;
	RequestSlowFullListRefresh();
	ClearSelection();
}

const FSourcesData& SAssetView::GetSourcesData() const
{
	return SourcesData;
}

bool SAssetView::IsAssetPathSelected() const
{
	int32 NumAssetPaths, NumClassPaths;
	ContentBrowserUtils::CountPathTypes(SourcesData.PackagePaths, NumAssetPaths, NumClassPaths);

	// Check that only asset paths are selected
	return NumAssetPaths > 0 && NumClassPaths == 0;
}

void SAssetView::SetBackendFilter(const FARFilter& InBackendFilter)
{
	// Update the path and collection lists
	BackendFilter = InBackendFilter;
	RequestSlowFullListRefresh();
}

void SAssetView::OnCreateNewFolder(const FString& FolderName, const FString& FolderPath)
{
	// we should only be creating one deferred folder per tick
	check(!DeferredFolderToCreate.IsValid());

	// Make sure we are showing the location of the new folder (we may have created it in a folder)
	OnPathSelected.Execute(FolderPath);

	DeferredFolderToCreate = MakeShareable(new FCreateDeferredFolderData());
	DeferredFolderToCreate->FolderName = FolderName;
	DeferredFolderToCreate->FolderPath = FolderPath;
}

void SAssetView::DeferredCreateNewFolder()
{
	if(DeferredFolderToCreate.IsValid())
	{
		TSharedPtr<FAssetViewFolder> NewItem = MakeShareable(new FAssetViewFolder(DeferredFolderToCreate->FolderPath / DeferredFolderToCreate->FolderName));
		NewItem->bNewFolder = true;
		NewItem->bRenameWhenScrolledIntoview = true;
		FilteredAssetItems.Insert( NewItem, 0 );

		SetSelection(NewItem);
		RequestScrollIntoView(NewItem);

		DeferredFolderToCreate.Reset();
	}
}

void SAssetView::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	if ( !ensure(AssetClass || Factory) )
	{
		return;
	}

	if ( AssetClass && Factory && !ensure(AssetClass->IsChildOf(Factory->GetSupportedClass())) )
	{
		return;
	}
	
	// we should only be creating one deferred asset per tick
	check(!DeferredAssetToCreate.IsValid());

	// Make sure we are showing the location of the new asset (we may have created it in a folder)
	OnPathSelected.Execute(PackagePath);

	// Defer asset creation until next tick, so we get a chance to refresh the view
	DeferredAssetToCreate = MakeShareable(new FCreateDeferredAssetData());
	DeferredAssetToCreate->DefaultAssetName = DefaultAssetName;
	DeferredAssetToCreate->PackagePath = PackagePath;
	DeferredAssetToCreate->AssetClass = AssetClass;
	DeferredAssetToCreate->Factory = Factory;
}

void SAssetView::DeferredCreateNewAsset()
{
	if(DeferredAssetToCreate.IsValid())
	{
		FString PackageNameStr = DeferredAssetToCreate->PackagePath + "/" + DeferredAssetToCreate->DefaultAssetName;
		FName PackageName = FName(*PackageNameStr);
		FName PackagePathFName = FName(*DeferredAssetToCreate->PackagePath);
		FName AssetName = FName(*DeferredAssetToCreate->DefaultAssetName);
		FName AssetClassName = DeferredAssetToCreate->AssetClass->GetFName();

		FAssetData NewAssetData(PackageName, PackagePathFName, AssetName, AssetClassName);
		TSharedPtr<FAssetViewItem> NewItem = MakeShareable(new FAssetViewCreation(NewAssetData, DeferredAssetToCreate->AssetClass, DeferredAssetToCreate->Factory));

		NewItem->bRenameWhenScrolledIntoview = true;
		FilteredAssetItems.Insert( NewItem, 0 );
		SortManager.SortList(FilteredAssetItems, MajorityAssetType, CustomColumns);

		SetSelection(NewItem);
		RequestScrollIntoView(NewItem);

		FEditorDelegates::OnNewAssetCreated.Broadcast(DeferredAssetToCreate->Factory);

		DeferredAssetToCreate.Reset();
	}
}

void SAssetView::DuplicateAsset(const FString& PackagePath, const TWeakObjectPtr<UObject>& OriginalObject)
{
	if ( !ensure(OriginalObject.IsValid()) )
	{
		return;
	}

	FString AssetNameStr;
	FString PackageNameStr;

	// Find a unique default name for the duplicated asset
	static FName AssetToolsModuleName = FName("AssetTools");
	FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>(AssetToolsModuleName);
	AssetToolsModule.Get().CreateUniqueAssetName(PackagePath + TEXT("/") + OriginalObject->GetName(), TEXT(""), PackageNameStr, AssetNameStr);

	FName PackageName = FName(*PackageNameStr);
	FName PackagePathFName = FName(*PackagePath);
	FName AssetName = FName(*AssetNameStr);
	FName AssetClass = OriginalObject->GetClass()->GetFName();
	
	FAssetData NewAssetData(PackageName, PackagePathFName, AssetName, AssetClass);
	TSharedPtr<FAssetViewItem> NewItem = MakeShareable(new FAssetViewDuplication(NewAssetData, OriginalObject));
	NewItem->bRenameWhenScrolledIntoview = true;

	// Insert into the list and sort
	FilteredAssetItems.Insert( NewItem, 0 );
	SortManager.SortList(FilteredAssetItems, MajorityAssetType, CustomColumns);

	SetSelection(NewItem);
	RequestScrollIntoView(NewItem);
}

void SAssetView::RenameAsset(const FAssetData& ItemToRename)
{
	for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item.IsValid() && Item->GetType() != EAssetItemType::Folder )	
		{
			const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
			if ( ItemAsAsset->Data.ObjectPath == ItemToRename.ObjectPath )
			{
				ItemAsAsset->bRenameWhenScrolledIntoview = true;

				SetSelection(Item);
				RequestScrollIntoView(Item);
				break;
			}
		}
	}
}

void SAssetView::RenameFolder(const FString& FolderToRename)
{
	for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item.IsValid() && Item->GetType() == EAssetItemType::Folder )	
		{
			const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
			if ( ItemAsFolder->FolderPath == FolderToRename )
			{
				ItemAsFolder->bRenameWhenScrolledIntoview = true;

				SetSelection(Item);
				RequestScrollIntoView(Item);
				break;
			}
		}
	}
}

void SAssetView::SyncToAssets( const TArray<FAssetData>& AssetDataList, const bool bFocusOnSync )
{
	PendingSyncItems.Reset();
	for (const FAssetData& AssetData : AssetDataList)
	{
		PendingSyncItems.SelectedAssets.Add(AssetData.ObjectPath);
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::SyncToFolders(const TArray<FString>& FolderList, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();
	PendingSyncItems.SelectedFolders = TSet<FString>(FolderList);

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::SyncTo(const FContentBrowserSelection& ItemSelection, const bool bFocusOnSync)
{
	PendingSyncItems.Reset();
	PendingSyncItems.SelectedFolders = TSet<FString>(ItemSelection.SelectedFolders);
	for (const FAssetData& AssetData : ItemSelection.SelectedAssets)
	{
		PendingSyncItems.SelectedAssets.Add(AssetData.ObjectPath);
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::SyncToSelection( const bool bFocusOnSync )
{
	PendingSyncItems.Reset();

	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	for (const TSharedPtr<FAssetViewItem>& Item : SelectedItems)
	{
		if (Item.IsValid())
		{
			if (Item->GetType() == EAssetItemType::Folder)
			{
				PendingSyncItems.SelectedFolders.Add(StaticCastSharedPtr<FAssetViewFolder>(Item)->FolderPath);
			}
			else
			{
				PendingSyncItems.SelectedAssets.Add(StaticCastSharedPtr<FAssetViewAsset>(Item)->Data.ObjectPath);
			}
		}
	}

	bPendingFocusOnSync = bFocusOnSync;
}

void SAssetView::ApplyHistoryData( const FHistoryData& History )
{
	SetSourcesData(History.SourcesData);
	PendingSyncItems = History.SelectionData;
	bPendingFocusOnSync = true;
}

TArray<TSharedPtr<FAssetViewItem>> SAssetView::GetSelectedItems() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: return ListView->GetSelectedItems();
		case EAssetViewType::Tile: return TileView->GetSelectedItems();
		case EAssetViewType::Column: return ColumnView->GetSelectedItems();
		default:
		ensure(0); // Unknown list type
		return TArray<TSharedPtr<FAssetViewItem>>();
	}
}

TArray<FAssetData> SAssetView::GetSelectedAssets() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	TArray<FAssetData> SelectedAssets;
	for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;

		// Only report non-temporary & non-folder items
		if ( Item.IsValid() && !Item->IsTemporaryItem() && Item->GetType() != EAssetItemType::Folder )	
		{
			SelectedAssets.Add(StaticCastSharedPtr<FAssetViewAsset>(Item)->Data);
		}
	}

	return SelectedAssets;
}

TArray<FString> SAssetView::GetSelectedFolders() const
{
	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	TArray<FString> SelectedFolders;
	for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item.IsValid() && Item->GetType() == EAssetItemType::Folder )	
		{
			SelectedFolders.Add(StaticCastSharedPtr<FAssetViewFolder>(Item)->FolderPath);
		}
	}

	return SelectedFolders;
}

void SAssetView::RequestSlowFullListRefresh()
{
	bSlowFullListRefreshRequested = true;
}

void SAssetView::RequestQuickFrontendListRefresh()
{
	bQuickFrontendListRefreshRequested = true;
}

void SAssetView::RequestAddNewAssetsNextFrame()
{
	LastProcessAddsTime = FPlatformTime::Seconds() - TimeBetweenAddingNewAssets;
}

FString SAssetView::GetThumbnailScaleSettingPath(const FString& SettingsString) const
{
	return SettingsString + TEXT(".ThumbnailSizeScale");
}

FString SAssetView::GetCurrentViewTypeSettingPath(const FString& SettingsString) const
{
	return SettingsString + TEXT(".CurrentViewType");
}

void SAssetView::SaveSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString) const
{
	GConfig->SetFloat(*IniSection, *GetThumbnailScaleSettingPath(SettingsString), ThumbnailScaleSliderValue.Get(), IniFilename);
	GConfig->SetInt(*IniSection, *GetCurrentViewTypeSettingPath(SettingsString), CurrentViewType, IniFilename);
	
	GConfig->SetArray(*IniSection, *(SettingsString + TEXT(".HiddenColumns")), HiddenColumnNames, IniFilename);
}

void SAssetView::LoadSettings(const FString& IniFilename, const FString& IniSection, const FString& SettingsString)
{
	float Scale = 0.f;
	if ( GConfig->GetFloat(*IniSection, *GetThumbnailScaleSettingPath(SettingsString), Scale, IniFilename) )
	{
		// Clamp value to normal range and update state
		Scale = FMath::Clamp<float>(Scale, 0.f, 1.f);
		SetThumbnailScale(Scale);
	}

	int32 ViewType = EAssetViewType::Tile;
	if ( GConfig->GetInt(*IniSection, *GetCurrentViewTypeSettingPath(SettingsString), ViewType, IniFilename) )
	{
		// Clamp value to normal range and update state
		if ( ViewType < 0 || ViewType >= EAssetViewType::MAX)
		{
			ViewType = EAssetViewType::Tile;
		}
		SetCurrentViewType( (EAssetViewType::Type)ViewType );
	}
	
	TArray<FString> LoadedHiddenColumnNames;
	GConfig->GetArray(*IniSection, *(SettingsString + TEXT(".HiddenColumns")), LoadedHiddenColumnNames, IniFilename);
	if (LoadedHiddenColumnNames.Num() > 0)
	{
		HiddenColumnNames = LoadedHiddenColumnNames;
	}
}

// Adjusts the selected asset by the selection delta, which should be +1 or -1)
void SAssetView::AdjustActiveSelection(int32 SelectionDelta)
{
	// Find the index of the first selected item
	TArray<TSharedPtr<FAssetViewItem>> SelectionSet = GetSelectedItems();
	
	int32 SelectedSuggestion = INDEX_NONE;

	if (SelectionSet.Num() > 0)
	{
		if (!FilteredAssetItems.Find(SelectionSet[0], /*out*/ SelectedSuggestion))
		{
			// Should never happen
			ensureMsgf(false, TEXT("SAssetView has a selected item that wasn't in the filtered list"));
			return;
		}
	}
	else
	{
		SelectedSuggestion = 0;
		SelectionDelta = 0;
	}

	if (FilteredAssetItems.Num() > 0)
	{
		// Move up or down one, wrapping around
		SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredAssetItems.Num()) % FilteredAssetItems.Num();

		// Pick the new asset
		const TSharedPtr<FAssetViewItem>& NewSelection = FilteredAssetItems[SelectedSuggestion];

		RequestScrollIntoView(NewSelection);
		SetSelection(NewSelection);
	}
	else
	{
		ClearSelection();
	}
}

void SAssetView::ProcessRecentlyLoadedOrChangedAssets()
{
	if ( RecentlyLoadedOrChangedAssets.Num() > 0 )
	{
		TMap< FName, TWeakObjectPtr<UObject> > NextRecentlyLoadedOrChangedMap = RecentlyLoadedOrChangedAssets;

		for (int32 AssetIdx = FilteredAssetItems.Num() - 1; AssetIdx >= 0; --AssetIdx)
		{
			if(FilteredAssetItems[AssetIdx]->GetType() != EAssetItemType::Folder)
			{
				const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[AssetIdx]);
				const FName ObjectPath = ItemAsAsset->Data.ObjectPath;
				const TWeakObjectPtr<UObject>* WeakAssetPtr = RecentlyLoadedOrChangedAssets.Find( ObjectPath );
				if ( WeakAssetPtr && (*WeakAssetPtr).IsValid() )
				{
					NextRecentlyLoadedOrChangedMap.Remove(ObjectPath);

					// Found the asset in the filtered items list, update it
					const UObject* Asset = (*WeakAssetPtr).Get();
					FAssetData AssetData(Asset);

					bool bShouldRemoveAsset = false;
					TArray<FAssetData> AssetDataThatPassesFilter;
					AssetDataThatPassesFilter.Add(AssetData);
					RunAssetsThroughBackendFilter(AssetDataThatPassesFilter);
					if ( AssetDataThatPassesFilter.Num() == 0 )
					{
						bShouldRemoveAsset = true;
					}

					if ( !bShouldRemoveAsset && OnShouldFilterAsset.IsBound() && OnShouldFilterAsset.Execute(AssetData) )
					{
						bShouldRemoveAsset = true;
					}

					if ( !bShouldRemoveAsset && (IsFrontendFilterActive() && !PassesCurrentFrontendFilter(AssetData)) )
					{
						bShouldRemoveAsset = true;
					}

					if ( bShouldRemoveAsset )
					{
						FilteredAssetItems.RemoveAt(AssetIdx);
					}
					else
					{
						// Update the asset data on the item
						ItemAsAsset->SetAssetData(AssetData);

						// Update the custom column data
						for (const FAssetViewCustomColumn& Column : CustomColumns)
						{
							if (ItemAsAsset->CustomColumnData.Find(Column.ColumnName))
							{
								ItemAsAsset->CustomColumnData.Add(Column.ColumnName, Column.OnGetColumnData.Execute(ItemAsAsset->Data, Column.ColumnName));
							}
						}
					}

					RefreshList();
				}
			}
		}

		if( FilteredRecentlyAddedAssets.Num() > 0 || RecentlyAddedAssets.Num() > 0 )
		{
			//Keep unprocessed items as we are still processing assets
			RecentlyLoadedOrChangedAssets = NextRecentlyLoadedOrChangedMap;
		}
		else
		{
			//No more assets coming in so if we haven't found them now we aren't going to
			RecentlyLoadedOrChangedAssets.Empty();
		}
	}
}

void SAssetView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	CalculateFillScale( AllottedGeometry );

	CurrentTime = InCurrentTime;

	// If there were any assets that were recently added via the asset registry, process them now
	ProcessRecentlyAddedAssets();

	// If there were any assets loaded since last frame that we are currently displaying thumbnails for, push them on the render stack now.
	ProcessRecentlyLoadedOrChangedAssets();

	CalculateThumbnailHintColorAndOpacity();

	if ( bPendingUpdateThumbnails )
	{
		UpdateThumbnails();
		bPendingUpdateThumbnails = false;
	}

	if (bSlowFullListRefreshRequested)
	{
		RefreshSourceItems();
		bSlowFullListRefreshRequested = false;
		bQuickFrontendListRefreshRequested = true;
	}

	if (QueriedAssetItems.Num() > 0)
	{
		check(OnShouldFilterAsset.IsBound());
		double TickStartTime = FPlatformTime::Seconds();

		// Mark the first amortize time
		if (AmortizeStartTime == 0)
		{
			AmortizeStartTime = FPlatformTime::Seconds();
			bIsWorking = true;
		}

		ProcessQueriedItems(TickStartTime);

		if (QueriedAssetItems.Num() == 0)
		{
			TotalAmortizeTime += FPlatformTime::Seconds() - AmortizeStartTime;
			AmortizeStartTime = 0;
			bIsWorking = false;
		}
		else
		{
			// Need to finish processing queried items before rest of function is safe
			return;
		}
	}

	if (bQuickFrontendListRefreshRequested)
	{
		ResetQuickJump();
		
		RefreshFilteredItems();
		RefreshFolders();
		// Don't sync to selection if we are just going to do it below
		SortList(!PendingSyncItems.Num());

		bQuickFrontendListRefreshRequested = false;
	}

	if ( PendingSyncItems.Num() > 0 )
	{
		if (bPendingSortFilteredItems)
		{
			// Don't sync to selection because we are just going to do it below
			SortList(/*bSyncToSelection=*/false);
		}
		
		bBulkSelecting = true;
		ClearSelection();
		bool bFoundScrollIntoViewTarget = false;

		for ( auto ItemIt = FilteredAssetItems.CreateConstIterator(); ItemIt; ++ItemIt )
		{
			const auto& Item = *ItemIt;
			if(Item.IsValid())
			{
				if(Item->GetType() == EAssetItemType::Folder)
				{
					const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
					if ( PendingSyncItems.SelectedFolders.Contains(ItemAsFolder->FolderPath) )
					{
						SetItemSelection(*ItemIt, true, ESelectInfo::OnNavigation);

						// Scroll the first item in the list that can be shown into view
						if ( !bFoundScrollIntoViewTarget )
						{
							RequestScrollIntoView(Item);
							bFoundScrollIntoViewTarget = true;
						}
					}
				}
				else
				{
					const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
					if ( PendingSyncItems.SelectedAssets.Contains(ItemAsAsset->Data.ObjectPath) )
					{
						SetItemSelection(*ItemIt, true, ESelectInfo::OnNavigation);

						// Scroll the first item in the list that can be shown into view
						if ( !bFoundScrollIntoViewTarget )
						{
							RequestScrollIntoView(Item);
							bFoundScrollIntoViewTarget = true;
						}
					}
				}
			}
		}
	
		bBulkSelecting = false;

		if (bShouldNotifyNextAssetSync && !bUserSearching)
		{
			AssetSelectionChanged(TSharedPtr<FAssetViewAsset>(), ESelectInfo::Direct);
		}

		// Default to always notifying
		bShouldNotifyNextAssetSync = true;

		PendingSyncItems.Reset();

		if (bAllowFocusOnSync && bPendingFocusOnSync)
		{
			FocusList();
		}
	}

	if ( IsHovered() )
	{
		// This prevents us from sorting the view immediately after the cursor leaves it
		LastSortTime = CurrentTime;
	}
	else if ( bPendingSortFilteredItems && InCurrentTime > LastSortTime + SortDelaySeconds )
	{
		SortList();
	}

	// create any assets & folders we need to now
	DeferredCreateNewAsset();
	DeferredCreateNewFolder();

	// Do quick-jump last as the Tick function might have canceled it
	if(QuickJumpData.bHasChangedSinceLastTick)
	{
		QuickJumpData.bHasChangedSinceLastTick = false;

		const bool bWasJumping = QuickJumpData.bIsJumping;
		QuickJumpData.bIsJumping = true;

		QuickJumpData.LastJumpTime = InCurrentTime;
		QuickJumpData.bHasValidMatch = PerformQuickJump(bWasJumping);
	}
	else if(QuickJumpData.bIsJumping && InCurrentTime > QuickJumpData.LastJumpTime + JumpDelaySeconds)
	{
		ResetQuickJump();
	}

	TSharedPtr<FAssetViewItem> AssetAwaitingRename = AwaitingRename.Pin();
	if (AssetAwaitingRename.IsValid())
	{
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if (!OwnerWindow.IsValid())
		{
			AssetAwaitingRename->bRenameWhenScrolledIntoview = false;
			AwaitingRename = nullptr;
		}
		else if (OwnerWindow->HasAnyUserFocusOrFocusedDescendants())
		{
			AssetAwaitingRename->RenamedRequestEvent.ExecuteIfBound();
			AssetAwaitingRename->bRenameWhenScrolledIntoview = false;
			AwaitingRename = nullptr;
		}
	}
}

void SAssetView::CalculateFillScale( const FGeometry& AllottedGeometry )
{
	if ( bFillEmptySpaceInTileView && CurrentViewType == EAssetViewType::Tile )
	{
		float ItemWidth = GetTileViewItemBaseWidth();

		// Scrollbars are 16, but we add 1 to deal with half pixels.
		const float ScrollbarWidth = 16 + 1;
		float TotalWidth = AllottedGeometry.GetLocalSize().X - ( ScrollbarWidth / AllottedGeometry.Scale );
		float Coverage = TotalWidth / ItemWidth;
		int32 Items = (int)( TotalWidth / ItemWidth );

		// If there isn't enough room to support even a single item, don't apply a fill scale.
		if ( Items > 0 )
		{
			float GapSpace = ItemWidth * ( Coverage - Items );
			float ExpandAmount = GapSpace / (float)Items;
			FillScale = ( ItemWidth + ExpandAmount ) / ItemWidth;
			FillScale = FMath::Max( 1.0f, FillScale );
		}
		else
		{
			FillScale = 1.0f;
		}
	}
	else
	{
		FillScale = 1.0f;
	}
}

void SAssetView::CalculateThumbnailHintColorAndOpacity()
{
	if ( HighlightedText.Get().IsEmpty() )
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsForward() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtEnd() ) 
		{
			ThumbnailHintFadeInSequence.PlayReverse(this->AsShared());
		}
	}
	else 
	{
		if ( ThumbnailHintFadeInSequence.IsPlaying() )
		{
			if ( ThumbnailHintFadeInSequence.IsInReverse() )
			{
				ThumbnailHintFadeInSequence.Reverse();
			}
		}
		else if ( ThumbnailHintFadeInSequence.IsAtStart() ) 
		{
			ThumbnailHintFadeInSequence.Play(this->AsShared());
		}
	}

	const float Opacity = ThumbnailHintFadeInSequence.GetLerp();
	ThumbnailHintColorAndOpacity = FLinearColor( 1.0, 1.0, 1.0, Opacity );
}

void SAssetView::ProcessQueriedItems( const double TickStartTime )
{
	const bool bFlushFullBuffer = TickStartTime < 0;

	bool ListNeedsRefresh = false;
	int32 AssetIndex = 0;
	for ( AssetIndex = QueriedAssetItems.Num() - 1; AssetIndex >= 0 ; AssetIndex--)
	{
		if ( !OnShouldFilterAsset.Execute( QueriedAssetItems[AssetIndex] ) )
		{
			AssetItems.Add( QueriedAssetItems[AssetIndex] );

			if ( !IsFrontendFilterActive() || PassesCurrentFrontendFilter(QueriedAssetItems[AssetIndex]))
			{
				const FAssetData& AssetData = QueriedAssetItems[AssetIndex];
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				ListNeedsRefresh = true;
				bPendingSortFilteredItems = true;
			}
		}

		// Check to see if we have run out of time in this tick
		if ( !bFlushFullBuffer && (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
		{
			break;
		}
	}

	// Trim the results array
	if (AssetIndex > 0)
	{
		QueriedAssetItems.RemoveAt( AssetIndex, QueriedAssetItems.Num() - AssetIndex );
	}
	else
	{
		QueriedAssetItems.Empty();
	}

	if ( ListNeedsRefresh )
	{
		RefreshList();
	}
}

void SAssetView::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr< FAssetDragDropOp > AssetDragDropOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
	if( AssetDragDropOp.IsValid() )
	{
		AssetDragDropOp->ResetToDefaultToolTip();
		return;
	}

	TSharedPtr<FDragDropOperation> DragDropOp = DragDropEvent.GetOperation();
	if (DragDropOp.IsValid())
	{
		// Do we have a custom handler for this drag event?
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		const TArray<FAssetViewDragAndDropExtender>& AssetViewDragAndDropExtenders = ContentBrowserModule.GetAssetViewDragAndDropExtenders();
		for (const auto& AssetViewDragAndDropExtender : AssetViewDragAndDropExtenders)
		{
			if (AssetViewDragAndDropExtender.OnDragLeaveDelegate.IsBound() && AssetViewDragAndDropExtender.OnDragLeaveDelegate.Execute(FAssetViewDragAndDropExtender::FPayload(DragDropOp, SourcesData.PackagePaths, SourcesData.Collections)))
			{
				return;
			}
		}
	}
}

FReply SAssetView::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> DragDropOp = DragDropEvent.GetOperation();
	if (DragDropOp.IsValid())
	{
		// Do we have a custom handler for this drag event?
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		const TArray<FAssetViewDragAndDropExtender>& AssetViewDragAndDropExtenders = ContentBrowserModule.GetAssetViewDragAndDropExtenders();
		for (const auto& AssetViewDragAndDropExtender : AssetViewDragAndDropExtenders)
		{
			if (AssetViewDragAndDropExtender.OnDragOverDelegate.IsBound() && AssetViewDragAndDropExtender.OnDragOverDelegate.Execute(FAssetViewDragAndDropExtender::FPayload(DragDropOp, SourcesData.PackagePaths, SourcesData.Collections)))
			{
				return FReply::Handled();
			}
		}
	}

	if (SourcesData.HasPackagePaths())
	{
		// Note: We don't test IsAssetPathSelected here as we need to prevent dropping assets on class paths
		const FString DestPath = SourcesData.PackagePaths[0].ToString();

		bool bUnused = false;
		DragDropHandler::ValidateDragDropOnAssetFolder(MyGeometry, DragDropEvent, DestPath, bUnused);
		return FReply::Handled();
	}
	else if (HasSingleCollectionSource())
	{
		TArray< FAssetData > AssetDatas = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);

		if (AssetDatas.Num() > 0)
		{
			TSharedPtr<FAssetDragDropOp> AssetDragDropOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
			if (AssetDragDropOp.IsValid())
			{
				TArray< FName > ObjectPaths;
				FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
				const FCollectionNameType& Collection = SourcesData.Collections[0];
				CollectionManagerModule.Get().GetObjectsInCollection(Collection.Name, Collection.Type, ObjectPaths);

				bool IsValidDrop = false;
				for (const auto& AssetData : AssetDatas)
				{
					if (AssetData.GetClass()->IsChildOf(UClass::StaticClass()))
					{
						continue;
					}

					if (!ObjectPaths.Contains(AssetData.ObjectPath))
					{
						IsValidDrop = true;
						break;
					}
				}

				if (IsValidDrop)
				{
					AssetDragDropOp->SetToolTip(NSLOCTEXT("AssetView", "OnDragOverCollection", "Add to Collection"), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
				}
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> DragDropOp = DragDropEvent.GetOperation();
	if (DragDropOp.IsValid())
	{
		// Do we have a custom handler for this drag event?
		FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>("ContentBrowser");
		const TArray<FAssetViewDragAndDropExtender>& AssetViewDragAndDropExtenders = ContentBrowserModule.GetAssetViewDragAndDropExtenders();
		for (const auto& AssetViewDragAndDropExtender : AssetViewDragAndDropExtenders)
		{
			if (AssetViewDragAndDropExtender.OnDropDelegate.IsBound() && AssetViewDragAndDropExtender.OnDropDelegate.Execute(FAssetViewDragAndDropExtender::FPayload(DragDropOp, SourcesData.PackagePaths, SourcesData.Collections)))
			{
				return FReply::Handled();
			}
		}
	}

	if (SourcesData.HasPackagePaths())
	{
		// Note: We don't test IsAssetPathSelected here as we need to prevent dropping assets on class paths
		const FString DestPath = SourcesData.PackagePaths[0].ToString();

		bool bUnused = false;
		if (DragDropHandler::ValidateDragDropOnAssetFolder(MyGeometry, DragDropEvent, DestPath, bUnused))
		{
			// Handle drag drop for import
			TSharedPtr<FExternalDragOperation> ExternalDragDropOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
			if (ExternalDragDropOp.IsValid())
			{
				if (ExternalDragDropOp->HasFiles())
				{
					TArray<FString> ImportFiles;
					TMap<FString, UObject*> ReimportFiles;
					FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
					FString RootDestinationPath = SourcesData.PackagePaths[0].ToString();
					TArray<TPair<FString, FString>> FilesAndDestinations;
					const TArray<FString>& DragFiles = ExternalDragDropOp->GetFiles();
					AssetToolsModule.Get().ExpandDirectories(DragFiles, RootDestinationPath, FilesAndDestinations);

					TArray<int32> ReImportIndexes;
					for (int32 FileIdx = 0; FileIdx < FilesAndDestinations.Num(); ++FileIdx)
					{
						const FString& Filename = FilesAndDestinations[FileIdx].Key;
						const FString& DestinationPath = FilesAndDestinations[FileIdx].Value;
						FString Name = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(Filename));
						FString PackageName = DestinationPath + TEXT("/") + Name;

						// We can not create assets that share the name of a map file in the same location
						if (FEditorFileUtils::IsMapPackageAsset(PackageName))
						{
							//The error message will be log in the import process
							ImportFiles.Add(Filename);
							continue;
						}
						//Check if package exist in memory
						UPackage* Pkg = FindPackage(nullptr, *PackageName);
						bool IsPkgExist = Pkg != nullptr;
						//check if package exist on file
						if (!IsPkgExist && !FPackageName::DoesPackageExist(PackageName))
						{
							ImportFiles.Add(Filename);
							continue;
						}
						if (Pkg == nullptr)
						{
							Pkg = CreatePackage(nullptr, *PackageName);
							if (Pkg == nullptr)
							{
								//Cannot create a package that don't exist on disk or in memory!!!
								//The error message will be log in the import process
								ImportFiles.Add(Filename);
								continue;
							}
						}
						// Make sure the destination package is loaded
						Pkg->FullyLoad();

						// Check for an existing object
						UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), Pkg, *Name);
						if (ExistingObject != nullptr)
						{
							ReimportFiles.Add(Filename, ExistingObject);
							ReImportIndexes.Add(FileIdx);
						}
						else
						{
							ImportFiles.Add(Filename);
						}
					}
					//Reimport
					for (auto kvp : ReimportFiles)
					{
						FReimportManager::Instance()->Reimport(kvp.Value, false, true, kvp.Key);
					}
					//Import
					if (ImportFiles.Num() > 0)
					{
						//Remove it in reverse so the smaller index are still valid
						for (int32 IndexToRemove = ReImportIndexes.Num() - 1; IndexToRemove >= 0; --IndexToRemove)
						{
							FilesAndDestinations.RemoveAt(ReImportIndexes[IndexToRemove]);
						}
						AssetToolsModule.Get().ImportAssets(ImportFiles, SourcesData.PackagePaths[0].ToString(), nullptr, true, &FilesAndDestinations);
					}
				}
			}

			TSharedPtr<FAssetDragDropOp> AssetDragDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
			if (AssetDragDropOp.IsValid())
			{
				OnAssetsOrPathsDragDropped(AssetDragDropOp->GetAssets(), AssetDragDropOp->GetAssetPaths(), DestPath);
			}
		}
		return FReply::Handled();
	}
	else if (HasSingleCollectionSource())
	{
		TArray<FAssetData> SelectedAssetDatas = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);

		if (SelectedAssetDatas.Num() > 0)
		{
			TArray<FName> ObjectPaths;
			for (const auto& AssetData : SelectedAssetDatas)
			{
				if (!AssetData.GetClass()->IsChildOf(UClass::StaticClass()))
				{
					ObjectPaths.Add(AssetData.ObjectPath);
				}
			}

			if (ObjectPaths.Num() > 0)
			{
				FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
				const FCollectionNameType& Collection = SourcesData.Collections[0];
				CollectionManagerModule.Get().AddToCollection(Collection.Name, Collection.Type, ObjectPaths);
			}

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnKeyChar( const FGeometry& MyGeometry,const FCharacterEvent& InCharacterEvent )
{
	const bool bIsControlOrCommandDown = InCharacterEvent.IsControlDown() || InCharacterEvent.IsCommandDown();
	
	const bool bTestOnly = false;
	if(HandleQuickJumpKeyDown(InCharacterEvent.GetCharacter(), bIsControlOrCommandDown, InCharacterEvent.IsAltDown(), bTestOnly).IsEventHandled())
	{
		return FReply::Handled();
	}

	// If the user pressed a key we couldn't handle, reset the quick-jump search
	ResetQuickJump();

	return FReply::Unhandled();
}

static bool IsValidObjectPath(const FString& Path)
{
	int32 NameStartIndex = INDEX_NONE;
	Path.FindChar(TCHAR('\''), NameStartIndex);
	if (NameStartIndex != INDEX_NONE)
	{
		int32 NameEndIndex = INDEX_NONE;
		Path.FindLastChar(TCHAR('\''), NameEndIndex);
		if (NameEndIndex > NameStartIndex)
		{
			const FString ClassName = Path.Left(NameStartIndex);
			const FString PathName = Path.Mid(NameStartIndex + 1, NameEndIndex - NameStartIndex - 1);

			UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassName, true);
			if (Class)
			{
				return FPackageName::IsValidLongPackageName(FPackageName::ObjectPathToPackageName(PathName));
			}
		}
	}

	return false;
}

static bool ContainsT3D(const FString& ClipboardText)
{
	return (ClipboardText.StartsWith(TEXT("Begin Object")) && ClipboardText.EndsWith(TEXT("End Object")))
		|| (ClipboardText.StartsWith(TEXT("Begin Map")) && ClipboardText.EndsWith(TEXT("End Map")));
}

FReply SAssetView::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	const bool bIsControlOrCommandDown = InKeyEvent.IsControlDown() || InKeyEvent.IsCommandDown();
	
	if (bIsControlOrCommandDown && InKeyEvent.GetCharacter() == 'V' && IsAssetPathSelected())
	{
		FString AssetPaths;
		TArray<FString> AssetPathsSplit;

		// Get the copied asset paths
		FPlatformApplicationMisc::ClipboardPaste(AssetPaths);

		// Make sure the clipboard does not contain T3D
		AssetPaths.TrimEndInline();
		if (!ContainsT3D(AssetPaths))
		{
			AssetPaths.ParseIntoArrayLines(AssetPathsSplit);

			// Get assets and copy them
			TArray<UObject*> AssetsToCopy;
			for (const FString& AssetPath : AssetPathsSplit)
			{
				// Validate string
				if (IsValidObjectPath(AssetPath))
				{
					UObject* ObjectToCopy = LoadObject<UObject>(nullptr, *AssetPath);
					if (ObjectToCopy && !ObjectToCopy->IsA(UClass::StaticClass()))
					{
						AssetsToCopy.Add(ObjectToCopy);
					}
				}
			}

			if (AssetsToCopy.Num())
			{
				ContentBrowserUtils::CopyAssets(AssetsToCopy, SourcesData.PackagePaths[0].ToString());
			}
		}

		return FReply::Handled();
	}
	// Swallow the key-presses used by the quick-jump in OnKeyChar to avoid other things (such as the viewport commands) getting them instead
	// eg) Pressing "W" without this would set the viewport to "translate" mode
	else if(HandleQuickJumpKeyDown(InKeyEvent.GetCharacter(), bIsControlOrCommandDown, InKeyEvent.IsAltDown(), /*bTestOnly*/true).IsEventHandled())
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SAssetView::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseEvent.IsControlDown() )
	{
		const float DesiredScale = FMath::Clamp<float>(GetThumbnailScale() + ( MouseEvent.GetWheelDelta() * 0.05f ), 0.0f, 1.0f);
		if ( DesiredScale != GetThumbnailScale() )
		{
			SetThumbnailScale( DesiredScale );
		}		
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SAssetView::OnFocusChanging( const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	ResetQuickJump();
}

TSharedRef<SAssetTileView> SAssetView::CreateTileView()
{
	return SNew(SAssetTileView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateTile(this, &SAssetView::MakeTileViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SAssetView::GetTileViewItemHeight)
		.ItemWidth(this, &SAssetView::GetTileViewItemWidth);
}

TSharedRef<SAssetListView> SAssetView::CreateListView()
{
	return SNew(SAssetListView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateRow(this, &SAssetView::MakeListViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.ItemHeight(this, &SAssetView::GetListViewItemHeight);
}

TSharedRef<SAssetColumnView> SAssetView::CreateColumnView()
{
	TSharedPtr<SAssetColumnView> NewColumnView = SNew(SAssetColumnView)
		.SelectionMode( SelectionMode )
		.ListItemsSource(&FilteredAssetItems)
		.OnGenerateRow(this, &SAssetView::MakeColumnViewWidget)
		.OnItemScrolledIntoView(this, &SAssetView::ItemScrolledIntoView)
		.OnContextMenuOpening(this, &SAssetView::OnGetContextMenuContent)
		.OnMouseButtonDoubleClick(this, &SAssetView::OnListMouseButtonDoubleClick)
		.OnSelectionChanged(this, &SAssetView::AssetSelectionChanged)
		.Visibility(this, &SAssetView::GetColumnViewVisibility)
		.HeaderRow
		(
			SNew(SHeaderRow)
			.ResizeMode(ESplitterResizeMode::FixedSize)
			+ SHeaderRow::Column(SortManager.NameColumnId)
			.FillWidth(300)
			.SortMode( TAttribute< EColumnSortMode::Type >::Create( TAttribute< EColumnSortMode::Type >::FGetter::CreateSP( this, &SAssetView::GetColumnSortMode, SortManager.NameColumnId ) ) )
			.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.NameColumnId)))
			.OnSort( FOnSortModeChanged::CreateSP( this, &SAssetView::OnSortColumnHeader ) )
			.DefaultLabel( LOCTEXT("Column_Name", "Name") )
			.ShouldGenerateWidget(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SAssetView::ShouldColumnGenerateWidget, SortManager.NameColumnId.ToString())))
			.MenuContent()
			[
				CreateRowHeaderMenuContent(SortManager.NameColumnId.ToString())
			]
		);

	NewColumnView->GetHeaderRow()->SetOnGetMaxRowSizeForColumn(FOnGetMaxRowSizeForColumn::CreateRaw(NewColumnView.Get(), &SAssetColumnView::GetMaxRowSizeForColumn));


	NumVisibleColumns = HiddenColumnNames.Contains(SortManager.NameColumnId.ToString()) ? 0 : 1;

	if(bShowTypeInColumnView)
	{
		NewColumnView->GetHeaderRow()->AddColumn(
				SHeaderRow::Column(SortManager.ClassColumnId)
				.FillWidth(160)
				.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, SortManager.ClassColumnId)))
				.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.ClassColumnId)))
				.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
				.DefaultLabel(LOCTEXT("Column_Class", "Type"))
				.ShouldGenerateWidget(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SAssetView::ShouldColumnGenerateWidget, SortManager.ClassColumnId.ToString())))
				.MenuContent()
				[
					CreateRowHeaderMenuContent(SortManager.ClassColumnId.ToString())
				]
			);

		NumVisibleColumns += HiddenColumnNames.Contains(SortManager.ClassColumnId.ToString()) ? 0 : 1;
	}


	if (bShowPathInColumnView)
	{
		NewColumnView->GetHeaderRow()->AddColumn(
				SHeaderRow::Column(SortManager.PathColumnId)
				.FillWidth(160)
				.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, SortManager.PathColumnId)))
				.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, SortManager.PathColumnId)))
				.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
				.DefaultLabel(LOCTEXT("Column_Path", "Path"))
				.ShouldGenerateWidget(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SAssetView::ShouldColumnGenerateWidget, SortManager.PathColumnId.ToString())))
				.MenuContent()
				[
					CreateRowHeaderMenuContent(SortManager.PathColumnId.ToString())
				]
			);


		NumVisibleColumns += HiddenColumnNames.Contains(SortManager.PathColumnId.ToString()) ? 0 : 1;
	}

	return NewColumnView.ToSharedRef();
}

bool SAssetView::IsValidSearchToken(const FString& Token) const
{
	if ( Token.Len() == 0 )
	{
		return false;
	}

	// A token may not be only apostrophe only, or it will match every asset because the text filter compares against the pattern Class'ObjectPath'
	if ( Token.Len() == 1 && Token[0] == '\'' )
	{
		return false;
	}

	return true;
}

void SAssetView::RefreshSourceItems()
{
	// Load the asset registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	RecentlyLoadedOrChangedAssets.Empty();
	RecentlyAddedAssets.Empty();
	FilteredRecentlyAddedAssets.Empty();
	QueriedAssetItems.Empty();
	AssetItems.Empty();
	FilteredAssetItems.Empty();
	VisibleItems.Empty();
	RelevantThumbnails.Empty();
	Folders.Empty();

	TArray<FAssetData>& Items = OnShouldFilterAsset.IsBound() ? QueriedAssetItems : AssetItems;

	const bool bShowAll = SourcesData.IsEmpty() && BackendFilter.IsEmpty();

	bool bShowClasses = false;
	TArray<FName> ClassPathsToShow;

	if ( bShowAll )
	{
		AssetRegistryModule.Get().GetAllAssets(Items);
		bShowClasses = IsShowingCppContent();
		bWereItemsRecursivelyFiltered = true;
	}
	else
	{
		// Assemble the filter using the current sources
		// force recursion when the user is searching
		const bool bRecurse = ShouldFilterRecursively();
		const bool bUsingFolders = IsShowingFolders();
		const bool bIsDynamicCollection = SourcesData.IsDynamicCollection();
		FARFilter Filter = SourcesData.MakeFilter(bRecurse, bUsingFolders);

		// Add the backend filters from the filter list
		Filter.Append(BackendFilter);

		bWereItemsRecursivelyFiltered = bRecurse;

		// Move any class paths into their own array
		Filter.PackagePaths.RemoveAll([&ClassPathsToShow](const FName& PackagePath) -> bool
		{
			if(ContentBrowserUtils::IsClassPath(PackagePath.ToString()))
			{
				ClassPathsToShow.Add(PackagePath);
				return true;
			}
			return false;
		});

		// Only show classes if we have class paths, and the filter allows classes to be shown
		const bool bFilterAllowsClasses = IsShowingCppContent() && (Filter.ClassNames.Num() == 0 || Filter.ClassNames.Contains(NAME_Class));
		bShowClasses = (ClassPathsToShow.Num() > 0 || bIsDynamicCollection) && bFilterAllowsClasses;

		if ( SourcesData.HasCollections() && Filter.ObjectPaths.Num() == 0 && !bIsDynamicCollection )
		{
			// This is an empty collection, no asset will pass the check
		}
		else if ( ClassPathsToShow.Num() > 0 && Filter.PackagePaths.Num() == 0 )
		{
			// Only class paths are selected, no asset will pass the check
		}
		else
		{
			// Add assets found in the asset registry
			AssetRegistryModule.Get().GetAssets(Filter, Items);
		}

		if ( bFilterAllowsClasses )
		{
			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();

			// Include objects from child collections if we're recursing
			const ECollectionRecursionFlags::Flags CollectionRecursionMode = (Filter.bRecursivePaths) ? ECollectionRecursionFlags::SelfAndChildren : ECollectionRecursionFlags::Self;

			TArray< FName > ClassPaths;
			for (const FCollectionNameType& Collection : SourcesData.Collections)
			{
				CollectionManagerModule.Get().GetClassesInCollection( Collection.Name, Collection.Type, ClassPaths, CollectionRecursionMode );
			}

			for (const FName& ClassPath : ClassPaths)
			{
				UClass* Class = FindObject<UClass>(ANY_PACKAGE, *ClassPath.ToString());

				if ( Class != NULL )
				{
					Items.Add( Class );
				}
			}
		}
	}

	// If we are showing classes in the asset list...
	if (bShowClasses)
	{
		// Load the native class hierarchy
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();

		FNativeClassHierarchyFilter ClassFilter;
		ClassFilter.ClassPaths = ClassPathsToShow;
		ClassFilter.bRecursivePaths = ShouldFilterRecursively() || !IsShowingFolders() || !ClassPathsToShow.Num();

		// Find all the classes that match the current criteria
		TArray<UClass*> MatchingClasses;
		NativeClassHierarchy->GetMatchingClasses(ClassFilter, MatchingClasses);
		for(UClass* CurrentClass : MatchingClasses)
		{
			Items.Add(FAssetData(CurrentClass));
		}
	}

	// Remove any assets that should be filtered out any redirectors and non-assets
	const bool bDisplayEngine = IsShowingEngineContent();
	const bool bDisplayPlugins = IsShowingPluginContent();
	const bool bDisplayL10N = IsShowingLocalizedContent();
	for (int32 AssetIdx = Items.Num() - 1; AssetIdx >= 0; --AssetIdx)
	{
		const FAssetData& Item = Items[AssetIdx];
		// Do not show redirectors if they are not the main asset in the uasset file.
		const bool IsMainlyARedirector = Item.AssetClass == UObjectRedirector::StaticClass()->GetFName() && !Item.IsUAsset();
		// If this is an engine folder, and we don't want to show them, remove
		const bool IsHiddenEngineFolder = !bDisplayEngine && ContentBrowserUtils::IsEngineFolder(Item.PackagePath.ToString());
		// If this is a plugin folder, and we don't want to show them, remove
		const bool IsAHiddenGameProjectPluginFolder = !bDisplayPlugins && ContentBrowserUtils::IsPluginFolder(Item.PackagePath.ToString(), EPluginLoadedFrom::Project);
		// If this is an engine plugin folder, and we don't want to show them, remove
		const bool IsAHiddenEnginePluginFolder = (!bDisplayEngine || !bDisplayPlugins) && ContentBrowserUtils::IsPluginFolder(Item.PackagePath.ToString(), EPluginLoadedFrom::Engine);
		// Do not show localized content folders.
		const bool IsTheHiddenLocalizedContentFolder = !bDisplayL10N && ContentBrowserUtils::IsLocalizationFolder(Item.PackagePath.ToString());

		const bool ShouldFilterOut = IsMainlyARedirector || IsHiddenEngineFolder || IsAHiddenGameProjectPluginFolder || IsAHiddenEnginePluginFolder || IsTheHiddenLocalizedContentFolder;
		if (ShouldFilterOut)
		{
			Items.RemoveAtSwap(AssetIdx);
		}
	}
}

bool SAssetView::ShouldFilterRecursively() const
{
	// Quick check for conditions which force recursive filtering
	if (bUserSearching)
	{
		return true;
	}

	// In some cases we want to not filter recursively even if we have a backend filter (e.g. the open level window)
	// Most of the time, bFilterRecursivelyWithBackendFilter is true
	if ( bFilterRecursivelyWithBackendFilter && !BackendFilter.IsEmpty() )
	{
		return true;
	}

	// Otherwise, check if there are any non-inverse frontend filters selected
	if (FrontendFilters.IsValid())
	{
		for (int32 FilterIndex = 0; FilterIndex < FrontendFilters->Num(); ++FilterIndex)
		{
			const auto* Filter = static_cast<FFrontendFilter*>(FrontendFilters->GetFilterAtIndex(FilterIndex).Get());
			if (Filter)
			{
				if (!Filter->IsInverseFilter())
				{
					return true;
				}
			}
		}
	}

	// No filters, do not override folder view with recursive filtering
	return false;
}

void SAssetView::RefreshFilteredItems()
{
	//Build up a map of the existing AssetItems so we can preserve them while filtering
	TMap< FName, TSharedPtr< FAssetViewAsset > > ItemToObjectPath;
	for (int Index = 0; Index < FilteredAssetItems.Num(); Index++)
	{
		if(FilteredAssetItems[Index].IsValid() && FilteredAssetItems[Index]->GetType() != EAssetItemType::Folder)
		{
			TSharedPtr<FAssetViewAsset> Item = StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[Index]);

			// Clear custom column data
			Item->CustomColumnData.Reset();

			ItemToObjectPath.Add( Item->Data.ObjectPath, Item );
		}
	}

	// Empty all the filtered lists
	FilteredAssetItems.Empty();
	VisibleItems.Empty();
	RelevantThumbnails.Empty();
	Folders.Empty();

	// true if the results from the asset registry query are filtered further by the content browser
	const bool bIsFrontendFilterActive = IsFrontendFilterActive();

	// true if we are looking at columns so we need to determine the majority asset type
	const bool bGatherAssetTypeCount = CurrentViewType == EAssetViewType::Column;
	TMap<FName, int32> AssetTypeCount;

	if ( bIsFrontendFilterActive && FrontendFilters.IsValid() )
	{
		const bool bRecurse = ShouldFilterRecursively();
		const bool bUsingFolders = IsShowingFolders();
		FARFilter CombinedFilter = SourcesData.MakeFilter(bRecurse, bUsingFolders);
		CombinedFilter.Append(BackendFilter);

		// Let the frontend filters know the currently used filter in case it is necessary to conditionally filter based on path or class filters
		for ( int32 FilterIdx = 0; FilterIdx < FrontendFilters->Num(); ++FilterIdx )
		{
			// There are only FFrontendFilters in this collection
			const TSharedPtr<FFrontendFilter>& Filter = StaticCastSharedPtr<FFrontendFilter>( FrontendFilters->GetFilterAtIndex(FilterIdx) );
			if ( Filter.IsValid() )
			{
				Filter->SetCurrentFilter(CombinedFilter);
			}
		}
	}

	if ( bIsFrontendFilterActive && bGatherAssetTypeCount )
	{
		// Check the frontend filter for every asset and keep track of how many assets were found of each type
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			if ( PassesCurrentFrontendFilter(AssetData) )
			{
				const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

				if ( AssetItem != NULL )
				{
					FilteredAssetItems.Add(*AssetItem);
				}
				else
				{
					FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				}

				int32* TypeCount = AssetTypeCount.Find(AssetData.AssetClass);
				if ( TypeCount )
				{
					(*TypeCount)++;
				}
				else
				{
					AssetTypeCount.Add(AssetData.AssetClass, 1);
				}
			}
		}
	}
	else if ( bIsFrontendFilterActive && !bGatherAssetTypeCount )
	{
		// Check the frontend filter for every asset and don't worry about asset type counts
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			if ( PassesCurrentFrontendFilter(AssetData) )
			{
				const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

				if ( AssetItem != NULL )
				{
					FilteredAssetItems.Add(*AssetItem);
				}
				else
				{
					FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
				}
			}
		}
	}
	else if ( !bIsFrontendFilterActive && bGatherAssetTypeCount )
	{
		// Don't need to check the frontend filter for every asset but keep track of how many assets were found of each type
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

			if ( AssetItem != NULL )
			{
				FilteredAssetItems.Add(*AssetItem);
			}
			else
			{
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
			}

			int32* TypeCount = AssetTypeCount.Find(AssetData.AssetClass);
			if ( TypeCount )
			{
				(*TypeCount)++;
			}
			else
			{
				AssetTypeCount.Add(AssetData.AssetClass, 1);
			}
		}
	}
	else if ( !bIsFrontendFilterActive && !bGatherAssetTypeCount )
	{
		// Don't check the frontend filter and don't count the number of assets of each type. Just add all assets.
		for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
		{
			const FAssetData& AssetData = AssetItems[AssetIdx];
			const TSharedPtr< FAssetViewAsset >* AssetItem = ItemToObjectPath.Find( AssetData.ObjectPath );

			if ( AssetItem != NULL )
			{
				FilteredAssetItems.Add(*AssetItem);
			}
			else
			{
				FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
			}
		}
	}
	else
	{
		// The above cases should handle all combinations of bIsFrontendFilterActive and bGatherAssetTypeCount
		ensure(0);
	}

	if ( bGatherAssetTypeCount )
	{
		int32 HighestCount = 0;
		FName HighestType;
		for ( auto TypeIt = AssetTypeCount.CreateConstIterator(); TypeIt; ++TypeIt )
		{
			if ( TypeIt.Value() > HighestCount )
			{
				HighestType = TypeIt.Key();
				HighestCount = TypeIt.Value();
			}
		}

		SetMajorityAssetType(HighestType);
	}
}

void SAssetView::RefreshFolders()
{
	if(!IsShowingFolders() || ShouldFilterRecursively())
	{
		return;
	}
	
	// Split the selected paths into asset and class paths
	TArray<FName> AssetPathsToShow;
	TArray<FName> ClassPathsToShow;
	for(const FName& PackagePath : SourcesData.PackagePaths)
	{
		if(ContentBrowserUtils::IsClassPath(PackagePath.ToString()))
		{
			ClassPathsToShow.Add(PackagePath);
		}
		else
		{
			AssetPathsToShow.Add(PackagePath);
		}
	}

	TArray<FString> FoldersToAdd;

	TSharedRef<FEmptyFolderVisibilityManager> EmptyFolderVisibilityManager = FContentBrowserSingleton::Get().GetEmptyFolderVisibilityManager();

	const bool bDisplayEmpty = IsShowingEmptyFolders();
	const bool bDisplayDev = IsShowingDevelopersContent();
	const bool bDisplayL10N = IsShowingLocalizedContent();
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	{
		TArray<FString> SubPaths;
		for(const FName& PackagePath : AssetPathsToShow)
		{
			SubPaths.Reset();
			AssetRegistryModule.Get().GetSubPaths(PackagePath.ToString(), SubPaths, false);

			for(const FString& SubPath : SubPaths)
			{
				if (!bDisplayEmpty && !EmptyFolderVisibilityManager->ShouldShowPath(SubPath))
				{
					continue;
				}

				if (!bDisplayDev && ContentBrowserUtils::IsDevelopersFolder(SubPath))
				{
					continue;
				}

				if (!bDisplayL10N && ContentBrowserUtils::IsLocalizationFolder(SubPath))
				{
					continue;
				}

				if(!Folders.Contains(SubPath))
				{
					FoldersToAdd.Add(SubPath);
				}
			}
		}
	}

	// If we are showing classes in the asset list then we need to show their folders too
	if(IsShowingCppContent() && ClassPathsToShow.Num() > 0)
	{
		// Load the native class hierarchy
		TSharedRef<FNativeClassHierarchy> NativeClassHierarchy = FContentBrowserSingleton::Get().GetNativeClassHierarchy();

		FNativeClassHierarchyFilter ClassFilter;
		ClassFilter.ClassPaths = ClassPathsToShow;
		ClassFilter.bRecursivePaths = false;

		// Find all the classes that match the current criteria
		TArray<FString> MatchingFolders;
		NativeClassHierarchy->GetMatchingFolders(ClassFilter, MatchingFolders);
		FoldersToAdd.Append(MatchingFolders);
	}

	// Add folders for any child collections of the currently selected collections
	if(SourcesData.HasCollections())
	{
		FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
		
		TArray<FCollectionNameType> ChildCollections;
		for(const FCollectionNameType& Collection : SourcesData.Collections)
		{
			ChildCollections.Reset();
			CollectionManagerModule.Get().GetChildCollections(Collection.Name, Collection.Type, ChildCollections);

			for(const FCollectionNameType& ChildCollection : ChildCollections)
			{
				// Use "Collections" as the root of the path to avoid this being confused with other asset view folders - see ContentBrowserUtils::IsCollectionPath
				FoldersToAdd.Add(FString::Printf(TEXT("/Collections/%s/%s"), ECollectionShareType::ToString(ChildCollection.Type), *ChildCollection.Name.ToString()));
			}
		}
	}

	if(FoldersToAdd.Num() > 0)
	{
		for(const FString& FolderPath : FoldersToAdd)
		{
			FilteredAssetItems.Add(MakeShareable(new FAssetViewFolder(FolderPath)));
			Folders.Add(FolderPath);
		}

		RefreshList();
		bPendingSortFilteredItems = true;
	}
}

void SAssetView::SetMajorityAssetType(FName NewMajorityAssetType)
{
	auto IsFixedColumn = [this](FName InColumnId)
	{
		const bool bIsFixedNameColumn = InColumnId == SortManager.NameColumnId;
		const bool bIsFixedClassColumn = bShowTypeInColumnView && InColumnId == SortManager.ClassColumnId;
		const bool bIsFixedPathColumn = bShowPathInColumnView && InColumnId == SortManager.PathColumnId;
		return bIsFixedNameColumn || bIsFixedClassColumn || bIsFixedPathColumn;
	};

	if ( NewMajorityAssetType != MajorityAssetType )
	{
		UE_LOG(LogContentBrowser, Verbose, TEXT("The majority of assets in the view are of type: %s"), *NewMajorityAssetType.ToString());

		MajorityAssetType = NewMajorityAssetType;

		TArray<FName> AddedColumns;

		// Since the asset type has changed, remove all columns except name and class
		const TIndirectArray<SHeaderRow::FColumn>& Columns = ColumnView->GetHeaderRow()->GetColumns();

		for ( int32 ColumnIdx = Columns.Num() - 1; ColumnIdx >= 0; --ColumnIdx )
		{
			const FName ColumnId = Columns[ColumnIdx].ColumnId;

			if ( ColumnId != NAME_None && !IsFixedColumn(ColumnId) )
			{
				ColumnView->GetHeaderRow()->RemoveColumn(ColumnId);
			}
		}

		// Keep track of the current column name to see if we need to change it now that columns are being removed
		// Name, Class, and Path are always relevant
		struct FSortOrder
		{
			bool bSortRelevant;
			FName SortColumn;
			FSortOrder(bool bInSortRelevant, const FName& InSortColumn) : bSortRelevant(bInSortRelevant), SortColumn(InSortColumn) {}
		};
		TArray<FSortOrder> CurrentSortOrder;
		for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
		{
			const FName SortColumn = SortManager.GetSortColumnId(static_cast<EColumnSortPriority::Type>(PriorityIdx));
			if (SortColumn != NAME_None)
			{
				const bool bSortRelevant = SortColumn == FAssetViewSortManager::NameColumnId
					|| SortColumn == FAssetViewSortManager::ClassColumnId
					|| SortColumn == FAssetViewSortManager::PathColumnId;
				CurrentSortOrder.Add(FSortOrder(bSortRelevant, SortColumn));
			}
		}

		// Add custom columns
		for (const FAssetViewCustomColumn& Column : CustomColumns)
		{
			FName TagName = Column.ColumnName;

			if (AddedColumns.Contains(TagName))
			{
				continue;
			}
			AddedColumns.Add(TagName);

			ColumnView->GetHeaderRow()->AddColumn(
				SHeaderRow::Column(TagName)
				.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, TagName)))
				.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, TagName)))
				.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
				.DefaultLabel(Column.DisplayName)
				.DefaultTooltip(Column.TooltipText)
				.FillWidth(180)
				.ShouldGenerateWidget(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SAssetView::ShouldColumnGenerateWidget, TagName.ToString())))
				.MenuContent()
				[
					CreateRowHeaderMenuContent(TagName.ToString())
				]);

			NumVisibleColumns += HiddenColumnNames.Contains(TagName.ToString()) ? 0 : 1;

			// If we found a tag the matches the column we are currently sorting on, there will be no need to change the column
			for (int32 SortIdx = 0; SortIdx < CurrentSortOrder.Num(); SortIdx++)
			{
				if (TagName == CurrentSortOrder[SortIdx].SortColumn)
				{
					CurrentSortOrder[SortIdx].bSortRelevant = true;
				}
			}
		}

		// If we have a new majority type, add the new type's columns
		if ( NewMajorityAssetType != NAME_None )
		{
			// Determine the columns by querying the CDO for the tag map
			UClass* TypeClass = FindObject<UClass>(ANY_PACKAGE, *NewMajorityAssetType.ToString());
			if ( TypeClass )
			{
				UObject* CDO = TypeClass->GetDefaultObject();
				if ( CDO )
				{
					TArray<UObject::FAssetRegistryTag> AssetRegistryTags;
					CDO->GetAssetRegistryTags(AssetRegistryTags);

					// Add a column for every tag that isn't hidden or using a reserved name
					for ( auto TagIt = AssetRegistryTags.CreateConstIterator(); TagIt; ++TagIt )
					{
						if ( TagIt->Type != UObject::FAssetRegistryTag::TT_Hidden )
						{
							const FName TagName = TagIt->Name;

							if (IsFixedColumn(TagName))
							{
								// Reserved name
								continue;
							}

							if ( !OnAssetTagWantsToBeDisplayed.IsBound() || OnAssetTagWantsToBeDisplayed.Execute(NewMajorityAssetType, TagName) )
							{
								if (AddedColumns.Contains(TagName))
								{
									continue;
								}
								AddedColumns.Add(TagName);

								// Get tag metadata
								TMap<FName, UObject::FAssetRegistryTagMetadata> MetadataMap;
								CDO->GetAssetRegistryTagMetadata(MetadataMap);
								const UObject::FAssetRegistryTagMetadata* Metadata = MetadataMap.Find(TagName);

								FText DisplayName;
								if (Metadata != nullptr && !Metadata->DisplayName.IsEmpty())
								{
									DisplayName = Metadata->DisplayName;
								}
								else
								{
									DisplayName = FText::FromName(TagName);
								}

								FText TooltipText;
								if (Metadata != nullptr && !Metadata->TooltipText.IsEmpty())
								{
									TooltipText = Metadata->TooltipText;
								}
								else
								{
									// If the tag name corresponds to a property name, use the property tooltip
									UProperty* Property = FindField<UProperty>(TypeClass, TagName);
									TooltipText = (Property != nullptr) ? Property->GetToolTipText() : FText::FromString(FName::NameToDisplayString(TagName.ToString(), false));
								}

								ColumnView->GetHeaderRow()->AddColumn(
									SHeaderRow::Column(TagName)
									.SortMode(TAttribute< EColumnSortMode::Type >::Create(TAttribute< EColumnSortMode::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortMode, TagName)))
									.SortPriority(TAttribute< EColumnSortPriority::Type >::Create(TAttribute< EColumnSortPriority::Type >::FGetter::CreateSP(this, &SAssetView::GetColumnSortPriority, TagName)))
									.OnSort(FOnSortModeChanged::CreateSP(this, &SAssetView::OnSortColumnHeader))
									.DefaultLabel(DisplayName)
									.DefaultTooltip(TooltipText)
									.FillWidth(180)
									.ShouldGenerateWidget(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SAssetView::ShouldColumnGenerateWidget, TagName.ToString())))
									.MenuContent()
									[
										CreateRowHeaderMenuContent(TagName.ToString())
									]);								
								
								NumVisibleColumns += HiddenColumnNames.Contains(TagName.ToString()) ? 0 : 1;

								// If we found a tag the matches the column we are currently sorting on, there will be no need to change the column
								for (int32 SortIdx = 0; SortIdx < CurrentSortOrder.Num(); SortIdx++)
								{
									if (TagName == CurrentSortOrder[SortIdx].SortColumn)
									{
										CurrentSortOrder[SortIdx].bSortRelevant = true;
									}
								}
							}
						}
					}
				}			
			}	
		}

		// Are any of the sort columns irrelevant now, if so remove them from the list
		bool CurrentSortChanged = false;
		for (int32 SortIdx = CurrentSortOrder.Num() - 1; SortIdx >= 0; SortIdx--)
		{
			if (!CurrentSortOrder[SortIdx].bSortRelevant)
			{
				CurrentSortOrder.RemoveAt(SortIdx);
				CurrentSortChanged = true;
			}
		}
		if (CurrentSortOrder.Num() > 0 && CurrentSortChanged)
		{
			// Sort order has changed, update the columns keeping those that are relevant
			int32 PriorityNum = EColumnSortPriority::Primary;
			for (int32 SortIdx = 0; SortIdx < CurrentSortOrder.Num(); SortIdx++)
			{
				check(CurrentSortOrder[SortIdx].bSortRelevant);
				if (!SortManager.SetOrToggleSortColumn(static_cast<EColumnSortPriority::Type>(PriorityNum), CurrentSortOrder[SortIdx].SortColumn))
				{
					// Toggle twice so mode is preserved if this isn't a new column assignation
					SortManager.SetOrToggleSortColumn(static_cast<EColumnSortPriority::Type>(PriorityNum), CurrentSortOrder[SortIdx].SortColumn);
				}				
				bPendingSortFilteredItems = true;
				PriorityNum++;
			}
		}
		else if (CurrentSortOrder.Num() == 0)
		{
			// If the current sort column is no longer relevant, revert to "Name" and resort when convenient
			SortManager.ResetSort();
			bPendingSortFilteredItems = true;
		}
	}
}

void SAssetView::OnAssetsAddedToCollection( const FCollectionNameType& Collection, const TArray< FName >& ObjectPaths )
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	for (int Index = 0; Index < ObjectPaths.Num(); Index++)
	{
		OnAssetAdded( AssetRegistryModule.Get().GetAssetByObjectPath( ObjectPaths[Index] ) );
	}
}

void SAssetView::OnAssetAdded(const FAssetData& AssetData)
{
	RecentlyAddedAssets.Add(AssetData);
}

void SAssetView::ProcessRecentlyAddedAssets()
{
	if (
		(RecentlyAddedAssets.Num() > 2048) ||
		(RecentlyAddedAssets.Num() > 0 && FPlatformTime::Seconds() - LastProcessAddsTime >= TimeBetweenAddingNewAssets)
		)
	{
		RunAssetsThroughBackendFilter(RecentlyAddedAssets);
		FilteredRecentlyAddedAssets.Append(RecentlyAddedAssets);
		RecentlyAddedAssets.Empty();
		LastProcessAddsTime = FPlatformTime::Seconds();
	}

	if (FilteredRecentlyAddedAssets.Num() > 0)
	{
		double TickStartTime = FPlatformTime::Seconds();
		bool bNeedsRefresh = false;

		TSet<FName> ExistingObjectPaths;
		for ( auto AssetIt = AssetItems.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			ExistingObjectPaths.Add((*AssetIt).ObjectPath);
		}

		for ( auto AssetIt = QueriedAssetItems.CreateConstIterator(); AssetIt; ++AssetIt )
		{
			ExistingObjectPaths.Add((*AssetIt).ObjectPath);
		}

		int32 AssetIdx = 0;
		for ( ; AssetIdx < FilteredRecentlyAddedAssets.Num(); ++AssetIdx )
		{
			const FAssetData& AssetData = FilteredRecentlyAddedAssets[AssetIdx];
			if ( !ExistingObjectPaths.Contains(AssetData.ObjectPath) )
			{
				if ( AssetData.AssetClass != UObjectRedirector::StaticClass()->GetFName() || AssetData.IsUAsset() )
				{
					if ( !OnShouldFilterAsset.IsBound() || !OnShouldFilterAsset.Execute(AssetData) )
					{
						// Add the asset to the list
						int32 AddedAssetIdx = AssetItems.Add(AssetData);
						ExistingObjectPaths.Add(AssetData.ObjectPath);
						if (!IsFrontendFilterActive() || PassesCurrentFrontendFilter(AssetData))
						{
							FilteredAssetItems.Add(MakeShareable(new FAssetViewAsset(AssetData)));
							bNeedsRefresh = true;
							bPendingSortFilteredItems = true;
						}
					}
				}
			}

			if ( (FPlatformTime::Seconds() - TickStartTime) > MaxSecondsPerFrame)
			{
				// Increment the index to properly trim the buffer below
				++AssetIdx;
				break;
			}
		}

		// Trim the results array
		if (AssetIdx > 0)
		{
			FilteredRecentlyAddedAssets.RemoveAt(0, AssetIdx);
		}

		if (bNeedsRefresh)
		{
			RefreshList();
		}
	}
}

void SAssetView::OnAssetsRemovedFromCollection( const FCollectionNameType& Collection, const TArray< FName >& ObjectPaths )
{
	if ( !SourcesData.Collections.Contains( Collection ) )
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	for (int Index = 0; Index < ObjectPaths.Num(); Index++)
	{
		OnAssetRemoved( AssetRegistryModule.Get().GetAssetByObjectPath( ObjectPaths[Index] ) );
	}
}

void SAssetView::OnAssetRemoved(const FAssetData& AssetData)
{
	RemoveAssetByPath( AssetData.ObjectPath );
	RecentlyAddedAssets.RemoveSingleSwap(AssetData);
}

void SAssetView::OnAssetRegistryPathAdded(const FString& Path)
{
	if(IsShowingFolders() && !ShouldFilterRecursively())
	{
		TSharedRef<FEmptyFolderVisibilityManager> EmptyFolderVisibilityManager = FContentBrowserSingleton::Get().GetEmptyFolderVisibilityManager();

		// If this isn't a developer folder or we want to show them, continue
		const bool bDisplayEmpty = IsShowingEmptyFolders();
		const bool bDisplayDev = IsShowingDevelopersContent();
		const bool bDisplayL10N = IsShowingLocalizedContent();
		if ((bDisplayEmpty || EmptyFolderVisibilityManager->ShouldShowPath(Path)) &&  
			(bDisplayDev || !ContentBrowserUtils::IsDevelopersFolder(Path)) && 
			(bDisplayL10N || !ContentBrowserUtils::IsLocalizationFolder(Path))
			)
		{
			for (const FName& SourcePathName : SourcesData.PackagePaths)
			{
				const FString SourcePath = SourcePathName.ToString();
				if(Path.StartsWith(SourcePath))
				{
					const FString SubPath = Path.RightChop(SourcePath.Len());
					
					TArray<FString> SubPathItemList;
					SubPath.ParseIntoArray(SubPathItemList, TEXT("/"), /*InCullEmpty=*/true);

					if(SubPathItemList.Num() > 0)
					{
						const FString NewSubFolder = SourcePath / SubPathItemList[0];
						if(!Folders.Contains(NewSubFolder))
						{
							FilteredAssetItems.Add(MakeShareable(new FAssetViewFolder(NewSubFolder)));
							RefreshList();
							Folders.Add(NewSubFolder);
							bPendingSortFilteredItems = true;
						}
					}
				}
			}
		}
	}
}

void SAssetView::OnAssetRegistryPathRemoved(const FString& Path)
{
	FString* Folder = Folders.Find(Path);
	if(Folder != NULL)
	{
		Folders.Remove(Path);

		for (int32 AssetIdx = 0; AssetIdx < FilteredAssetItems.Num(); ++AssetIdx)
		{
			if(FilteredAssetItems[AssetIdx]->GetType() == EAssetItemType::Folder)
			{
				if ( StaticCastSharedPtr<FAssetViewFolder>(FilteredAssetItems[AssetIdx])->FolderPath == Path )
				{
					// Found the folder in the filtered items list, remove it
					FilteredAssetItems.RemoveAt(AssetIdx);
					RefreshList();
					break;
				}
			}
		}
	}
}

void SAssetView::OnFolderPopulated(const FString& Path)
{
	OnAssetRegistryPathAdded(Path);
}

void SAssetView::RemoveAssetByPath( const FName& ObjectPath )
{
	bool bFoundAsset = false;
	for (int32 AssetIdx = 0; AssetIdx < AssetItems.Num(); ++AssetIdx)
	{
		if ( AssetItems[AssetIdx].ObjectPath == ObjectPath )
		{
			// Found the asset in the cached list, remove it
			AssetItems.RemoveAt(AssetIdx);
			bFoundAsset = true;
			break;
		}
	}

	if ( bFoundAsset )
	{
		// If it was in the AssetItems list, see if it is also in the FilteredAssetItems list
		for (int32 AssetIdx = 0; AssetIdx < FilteredAssetItems.Num(); ++AssetIdx)
		{
			if(FilteredAssetItems[AssetIdx].IsValid() && FilteredAssetItems[AssetIdx]->GetType() != EAssetItemType::Folder)
			{
				if ( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[AssetIdx])->Data.ObjectPath == ObjectPath && !FilteredAssetItems[AssetIdx]->IsTemporaryItem() )
				{
					// Found the asset in the filtered items list, remove it
					FilteredAssetItems.RemoveAt(AssetIdx);
					RefreshList();
					break;
				}
			}
		}
	}
	else
	{
		//Make sure we don't have the item still queued up for processing
		for (int32 AssetIdx = 0; AssetIdx < QueriedAssetItems.Num(); ++AssetIdx)
		{
			if ( QueriedAssetItems[AssetIdx].ObjectPath == ObjectPath )
			{
				// Found the asset in the cached list, remove it
				QueriedAssetItems.RemoveAt(AssetIdx);
				bFoundAsset = true;
				break;
			}
		}
	}
}

void SAssetView::OnCollectionRenamed( const FCollectionNameType& OriginalCollection, const FCollectionNameType& NewCollection )
{
	int32 FoundIndex = INDEX_NONE;
	if ( SourcesData.Collections.Find( OriginalCollection, FoundIndex ) )
	{
		SourcesData.Collections[ FoundIndex ] = NewCollection;
	}
}

void SAssetView::OnCollectionUpdated( const FCollectionNameType& Collection )
{
	// A collection has changed in some way, so we need to refresh our backend list
	RequestSlowFullListRefresh();
}

void SAssetView::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	// Remove the old asset, if it exists
	RemoveAssetByPath( FName( *OldObjectPath ) );

	// Add the new asset, if it should be in the cached list
	OnAssetAdded( AssetData );

	// Force an update of the recently added asset next frame
	RequestAddNewAssetsNextFrame();
}

void SAssetView::OnAssetLoaded(UObject* Asset)
{
	if ( Asset != NULL )
	{
		RecentlyLoadedOrChangedAssets.Add( FName(*Asset->GetPathName()), Asset );
	}
}

void SAssetView::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Object != nullptr && Object->IsAsset())
	{
		RecentlyLoadedOrChangedAssets.Add( FName(*Object->GetPathName()), Object);
	}
}

void SAssetView::OnClassHierarchyUpdated()
{
	// The class hierarchy has changed in some way, so we need to refresh our backend list
	RequestSlowFullListRefresh();
}

void SAssetView::OnFrontendFiltersChanged()
{
	RequestQuickFrontendListRefresh();

	// If we're not operating on recursively filtered data, we need to ensure a full slow
	// refresh is performed.
	if ( ShouldFilterRecursively() && !bWereItemsRecursivelyFiltered )
	{
		RequestSlowFullListRefresh();
	}
}

bool SAssetView::IsFrontendFilterActive() const
{
	return ( FrontendFilters.IsValid() && FrontendFilters->Num() > 0 );
}

bool SAssetView::PassesCurrentFrontendFilter(const FAssetData& Item) const
{
	// Check the frontend filters list
	if ( FrontendFilters.IsValid() && !FrontendFilters->PassesAllFilters(Item) )
	{
		return false;
	}

	return true;
}

void SAssetView::RunAssetsThroughBackendFilter(TArray<FAssetData>& InOutAssetDataList) const
{
	const bool bRecurse = ShouldFilterRecursively();
	const bool bUsingFolders = IsShowingFolders();
	const bool bIsDynamicCollection = SourcesData.IsDynamicCollection();
	FARFilter Filter = SourcesData.MakeFilter(bRecurse, bUsingFolders);
	
	if ( SourcesData.HasCollections() && Filter.ObjectPaths.Num() == 0 && !bIsDynamicCollection )
	{
		// This is an empty collection, no asset will pass the check
		InOutAssetDataList.Empty();
	}
	else
	{
		// Actually append the backend filter
		Filter.Append(BackendFilter);

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().RunAssetsThroughFilter(InOutAssetDataList, Filter);

		if ( SourcesData.HasCollections() && !bIsDynamicCollection )
		{
			// Include objects from child collections if we're recursing
			const ECollectionRecursionFlags::Flags CollectionRecursionMode = (Filter.bRecursivePaths) ? ECollectionRecursionFlags::SelfAndChildren : ECollectionRecursionFlags::Self;

			FCollectionManagerModule& CollectionManagerModule = FCollectionManagerModule::GetModule();
			TArray< FName > CollectionObjectPaths;
			for (const FCollectionNameType& Collection : SourcesData.Collections)
			{
				CollectionManagerModule.Get().GetObjectsInCollection(Collection.Name, Collection.Type, CollectionObjectPaths, CollectionRecursionMode);
			}

			for ( int32 AssetDataIdx = InOutAssetDataList.Num() - 1; AssetDataIdx >= 0; --AssetDataIdx )
			{
				const FAssetData& AssetData = InOutAssetDataList[AssetDataIdx];

				if ( !CollectionObjectPaths.Contains( AssetData.ObjectPath ) )
				{
					InOutAssetDataList.RemoveAtSwap(AssetDataIdx);
				}
			}
		}
	}
}

void SAssetView::SortList(bool bSyncToSelection)
{
	if ( !IsRenamingAsset() )
	{
		SortManager.SortList(FilteredAssetItems, MajorityAssetType, CustomColumns);

		// Update the thumbnails we were using since the order has changed
		bPendingUpdateThumbnails = true;

		if ( bSyncToSelection )
		{
			// Make sure the selection is in view
			const bool bFocusOnSync = false;
			SyncToSelection(bFocusOnSync);
		}

		RefreshList();
		bPendingSortFilteredItems = false;
		LastSortTime = CurrentTime;
	}
	else
	{
		bPendingSortFilteredItems = true;
	}
}

FLinearColor SAssetView::GetThumbnailHintColorAndOpacity() const
{
	//We update this color in tick instead of here as an optimization
	return ThumbnailHintColorAndOpacity;
}

FSlateColor SAssetView::GetViewButtonForegroundColor() const
{
	static const FName InvertedForegroundName("InvertedForeground");
	static const FName DefaultForegroundName("DefaultForeground");

	return ViewOptionsComboButton->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
}

TSharedRef<SWidget> SAssetView::GetViewButtonContent()
{
	// Get all menu extenders for this context menu from the content browser module
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>( TEXT("ContentBrowser") );
	TArray<FContentBrowserMenuExtender> MenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewViewMenuExtenders();

	TArray<TSharedPtr<FExtender>> Extenders;
	for (int32 i = 0; i < MenuExtenderDelegates.Num(); ++i)
	{
		if (MenuExtenderDelegates[i].IsBound())
		{
			Extenders.Add(MenuExtenderDelegates[i].Execute());
		}
	}
	TSharedPtr<FExtender> MenuExtender = FExtender::Combine(Extenders);

	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, NULL, MenuExtender, /*bCloseSelfOnly=*/ true);

	MenuBuilder.BeginSection("AssetViewType", LOCTEXT("ViewTypeHeading", "View Type"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("TileViewOption", "Tiles"),
			LOCTEXT("TileViewOptionToolTip", "View assets as tiles in a grid."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewType, EAssetViewType::Tile ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::Tile )
				),
			NAME_None,
			EUserInterfaceActionType::RadioButton
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ListViewOption", "List"),
			LOCTEXT("ListViewOptionToolTip", "View assets in a list with thumbnails."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewType, EAssetViewType::List ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::List )
				),
			NAME_None,
			EUserInterfaceActionType::RadioButton
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ColumnViewOption", "Columns"),
			LOCTEXT("ColumnViewOptionToolTip", "View assets in a list with columns of details."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::SetCurrentViewType, EAssetViewType::Column ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsCurrentViewType, EAssetViewType::Column )
				),
			NAME_None,
			EUserInterfaceActionType::RadioButton
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("View", LOCTEXT("ViewHeading", "View"));
	{
		auto CreateShowFoldersSubMenu = [this](FMenuBuilder& SubMenuBuilder)
		{
			SubMenuBuilder.AddMenuEntry(
				LOCTEXT("ShowEmptyFoldersOption", "Show Empty Folders"),
				LOCTEXT("ShowEmptyFoldersOptionToolTip", "Show empty folders in the view as well as assets?"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP( this, &SAssetView::ToggleShowEmptyFolders ),
					FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowEmptyFoldersAllowed ),
					FIsActionChecked::CreateSP( this, &SAssetView::IsShowingEmptyFolders )
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		};

		MenuBuilder.AddSubMenu(
			LOCTEXT("ShowFoldersOption", "Show Folders"),
			LOCTEXT("ShowFoldersOptionToolTip", "Show folders in the view as well as assets?"),
			FNewMenuDelegate::CreateLambda(CreateShowFoldersSubMenu),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowFolders ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowFoldersAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingFolders )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowCollectionOption", "Show Collections"),
			LOCTEXT("ShowCollectionOptionToolTip", "Show the collections list in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowCollections ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowCollectionsAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingCollections )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Content", LOCTEXT("ContentHeading", "Content"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowCppClassesOption", "Show C++ Classes"),
			LOCTEXT("ShowCppClassesOptionToolTip", "Show C++ classes in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowCppContent ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowCppContentAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingCppContent )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowDevelopersContentOption", "Show Developers Content"),
			LOCTEXT("ShowDevelopersContentOptionToolTip", "Show developers content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowDevelopersContent ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsToggleShowDevelopersContentAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingDevelopersContent )
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowEngineFolderOption", "Show Engine Content"),
			LOCTEXT("ShowEngineFolderOptionToolTip", "Show engine content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowEngineContent ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingEngineContent )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowPluginFolderOption", "Show Plugin Content"),
			LOCTEXT("ShowPluginFolderOptionToolTip", "Show plugin content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleShowPluginContent ),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingPluginContent )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ShowLocalizedContentOption", "Show Localized Content"),
			LOCTEXT("ShowLocalizedContentOptionToolTip", "Show localized content in the view?"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SAssetView::ToggleShowLocalizedContent),
				FCanExecuteAction::CreateSP(this, &SAssetView::IsToggleShowLocalizedContentAllowed),
				FIsActionChecked::CreateSP(this, &SAssetView::IsShowingLocalizedContent)
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("AssetThumbnails", LOCTEXT("ThumbnailsHeading", "Thumbnails"));
	{
		MenuBuilder.AddWidget(
			SNew(SSlider)
				.ToolTipText( LOCTEXT("ThumbnailScaleToolTip", "Adjust the size of thumbnails.") )
				.Value( this, &SAssetView::GetThumbnailScale )
				.OnValueChanged( this, &SAssetView::SetThumbnailScale )
				.Locked( this, &SAssetView::IsThumbnailScalingLocked ),
			LOCTEXT("ThumbnailScaleLabel", "Scale"),
			/*bNoIndent=*/true
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("ThumbnailEditModeOption", "Thumbnail Edit Mode"),
			LOCTEXT("ThumbnailEditModeOptionToolTip", "Toggle thumbnail editing mode. When in this mode you can rotate the camera on 3D thumbnails by dragging them."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleThumbnailEditMode ),
				FCanExecuteAction::CreateSP( this, &SAssetView::IsThumbnailEditModeAllowed ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsThumbnailEditMode )
				),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RealTimeThumbnailsOption", "Real-Time Thumbnails"),
			LOCTEXT("RealTimeThumbnailsOptionToolTip", "Renders the assets thumbnails in real-time"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP( this, &SAssetView::ToggleRealTimeThumbnails ),
				FCanExecuteAction::CreateSP( this, &SAssetView::CanShowRealTimeThumbnails ),
				FIsActionChecked::CreateSP( this, &SAssetView::IsShowingRealTimeThumbnails )
			),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
			);
	}
	MenuBuilder.EndSection();

	if (GetColumnViewVisibility() == EVisibility::Visible)
	{
		MenuBuilder.BeginSection("AssetColumns", LOCTEXT("ToggleColumnsHeading", "Columns"));
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("ToggleColumnsMenu", "Toggle columns"),
				LOCTEXT("ToggleColumnsMenuTooltip", "Show or hide specific columns."),
				FNewMenuDelegate::CreateSP(this, &SAssetView::FillToggleColumnsMenu),
				false,
				FSlateIcon(),
				false
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ResetColumns", "Reset Columns"),
				LOCTEXT("ResetColumnsToolTip", "Reset all columns to be visible again."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SAssetView::ResetColumns)),
				NAME_None,
				EUserInterfaceActionType::Button
				);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ExportColumns", "Export to CSV"),
				LOCTEXT("ExportColumnsToolTip", "Export column data to CSV."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SAssetView::ExportColumns)),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}

void SAssetView::ToggleShowFolders()
{
	check( IsToggleShowFoldersAllowed() );
	GetMutableDefault<UContentBrowserSettings>()->DisplayFolders = !GetDefault<UContentBrowserSettings>()->DisplayFolders;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowFoldersAllowed() const
{
	return bCanShowFolders;
}

bool SAssetView::IsShowingFolders() const
{
	return IsToggleShowFoldersAllowed() && GetDefault<UContentBrowserSettings>()->DisplayFolders;
}

void SAssetView::ToggleShowEmptyFolders()
{
	check( IsToggleShowEmptyFoldersAllowed() );
	GetMutableDefault<UContentBrowserSettings>()->DisplayEmptyFolders = !GetDefault<UContentBrowserSettings>()->DisplayEmptyFolders;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowEmptyFoldersAllowed() const
{
	return bCanShowFolders;
}

bool SAssetView::IsShowingEmptyFolders() const
{
	return IsToggleShowEmptyFoldersAllowed() && GetDefault<UContentBrowserSettings>()->DisplayEmptyFolders;
}

void SAssetView::ToggleRealTimeThumbnails()
{
	check( CanShowRealTimeThumbnails() );
	GetMutableDefault<UContentBrowserSettings>()->RealTimeThumbnails = !GetDefault<UContentBrowserSettings>()->RealTimeThumbnails;
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::CanShowRealTimeThumbnails() const
{
	return bCanShowRealTimeThumbnails;
}

bool SAssetView::IsShowingRealTimeThumbnails() const
{
	return CanShowRealTimeThumbnails() && GetDefault<UContentBrowserSettings>()->RealTimeThumbnails;
}

void SAssetView::ToggleShowPluginContent()
{
	bool bDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
	bool bRawDisplayPlugins = GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders( true );

	// Only if both these flags are false when toggling we want to enable the flag, otherwise we're toggling off
	if ( !bDisplayPlugins && !bRawDisplayPlugins )
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders( true );
	}
	else
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders( false );
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayPluginFolders( false, true );
	}	
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsShowingPluginContent() const
{
	return GetDefault<UContentBrowserSettings>()->GetDisplayPluginFolders();
}

void SAssetView::ToggleShowEngineContent()
{
	bool bDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
	bool bRawDisplayEngine = GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder( true );

	// Only if both these flags are false when toggling we want to enable the flag, otherwise we're toggling off
	if ( !bDisplayEngine && !bRawDisplayEngine )
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder( true );
	}
	else
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder( false );
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayEngineFolder( false, true );
	}	
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsShowingEngineContent() const
{
	return GetDefault<UContentBrowserSettings>()->GetDisplayEngineFolder();
}

void SAssetView::ToggleShowDevelopersContent()
{
	bool bDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
	bool bRawDisplayDev = GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder( true );

	// Only if both these flags are false when toggling we want to enable the flag, otherwise we're toggling off
	if ( !bDisplayDev && !bRawDisplayDev )
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder( true );
	}
	else
	{
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder( false );
		GetMutableDefault<UContentBrowserSettings>()->SetDisplayDevelopersFolder( false, true );
	}	
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowDevelopersContentAllowed() const
{
	return bCanShowDevelopersFolder;
}

bool SAssetView::IsShowingDevelopersContent() const
{
	return IsToggleShowDevelopersContentAllowed() && GetDefault<UContentBrowserSettings>()->GetDisplayDevelopersFolder();
}

void SAssetView::ToggleShowLocalizedContent()
{
	GetMutableDefault<UContentBrowserSettings>()->SetDisplayL10NFolder(!GetDefault<UContentBrowserSettings>()->GetDisplayL10NFolder());
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowLocalizedContentAllowed() const
{
	return true;
}

bool SAssetView::IsShowingLocalizedContent() const
{
	return IsToggleShowLocalizedContentAllowed() && GetDefault<UContentBrowserSettings>()->GetDisplayL10NFolder();
}

void SAssetView::ToggleShowCollections()
{
	const bool bDisplayCollections = GetDefault<UContentBrowserSettings>()->GetDisplayCollections();
	GetMutableDefault<UContentBrowserSettings>()->SetDisplayCollections( !bDisplayCollections );
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowCollectionsAllowed() const
{
	return bCanShowCollections;
}

bool SAssetView::IsShowingCollections() const
{
	return IsToggleShowCollectionsAllowed() && GetDefault<UContentBrowserSettings>()->GetDisplayCollections();
}

void SAssetView::ToggleShowCppContent()
{
	const bool bDisplayCppFolders = GetDefault<UContentBrowserSettings>()->GetDisplayCppFolders();
	GetMutableDefault<UContentBrowserSettings>()->SetDisplayCppFolders(!bDisplayCppFolders);
	GetMutableDefault<UContentBrowserSettings>()->PostEditChange();
}

bool SAssetView::IsToggleShowCppContentAllowed() const
{
	return bCanShowClasses;
}

bool SAssetView::IsShowingCppContent() const
{
	return IsToggleShowCppContentAllowed() && GetDefault<UContentBrowserSettings>()->GetDisplayCppFolders();
}

void SAssetView::SetCurrentViewType(EAssetViewType::Type NewType)
{
	if ( ensure(NewType != EAssetViewType::MAX) && NewType != CurrentViewType )
	{
		ResetQuickJump();

		CurrentViewType = NewType;
		CreateCurrentView();

		SyncToSelection();

		// Clear relevant thumbnails to render fresh ones in the new view if needed
		RelevantThumbnails.Empty();
		VisibleItems.Empty();

		if ( NewType == EAssetViewType::Tile )
		{
			CurrentThumbnailSize = TileViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::List )
		{
			CurrentThumbnailSize = ListViewThumbnailSize;
			bPendingUpdateThumbnails = true;
		}
		else if ( NewType == EAssetViewType::Column )
		{
			// No thumbnails, but we do need to refresh filtered items to determine a majority asset type
			MajorityAssetType = NAME_None;
			RefreshFilteredItems();
			RefreshFolders();
			SortList();
		}

		FSlateApplication::Get().DismissAllMenus();
	}
}

void SAssetView::CreateCurrentView()
{
	TileView.Reset();
	ListView.Reset();
	ColumnView.Reset();

	TSharedRef<SWidget> NewView = SNullWidget::NullWidget;
	switch (CurrentViewType)
	{
		case EAssetViewType::Tile:
			TileView = CreateTileView();
			NewView = CreateShadowOverlay(TileView.ToSharedRef());
			break;
		case EAssetViewType::List:
			ListView = CreateListView();
			NewView = CreateShadowOverlay(ListView.ToSharedRef());
			break;
		case EAssetViewType::Column:
			ColumnView = CreateColumnView();
			NewView = CreateShadowOverlay(ColumnView.ToSharedRef());
			break;
	}
	
	ViewContainer->SetContent( NewView );
}

TSharedRef<SWidget> SAssetView::CreateShadowOverlay( TSharedRef<STableViewBase> Table )
{
	return SNew(SScrollBorder, Table)
		[
			Table
		];
}

EAssetViewType::Type SAssetView::GetCurrentViewType() const
{
	return CurrentViewType;
}

bool SAssetView::IsCurrentViewType(EAssetViewType::Type ViewType) const
{
	return GetCurrentViewType() == ViewType;
}

void SAssetView::FocusList() const
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: FSlateApplication::Get().SetKeyboardFocus(ListView, EFocusCause::SetDirectly); break;
		case EAssetViewType::Tile: FSlateApplication::Get().SetKeyboardFocus(TileView, EFocusCause::SetDirectly); break;
		case EAssetViewType::Column: FSlateApplication::Get().SetKeyboardFocus(ColumnView, EFocusCause::SetDirectly); break;
	}
}

void SAssetView::RefreshList()
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->RequestListRefresh(); break;
		case EAssetViewType::Tile: TileView->RequestListRefresh(); break;
		case EAssetViewType::Column: ColumnView->RequestListRefresh(); break;
	}
}

void SAssetView::SetSelection(const TSharedPtr<FAssetViewItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->SetSelection(Item); break;
		case EAssetViewType::Tile: TileView->SetSelection(Item); break;
		case EAssetViewType::Column: ColumnView->SetSelection(Item); break;
	}
}

void SAssetView::SetItemSelection(const TSharedPtr<FAssetViewItem>& Item, bool bSelected, const ESelectInfo::Type SelectInfo)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->SetItemSelection(Item, bSelected, SelectInfo); break;
		case EAssetViewType::Tile: TileView->SetItemSelection(Item, bSelected, SelectInfo); break;
		case EAssetViewType::Column: ColumnView->SetItemSelection(Item, bSelected, SelectInfo); break;
	}
}

void SAssetView::RequestScrollIntoView(const TSharedPtr<FAssetViewItem>& Item)
{
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->RequestScrollIntoView(Item); break;
		case EAssetViewType::Tile: TileView->RequestScrollIntoView(Item); break;
		case EAssetViewType::Column: ColumnView->RequestScrollIntoView(Item); break;
	}
}

void SAssetView::OnOpenAssetsOrFolders()
{
	TArray<FAssetData> SelectedAssets = GetSelectedAssets();
	TArray<FString> SelectedFolders = GetSelectedFolders();
	if (SelectedAssets.Num() > 0 && SelectedFolders.Num() == 0)
	{
		OnAssetsActivated.ExecuteIfBound(SelectedAssets, EAssetTypeActivationMethod::Opened);
	}
	else if (SelectedAssets.Num() == 0 && SelectedFolders.Num() > 0)
	{
		OnPathSelected.ExecuteIfBound(SelectedFolders[0]);
	}
}

void SAssetView::OnPreviewAssets()
{
	OnAssetsActivated.ExecuteIfBound(GetSelectedAssets(), EAssetTypeActivationMethod::Previewed);
}

void SAssetView::ClearSelection(bool bForceSilent)
{
	const bool bTempBulkSelectingValue = bForceSilent ? true : bBulkSelecting;
	TGuardValue<bool>(bBulkSelecting, bTempBulkSelectingValue);
	switch ( GetCurrentViewType() )
	{
		case EAssetViewType::List: ListView->ClearSelection(); break;
		case EAssetViewType::Tile: TileView->ClearSelection(); break;
		case EAssetViewType::Column: ColumnView->ClearSelection(); break;
	}
}

TSharedRef<ITableRow> SAssetView::MakeListViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewAsset>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	if(AssetItem->GetType() == EAssetItemType::Folder)
	{
		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetListItem> Item =
			SNew(SAssetListItem)
			.AssetItem(AssetItem)
			.ItemHeight(this, &SAssetView::GetListViewItemHeight)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText(HighlightedText)
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) )
			.OnAssetsOrPathsDragDropped(this, &SAssetView::OnAssetsOrPathsDragDropped)
			.OnFilesDragDropped(this, &SAssetView::OnFilesDragDropped);

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
	else
	{
		TSharedPtr<FAssetViewAsset> AssetItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(AssetItem);

		TSharedPtr<FAssetThumbnail>* AssetThumbnailPtr = RelevantThumbnails.Find(AssetItemAsAsset);
		TSharedPtr<FAssetThumbnail> AssetThumbnail;
		if ( AssetThumbnailPtr )
		{
			AssetThumbnail = *AssetThumbnailPtr;
		}
		else
		{
			const float ThumbnailResolution = ListViewThumbnailResolution;
			AssetThumbnail = MakeShareable( new FAssetThumbnail( AssetItemAsAsset->Data, ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool ) );
			RelevantThumbnails.Add( AssetItemAsAsset, AssetThumbnail );
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}

		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
		.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetListItem> Item =
			SNew(SAssetListItem)
			.AssetThumbnail(AssetThumbnail)
			.AssetItem(AssetItem)
			.ThumbnailPadding(ListViewThumbnailPadding)
			.ItemHeight(this, &SAssetView::GetListViewItemHeight)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText(HighlightedText)
			.ThumbnailEditMode(this, &SAssetView::IsThumbnailEditMode)
			.ThumbnailLabel( ThumbnailLabel )
			.ThumbnailHintColorAndOpacity( this, &SAssetView::GetThumbnailHintColorAndOpacity )
			.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) )
			.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
			.OnVisualizeAssetToolTip(OnVisualizeAssetToolTip)
			.OnAssetToolTipClosing(OnAssetToolTipClosing);

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
}

TSharedRef<ITableRow> SAssetView::MakeTileViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewAsset>>, OwnerTable );
	}

	VisibleItems.Add(AssetItem);
	bPendingUpdateThumbnails = true;

	if(AssetItem->GetType() == EAssetItemType::Folder)
	{
		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style( FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow" )
			.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
			.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetTileItem> Item =
			SNew(SAssetTileItem)
			.AssetItem(AssetItem)
			.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) )
			.OnAssetsOrPathsDragDropped(this, &SAssetView::OnAssetsOrPathsDragDropped)
			.OnFilesDragDropped(this, &SAssetView::OnFilesDragDropped);

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
	else
	{
		TSharedPtr<FAssetViewAsset> AssetItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(AssetItem);

		TSharedPtr<FAssetThumbnail>* AssetThumbnailPtr = RelevantThumbnails.Find(AssetItemAsAsset);
		TSharedPtr<FAssetThumbnail> AssetThumbnail;
		if ( AssetThumbnailPtr )
		{
			AssetThumbnail = *AssetThumbnailPtr;
		}
		else
		{
			const float ThumbnailResolution = TileViewThumbnailResolution;
			AssetThumbnail = MakeShareable( new FAssetThumbnail( AssetItemAsAsset->Data, ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool ) );
			RelevantThumbnails.Add( AssetItemAsAsset, AssetThumbnail );
			AssetThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render
		}

		TSharedPtr< STableRow<TSharedPtr<FAssetViewItem>> > TableRowWidget;
		SAssignNew( TableRowWidget, STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
		.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow")
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem );

		TSharedRef<SAssetTileItem> Item =
			SNew(SAssetTileItem)
			.AssetThumbnail(AssetThumbnail)
			.AssetItem(AssetItem)
			.ThumbnailPadding(TileViewThumbnailPadding)
			.ItemWidth(this, &SAssetView::GetTileViewItemWidth)
			.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
			.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
			.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
			.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
			.ShouldAllowToolTip(this, &SAssetView::ShouldAllowToolTips)
			.HighlightText( HighlightedText )
			.ThumbnailEditMode(this, &SAssetView::IsThumbnailEditMode)
			.ThumbnailLabel( ThumbnailLabel )
			.ThumbnailHintColorAndOpacity( this, &SAssetView::GetThumbnailHintColorAndOpacity )
			.AllowThumbnailHintLabel( AllowThumbnailHintLabel )
			.IsSelected( FIsSelected::CreateSP(TableRowWidget.Get(), &STableRow<TSharedPtr<FAssetViewItem>>::IsSelectedExclusively) )
			.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
			.OnVisualizeAssetToolTip( OnVisualizeAssetToolTip )
			.OnAssetToolTipClosing( OnAssetToolTipClosing );

		TableRowWidget->SetContent(Item);

		return TableRowWidget.ToSharedRef();
	}
}

TSharedRef<ITableRow> SAssetView::MakeColumnViewWidget(TSharedPtr<FAssetViewItem> AssetItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return SNew( STableRow<TSharedPtr<FAssetViewItem>>, OwnerTable )
			.Style(FEditorStyle::Get(), "ContentBrowser.AssetListView.TableRow");
	}

	// Update the cached custom data
	if (AssetItem->GetType() == EAssetItemType::Normal)
	{
		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(AssetItem);
		for (const FAssetViewCustomColumn& Column : CustomColumns)
		{
			if (!ItemAsAsset->CustomColumnData.Find(Column.ColumnName))
			{
				ItemAsAsset->CustomColumnData.Add(Column.ColumnName, Column.OnGetColumnData.Execute(ItemAsAsset->Data, Column.ColumnName));
			}
		}
	}
	
	return
		SNew( SAssetColumnViewRow, OwnerTable )
		.OnDragDetected( this, &SAssetView::OnDraggingAssetItem )
		.Cursor( bAllowDragging ? EMouseCursor::GrabHand : EMouseCursor::Default )
		.AssetColumnItem(
			SNew(SAssetColumnItem)
				.AssetItem(AssetItem)
				.OnRenameBegin(this, &SAssetView::AssetRenameBegin)
				.OnRenameCommit(this, &SAssetView::AssetRenameCommit)
				.OnVerifyRenameCommit(this, &SAssetView::AssetVerifyRenameCommit)
				.OnItemDestroyed(this, &SAssetView::AssetItemWidgetDestroyed)
				.HighlightText( HighlightedText )
				.OnAssetsOrPathsDragDropped(this, &SAssetView::OnAssetsOrPathsDragDropped)
				.OnFilesDragDropped(this, &SAssetView::OnFilesDragDropped)
				.OnGetCustomAssetToolTip(OnGetCustomAssetToolTip)
				.OnVisualizeAssetToolTip( OnVisualizeAssetToolTip )
				.OnAssetToolTipClosing( OnAssetToolTipClosing )
		);
}

UObject* SAssetView::CreateAssetFromTemporary(FString InName, const TSharedPtr<FAssetViewAsset>& InItem, FText& OutErrorText)
{
	UObject* Asset = NULL;

	const EAssetItemType::Type ItemType = InItem->GetType();
	if ( ItemType == EAssetItemType::Creation )
	{
		// Committed creation
		TSharedPtr<FAssetViewCreation> CreationItem = StaticCastSharedPtr<FAssetViewCreation>(InItem);
		UFactory* Factory = CreationItem->Factory;
		UClass* AssetClass = CreationItem->AssetClass;
		FString PackagePath = CreationItem->Data.PackagePath.ToString();

		// Remove the temporary item before we do any work to ensure the new item creation is not prevented.
		FilteredAssetItems.Remove(InItem);
		RefreshList();

		if ( AssetClass || Factory )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			Asset = AssetToolsModule.Get().CreateAsset(InName, PackagePath, AssetClass, Factory, FName("ContentBrowserNewAsset"));
		}

		if ( Asset == NULL )
		{
			OutErrorText = LOCTEXT("AssetCreationFailed", "Failed to create asset.");
		}
	}
	else if ( ItemType == EAssetItemType::Duplication )
	{
		// Committed duplication
		TSharedPtr<FAssetViewDuplication> DuplicationItem = StaticCastSharedPtr<FAssetViewDuplication>(InItem);
		UObject* SourceObject = DuplicationItem->SourceObject.Get();
		FString PackagePath = DuplicationItem->Data.PackagePath.ToString();

		// Remove the temporary item before we do any work to ensure the new item creation is not prevented.
		FilteredAssetItems.Remove(InItem);
		RefreshList();

		if ( SourceObject )
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
			Asset = AssetToolsModule.Get().DuplicateAsset(InName, PackagePath, SourceObject);
		}

		if ( Asset == NULL )
		{
			OutErrorText = LOCTEXT("AssetCreationFailed", "Failed to create asset.");
		}
	}

	return Asset;
}

void SAssetView::AssetItemWidgetDestroyed(const TSharedPtr<FAssetViewItem>& Item)
{
	if(RenamingAsset.Pin().Get() == Item.Get())
	{
		/* Check if the item is in a temp state and if it is, commit using the default name so that it does not entirely vanish on the user.
		   This keeps the functionality consistent for content to never be in a temporary state */
		if ( Item.IsValid() && Item->IsTemporaryItem() && Item->GetType() != EAssetItemType::Folder )
		{
			FText OutErrorText;
			const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
			CreateAssetFromTemporary(ItemAsAsset->Data.AssetName.ToString(), ItemAsAsset, OutErrorText);

			// Remove the temporary item.
			FilteredAssetItems.Remove(Item);
			RefreshList();
		}

		RenamingAsset.Reset();
	}

	if ( VisibleItems.Remove(Item) != INDEX_NONE )
	{
		bPendingUpdateThumbnails = true;
	}
}

void SAssetView::UpdateThumbnails()
{
	int32 MinItemIdx = INDEX_NONE;
	int32 MaxItemIdx = INDEX_NONE;
	int32 MinVisibleItemIdx = INDEX_NONE;
	int32 MaxVisibleItemIdx = INDEX_NONE;

	const int32 HalfNumOffscreenThumbnails = NumOffscreenThumbnails * 0.5;
	for ( auto ItemIt = VisibleItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		int32 ItemIdx = FilteredAssetItems.Find(*ItemIt);
		if ( ItemIdx != INDEX_NONE )
		{
			const int32 ItemIdxLow = FMath::Max<int32>(0, ItemIdx - HalfNumOffscreenThumbnails);
			const int32 ItemIdxHigh = FMath::Min<int32>(FilteredAssetItems.Num() - 1, ItemIdx + HalfNumOffscreenThumbnails);
			if ( MinItemIdx == INDEX_NONE || ItemIdxLow < MinItemIdx )
			{
				MinItemIdx = ItemIdxLow;
			}
			if ( MaxItemIdx == INDEX_NONE || ItemIdxHigh > MaxItemIdx )
			{
				MaxItemIdx = ItemIdxHigh;
			}
			if ( MinVisibleItemIdx == INDEX_NONE || ItemIdx < MinVisibleItemIdx )
			{
				MinVisibleItemIdx = ItemIdx;
			}
			if ( MaxVisibleItemIdx == INDEX_NONE || ItemIdx > MaxVisibleItemIdx )
			{
				MaxVisibleItemIdx = ItemIdx;
			}
		}
	}

	if ( MinItemIdx != INDEX_NONE && MaxItemIdx != INDEX_NONE && MinVisibleItemIdx != INDEX_NONE && MaxVisibleItemIdx != INDEX_NONE )
	{
		// We have a new min and a new max, compare it to the old min and max so we can create new thumbnails
		// when appropriate and remove old thumbnails that are far away from the view area.
		TMap< TSharedPtr<FAssetViewAsset>, TSharedPtr<FAssetThumbnail> > NewRelevantThumbnails;

		// Operate on offscreen items that are furthest away from the visible items first since the thumbnail pool processes render requests in a LIFO order.
		while (MinItemIdx < MinVisibleItemIdx || MaxItemIdx > MaxVisibleItemIdx)
		{
			const int32 LowEndDistance = MinVisibleItemIdx - MinItemIdx;
			const int32 HighEndDistance = MaxItemIdx - MaxVisibleItemIdx;

			if ( HighEndDistance > LowEndDistance )
			{
				if(FilteredAssetItems.IsValidIndex(MaxItemIdx) && FilteredAssetItems[MaxItemIdx]->GetType() != EAssetItemType::Folder)
				{
					AddItemToNewThumbnailRelevancyMap( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[MaxItemIdx]), NewRelevantThumbnails );
				}
				MaxItemIdx--;
			}
			else
			{
				if(FilteredAssetItems.IsValidIndex(MinItemIdx) && FilteredAssetItems[MinItemIdx]->GetType() != EAssetItemType::Folder)
				{
					AddItemToNewThumbnailRelevancyMap( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[MinItemIdx]), NewRelevantThumbnails );
				}
				MinItemIdx++;
			}
		}

		// Now operate on VISIBLE items then prioritize them so they are rendered first
		TArray< TSharedPtr<FAssetThumbnail> > ThumbnailsToPrioritize;
		for ( int32 ItemIdx = MinVisibleItemIdx; ItemIdx <= MaxVisibleItemIdx; ++ItemIdx )
		{
			if(FilteredAssetItems.IsValidIndex(ItemIdx) && FilteredAssetItems[ItemIdx]->GetType() != EAssetItemType::Folder)
			{
				TSharedPtr<FAssetThumbnail> Thumbnail = AddItemToNewThumbnailRelevancyMap( StaticCastSharedPtr<FAssetViewAsset>(FilteredAssetItems[ItemIdx]), NewRelevantThumbnails );
				if ( Thumbnail.IsValid() )
				{
					ThumbnailsToPrioritize.Add(Thumbnail);
				}
			}
		}

		// Now prioritize all thumbnails there were in the visible range
		if ( ThumbnailsToPrioritize.Num() > 0 )
		{
			AssetThumbnailPool->PrioritizeThumbnails(ThumbnailsToPrioritize, CurrentThumbnailSize, CurrentThumbnailSize);
		}

		// Assign the new map of relevant thumbnails. This will remove any entries that were no longer relevant.
		RelevantThumbnails = NewRelevantThumbnails;
	}
}

TSharedPtr<FAssetThumbnail> SAssetView::AddItemToNewThumbnailRelevancyMap(const TSharedPtr<FAssetViewAsset>& Item, TMap< TSharedPtr<FAssetViewAsset>, TSharedPtr<FAssetThumbnail> >& NewRelevantThumbnails)
{
	const TSharedPtr<FAssetThumbnail>* Thumbnail = RelevantThumbnails.Find(Item);
	if ( Thumbnail )
	{
		// The thumbnail is still relevant, add it to the new list
		NewRelevantThumbnails.Add(Item, *Thumbnail);

		return *Thumbnail;
	}
	else
	{
		if ( !ensure(CurrentThumbnailSize > 0 && CurrentThumbnailSize <= MAX_THUMBNAIL_SIZE) )
		{
			// Thumbnail size must be in a sane range
			CurrentThumbnailSize = 64;
		}

		// The thumbnail newly relevant, create a new thumbnail
		const float ThumbnailResolution = CurrentThumbnailSize * MaxThumbnailScale;
		TSharedPtr<FAssetThumbnail> NewThumbnail = MakeShareable( new FAssetThumbnail( Item->Data, ThumbnailResolution, ThumbnailResolution, AssetThumbnailPool ) );
		NewRelevantThumbnails.Add( Item, NewThumbnail );
		NewThumbnail->GetViewportRenderTargetTexture(); // Access the texture once to trigger it to render

		return NewThumbnail;
	}
}

void SAssetView::AssetSelectionChanged( TSharedPtr< struct FAssetViewItem > AssetItem, ESelectInfo::Type SelectInfo )
{
	if ( !bBulkSelecting )
	{
		if ( AssetItem.IsValid() && AssetItem->GetType() != EAssetItemType::Folder )
		{
			OnAssetSelected.ExecuteIfBound(StaticCastSharedPtr<FAssetViewAsset>(AssetItem)->Data);
		}
		else
		{
			OnAssetSelected.ExecuteIfBound(FAssetData());
		}
	}
}

void SAssetView::ItemScrolledIntoView(TSharedPtr<struct FAssetViewItem> AssetItem, const TSharedPtr<ITableRow>& Widget )
{
	if ( AssetItem->bRenameWhenScrolledIntoview )
	{
		// Make sure we have window focus to avoid the inline text editor from canceling itself if we try to click on it
		// This can happen if creating an asset opens an intermediary window which steals our focus, 
		// eg, the blueprint and slate widget style class windows (TTP# 314240)
		TSharedPtr<SWindow> OwnerWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		if(OwnerWindow.IsValid())
		{
			OwnerWindow->BringToFront();
		}

		AwaitingRename = AssetItem;
	}
}

TSharedPtr<SWidget> SAssetView::OnGetContextMenuContent()
{
	if ( CanOpenContextMenu() )
	{
		const TArray<FString> SelectedFolders = GetSelectedFolders();
		if(SelectedFolders.Num() > 0)
		{
			return OnGetFolderContextMenu.Execute(SelectedFolders, OnGetPathContextMenuExtender, FOnCreateNewFolder::CreateSP(this, &SAssetView::OnCreateNewFolder));
		}
		else
		{
			return OnGetAssetContextMenu.Execute(GetSelectedAssets());
		}
	}
	
	return NULL;
}

bool SAssetView::CanOpenContextMenu() const
{
	if ( !OnGetAssetContextMenu.IsBound() )
	{
		// You can only a summon a context menu if one is set up
		return false;
	}

	if ( IsThumbnailEditMode() )
	{
		// You can not summon a context menu for assets when in thumbnail edit mode because right clicking may happen inadvertently while adjusting thumbnails.
		return false;
	}

	TArray<FAssetData> SelectedAssets = GetSelectedAssets();

	// Detect if at least one temporary item was selected. If there were no valid assets selected and a temporary one was, then deny the context menu.
	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	bool bAtLeastOneTemporaryItemFound = false;
	for ( auto ItemIt = SelectedItems.CreateConstIterator(); ItemIt; ++ItemIt )
	{
		const TSharedPtr<FAssetViewItem>& Item = *ItemIt;
		if ( Item->IsTemporaryItem() )
		{
			bAtLeastOneTemporaryItemFound = true;
		}
	}

	// If there were no valid assets found, but some invalid assets were found, deny the context menu
	if ( SelectedAssets.Num() == 0 && bAtLeastOneTemporaryItemFound )
	{
		return false;
	}

	if ( SelectedAssets.Num() == 0 && SourcesData.HasCollections() )
	{
		// Don't allow a context menu when we're viewing a collection and have no assets selected
		return false;
	}

	// Build a list of selected object paths
	TArray<FString> ObjectPaths;
	for(auto AssetIt = SelectedAssets.CreateConstIterator(); AssetIt; ++AssetIt)
	{
		ObjectPaths.Add( AssetIt->ObjectPath.ToString() );
	}

	bool bLoadSuccessful = true;

	if ( bPreloadAssetsForContextMenu )
	{
		TArray<UObject*> LoadedObjects;
		const bool bAllowedToPrompt = false;
		bLoadSuccessful = ContentBrowserUtils::LoadAssetsIfNeeded(ObjectPaths, LoadedObjects, bAllowedToPrompt);
	}

	// Do not show the context menu if the load failed
	return bLoadSuccessful;
}

void SAssetView::OnListMouseButtonDoubleClick(TSharedPtr<FAssetViewItem> AssetItem)
{
	if ( !ensure(AssetItem.IsValid()) )
	{
		return;
	}

	if ( IsThumbnailEditMode() )
	{
		// You can not activate assets when in thumbnail edit mode because double clicking may happen inadvertently while adjusting thumbnails.
		return;
	}

	if ( AssetItem->GetType() == EAssetItemType::Folder )
	{
		OnPathSelected.ExecuteIfBound(StaticCastSharedPtr<FAssetViewFolder>(AssetItem)->FolderPath);
		return;
	}

	if ( AssetItem->IsTemporaryItem() )
	{
		// You may not activate temporary items, they are just for display.
		return;
	}

	TArray<FAssetData> ActivatedAssets;
	ActivatedAssets.Add(StaticCastSharedPtr<FAssetViewAsset>(AssetItem)->Data);
	OnAssetsActivated.ExecuteIfBound( ActivatedAssets, EAssetTypeActivationMethod::DoubleClicked );
}

FReply SAssetView::OnDraggingAssetItem( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (bAllowDragging)
	{
		TArray<FAssetData> DraggedAssets;
		TArray<FString> DraggedAssetPaths;

		// Work out which assets to drag
		{
			TArray<FAssetData> AssetDataList = GetSelectedAssets();
			for (const FAssetData& AssetData : AssetDataList)
			{
				// Skip invalid assets and redirectors
				if (AssetData.IsValid() && AssetData.AssetClass != UObjectRedirector::StaticClass()->GetFName())
				{
					DraggedAssets.Add(AssetData);
				}
			}
		}

		// Work out which asset paths to drag
		{
			TArray<FString> SelectedFolders = GetSelectedFolders();
			if (SelectedFolders.Num() > 0 && !SourcesData.HasCollections())
			{
				DraggedAssetPaths = MoveTemp(SelectedFolders);
			}
		}

		// Use the custom drag handler?
		if (DraggedAssets.Num() > 0 && FEditorDelegates::OnAssetDragStarted.IsBound())
		{
			FEditorDelegates::OnAssetDragStarted.Broadcast(DraggedAssets, nullptr);
			return FReply::Handled();
		}
		
		// Use the standard drag handler?
		if ((DraggedAssets.Num() > 0 || DraggedAssetPaths.Num() > 0) && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		{
			return FReply::Handled().BeginDragDrop(FAssetDragDropOp::New(MoveTemp(DraggedAssets), MoveTemp(DraggedAssetPaths)));
		}
	}

	return FReply::Unhandled();
}

bool SAssetView::AssetVerifyRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FText& NewName, const FSlateRect& MessageAnchor, FText& OutErrorMessage)
{
	// Everything other than a folder is considered an asset, including "Creation" and "Duplication"
	// See FAssetViewCreation and FAssetViewDuplication
	const bool bIsAssetType = Item->GetType() != EAssetItemType::Folder;

	FString NewNameString = NewName.ToString();
	if ( bIsAssetType )
	{
		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
		if ( !Item->IsTemporaryItem() && NewNameString == ItemAsAsset->Data.AssetName.ToString() )
		{
			return true;
		}
	}
	else
	{
		const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
		if (NewNameString == ItemAsFolder->FolderName.ToString())
		{
			return true;
		}
	}

	if ( bIsAssetType )
	{
		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
		const FString NewObjectPath = ItemAsAsset->Data.PackagePath.ToString() / NewNameString + TEXT(".") + NewNameString;
		return ContentBrowserUtils::IsValidObjectPathForCreate(NewObjectPath, OutErrorMessage);
	}
	else
	{
		const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
		const FString FolderPath = FPaths::GetPath(ItemAsFolder->FolderPath);
		return ContentBrowserUtils::IsValidFolderPathForCreate(FolderPath, NewNameString, OutErrorMessage);
	}

	return true;
}

void SAssetView::AssetRenameBegin(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor)
{
	check(!RenamingAsset.IsValid());
	RenamingAsset = Item;
}

void SAssetView::AssetRenameCommit(const TSharedPtr<FAssetViewItem>& Item, const FString& NewName, const FSlateRect& MessageAnchor, const ETextCommit::Type CommitType)
{
	const EAssetItemType::Type ItemType = Item->GetType();

	// If the item had a factory, create a new object, otherwise rename
	bool bSuccess = false;
	UObject* Asset = NULL;
	FText ErrorMessage;
	if ( ItemType == EAssetItemType::Normal )
	{
		const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);

		// Check if the name is different
		if( NewName.Equals(ItemAsAsset->Data.AssetName.ToString(), ESearchCase::CaseSensitive) )
		{
			RenamingAsset.Reset();
			return;
		}

		// Committed rename
		Asset = ItemAsAsset->Data.GetAsset();
		if(Asset == NULL)
		{
			//put back the original name
			RenamingAsset.Reset();
			
			//Notify the user rename fail and link the output log
			FNotificationInfo Info(LOCTEXT("RenameAssetsFailed", "Failed to rename assets"));
			Info.ExpireDuration = 5.0f;
			Info.Hyperlink = FSimpleDelegate::CreateStatic([](){ FGlobalTabmanager::Get()->InvokeTab(FName("OutputLog")); });
			Info.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
			FSlateNotificationManager::Get().AddNotification(Info);
			
			//Set the content browser error message
			ErrorMessage = LOCTEXT("RenameAssetsFailed", "Failed to rename assets");
		}
		else
		{
			ContentBrowserUtils::RenameAsset(Asset, NewName, ErrorMessage);
			bSuccess = true;
		}
	}
	else if ( ItemType == EAssetItemType::Creation || ItemType == EAssetItemType::Duplication )
	{
		if (CommitType == ETextCommit::OnCleared)
		{
			// Clearing the rename box on a newly created asset cancels the entire creation process
			FilteredAssetItems.Remove(Item);
			RefreshList();
		}
		else
		{
			Asset = CreateAssetFromTemporary(NewName, StaticCastSharedPtr<FAssetViewAsset>(Item), ErrorMessage);
			bSuccess = Asset != NULL;
		}
	}
	else if( ItemType == EAssetItemType::Folder )
	{
		const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
		if(ItemAsFolder->bNewFolder)
		{
			ItemAsFolder->bNewFolder = false;

			if (CommitType == ETextCommit::OnCleared)
			{
				// Clearing the rename box on a newly created folder cancels the entire creation process
				FilteredAssetItems.Remove(Item);
				RefreshList();
			}
			else
			{
				const FString NewPath = FPaths::GetPath(ItemAsFolder->FolderPath) / NewName;
				FText ErrorText;
				if( ContentBrowserUtils::IsValidFolderName(NewName, ErrorText) &&
					!ContentBrowserUtils::DoesFolderExist(NewPath))
				{
					// ensure the folder exists on disk
					FString NewPathOnDisk;
					bSuccess = FPackageName::TryConvertLongPackageNameToFilename(NewPath, NewPathOnDisk) && IFileManager::Get().MakeDirectory(*NewPathOnDisk, true);

					if (bSuccess)
					{
						TSharedRef<FEmptyFolderVisibilityManager> EmptyFolderVisibilityManager = FContentBrowserSingleton::Get().GetEmptyFolderVisibilityManager();
						EmptyFolderVisibilityManager->SetAlwaysShowPath(NewPath);

						FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
						bSuccess = AssetRegistryModule.Get().AddPath(NewPath);
					}
				}

				// remove this temp item - a new one will have been added by the asset registry callback
				FilteredAssetItems.Remove(Item);
				RefreshList();

				if(!bSuccess)
				{
					ErrorMessage = LOCTEXT("CreateFolderFailed", "Failed to create folder.");
				}
			}
		}
		else if(NewName != ItemAsFolder->FolderName.ToString())
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			// first create the new folder
			const FString NewPath = FPaths::GetPath(ItemAsFolder->FolderPath) / NewName;
			FText ErrorText;
			if( ContentBrowserUtils::IsValidFolderName(NewName, ErrorText) &&
				!ContentBrowserUtils::DoesFolderExist(NewPath))
			{
				// ensure the folder exists on disk
				FString NewPathOnDisk;
				bSuccess = FPackageName::TryConvertLongPackageNameToFilename(NewPath, NewPathOnDisk) && IFileManager::Get().MakeDirectory(*NewPathOnDisk, true);

				if (bSuccess)
				{
					bSuccess = AssetRegistryModule.Get().AddPath(NewPath);
				}
			}

			if(bSuccess)
			{
				// move any assets in our folder
				TArray<FAssetData> AssetsInFolder;
				AssetRegistryModule.Get().GetAssetsByPath(*ItemAsFolder->FolderPath, AssetsInFolder, true);
				TArray<UObject*> ObjectsInFolder;
				ContentBrowserUtils::GetObjectsInAssetData(AssetsInFolder, ObjectsInFolder);
				ContentBrowserUtils::MoveAssets(ObjectsInFolder, NewPath, ItemAsFolder->FolderPath);

				// Now check to see if the original folder is empty, if so we can delete it
				TArray<FAssetData> AssetsInOriginalFolder;
				AssetRegistryModule.Get().GetAssetsByPath(*ItemAsFolder->FolderPath, AssetsInOriginalFolder, true);
				if(AssetsInOriginalFolder.Num() == 0)
				{
					TArray<FString> FoldersToDelete;
					FoldersToDelete.Add(ItemAsFolder->FolderPath);
					ContentBrowserUtils::DeleteFolders(FoldersToDelete);
				}
			}

			RequestQuickFrontendListRefresh();
		}		
	}
	else
	{
		// Unknown AssetItemType
		ensure(0);
	}

	if ( bSuccess )
	{
		// Sort in the new item
		bPendingSortFilteredItems = true;
		RequestQuickFrontendListRefresh();

		if ( ItemType == EAssetItemType::Folder )
		{
			const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
			const FString NewPath = FPaths::GetPath(ItemAsFolder->FolderPath) / NewName;

			// Sync the view to the new folder
			TArray<FString> FolderList;
			FolderList.Add(NewPath);
			SyncToFolders(FolderList);
		}
		else
		{
			if ( ensure(Asset != NULL) )
			{
				// Refresh the thumbnail
				const TSharedPtr<FAssetThumbnail>* AssetThumbnail = RelevantThumbnails.Find(StaticCastSharedPtr<FAssetViewAsset>(Item));
				if ( AssetThumbnail )
				{
					AssetThumbnailPool->RefreshThumbnail(*AssetThumbnail);
				}

				// Sync to its location
				TArray<FAssetData> AssetDataList;
				new(AssetDataList) FAssetData(Asset);

				if ( OnAssetRenameCommitted.IsBound() && !bUserSearching)
				{
					// If our parent wants to potentially handle the sync, let it, but only if we're not currently searching (or it would cancel the search)
					OnAssetRenameCommitted.Execute(AssetDataList); 
				}
				else
				{
					// Otherwise, sync just the view
					SyncToAssets(AssetDataList);
				}
			}
		}
	}
	else if ( !ErrorMessage.IsEmpty() )
	{
		// Prompt the user with the reason the rename/creation failed
		ContentBrowserUtils::DisplayMessage(ErrorMessage, MessageAnchor, SharedThis(this));
	}

	RenamingAsset.Reset();
}

bool SAssetView::IsRenamingAsset() const
{
	return RenamingAsset.IsValid();
}

bool SAssetView::ShouldAllowToolTips() const
{
	bool bIsRightClickScrolling = false;
	switch( CurrentViewType )
	{
		case EAssetViewType::List:
			bIsRightClickScrolling = ListView->IsRightClickScrolling();
			break;

		case EAssetViewType::Tile:
			bIsRightClickScrolling = TileView->IsRightClickScrolling();
			break;

		case EAssetViewType::Column:
			bIsRightClickScrolling = ColumnView->IsRightClickScrolling();
			break;

		default:
			bIsRightClickScrolling = false;
			break;
	}

	return !bIsRightClickScrolling && !IsThumbnailEditMode() && !IsRenamingAsset();
}

bool SAssetView::IsThumbnailEditMode() const
{
	return IsThumbnailEditModeAllowed() && bThumbnailEditMode;
}

bool SAssetView::IsThumbnailEditModeAllowed() const
{
	return bAllowThumbnailEditMode && GetCurrentViewType() != EAssetViewType::Column;
}

FReply SAssetView::EndThumbnailEditModeClicked()
{
	bThumbnailEditMode = false;

	return FReply::Handled();
}

FText SAssetView::GetAssetCountText() const
{
	const int32 NumAssets = FilteredAssetItems.Num();
	const int32 NumSelectedAssets = GetSelectedItems().Num();

	FText AssetCount = FText::GetEmpty();
	if ( NumSelectedAssets == 0 )
	{
		if ( NumAssets == 1 )
		{
			AssetCount = LOCTEXT("AssetCountLabelSingular", "1 item");
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPlural", "{0} items"), FText::AsNumber(NumAssets) );
		}
	}
	else
	{
		if ( NumAssets == 1 )
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelSingularPlusSelection", "1 item ({0} selected)"), FText::AsNumber(NumSelectedAssets) );
		}
		else
		{
			AssetCount = FText::Format( LOCTEXT("AssetCountLabelPluralPlusSelection", "{0} items ({1} selected)"), FText::AsNumber(NumAssets), FText::AsNumber(NumSelectedAssets) );
		}
	}

	return AssetCount;
}

EVisibility SAssetView::GetEditModeLabelVisibility() const
{
	return IsThumbnailEditMode() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetListViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::List ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetTileViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Tile ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SAssetView::GetColumnViewVisibility() const
{
	return GetCurrentViewType() == EAssetViewType::Column ? EVisibility::Visible : EVisibility::Collapsed;
}

void SAssetView::ToggleThumbnailEditMode()
{
	bThumbnailEditMode = !bThumbnailEditMode;
}

float SAssetView::GetThumbnailScale() const
{
	return ThumbnailScaleSliderValue.Get();
}

void SAssetView::SetThumbnailScale( float NewValue )
{
	ThumbnailScaleSliderValue = NewValue;
	RefreshList();
}

bool SAssetView::IsThumbnailScalingLocked() const
{
	return GetCurrentViewType() == EAssetViewType::Column;
}

float SAssetView::GetListViewItemHeight() const
{
	return (ListViewThumbnailSize + ListViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SAssetView::GetTileViewItemHeight() const
{
	return TileViewNameHeight + GetTileViewItemBaseHeight() * FillScale;
}

float SAssetView::GetTileViewItemBaseHeight() const
{
	return (TileViewThumbnailSize + TileViewThumbnailPadding * 2) * FMath::Lerp(MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale());
}

float SAssetView::GetTileViewItemWidth() const
{
	return GetTileViewItemBaseWidth() * FillScale;
}

float SAssetView::GetTileViewItemBaseWidth() const //-V524
{
	return ( TileViewThumbnailSize + TileViewThumbnailPadding * 2 ) * FMath::Lerp( MinThumbnailScale, MaxThumbnailScale, GetThumbnailScale() );
}

EColumnSortMode::Type SAssetView::GetColumnSortMode(const FName ColumnId) const
{
	for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
	{
		const EColumnSortPriority::Type SortPriority = static_cast<EColumnSortPriority::Type>(PriorityIdx);
		if (ColumnId == SortManager.GetSortColumnId(SortPriority))
		{
			return SortManager.GetSortMode(SortPriority);
		}
	}
	return EColumnSortMode::None;
}

EColumnSortPriority::Type SAssetView::GetColumnSortPriority(const FName ColumnId) const
{
	for (int32 PriorityIdx = 0; PriorityIdx < EColumnSortPriority::Max; PriorityIdx++)
	{
		const EColumnSortPriority::Type SortPriority = static_cast<EColumnSortPriority::Type>(PriorityIdx);
		if (ColumnId == SortManager.GetSortColumnId(SortPriority))
		{
			return SortPriority;
		}
	}
	return EColumnSortPriority::Primary;
}

void SAssetView::OnSortColumnHeader(const EColumnSortPriority::Type SortPriority, const FName& ColumnId, const EColumnSortMode::Type NewSortMode)
{
	SortManager.SetSortColumnId(SortPriority, ColumnId);
	SortManager.SetSortMode(SortPriority, NewSortMode);
	SortList();
}

EVisibility SAssetView::IsAssetShowWarningTextVisible() const
{
	return FilteredAssetItems.Num() > 0 ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FText SAssetView::GetAssetShowWarningText() const
{
	if (AssetShowWarningText.IsSet())
	{
		return AssetShowWarningText.Get();
	}
	
	FText NothingToShowText, DropText;
	if (ShouldFilterRecursively())
	{
		NothingToShowText = LOCTEXT( "NothingToShowCheckFilter", "No results, check your filter." );
	}

	if ( SourcesData.HasCollections() && !SourcesData.IsDynamicCollection() )
	{
		DropText = LOCTEXT( "DragAssetsHere", "Drag and drop assets here to add them to the collection." );
	}
	else if ( OnGetAssetContextMenu.IsBound() )
	{
		DropText = LOCTEXT( "DropFilesOrRightClick", "Drop files here or right click to create content." );
	}
	
	return NothingToShowText.IsEmpty() ? DropText : FText::Format(LOCTEXT("NothingToShowPattern", "{0}\n\n{1}"), NothingToShowText, DropText);
}

bool SAssetView::HasSingleCollectionSource() const
{
	return ( SourcesData.Collections.Num() == 1 && SourcesData.PackagePaths.Num() == 0 );
}

void SAssetView::OnAssetsOrPathsDragDropped(const TArray<FAssetData>& AssetList, const TArray<FString>& AssetPaths, const FString& DestinationPath)
{
	DragDropHandler::HandleDropOnAssetFolder(
		SharedThis(this), 
		AssetList, 
		AssetPaths, 
		DestinationPath, 
		FText::FromString(FPaths::GetCleanFilename(DestinationPath)), 
		DragDropHandler::FExecuteCopyOrMove::CreateSP(this, &SAssetView::ExecuteDropCopy),
		DragDropHandler::FExecuteCopyOrMove::CreateSP(this, &SAssetView::ExecuteDropMove)
		);
}

void SAssetView::OnFilesDragDropped(const TArray<FString>& AssetList, const FString& DestinationPath)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().ImportAssets( AssetList, DestinationPath );
}

void SAssetView::ExecuteDropCopy(TArray<FAssetData> AssetList, TArray<FString> AssetPaths, FString DestinationPath)
{
	int32 NumItemsCopied = 0;

	if (AssetList.Num() > 0)
	{
		TArray<UObject*> DroppedObjects;
		ContentBrowserUtils::GetObjectsInAssetData(AssetList, DroppedObjects);

		TArray<UObject*> NewObjects;
		ObjectTools::DuplicateObjects(DroppedObjects, TEXT(""), DestinationPath, /*bOpenDialog=*/false, &NewObjects);

		NumItemsCopied += NewObjects.Num();
	}

	if (AssetPaths.Num() > 0)
	{
		if (ContentBrowserUtils::CopyFolders(AssetPaths, DestinationPath))
		{
			NumItemsCopied += AssetPaths.Num();
		}
	}

	// If any items were duplicated, report the success
	if (NumItemsCopied > 0)
	{
		const FText Message = FText::Format(LOCTEXT("AssetItemsDroppedCopy", "{0} {0}|plural(one=item,other=items) copied"), NumItemsCopied);
		const FVector2D& CursorPos = FSlateApplication::Get().GetCursorPos();
		FSlateRect MessageAnchor(CursorPos.X, CursorPos.Y, CursorPos.X, CursorPos.Y);
		ContentBrowserUtils::DisplayMessage(Message, MessageAnchor, SharedThis(this));
	}
}

void SAssetView::ExecuteDropMove(TArray<FAssetData> AssetList, TArray<FString> AssetPaths, FString DestinationPath)
{
	if (AssetList.Num() > 0)
	{
		TArray<UObject*> DroppedObjects;
		ContentBrowserUtils::GetObjectsInAssetData(AssetList, DroppedObjects);

		ContentBrowserUtils::MoveAssets(DroppedObjects, DestinationPath);
	}

	if (AssetPaths.Num() > 0)
	{
		ContentBrowserUtils::MoveFolders(AssetPaths, DestinationPath);
	}
}

void SAssetView::SetUserSearching(bool bInSearching)
{
	if(bUserSearching != bInSearching)
	{
		RequestSlowFullListRefresh();
	}
	bUserSearching = bInSearching;
}

void SAssetView::HandleSettingChanged(FName PropertyName)
{
	if ((PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayFolders)) ||
		(PropertyName == GET_MEMBER_NAME_CHECKED(UContentBrowserSettings, DisplayEmptyFolders)) ||
		(PropertyName == "DisplayDevelopersFolder") ||
		(PropertyName == "DisplayEngineFolder") ||
		(PropertyName == NAME_None))	// @todo: Needed if PostEditChange was called manually, for now
	{
		RequestSlowFullListRefresh();
	}
}

FText SAssetView::GetQuickJumpTerm() const
{
	return FText::FromString(QuickJumpData.JumpTerm);
}

EVisibility SAssetView::IsQuickJumpVisible() const
{
	return (QuickJumpData.JumpTerm.IsEmpty()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible;
}

FSlateColor SAssetView::GetQuickJumpColor() const
{
	return FEditorStyle::GetColor((QuickJumpData.bHasValidMatch) ? "InfoReporting.BackgroundColor" : "ErrorReporting.BackgroundColor");
}

void SAssetView::ResetQuickJump()
{
	QuickJumpData.JumpTerm.Empty();
	QuickJumpData.bIsJumping = false;
	QuickJumpData.bHasChangedSinceLastTick = false;
	QuickJumpData.bHasValidMatch = false;
}

FReply SAssetView::HandleQuickJumpKeyDown(const TCHAR InCharacter, const bool bIsControlDown, const bool bIsAltDown, const bool bTestOnly)
{
	// Check for special characters
	if(bIsControlDown || bIsAltDown)
	{
		return FReply::Unhandled();
	}

	// Check for invalid characters
	for(int InvalidCharIndex = 0; InvalidCharIndex < ARRAY_COUNT(INVALID_OBJECTNAME_CHARACTERS) - 1; ++InvalidCharIndex)
	{
		if(InCharacter == INVALID_OBJECTNAME_CHARACTERS[InvalidCharIndex])
		{
			return FReply::Unhandled();
		}
	}

	switch(InCharacter)
	{
	// Ignore some other special characters that we don't want to be entered into the buffer
	case 0:		// Any non-character key press, e.g. f1-f12, Delete, Pause/Break, etc.
				// These should be explicitly not handled so that their input bindings are handled higher up the chain.

	case 8:		// Backspace
	case 13:	// Enter
	case 27:	// Esc
		return FReply::Unhandled();

	default:
		break;
	}

	// Any other character!
	if(!bTestOnly)
	{
		QuickJumpData.JumpTerm.AppendChar(InCharacter);
		QuickJumpData.bHasChangedSinceLastTick = true;
	}

	return FReply::Handled();
}

bool SAssetView::PerformQuickJump(const bool bWasJumping)
{
	auto GetAssetViewItemName = [](const TSharedPtr<FAssetViewItem> &Item) -> FString
	{
		switch(Item->GetType())
		{
		case EAssetItemType::Normal:
			{
				const TSharedPtr<FAssetViewAsset>& ItemAsAsset = StaticCastSharedPtr<FAssetViewAsset>(Item);
				return ItemAsAsset->Data.AssetName.ToString();
			}

		case EAssetItemType::Folder:
			{
				const TSharedPtr<FAssetViewFolder>& ItemAsFolder = StaticCastSharedPtr<FAssetViewFolder>(Item);
				return ItemAsFolder->FolderName.ToString();
			}

		default:
			return FString();
		}
	};

	auto JumpToNextMatch = [this, &GetAssetViewItemName](const int StartIndex, const int EndIndex) -> bool
	{
		check(StartIndex >= 0);
		check(EndIndex <= FilteredAssetItems.Num());

		for(int NewSelectedItemIndex = StartIndex; NewSelectedItemIndex < EndIndex; ++NewSelectedItemIndex)
		{
			TSharedPtr<FAssetViewItem>& NewSelectedItem = FilteredAssetItems[NewSelectedItemIndex];
			const FString NewSelectedItemName = GetAssetViewItemName(NewSelectedItem);
			if(NewSelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
			{
				SetSelection(NewSelectedItem);
				RequestScrollIntoView(NewSelectedItem);
				return true;
			}
		}

		return false;
	};

	TArray<TSharedPtr<FAssetViewItem>> SelectedItems = GetSelectedItems();
	TSharedPtr<FAssetViewItem> SelectedItem = (SelectedItems.Num()) ? SelectedItems[0] : nullptr;

	// If we have a selection, and we were already jumping, first check to see whether 
	// the current selection still matches the quick-jump term; if it does, we do nothing
	if(bWasJumping && SelectedItem.IsValid())
	{
		const FString SelectedItemName = GetAssetViewItemName(SelectedItem);
		if(SelectedItemName.StartsWith(QuickJumpData.JumpTerm, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	// We need to move on to the next match in FilteredAssetItems that starts with the given quick-jump term
	const int SelectedItemIndex = (SelectedItem.IsValid()) ? FilteredAssetItems.Find(SelectedItem) : INDEX_NONE;
	const int StartIndex = (SelectedItemIndex == INDEX_NONE) ? 0 : SelectedItemIndex + 1;
	
	bool ValidMatch = JumpToNextMatch(StartIndex, FilteredAssetItems.Num());
	if(!ValidMatch && StartIndex > 0)
	{
		// If we didn't find a match, we need to loop around and look again from the start (assuming we weren't already)
		return JumpToNextMatch(0, StartIndex);
	}

	return ValidMatch;
}

void SAssetView::FillToggleColumnsMenu(FMenuBuilder& MenuBuilder)
{
	// Column view may not be valid if we toggled off columns view while the columns menu was open
	if(ColumnView.IsValid())
	{
		const TIndirectArray<SHeaderRow::FColumn> Columns = ColumnView->GetHeaderRow()->GetColumns();

		for (int32 ColumnIndex = 0; ColumnIndex < Columns.Num(); ++ColumnIndex)
		{
			const FString ColumnName = Columns[ColumnIndex].ColumnId.ToString();

			MenuBuilder.AddMenuEntry(
				Columns[ColumnIndex].DefaultText,
				LOCTEXT("ShowHideColumnTooltip", "Show or hide column"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateSP(this, &SAssetView::ToggleColumn, ColumnName),
					FCanExecuteAction::CreateSP(this, &SAssetView::CanToggleColumn, ColumnName),
					FIsActionChecked::CreateSP(this, &SAssetView::IsColumnVisible, ColumnName),
					EUIActionRepeatMode::RepeatEnabled
				),
				NAME_None,
				EUserInterfaceActionType::Check
			);
		}
	}
}

void SAssetView::ResetColumns()
{
	HiddenColumnNames.Empty();
	NumVisibleColumns = ColumnView->GetHeaderRow()->GetColumns().Num();
	ColumnView->GetHeaderRow()->RefreshColumns();
	ColumnView->RebuildList();
}

void SAssetView::ExportColumns()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	const FText Title = LOCTEXT("ExportToCSV", "Export columns as CSV...");
	const FString FileTypes = TEXT("Data Table CSV (*.csv)|*.csv");

	TArray<FString> OutFilenames;
	DesktopPlatform->SaveFileDialog(
		ParentWindowWindowHandle,
		Title.ToString(),
		TEXT(""),
		TEXT("Report.csv"),
		FileTypes,
		EFileDialogFlags::None,
		OutFilenames
	);

	if (OutFilenames.Num() > 0)
	{
		const TIndirectArray<SHeaderRow::FColumn>& Columns = ColumnView->GetHeaderRow()->GetColumns();

		TArray<FName> ColumnNames;
		for (const SHeaderRow::FColumn& Column : Columns)
		{
			ColumnNames.Add(Column.ColumnId);
		}

		FString SaveString;
		SortManager.ExportColumnsToCSV(FilteredAssetItems, ColumnNames, CustomColumns, SaveString);

		FFileHelper::SaveStringToFile(SaveString, *OutFilenames[0]);
	}
}

void SAssetView::ToggleColumn(const FString ColumnName)
{
	SetColumnVisibility(ColumnName, HiddenColumnNames.Contains(ColumnName));
}

void SAssetView::SetColumnVisibility(const FString ColumnName, const bool bShow)
{
	if (!bShow)
	{
		--NumVisibleColumns;
		HiddenColumnNames.Add(ColumnName);
	}
	else
	{
		++NumVisibleColumns;
		check(HiddenColumnNames.Contains(ColumnName));
		HiddenColumnNames.Remove(ColumnName);
	}

	ColumnView->GetHeaderRow()->RefreshColumns();
	ColumnView->RebuildList();
}

bool SAssetView::CanToggleColumn(const FString ColumnName) const
{
	return (HiddenColumnNames.Contains(ColumnName) || NumVisibleColumns > 1);
}

bool SAssetView::IsColumnVisible(const FString ColumnName) const
{
	return !HiddenColumnNames.Contains(ColumnName);
}

bool SAssetView::ShouldColumnGenerateWidget(const FString ColumnName) const
{
	return !HiddenColumnNames.Contains(ColumnName);
}

TSharedRef<SWidget> SAssetView::CreateRowHeaderMenuContent(const FString ColumnName)
{
	FMenuBuilder MenuBuilder(true, NULL);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("HideColumn", "Hide Column"),
		LOCTEXT("HideColumnToolTip", "Hides this column."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SAssetView::SetColumnVisibility, ColumnName, false), FCanExecuteAction::CreateSP(this, &SAssetView::CanToggleColumn, ColumnName)),
		NAME_None,
		EUserInterfaceActionType::Button);

	return MenuBuilder.MakeWidget();
}

void SAssetView::ForceShowPluginFolder(bool bEnginePlugin)
{
	if (bEnginePlugin && !IsShowingEngineContent())
	{
		ToggleShowEngineContent();
	}

	if (!IsShowingPluginContent())
	{
		ToggleShowPluginContent();
	}
}

#undef LOCTEXT_NAMESPACE
