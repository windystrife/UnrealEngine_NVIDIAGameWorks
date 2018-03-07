// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Toolkits/AssetEditorManager.h"
#include "MessageEndpoint.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Modules/ModuleManager.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "AssetEditorMessages.h"
#include "MessageEndpointBuilder.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "LevelEditor.h"
#include "PackageReload.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "AssetEditorManager"

FAssetEditorManager* FAssetEditorManager::Instance = NULL;


FAssetEditorManager& FAssetEditorManager::Get()
{
	if (Instance == NULL)
	{
		Instance = new FAssetEditorManager();
	}

	return *Instance;
}

FAssetEditorManager::FAssetEditorManager()
	: bSavingOnShutdown(false)
	, bRequestRestorePreviouslyOpenAssets(false)
{
	// Message bus to receive requests to load assets
	MessageEndpoint = FMessageEndpoint::Builder("FAssetEditorManager")
		.Handling<FAssetEditorRequestOpenAsset>(this, &FAssetEditorManager::HandleRequestOpenAssetMessage)
		.WithInbox();

	if (MessageEndpoint.IsValid())
	{
		MessageEndpoint->Subscribe<FAssetEditorRequestOpenAsset>();
	}

	TickDelegate = FTickerDelegate::CreateRaw(this, &FAssetEditorManager::HandleTicker);
	FTicker::GetCoreTicker().AddTicker(TickDelegate, 1.f);

	FCoreUObjectDelegates::OnPackageReloaded.AddRaw(this, &FAssetEditorManager::HandlePackageReloaded);
}

void FAssetEditorManager::OnExit()
{
	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll(this);

	SaveOpenAssetEditors(true);

	TGuardValue<bool> GuardOnShutdown(bSavingOnShutdown, true);

	CloseAllAssetEditors();

	// Don't attempt to report usage stats if analytics isn't available
	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> EditorUsageAttribs;
		EditorUsageAttribs.Empty(2);
		for (auto Iter = EditorUsageAnalytics.CreateConstIterator(); Iter; ++Iter)
		{
			const FAssetEditorAnalyticInfo& Data = Iter.Value();
			EditorUsageAttribs.Reset();
			EditorUsageAttribs.Emplace(TEXT("TotalDuration.Seconds"), FString::Printf(TEXT("%.1f"), Data.SumDuration.GetTotalSeconds()));
			EditorUsageAttribs.Emplace(TEXT("OpenedInstances.Count"), FString::Printf(TEXT("%d"), Data.NumTimesOpened));

			const FString EventName = FString::Printf(TEXT("Editor.Usage.%s"), *Iter.Key().ToString());
			FEngineAnalytics::GetProvider().RecordEvent(EventName, EditorUsageAttribs);
		}
	}
}

void FAssetEditorManager::AddReferencedObjects( FReferenceCollector& Collector )
{
	TMap<UObject*, UObject*> ModifiedKeys;
	for ( TMultiMap<UObject*, IAssetEditorInstance*>::TIterator It(OpenedAssets); It; ++It )
	{
		UObject* Asset = It.Key();
		if ( Asset != nullptr )
		{
			UObject* MutableReference = Asset;
			Collector.AddReferencedObject(MutableReference);
			if ( MutableReference != Asset )
			{
				ModifiedKeys.Add(Asset, MutableReference);
			}
		}
	}

	// If the pointer got deleted or swapped out due to hot reload.
	for ( auto& Entry : ModifiedKeys )
	{
		if ( Entry.Value )
		{
			// Find the existing editor instances bound to a modified object ptr.
			TArray<IAssetEditorInstance*> AssetEditors;
			OpenedAssets.MultiFind(Entry.Key, AssetEditors);

			// Remove the entry for the remapped pointer.
			OpenedAssets.Remove(Entry.Key);

			// Store all new instances for the moved pointer.
			for ( IAssetEditorInstance* AssetEditor : AssetEditors )
			{
				OpenedAssets.Add(Entry.Value, AssetEditor);
			}
		}
	}

	for (TMultiMap<IAssetEditorInstance*, UObject*>::TIterator It(OpenedEditors); It; ++It)
	{
		UObject* Asset = It->Value;
		if ( Asset != nullptr )
		{
			Collector.AddReferencedObject(It->Value);
		}
	}
}


IAssetEditorInstance* FAssetEditorManager::FindEditorForAsset(UObject* Asset, bool bFocusIfOpen)
{
	const TArray<IAssetEditorInstance*> AssetEditors = FindEditorsForAsset(Asset);

	IAssetEditorInstance* const * PrimaryEditor = AssetEditors.FindByPredicate( [](IAssetEditorInstance* Editor){ return Editor->IsPrimaryEditor(); } );
	
	const bool bEditorOpen = PrimaryEditor != NULL;
	if (bEditorOpen && bFocusIfOpen)
	{
		// @todo toolkit minor: We may need to handle this differently for world-centric vs standalone.  (multiple level editors, etc)
		(*PrimaryEditor)->FocusWindow(Asset);
	}

	return bEditorOpen ? *PrimaryEditor : NULL;
}


TArray<IAssetEditorInstance*> FAssetEditorManager::FindEditorsForAsset(UObject* Asset)
{
	TArray<IAssetEditorInstance*> AssetEditors;
	OpenedAssets.MultiFind(Asset, AssetEditors);
	return AssetEditors;
}


void FAssetEditorManager::CloseAllEditorsForAsset(UObject* Asset)
{
	TArray<IAssetEditorInstance*> EditorInstances = FindEditorsForAsset(Asset);

	for( auto EditorIter : EditorInstances )
	{
		EditorIter->CloseWindow();
	}
}

void FAssetEditorManager::RemoveAssetFromAllEditors(UObject* Asset)
{
	TArray<IAssetEditorInstance*> EditorInstances = FindEditorsForAsset(Asset);

	for (auto EditorIter : EditorInstances)
	{
		EditorIter->RemoveEditingAsset(Asset);
	}
}


void FAssetEditorManager::CloseOtherEditors( UObject* Asset, IAssetEditorInstance* OnlyEditor)
{
	TArray<UObject*> AllAssets;
	for (TMultiMap<UObject*, IAssetEditorInstance*>::TIterator It(OpenedAssets); It; ++It)
	{
		IAssetEditorInstance* Editor = It.Value();
		if(Asset == It.Key() && Editor != OnlyEditor)
		{
			Editor->CloseWindow();
		}
	}
}


TArray<UObject*> FAssetEditorManager::GetAllEditedAssets()
{
	TArray<UObject*> AllAssets;
	for (TMultiMap<UObject*, IAssetEditorInstance*>::TIterator It(OpenedAssets); It; ++It)
	{
		UObject* Asset = It.Key();
		if(Asset != NULL)
		{
			AllAssets.AddUnique(Asset);
		}
	}
	return AllAssets;
}


void FAssetEditorManager::NotifyAssetOpened(UObject* Asset, IAssetEditorInstance* InInstance)
{
	if (!OpenedEditors.Contains(InInstance))
	{
		FOpenedEditorTime EditorTime;
		EditorTime.EditorName = InInstance->GetEditorName();
		EditorTime.OpenedTime = FDateTime::UtcNow();

		OpenedEditorTimes.Add(InInstance, EditorTime);
	}

	OpenedAssets.Add(Asset, InInstance);
	OpenedEditors.Add(InInstance, Asset);

	AssetOpenedInEditorEvent.Broadcast(Asset, InInstance);

	SaveOpenAssetEditors(false);
}


void FAssetEditorManager::NotifyAssetsOpened( const TArray< UObject* >& Assets, IAssetEditorInstance* InInstance)
{
	for( auto AssetIter = Assets.CreateConstIterator(); AssetIter; ++AssetIter )
	{
		NotifyAssetOpened(*AssetIter, InInstance);
	}
}


void FAssetEditorManager::NotifyAssetClosed(UObject* Asset, IAssetEditorInstance* InInstance)
{
	OpenedEditors.RemoveSingle(InInstance, Asset);
	OpenedAssets.RemoveSingle(Asset, InInstance);

	SaveOpenAssetEditors(false);
}


void FAssetEditorManager::NotifyEditorClosed(IAssetEditorInstance* InInstance)
{
	// Remove all assets associated with the editor
	TArray<UObject*> Assets;
	OpenedEditors.MultiFind(InInstance, /*out*/ Assets);
	for (int32 AssetIndex = 0; AssetIndex < Assets.Num(); ++AssetIndex)
	{
		OpenedAssets.Remove(Assets[AssetIndex], InInstance);
	}

	// Remove the editor itself
	OpenedEditors.Remove(InInstance);
	FOpenedEditorTime EditorTime = OpenedEditorTimes.FindAndRemoveChecked(InInstance);

	// Record the editor open-close duration
	FAssetEditorAnalyticInfo& AnalyticsForThisAsset = EditorUsageAnalytics.FindOrAdd(EditorTime.EditorName);
	AnalyticsForThisAsset.SumDuration += FDateTime::UtcNow() - EditorTime.OpenedTime;
	AnalyticsForThisAsset.NumTimesOpened++;

	SaveOpenAssetEditors(false);
}


bool FAssetEditorManager::CloseAllAssetEditors()
{
	bool bAllEditorsClosed = true;
	for (TMultiMap<IAssetEditorInstance*, UObject*>::TIterator It(OpenedEditors); It; ++It)
	{
		IAssetEditorInstance* Editor = It.Key();
		if (Editor != NULL)
		{
			if ( !Editor->CloseWindow() )
			{
				bAllEditorsClosed = false;
			}
		}
	}

	return bAllEditorsClosed;
}


bool FAssetEditorManager::OpenEditorForAsset(UObject* Asset, const EToolkitMode::Type ToolkitMode, TSharedPtr< IToolkitHost > OpenedFromLevelEditor )
{
	check(Asset);
	// @todo toolkit minor: When "Edit Here" happens in a different level editor from the one that an asset is already
	//    being edited within, we should decide whether to disallow "Edit Here" in that case, or to close the old asset
	//    editor and summon it in the new level editor, or to just foreground the old level editor (current behavior)

	const bool bBringToFrontIfOpen = true;

	// Don't open asset editors for cooked packages
	if (UPackage* Package = Asset->GetOutermost())
	{
		if (Package->bIsCookedForEditor)
		{
			return false;
		}
	}
	
	AssetEditorRequestOpenEvent.Broadcast(Asset);

	if( FindEditorForAsset(Asset, bBringToFrontIfOpen) != nullptr )
	{
		// This asset is already open in an editor! (the call to FindEditorForAsset above will bring it to the front)
		return true;
	}
	else
	{
		GWarn->BeginSlowTask( LOCTEXT("OpenEditor", "Opening Editor..."), true);
	}


	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));

	TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass( Asset->GetClass() );
	
	auto ActualToolkitMode = ToolkitMode;
	if( AssetTypeActions.IsValid() )
	{
		if( AssetTypeActions.Pin()->ShouldForceWorldCentric() )
		{
			// This asset type prefers a specific toolkit mode
			ActualToolkitMode = EToolkitMode::WorldCentric;

			if( !OpenedFromLevelEditor.IsValid() )
			{
				// We don't have a level editor to spawn in world-centric mode, so we'll find one now
				// @todo sequencer: We should eventually eliminate this code (incl include dependencies) or change it to not make assumptions about a single level editor
				OpenedFromLevelEditor = FModuleManager::LoadModuleChecked< FLevelEditorModule >( "LevelEditor" ).GetFirstLevelEditor();
			}
		}
	}
	
	if( ActualToolkitMode != EToolkitMode::WorldCentric && OpenedFromLevelEditor.IsValid() )
	{
		// @todo toolkit minor: Kind of lame use of a static variable here to prime the new asset editor.  This was done to avoid refactoring a few dozen files for a very minor change.
		FAssetEditorToolkit::SetPreviousWorldCentricToolkitHostForNewAssetEditor( OpenedFromLevelEditor.ToSharedRef() );
	}

	// Disallow opening an asset editor for classes
	bool bCanSummonSimpleAssetEditor = !Asset->IsA<UClass>();

	if( AssetTypeActions.IsValid() )
	{
		TArray<UObject*> AssetsToEdit;
		AssetsToEdit.Add(Asset);

		// Some assets (like UWorlds) may be destroyed and recreated as part of opening. To protect against this, keep the path to the asset and try to re-find it if it disappeared.
		TWeakObjectPtr<UObject> WeakAsset = Asset;
		const FString AssetPath = Asset->GetPathName();

		AssetTypeActions.Pin()->OpenAssetEditor(AssetsToEdit, ActualToolkitMode == EToolkitMode::WorldCentric ? OpenedFromLevelEditor : TSharedPtr<IToolkitHost>());
		
		// If the Asset was destroyed, attempt to find it if it was recreated
		if ( !WeakAsset.IsValid() && !AssetPath.IsEmpty() )
		{
			Asset = FindObject<UObject>(nullptr, *AssetPath);
		}

		AssetEditorOpenedEvent.Broadcast(Asset);
	}
	else if( bCanSummonSimpleAssetEditor )
	{
		// No asset type actions for this asset. Just use a properties editor.
		FSimpleAssetEditor::CreateEditor(ActualToolkitMode, ActualToolkitMode == EToolkitMode::WorldCentric ? OpenedFromLevelEditor : TSharedPtr<IToolkitHost>(), Asset);
	}

	GWarn->EndSlowTask();
	return true;
}


bool FAssetEditorManager::OpenEditorForAssets( const TArray< UObject* >& Assets, const EToolkitMode::Type ToolkitMode, TSharedPtr< IToolkitHost > OpenedFromLevelEditor )
{
	if( Assets.Num() == 1 )
	{
		return OpenEditorForAsset(Assets[0], ToolkitMode, OpenedFromLevelEditor);
	}
	else if (Assets.Num() > 0)
	{
		TArray<UObject*> SkipOpenAssets;
		for (auto Asset : Assets)
		{
			// If any of the assets are already open or the package is cooked,
			// remove them from the list of assets to open an editor for
			UPackage* Package = Asset->GetOutermost();
			if( FindEditorForAsset(Asset, true) != nullptr || (Package && Package->bIsCookedForEditor) )
			{
				SkipOpenAssets.Add(Asset);
			}
		}

		// Verify that all the assets are of the same class
		bool bAssetClassesMatch = true;
		auto AssetClass = Assets[0]->GetClass();
		for (int32 i = 1; i < Assets.Num(); i++)
		{
			if (Assets[i]->GetClass() != AssetClass)
			{
				bAssetClassesMatch = false;
				break;
			}
		}

		// If the classes don't match or any of the selected assets are already open, just open each asset in its own editor.
		if (bAssetClassesMatch && SkipOpenAssets.Num() == 0)
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetClass);

			if (AssetTypeActions.IsValid())
			{
				GWarn->BeginSlowTask(LOCTEXT("OpenEditors", "Opening Editor(s)..."), true);

				// Determine the appropriate toolkit mode for the asset type
				auto ActualToolkitMode = ToolkitMode;
				if (AssetTypeActions.Pin()->ShouldForceWorldCentric())
				{
					// This asset type prefers a specific toolkit mode
					ActualToolkitMode = EToolkitMode::WorldCentric;

					if (!OpenedFromLevelEditor.IsValid())
					{
						// We don't have a level editor to spawn in world-centric mode, so we'll find one now
						// @todo sequencer: We should eventually eliminate this code (incl include dependencies) or change it to not make assumptions about a single level editor
						OpenedFromLevelEditor = FModuleManager::LoadModuleChecked< FLevelEditorModule >("LevelEditor").GetFirstLevelEditor();
					}
				}

				if (ActualToolkitMode != EToolkitMode::WorldCentric && OpenedFromLevelEditor.IsValid())
				{
					// @todo toolkit minor: Kind of lame use of a static variable here to prime the new asset editor.  This was done to avoid refactoring a few dozen files for a very minor change.
					FAssetEditorToolkit::SetPreviousWorldCentricToolkitHostForNewAssetEditor(OpenedFromLevelEditor.ToSharedRef());
				}

				// Some assets (like UWorlds) may be destroyed and recreated as part of opening. To protect against this, keep the path to each asset and try to re-find any if they disappear.
				struct FLocalAssetInfo
				{
					TWeakObjectPtr<UObject> WeakAsset;
					FString AssetPath;

					FLocalAssetInfo(const TWeakObjectPtr<UObject>& InWeakAsset, const FString& InAssetPath)
						: WeakAsset(InWeakAsset), AssetPath(InAssetPath) {}
				};

				TArray<FLocalAssetInfo> AssetInfoList;
				AssetInfoList.Reserve(Assets.Num());
				for (auto Asset : Assets)
				{
					AssetInfoList.Add(FLocalAssetInfo(Asset, Asset->GetPathName()));
				}

				// How to handle multiple assets is left up to the type actions (i.e. open a single shared editor or an editor for each)
				AssetTypeActions.Pin()->OpenAssetEditor(Assets, ActualToolkitMode == EToolkitMode::WorldCentric ? OpenedFromLevelEditor : TSharedPtr<IToolkitHost>());

				// If any assets were destroyed, attempt to find them if they were recreated
				for (int32 i = 0; i < Assets.Num(); i++)
				{
					auto& AssetInfo = AssetInfoList[i];
					auto Asset = Assets[i];

					if (!AssetInfo.WeakAsset.IsValid() && !AssetInfo.AssetPath.IsEmpty())
					{
						Asset = FindObject<UObject>(nullptr, *AssetInfo.AssetPath);
					}
				}

				//@todo if needed, broadcast the event for every asset. It is possible, however, that a single shared editor was opened by the AssetTypeActions, not an editor for each asset.
				/*AssetEditorOpenedEvent.Broadcast(Asset);*/

				GWarn->EndSlowTask();
			}
		}
		else
		{
			// Asset types don't match or some are already open, so just open individual editors for the unopened ones
			for (auto Asset : Assets)
			{
				if (!SkipOpenAssets.Contains(Asset))
				{
					OpenEditorForAsset(Asset, ToolkitMode, OpenedFromLevelEditor);
				}
			}
		}
	}

	return true;
}


FArchive& operator<<(FArchive& Ar,IAssetEditorInstance*& TypeRef)
{
	return Ar;
}


void FAssetEditorManager::HandleRequestOpenAssetMessage( const FAssetEditorRequestOpenAsset& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context )
{
	OpenEditorForAsset(Message.AssetName);
}

void FAssetEditorManager::OpenEditorForAsset(const FString& AssetPathName)
{
	// An asset needs loading
	UPackage* Package = LoadPackage(NULL, *AssetPathName, LOAD_NoRedirects);

	if (Package)
	{
		Package->FullyLoad();

		FString AssetName = FPaths::GetBaseFilename(AssetPathName);
		UObject* Object = FindObject<UObject>(Package, *AssetName);

		if (Object != NULL)
		{
			OpenEditorForAsset(Object);
		}
	}
}

bool FAssetEditorManager::HandleTicker( float DeltaTime )
{
	if (bRequestRestorePreviouslyOpenAssets)
	{
		RestorePreviouslyOpenAssets();
		bRequestRestorePreviouslyOpenAssets = false;
	}
	MessageEndpoint->ProcessInbox();

	return true;
}

void FAssetEditorManager::RequestRestorePreviouslyOpenAssets()
{
	// We defer the restore so that we guarantee that it happens once initialization is complete
	bRequestRestorePreviouslyOpenAssets = true;
}

void FAssetEditorManager::RestorePreviouslyOpenAssets()
{
	TArray<FString> OpenAssets;
	GConfig->GetArray(TEXT("AssetEditorManager"), TEXT("OpenAssetsAtExit"), OpenAssets, GEditorPerProjectIni);

	bool bCleanShutdown =  false;
	GConfig->GetBool(TEXT("AssetEditorManager"), TEXT("CleanShutdown"), bCleanShutdown, GEditorPerProjectIni);

	SaveOpenAssetEditors(false);

	if(OpenAssets.Num() > 0)
	{
		if(bCleanShutdown)
		{
			// Do we have permission to automatically re-open the assets, or should we ask?
			const bool bAutoRestore = GetDefault<UEditorLoadingSavingSettings>()->bRestoreOpenAssetTabsOnRestart;

			if(bAutoRestore)
			{
				// Pretend that we showed the notification and that the user clicked "Restore Now"
				OpenEditorsForAssets(OpenAssets);
			}
			else
			{
				// Has this notification previously been suppressed by the user?
				bool bSuppressNotification = false;
				GConfig->GetBool(TEXT("AssetEditorManager"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);

				if(!bSuppressNotification)
				{
					// Ask the user; this doesn't block so will reopen the assets later
					SpawnRestorePreviouslyOpenAssetsNotification(bCleanShutdown, OpenAssets);
				}
			}
		}
		else
		{
			// If we crashed, we always ask regardless of what the user previously said
			SpawnRestorePreviouslyOpenAssetsNotification(bCleanShutdown, OpenAssets);
		}
	}
}

void FAssetEditorManager::SpawnRestorePreviouslyOpenAssetsNotification(const bool bCleanShutdown, const TArray<FString>& AssetsToOpen)
{
	/** Utility functions for the notification which don't rely on the state from FAssetEditorManager */
	struct Local
	{
		static ECheckBoxState GetDontAskAgainCheckBoxState()
		{
			bool bSuppressNotification = false;
			GConfig->GetBool(TEXT("AssetEditorManager"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);
			return bSuppressNotification ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		}

		static void OnDontAskAgainCheckBoxStateChanged(ECheckBoxState NewState)
		{
			const bool bSuppressNotification = (NewState == ECheckBoxState::Checked);
			GConfig->SetBool(TEXT("AssetEditorManager"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);
		}
	};

	FNotificationInfo Info(bCleanShutdown 
		? LOCTEXT("RestoreOpenAssetsAfterClose_Message", "Assets were open when the Editor was last closed, would you like to restore them now?")
		: LOCTEXT("RestoreOpenAssetsAfterCrash", "The Editor did not shut down cleanly, would you like to attempt to restore previously open assets now?")
		);

	// Add the buttons
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("RestoreOpenAssetsAfterClose_Confirm", "Restore Now"), 
		FText(), 
		FSimpleDelegate::CreateRaw(this, &FAssetEditorManager::OnConfirmRestorePreviouslyOpenAssets, AssetsToOpen), 
		SNotificationItem::CS_None
		));
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("RestoreOpenAssetsAfterClose_Cancel", "Don't Restore"), 
		FText(), 
		FSimpleDelegate::CreateRaw(this, &FAssetEditorManager::OnCancelRestorePreviouslyOpenAssets), 
		SNotificationItem::CS_None
		));

	// We will let the notification expire automatically after 10 seconds
	Info.bFireAndForget = false;
	Info.ExpireDuration = 10.0f;

	// We want the auto-save to be subtle
	Info.bUseLargeFont = false;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = false;

	// Only let the user suppress the non-crash version
	if(bCleanShutdown)
	{
		Info.CheckBoxState = TAttribute<ECheckBoxState>::Create(&Local::GetDontAskAgainCheckBoxState);
		Info.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic(&Local::OnDontAskAgainCheckBoxStateChanged);
		Info.CheckBoxText = NSLOCTEXT("ModalDialogs", "DefaultCheckBoxMessage", "Don't show this again");
	}

	// Close any existing notification
	TSharedPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotification = RestorePreviouslyOpenAssetsNotificationPtr.Pin();
	if(RestorePreviouslyOpenAssetsNotification.IsValid())
	{
		RestorePreviouslyOpenAssetsNotification->ExpireAndFadeout();
	}

	RestorePreviouslyOpenAssetsNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
}

void FAssetEditorManager::OnConfirmRestorePreviouslyOpenAssets(TArray<FString> AssetsToOpen)
{
	// Close any existing notification
	TSharedPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotification = RestorePreviouslyOpenAssetsNotificationPtr.Pin();
	if (RestorePreviouslyOpenAssetsNotification.IsValid())
	{
		RestorePreviouslyOpenAssetsNotification->SetExpireDuration(0.0f);
		RestorePreviouslyOpenAssetsNotification->SetFadeOutDuration(0.5f);
		RestorePreviouslyOpenAssetsNotification->ExpireAndFadeout();

		// If the user suppressed the notification for future sessions, make sure this is reflected in their settings
		// Note: We do that inside this if statement so that we only do it if we were showing a UI they could interact with
		bool bSuppressNotification = false;
		GConfig->GetBool(TEXT("AssetEditorManager"), TEXT("SuppressRestorePreviouslyOpenAssetsNotification"), bSuppressNotification, GEditorPerProjectIni);
		UEditorLoadingSavingSettings& Settings = *GetMutableDefault<UEditorLoadingSavingSettings>();
		Settings.bRestoreOpenAssetTabsOnRestart = bSuppressNotification;
		Settings.PostEditChange();

		// we do this inside the condition so that it can only be done once. 
		OpenEditorsForAssets(AssetsToOpen);

	}
}

void FAssetEditorManager::OnCancelRestorePreviouslyOpenAssets()
{
	// Close any existing notification
	TSharedPtr<SNotificationItem> RestorePreviouslyOpenAssetsNotification = RestorePreviouslyOpenAssetsNotificationPtr.Pin();
	if (RestorePreviouslyOpenAssetsNotification.IsValid())
	{
		RestorePreviouslyOpenAssetsNotification->SetExpireDuration(0.0f);
		RestorePreviouslyOpenAssetsNotification->SetFadeOutDuration(0.5f);
		RestorePreviouslyOpenAssetsNotification->ExpireAndFadeout();
	}
}

void FAssetEditorManager::SaveOpenAssetEditors(bool bOnShutdown)
{
	if(!bSavingOnShutdown)
	{
		TArray<FString> OpenAssets;

		// Don't save a list of assets to restore if we are running under a debugger
		if(!FPlatformMisc::IsDebuggerPresent())
		{
			for (auto EditorPair : OpenedEditors)
			{
				IAssetEditorInstance* Editor = EditorPair.Key;
				if (Editor != NULL)
				{
					UObject* EditedObject = EditorPair.Value;
					if(EditedObject != NULL)
					{
						// only record assets that have a valid saved package
						UPackage* Package = EditedObject->GetOutermost();
						if(Package != NULL && Package->GetFileSize() != 0)
						{
							OpenAssets.Add(EditedObject->GetPathName());
						}
					}
				}
			}
		}

		GConfig->SetArray(TEXT("AssetEditorManager"), TEXT("OpenAssetsAtExit"), OpenAssets, GEditorPerProjectIni);
		GConfig->SetBool(TEXT("AssetEditorManager"), TEXT("CleanShutdown"), bOnShutdown, GEditorPerProjectIni);
		GConfig->Flush(false, GEditorPerProjectIni);
	}
}

void FAssetEditorManager::HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
{
	static TArray<UObject*> PendingAssetsToOpen;

	if (InPackageReloadPhase == EPackageReloadPhase::PrePackageFixup)
	{
		TArray<UObject*> OldAssets;
		for (auto AssetEditorPair : OpenedAssets)
		{
			UObject* NewAsset = nullptr;
			if (InPackageReloadedEvent->GetRepointedObject(AssetEditorPair.Key, NewAsset))
			{
				OldAssets.Add(AssetEditorPair.Key);
				if (NewAsset)
				{
					PendingAssetsToOpen.AddUnique(NewAsset);
				}
			}
		}

		for (UObject* OldAsset : OldAssets)
		{
			CloseAllEditorsForAsset(OldAsset);
		}
	}

	if (InPackageReloadPhase == EPackageReloadPhase::PostBatchPostGC)
	{
		for (UObject* NewAsset : PendingAssetsToOpen)
		{
			OpenEditorForAsset(NewAsset);
		}
		PendingAssetsToOpen.Reset();
	}
}

void FAssetEditorManager::OpenEditorsForAssets(const TArray<FString>& AssetsToOpen)
{
	for (const FString& AssetName : AssetsToOpen)
	{
		OpenEditorForAsset(AssetName);
	}
}

void FAssetEditorManager::OpenEditorsForAssets(const TArray<FName>& AssetsToOpen)
{
	for (const FName AssetName : AssetsToOpen)
	{
		OpenEditorForAsset(AssetName.ToString());
	}
}

#undef LOCTEXT_NAMESPACE
