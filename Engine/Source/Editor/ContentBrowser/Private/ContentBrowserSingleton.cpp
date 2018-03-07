// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserSingleton.h"
#include "Textures/SlateIcon.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Framework/Docking/TabManager.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "ContentBrowserLog.h"
#include "ContentBrowserUtils.h"
#include "SAssetPicker.h"
#include "SPathPicker.h"
#include "SCollectionPicker.h"
#include "SContentBrowser.h"
#include "ContentBrowserModule.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "IDocumentation.h"
#include "Interfaces/IMainFrameModule.h"
#include "SAssetDialog.h"
#include "TutorialMetaData.h"
#include "Widgets/Docking/SDockTab.h"
#include "NativeClassHierarchy.h"
#include "EmptyFolderVisibilityManager.h"
#include "CollectionAssetRegistryBridge.h"
#include "ContentBrowserCommands.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

FContentBrowserSingleton::FContentBrowserSingleton()
	: EmptyFolderVisibilityManager(MakeShared<FEmptyFolderVisibilityManager>())
	, CollectionAssetRegistryBridge(MakeShared<FCollectionAssetRegistryBridge>())
	, SettingsStringID(0)
{
	// Register the tab spawners for all content browsers
	const FSlateIcon ContentBrowserIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.TabIcon");
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	TSharedRef<FWorkspaceItem> ContentBrowserGroup = MenuStructure.GetToolsCategory()->AddGroup(
		LOCTEXT( "WorkspaceMenu_ContentBrowserCategory", "Content Browser" ),
		LOCTEXT( "ContentBrowserMenuTooltipText", "Open a Content Browser tab." ),
		ContentBrowserIcon,
		true);

	for ( int32 BrowserIdx = 0; BrowserIdx < ARRAY_COUNT(ContentBrowserTabIDs); BrowserIdx++ )
	{
		const FName TabID = FName(*FString::Printf(TEXT("ContentBrowserTab%d"), BrowserIdx + 1));
		ContentBrowserTabIDs[BrowserIdx] = TabID;

		const FText DefaultDisplayName = GetContentBrowserLabelWithIndex( BrowserIdx );

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner( TabID, FOnSpawnTab::CreateRaw(this, &FContentBrowserSingleton::SpawnContentBrowserTab, BrowserIdx) )
			.SetDisplayName(DefaultDisplayName)
			.SetTooltipText( LOCTEXT( "ContentBrowserMenuTooltipText", "Open a Content Browser tab." ) )
			.SetGroup( ContentBrowserGroup )
			.SetIcon(ContentBrowserIcon);
	}

	// Register a couple legacy tab ids
	FGlobalTabmanager::Get()->AddLegacyTabType( "LevelEditorContentBrowser", "ContentBrowserTab1" );
	FGlobalTabmanager::Get()->AddLegacyTabType( "MajorContentBrowserTab", "ContentBrowserTab2" );

	// Register to be notified when properties are edited
	FEditorDelegates::LoadSelectedAssetsIfNeeded.AddRaw(this, &FContentBrowserSingleton::OnEditorLoadSelectedAssetsIfNeeded);

	FContentBrowserCommands::Register();
}

FContentBrowserSingleton::~FContentBrowserSingleton()
{
	FEditorDelegates::LoadSelectedAssetsIfNeeded.RemoveAll(this);

	if ( FSlateApplication::IsInitialized() )
	{
		for ( int32 BrowserIdx = 0; BrowserIdx < ARRAY_COUNT(ContentBrowserTabIDs); BrowserIdx++ )
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner( ContentBrowserTabIDs[BrowserIdx] );
		}
	}
}

TSharedRef<SWidget> FContentBrowserSingleton::CreateAssetPicker(const FAssetPickerConfig& AssetPickerConfig)
{
	return SNew( SAssetPicker )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		.AssetPickerConfig(AssetPickerConfig);
}

TSharedRef<SWidget> FContentBrowserSingleton::CreatePathPicker(const FPathPickerConfig& PathPickerConfig)
{
	return SNew( SPathPicker )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		.PathPickerConfig(PathPickerConfig);
}

TSharedRef<class SWidget> FContentBrowserSingleton::CreateCollectionPicker(const FCollectionPickerConfig& CollectionPickerConfig)
{
	return SNew( SCollectionPicker )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		.CollectionPickerConfig(CollectionPickerConfig);
}

void FContentBrowserSingleton::CreateOpenAssetDialog(const FOpenAssetDialogConfig& InConfig,
													 const FOnAssetsChosenForOpen& InOnAssetsChosenForOpen,
													 const FOnAssetDialogCancelled& InOnAssetDialogCancelled)
{
	const bool bModal = false;
	TSharedRef<SAssetDialog> AssetDialog = SNew(SAssetDialog, InConfig);
	AssetDialog->SetOnAssetsChosenForOpen(InOnAssetsChosenForOpen);
	AssetDialog->SetOnAssetDialogCancelled(InOnAssetDialogCancelled);
	SharedCreateAssetDialogWindow(AssetDialog, InConfig, bModal);
}

TArray<FAssetData> FContentBrowserSingleton::CreateModalOpenAssetDialog(const FOpenAssetDialogConfig& InConfig)
{
	struct FModalResults
	{
		void OnAssetsChosenForOpen(const TArray<FAssetData>& SelectedAssets)
		{
			SavedResults = SelectedAssets;
		}

		TArray<FAssetData> SavedResults;
	};

	FModalResults ModalWindowResults;
	FOnAssetsChosenForOpen OnAssetsChosenForOpenDelegate = FOnAssetsChosenForOpen::CreateRaw(&ModalWindowResults, &FModalResults::OnAssetsChosenForOpen);

	const bool bModal = true;
	TSharedRef<SAssetDialog> AssetDialog = SNew(SAssetDialog, InConfig);
	AssetDialog->SetOnAssetsChosenForOpen(OnAssetsChosenForOpenDelegate);
	SharedCreateAssetDialogWindow(AssetDialog, InConfig, bModal);

	return ModalWindowResults.SavedResults;
}

void FContentBrowserSingleton::CreateSaveAssetDialog(const FSaveAssetDialogConfig& InConfig,
													 const FOnObjectPathChosenForSave& InOnObjectPathChosenForSave,
													 const FOnAssetDialogCancelled& InOnAssetDialogCancelled)
{
	const bool bModal = false;
	TSharedRef<SAssetDialog> AssetDialog = SNew(SAssetDialog, InConfig);
	AssetDialog->SetOnObjectPathChosenForSave(InOnObjectPathChosenForSave);
	AssetDialog->SetOnAssetDialogCancelled(InOnAssetDialogCancelled);
	SharedCreateAssetDialogWindow(AssetDialog, InConfig, bModal);
}

FString FContentBrowserSingleton::CreateModalSaveAssetDialog(const FSaveAssetDialogConfig& InConfig)
{
	struct FModalResults
	{
		void OnObjectPathChosenForSave(const FString& ObjectPath)
		{
			SavedResult = ObjectPath;
		}

		FString SavedResult;
	};

	FModalResults ModalWindowResults;
	FOnObjectPathChosenForSave OnObjectPathChosenForSaveDelegate = FOnObjectPathChosenForSave::CreateRaw(&ModalWindowResults, &FModalResults::OnObjectPathChosenForSave);

	const bool bModal = true;
	TSharedRef<SAssetDialog> AssetDialog = SNew(SAssetDialog, InConfig);
	AssetDialog->SetOnObjectPathChosenForSave(OnObjectPathChosenForSaveDelegate);
	SharedCreateAssetDialogWindow(AssetDialog, InConfig, bModal);

	return ModalWindowResults.SavedResult;
}

bool FContentBrowserSingleton::HasPrimaryContentBrowser() const
{
	if ( PrimaryContentBrowser.IsValid() )
	{
		// There is a primary content browser
		return true;
	}
	else
	{
		for (int32 BrowserIdx = 0; BrowserIdx < AllContentBrowsers.Num(); ++BrowserIdx)
		{
			if ( AllContentBrowsers[BrowserIdx].IsValid() )
			{
				// There is at least one valid content browser
				return true;
			}
		}

		// There were no valid content browsers
		return false;
	}
}

void FContentBrowserSingleton::FocusPrimaryContentBrowser(bool bFocusSearch)
{
	// See if the primary content browser is still valid
	if ( !PrimaryContentBrowser.IsValid() )
	{
		ChooseNewPrimaryBrowser();
	}

	if ( PrimaryContentBrowser.IsValid() )
	{
		FocusContentBrowser( PrimaryContentBrowser.Pin() );
	}
	else
	{
		// If we couldn't find a primary content browser, open one
		SummonNewBrowser();
	}

	// Do we also want to focus on the search box of the content browser?
	if ( bFocusSearch && PrimaryContentBrowser.IsValid() )
	{
		PrimaryContentBrowser.Pin()->SetKeyboardFocusOnSearch();
	}
}

void FContentBrowserSingleton::CreateNewAsset(const FString& DefaultAssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory)
{
	FocusPrimaryContentBrowser(false);

	if ( PrimaryContentBrowser.IsValid() )
	{
		PrimaryContentBrowser.Pin()->CreateNewAsset(DefaultAssetName, PackagePath, AssetClass, Factory);
	}
}

TSharedPtr<SContentBrowser> FContentBrowserSingleton::FindContentBrowserToSync(bool bAllowLockedBrowsers)
{
	TSharedPtr<SContentBrowser> ContentBrowserToSync;

	if ( !PrimaryContentBrowser.IsValid() )
	{
		ChooseNewPrimaryBrowser();
	}

	if ( PrimaryContentBrowser.IsValid() && (bAllowLockedBrowsers || !PrimaryContentBrowser.Pin()->IsLocked()) )
	{
		// If the primary content browser is not locked, sync it
		ContentBrowserToSync = PrimaryContentBrowser.Pin();
	}
	else
	{
		// If there is no primary or it is locked, find the first non-locked valid browser
		for (int32 BrowserIdx = 0; BrowserIdx < AllContentBrowsers.Num(); ++BrowserIdx)
		{
			if ( AllContentBrowsers[BrowserIdx].IsValid() && (bAllowLockedBrowsers || !AllContentBrowsers[BrowserIdx].Pin()->IsLocked()) )
			{
				ContentBrowserToSync = AllContentBrowsers[BrowserIdx].Pin();
				break;
			}
		}
	}

	if ( !ContentBrowserToSync.IsValid() )
	{
		// There are no valid, unlocked browsers, attempt to summon a new one.
		SummonNewBrowser(bAllowLockedBrowsers);

		// Now try to find a non-locked valid browser again, now that a new one may exist
		for (int32 BrowserIdx = 0; BrowserIdx < AllContentBrowsers.Num(); ++BrowserIdx)
		{
			if ( AllContentBrowsers[BrowserIdx].IsValid() && (bAllowLockedBrowsers || !AllContentBrowsers[BrowserIdx].Pin()->IsLocked()) )
			{
				ContentBrowserToSync = AllContentBrowsers[BrowserIdx].Pin();
				break;
			}
		}
	}

	if ( !ContentBrowserToSync.IsValid() )
	{
		UE_LOG( LogContentBrowser, Log, TEXT( "Unable to sync content browser, all browsers appear to be locked" ) );
	}

	return ContentBrowserToSync;
}

void FContentBrowserSingleton::SyncBrowserToAssets(const TArray<FAssetData>& AssetDataList, bool bAllowLockedBrowsers, bool bFocusContentBrowser)
{
	TSharedPtr<SContentBrowser> ContentBrowserToSync = FindContentBrowserToSync(bAllowLockedBrowsers);

	if ( ContentBrowserToSync.IsValid() )
	{
		// Finally, focus and sync the browser that was found
		if (bFocusContentBrowser)
		{
			FocusContentBrowser(ContentBrowserToSync);
		}
		ContentBrowserToSync->SyncToAssets(AssetDataList);
	}
}

void FContentBrowserSingleton::SyncBrowserToAssets(const TArray<UObject*>& AssetList, bool bAllowLockedBrowsers, bool bFocusContentBrowser)
{
	// Convert UObject* array to FAssetData array
	TArray<FAssetData> AssetDataList;
	for (int32 AssetIdx = 0; AssetIdx < AssetList.Num(); ++AssetIdx)
	{
		if ( AssetList[AssetIdx] )
		{
			AssetDataList.Add( FAssetData(AssetList[AssetIdx]) );
		}
	}

	SyncBrowserToAssets(AssetDataList, bAllowLockedBrowsers, bFocusContentBrowser);
}

void FContentBrowserSingleton::SyncBrowserToFolders(const TArray<FString>& FolderList, bool bAllowLockedBrowsers, bool bFocusContentBrowser)
{
	TSharedPtr<SContentBrowser> ContentBrowserToSync = FindContentBrowserToSync(bAllowLockedBrowsers);

	if ( ContentBrowserToSync.IsValid() )
	{
		// Finally, focus and sync the browser that was found
		if (bFocusContentBrowser)
		{
			FocusContentBrowser(ContentBrowserToSync);
		}
		ContentBrowserToSync->SyncToFolders(FolderList);
	}
}

void FContentBrowserSingleton::SyncBrowserTo(const FContentBrowserSelection& ItemSelection, bool bAllowLockedBrowsers, bool bFocusContentBrowser)
{
	TSharedPtr<SContentBrowser> ContentBrowserToSync = FindContentBrowserToSync(bAllowLockedBrowsers);

	if ( ContentBrowserToSync.IsValid() )
	{
		// Finally, focus and sync the browser that was found
		if (bFocusContentBrowser)
		{
			FocusContentBrowser(ContentBrowserToSync);
		}
		ContentBrowserToSync->SyncTo(ItemSelection);
	}
}

void FContentBrowserSingleton::GetSelectedAssets(TArray<FAssetData>& SelectedAssets)
{
	if ( PrimaryContentBrowser.IsValid() )
	{
		PrimaryContentBrowser.Pin()->GetSelectedAssets(SelectedAssets);
	}
}

void FContentBrowserSingleton::CaptureThumbnailFromViewport(FViewport* InViewport, TArray<FAssetData>& SelectedAssets)
{
	ContentBrowserUtils::CaptureThumbnailFromViewport(InViewport, SelectedAssets);
}


void FContentBrowserSingleton::OnEditorLoadSelectedAssetsIfNeeded()
{
	if ( PrimaryContentBrowser.IsValid() )
	{
		PrimaryContentBrowser.Pin()->LoadSelectedObjectsIfNeeded();
	}
}

FContentBrowserSingleton& FContentBrowserSingleton::Get()
{
	static const FName ModuleName = "ContentBrowser";
	FContentBrowserModule& Module = FModuleManager::GetModuleChecked<FContentBrowserModule>(ModuleName);
	return static_cast<FContentBrowserSingleton&>(Module.Get());
}

void FContentBrowserSingleton::SetPrimaryContentBrowser(const TSharedRef<SContentBrowser>& NewPrimaryBrowser)
{
	if ( PrimaryContentBrowser.IsValid() && PrimaryContentBrowser.Pin().ToSharedRef() == NewPrimaryBrowser )
	{
		// This is already the primary content browser
		return;
	}

	if ( PrimaryContentBrowser.IsValid() )
	{
		PrimaryContentBrowser.Pin()->SetIsPrimaryContentBrowser(false);
	}

	PrimaryContentBrowser = NewPrimaryBrowser;
	NewPrimaryBrowser->SetIsPrimaryContentBrowser(true);
}

void FContentBrowserSingleton::ContentBrowserClosed(const TSharedRef<SContentBrowser>& ClosedBrowser)
{
	// Find the browser in the all browsers list
	for (int32 BrowserIdx = AllContentBrowsers.Num() - 1; BrowserIdx >= 0; --BrowserIdx)
	{
		if ( !AllContentBrowsers[BrowserIdx].IsValid() || AllContentBrowsers[BrowserIdx].Pin() == ClosedBrowser )
		{
			AllContentBrowsers.RemoveAt(BrowserIdx);
		}
	}

	if ( !PrimaryContentBrowser.IsValid() || ClosedBrowser == PrimaryContentBrowser.Pin() )
	{
		ChooseNewPrimaryBrowser();
	}

	BrowserToLastKnownTabManagerMap.Add(ClosedBrowser->GetInstanceName(), ClosedBrowser->GetTabManager());
}

TSharedRef<FNativeClassHierarchy> FContentBrowserSingleton::GetNativeClassHierarchy()
{
	if(!NativeClassHierarchy.IsValid())
	{
		NativeClassHierarchy = MakeShareable(new FNativeClassHierarchy());
	}
	return NativeClassHierarchy.ToSharedRef();
}

TSharedRef<FEmptyFolderVisibilityManager> FContentBrowserSingleton::GetEmptyFolderVisibilityManager()
{
	return EmptyFolderVisibilityManager;
}

void FContentBrowserSingleton::SharedCreateAssetDialogWindow(const TSharedRef<SAssetDialog>& AssetDialog, const FSharedAssetDialogConfig& InConfig, bool bModal) const
{
	const FVector2D DefaultWindowSize(1152.0f, 648.0f);
	const FVector2D WindowSize = InConfig.WindowSizeOverride.IsZero() ? DefaultWindowSize : InConfig.WindowSizeOverride;
	const FText WindowTitle = InConfig.DialogTitleOverride.IsEmpty() ? LOCTEXT("GenericAssetDialogWindowHeader", "Asset Dialog") : InConfig.DialogTitleOverride;

	TSharedRef<SWindow> DialogWindow =
		SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(WindowSize);

	DialogWindow->SetContent(AssetDialog);

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow>& MainFrameParentWindow = MainFrameModule.GetParentWindow();
	if (MainFrameParentWindow.IsValid())
	{
		if (bModal)
		{
			FSlateApplication::Get().AddModalWindow(DialogWindow, MainFrameParentWindow.ToSharedRef());
		}
		else if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(DialogWindow, MainFrameParentWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(DialogWindow);
		}
	}
	else
	{
		if (ensureMsgf(!bModal, TEXT("Could not create asset dialog because modal windows must have a parent and this was called at a time where the mainframe window does not exist.")))
		{
			FSlateApplication::Get().AddWindow(DialogWindow);
		}
	}
}

void FContentBrowserSingleton::ChooseNewPrimaryBrowser()
{
	// Find the first valid browser and trim any invalid browsers along the way
	for (int32 BrowserIdx = 0; BrowserIdx < AllContentBrowsers.Num(); ++BrowserIdx)
	{
		if ( AllContentBrowsers[BrowserIdx].IsValid() )
		{
			SetPrimaryContentBrowser(AllContentBrowsers[BrowserIdx].Pin().ToSharedRef());
			break;
		}
		else
		{
			// Trim any invalid content browsers
			AllContentBrowsers.RemoveAt(BrowserIdx);
			BrowserIdx--;
		}
	}
}

void FContentBrowserSingleton::FocusContentBrowser(const TSharedPtr<SContentBrowser>& BrowserToFocus)
{
	if ( BrowserToFocus.IsValid() )
	{
		TSharedRef<SContentBrowser> Browser = BrowserToFocus.ToSharedRef();
		TSharedPtr<FTabManager> TabManager = Browser->GetTabManager();
		if ( TabManager.IsValid() )
		{
			TabManager->InvokeTab(Browser->GetInstanceName());
		}
	}
}

void FContentBrowserSingleton::SummonNewBrowser(bool bAllowLockedBrowsers)
{
	TSet<FName> OpenBrowserIDs;

	// Find all currently open browsers to help find the first open slot
	for (int32 BrowserIdx = AllContentBrowsers.Num() - 1; BrowserIdx >= 0; --BrowserIdx)
	{
		const TWeakPtr<SContentBrowser>& Browser = AllContentBrowsers[BrowserIdx];
		if ( Browser.IsValid() )
		{
			OpenBrowserIDs.Add(Browser.Pin()->GetInstanceName());
		}
	}
	
	FName NewTabName;
	for ( int32 BrowserIdx = 0; BrowserIdx < ARRAY_COUNT(ContentBrowserTabIDs); BrowserIdx++ )
	{
		FName TestTabID = ContentBrowserTabIDs[BrowserIdx];
		if ( !OpenBrowserIDs.Contains(TestTabID) && (bAllowLockedBrowsers || !IsLocked(TestTabID)) )
		{
			// Found the first index that is not currently open
			NewTabName = TestTabID;
			break;
		}
	}

	if ( NewTabName != NAME_None )
	{
		const TWeakPtr<FTabManager>& TabManagerToInvoke = BrowserToLastKnownTabManagerMap.FindRef(NewTabName);
		if ( TabManagerToInvoke.IsValid() )
		{
			TabManagerToInvoke.Pin()->InvokeTab(NewTabName);
		}
		else
		{
			FGlobalTabmanager::Get()->InvokeTab(NewTabName);
		}
	}
	else
	{
		// No available slots... don't summon anything
	}
}

TSharedRef<SWidget> FContentBrowserSingleton::CreateContentBrowser( const FName InstanceName, TSharedPtr<SDockTab> ContainingTab, const FContentBrowserConfig* ContentBrowserConfig )
{
	TSharedRef<SContentBrowser> NewBrowser =
		SNew( SContentBrowser, InstanceName, ContentBrowserConfig )
		.IsEnabled( FSlateApplication::Get().GetNormalExecutionAttribute() )
		.ContainingTab( ContainingTab );

	AllContentBrowsers.Add( NewBrowser );

	if( !PrimaryContentBrowser.IsValid() )
	{
		ChooseNewPrimaryBrowser();
	}

	return NewBrowser;
}


TSharedRef<SDockTab> FContentBrowserSingleton::SpawnContentBrowserTab( const FSpawnTabArgs& SpawnTabArgs, int32 BrowserIdx )
{	
	TAttribute<FText> Label = TAttribute<FText>::Create( TAttribute<FText>::FGetter::CreateRaw( this, &FContentBrowserSingleton::GetContentBrowserTabLabel, BrowserIdx ) );

	TSharedRef<SDockTab> NewTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label( Label )
		.ToolTip( IDocumentation::Get()->CreateToolTip( Label, nullptr, "Shared/ContentBrowser", "Tab" ) );

	TSharedRef<SWidget> NewBrowser = CreateContentBrowser( SpawnTabArgs.GetTabId().TabType, NewTab, nullptr );

	if ( !PrimaryContentBrowser.IsValid() )
	{
		ChooseNewPrimaryBrowser();
	}

	// Add wrapper for tutorial highlighting
	TSharedRef<SBox> Wrapper =
		SNew(SBox)
		.AddMetaData<FTutorialMetaData>(FTutorialMetaData(TEXT("ContentBrowser"), TEXT("ContentBrowserTab1")))
		[
			NewBrowser
		];

	NewTab->SetContent( Wrapper );

	return NewTab;
}

bool FContentBrowserSingleton::IsLocked(const FName& InstanceName) const
{
	// First try all the open browsers, as their locked state might be newer than the configs
	for (int32 AllBrowsersIdx = AllContentBrowsers.Num() - 1; AllBrowsersIdx >= 0; --AllBrowsersIdx)
	{
		const TWeakPtr<SContentBrowser>& Browser = AllContentBrowsers[AllBrowsersIdx];
		if ( Browser.IsValid() && Browser.Pin()->GetInstanceName() == InstanceName )
		{
			return Browser.Pin()->IsLocked();
		}
	}

	// Fallback to getting the locked state from the config instead
	bool bIsLocked = false;
	GConfig->GetBool(*SContentBrowser::SettingsIniSection, *(InstanceName.ToString() + TEXT(".Locked")), bIsLocked, GEditorPerProjectIni);
	return bIsLocked;
}

FText FContentBrowserSingleton::GetContentBrowserLabelWithIndex( int32 BrowserIdx )
{
	return FText::Format(LOCTEXT("ContentBrowserTabNameWithIndex", "Content Browser {0}"), FText::AsNumber(BrowserIdx + 1));
}

FText FContentBrowserSingleton::GetContentBrowserTabLabel(int32 BrowserIdx)
{
	int32 NumOpenContentBrowsers = 0;
	for (int32 AllBrowsersIdx = AllContentBrowsers.Num() - 1; AllBrowsersIdx >= 0; --AllBrowsersIdx)
	{
		const TWeakPtr<SContentBrowser>& Browser = AllContentBrowsers[AllBrowsersIdx];
		if ( Browser.IsValid() )
		{
			NumOpenContentBrowsers++;
		}
		else
		{
			AllContentBrowsers.RemoveAt(AllBrowsersIdx);
		}
	}

	if ( NumOpenContentBrowsers > 1 )
	{
		return GetContentBrowserLabelWithIndex( BrowserIdx );
	}
	else
	{
		return LOCTEXT("ContentBrowserTabName", "Content Browser");
	}
}

void FContentBrowserSingleton::SetSelectedPaths(const TArray<FString>& FolderPaths, bool bNeedsRefresh/* = false*/)
{
	// Make sure we have a valid browser
	if (!PrimaryContentBrowser.IsValid())
	{
		ChooseNewPrimaryBrowser();

		if (!PrimaryContentBrowser.IsValid())
		{
			SummonNewBrowser();
		}
	}

	if (PrimaryContentBrowser.IsValid())
	{
		PrimaryContentBrowser.Pin()->SetSelectedPaths(FolderPaths, bNeedsRefresh);
	}
}

void FContentBrowserSingleton::ForceShowPluginContent(bool bEnginePlugin)
{
	if (!PrimaryContentBrowser.IsValid())
	{
		ChooseNewPrimaryBrowser();

		if (!PrimaryContentBrowser.IsValid())
		{
			SummonNewBrowser();
		}
	}

	if (PrimaryContentBrowser.IsValid())
	{
		PrimaryContentBrowser.Pin()->ForceShowPluginContent(bEnginePlugin);
	}
}

#undef LOCTEXT_NAMESPACE
