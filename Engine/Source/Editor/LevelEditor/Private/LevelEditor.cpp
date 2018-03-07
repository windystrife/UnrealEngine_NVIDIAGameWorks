// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LevelEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditorViewport.h"
#include "EditorModes.h"
#include "UnrealEdGlobals.h"
#include "Misc/ConfigCacheIni.h"
#include "LevelViewportLayout.h"
#include "SLevelEditor.h"
#include "LightmapResRatioAdjust.h"
#include "LevelEditorActions.h"
#include "LevelEditorModesActions.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructure.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"
#include "MessageLogModule.h"
#include "EditorViewportCommands.h"
#include "LevelViewportActions.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "ISlateReflectorModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "IIntroTutorials.h"
#include "Interfaces/IProjectManager.h"
#include "LevelViewportLayoutEntity.h"
#include "PixelInspectorModule.h"

// @todo Editor: remove this circular dependency
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "Misc/EngineBuildSettings.h"

#define LOCTEXT_NAMESPACE "LevelEditor"

IMPLEMENT_MODULE( FLevelEditorModule, LevelEditor );

const FName LevelEditorApp = FName(TEXT("LevelEditorApp"));
const FName MainFrame("MainFrame");

FLevelEditorModule::FLevelEditorModule()
	: ToggleImmersiveConsoleCommand(
	TEXT( "LevelEditor.ToggleImmersive" ),
	TEXT( "Toggle 'Immersive Mode' for the active level editing viewport" ),
	FConsoleCommandDelegate::CreateRaw( this, &FLevelEditorModule::ToggleImmersiveOnActiveLevelViewport ) )
{
}


class SProjectBadge : public SBox
{
public:
	SLATE_BEGIN_ARGS(SProjectBadge) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		FString ProjectNameWatermarkPrefix;
		GConfig->GetString(TEXT("LevelEditor"), TEXT("ProjectNameWatermarkPrefix"), /*out*/ ProjectNameWatermarkPrefix, GEditorPerProjectIni);

		FColor BadgeBackgroundColor = FColor::Black;
		GConfig->GetColor(TEXT("LevelEditor"), TEXT("ProjectBadgeBackgroundColor"), /*out*/ BadgeBackgroundColor, GEditorPerProjectIni);

		FColor BadgeTextColor = FColor(128,128,128,255);
		GConfig->GetColor(TEXT("LevelEditor"), TEXT("ProjectBadgeTextColor"), /*out*/ BadgeTextColor, GEditorPerProjectIni);

		const FString EngineVersionString = FEngineVersion::Current().ToString(FEngineVersion::Current().HasChangelist() ? EVersionComponent::Changelist : EVersionComponent::Patch);
		
		FFormatNamedArguments Args;

		Args.Add(TEXT("ProjectNameWatermarkPrefix"), FText::FromString(ProjectNameWatermarkPrefix));
		Args.Add(TEXT("Branch"), FEngineBuildSettings::IsPerforceBuild() ? FText::FromString(FApp::GetBranchName()) : FText::GetEmpty());
		Args.Add(TEXT("GameName"), FText::FromString(FString(FApp::GetProjectName())));
		Args.Add(TEXT("EngineVersion"), (GetDefault<UEditorPerProjectUserSettings>()->bDisplayEngineVersionInBadge) ? FText::FromString("(" + EngineVersionString + ")") : FText());

		FText RightContentText;
		FText RightContentTooltip;

		const EBuildConfigurations::Type BuildConfig = FApp::GetBuildConfiguration();
		if (BuildConfig != EBuildConfigurations::Shipping && BuildConfig != EBuildConfigurations::Development && BuildConfig != EBuildConfigurations::Unknown)
		{
			Args.Add(TEXT("Config"), EBuildConfigurations::ToText(BuildConfig));
			RightContentText = FText::Format(NSLOCTEXT("UnrealEditor", "TitleBarRightContentAndConfig", "{ProjectNameWatermarkPrefix} {GameName} [{Config}] {Branch} {EngineVersion}"), Args);
		}
		else
		{
			RightContentText = FText::Format(NSLOCTEXT("UnrealEditor", "TitleBarRightContent", "{ProjectNameWatermarkPrefix} {GameName} {Branch} {EngineVersion}"), Args);
		}

		// Create the tooltip showing more detailed information
		FFormatNamedArguments TooltipArgs;
		TooltipArgs.Add(TEXT("Version"), FText::FromString(EngineVersionString));
		TooltipArgs.Add(TEXT("Branch"), FText::FromString(FApp::GetBranchName()));
		TooltipArgs.Add(TEXT("BuildConfiguration"), EBuildConfigurations::ToText(BuildConfig));
		TooltipArgs.Add(TEXT("BuildDate"), FText::FromString(FApp::GetBuildDate()));
		RightContentTooltip = FText::Format(NSLOCTEXT("UnrealEditor", "TitleBarRightContentTooltip", "Version: {Version}\nBranch: {Branch}\nBuild Configuration: {BuildConfiguration}\nBuild Date: {BuildDate}"), TooltipArgs);

		SetToolTipText(RightContentTooltip);

		SBox::Construct(SBox::FArguments()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.0f, 0.0f, 2.0f, 0.0f))
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("SProjectBadge.BadgeShape"))
				.Padding(FMargin(10.0f, 2.5f))
				.BorderBackgroundColor(BadgeBackgroundColor)
				.VAlign(VAlign_Top)
				[
					SNew(STextBlock)
					.Text(RightContentText)
					.Visibility(EVisibility::HitTestInvisible)
					.TextStyle(FEditorStyle::Get(),"SProjectBadge.Text")
					.ColorAndOpacity(BadgeTextColor)
				]
			]);
	}

	FVector2D GetSizeLastFrame() const
	{
		return GetDesiredSize();
	}

	// SWidget interface
	virtual EWindowZone::Type GetWindowZoneOverride() const override
	{
		return EWindowZone::TitleBar;
	}
	// End of SWidget interface

private:
	FGeometry CachedGeometry;
};

static FMargin GetRoomForBadge(TWeakPtr<SProjectBadge> ProjBadge)
{
	return FMargin(8.0f, 0.0f, ProjBadge.Pin()->GetSizeLastFrame().X + 8.0f, 0.0f);
}

TSharedRef<SDockTab> FLevelEditorModule::SpawnLevelEditor( const FSpawnTabArgs& InArgs )
{
	TSharedRef<SDockTab> LevelEditorTab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.ContentPadding( FMargin(0) );

	SetLevelEditorInstanceTab(LevelEditorTab);
	TSharedPtr< SWindow > OwnerWindow = InArgs.GetOwnerWindow();
	
	if ( !OwnerWindow.IsValid() )
	{
		IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>( MainFrame );
		OwnerWindow = MainFrameModule.GetParentWindow();
	}

	if ( OwnerWindow.IsValid() )
	{
		TSharedPtr<SLevelEditor> LevelEditorTmp;
		LevelEditorTab->SetContent( SAssignNew(LevelEditorTmp, SLevelEditor ) );
		SetLevelEditorInstance(LevelEditorTmp);
		LevelEditorTmp->Initialize( LevelEditorTab, OwnerWindow.ToSharedRef() );

		GLevelEditorModeTools().RemoveDefaultMode( FBuiltinEditorModes::EM_Default );
		GLevelEditorModeTools().AddDefaultMode( FBuiltinEditorModes::EM_Placement );
		GLevelEditorModeTools().DeactivateAllModes();
		GLevelEditorModeTools().ActivateDefaultMode();

	}

	IIntroTutorials& IntroTutorials = FModuleManager::LoadModuleChecked<IIntroTutorials>(TEXT("IntroTutorials"));
	TSharedRef<SWidget> TutorialWidget = IntroTutorials.CreateTutorialsWidget(TEXT("LevelEditor"), OwnerWindow);

	TSharedRef<SProjectBadge> ProjectBadge = SNew(SProjectBadge);
	TAttribute<FMargin> BadgeSizeGetter = TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateStatic(&GetRoomForBadge, TWeakPtr<SProjectBadge>(ProjectBadge)));
		
	TSharedPtr< SWidget > RightContent=
		SNew( SHorizontalBox )

// Put the level editor stats/notification widgets on the main window title bar since we don't have a menu bar on OS X
#if PLATFORM_MAC
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			LevelEditorTab->GetRightContent()
		]
#endif

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(BadgeSizeGetter)
		.VAlign(VAlign_Center)
		[
			TutorialWidget
		];

	LevelEditorTab->SetRightContent( RightContent.ToSharedRef() );

	LevelEditorTab->SetBackgroundContent(
		ProjectBadge
	);
	
	
	return LevelEditorTab;
}


/**
 * Called right after the module's DLL has been loaded and the module object has been created
 */
void FLevelEditorModule::StartupModule()
{
	// Our command context bindings depend on having the mainframe loaded
	FModuleManager::LoadModuleChecked<IMainFrameModule>(MainFrame);

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	ModeBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	NotificationBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	// Note this must come before any tab spawning because that can create the SLevelEditor and attempt to map commands
	FEditorViewportCommands::Register();
	FLevelViewportCommands::Register();
	FLevelEditorCommands::Register();
	FLevelEditorModesCommands::Register();

	// Bind level editor commands shared across an instance
	BindGlobalLevelEditorCommands();

	FViewportTypeDefinition ViewportType = FViewportTypeDefinition::FromType<FLevelViewportLayoutEntity>(FLevelViewportCommands::Get().SetDefaultViewportType);
	RegisterViewportType("Default", ViewportType);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	FGlobalTabmanager::Get()->RegisterTabSpawner("LevelEditor", FOnSpawnTab::CreateRaw( this, &FLevelEditorModule::SpawnLevelEditor ) )
		.SetDisplayName( NSLOCTEXT("LevelEditor", "LevelEditorTab", "Level Editor") );

	FModuleManager::LoadModuleChecked<ISlateReflectorModule>("SlateReflector").RegisterTabSpawner(MenuStructure.GetDeveloperToolsMiscCategory());

	FModuleManager::LoadModuleChecked<FPixelInspectorModule>("PixelInspectorModule").RegisterTabSpawner(MenuStructure.GetDeveloperToolsMiscCategory());

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing("BuildAndSubmitErrors", LOCTEXT("BuildAndSubmitErrors", "Build and Submit Errors"));
}

/**
 * Called before the module is unloaded, right before the module object is destroyed.
 */
void FLevelEditorModule::ShutdownModule()
{
	IProjectManager::Get().OnTargetPlatformsForCurrentProjectChanged().RemoveAll(this);

	if(FModuleManager::Get().IsModuleLoaded("MessageLog"))
	{
		FMessageLogModule& MessageLogModule = FModuleManager::GetModuleChecked<FMessageLogModule>("MessageLog");
		MessageLogModule.UnregisterLogListing("BuildAndSubmitErrors");
	}

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();

	// Stop PIE/SIE before unloading the level editor module
	// Otherwise, when the module is reloaded, it's likely to be in a bad state
	if (GUnrealEd && GUnrealEd->PlayWorld)
	{
		GUnrealEd->EndPlayMap();
	}

	// If the level editor tab is currently open, close it
	{
		TSharedPtr<SDockTab> LevelEditorTab = LevelEditorInstanceTabPtr.Pin();
		if (LevelEditorTab.IsValid())
		{
			LevelEditorTab->RemoveTabFromParent();
		}
		LevelEditorInstanceTabPtr.Reset();
	}

	// Clear out some globals that may be referencing this module
	SetLevelEditorTabManager(nullptr);
	WorkspaceMenu::GetModule().ResetLevelEditorCategory();

	if (FSlateApplication::IsInitialized() && FModuleManager::Get().IsModuleLoaded("SlateReflector"))
	{
		FGlobalTabmanager::Get()->UnregisterTabSpawner("LevelEditor");
		FModuleManager::GetModuleChecked<ISlateReflectorModule>("SlateReflector").UnregisterTabSpawner();
	}	

	FLevelEditorCommands::Unregister();
	FLevelEditorModesCommands::Unregister();
	FEditorViewportCommands::Unregister();
	FLevelViewportCommands::Unregister();
}

/**
 * Spawns a new property viewer
 * @todo This only works with the first level editor. Fix it.
 */
void FLevelEditorModule::SummonSelectionDetails()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->SyncDetailsToSelection();
}

void FLevelEditorModule::SummonBuildAndSubmit()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("LevelEditorBuildAndSubmit");
}

void FLevelEditorModule::SummonLevelBrowser()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("LevelEditorLevelBrowser");
}

void FLevelEditorModule::SummonWorldBrowserHierarchy()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("WorldBrowserHierarchy");
}

void FLevelEditorModule::SummonWorldBrowserDetails()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("WorldBrowserDetails");
}

void FLevelEditorModule::SummonWorldBrowserComposition()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	LevelEditorInstance->InvokeTab("WorldBrowserComposition");
}

// @todo remove when world-centric mode is added
void FLevelEditorModule::AttachSequencer( TSharedPtr<SWidget> SequencerWidget, TSharedPtr<IAssetEditorInstance> SequencerAssetEditor )
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();

	LevelEditorInstance->AttachSequencer( SequencerWidget, SequencerAssetEditor );
}

TSharedPtr<ILevelViewport> FLevelEditorModule::GetFirstActiveViewport()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	return (LevelEditorInstance.IsValid()) ? LevelEditorInstance->GetActiveViewport() : nullptr;
}

void FLevelEditorModule::FocusPIEViewport()
{
	TSharedPtr<SLevelEditor> LevelEditorInstance = LevelEditorInstancePtr.Pin();
	if( LevelEditorInstance.IsValid() && LevelEditorTabManager.IsValid() && LevelEditorInstance->HasActivePlayInEditorViewport() )
	{
		FGlobalTabmanager::Get()->DrawAttentionToTabManager( LevelEditorTabManager.ToSharedRef() );
	}
}

void FLevelEditorModule::FocusViewport()
{
	TSharedPtr<ILevelViewport> ActiveLevelViewport = GetFirstActiveViewport();
	if( ActiveLevelViewport.IsValid() )
	{
		TSharedRef< const SWidget > ViewportAsWidget = ActiveLevelViewport->AsWidget();
		FWidgetPath FocusWidgetPath;

		if( FSlateApplication::Get().GeneratePathToWidgetUnchecked( ViewportAsWidget, FocusWidgetPath ) )
		{
			FSlateApplication::Get().SetKeyboardFocus( FocusWidgetPath, EFocusCause::SetDirectly );
		}
	}
}

void FLevelEditorModule::BroadcastActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	ActorSelectionChangedEvent.Broadcast(NewSelection, bForceRefresh);
}

void FLevelEditorModule::BroadcastRedrawViewports( bool bInvalidateHitProxies )
{
	RedrawLevelEditingViewportsEvent.Broadcast( bInvalidateHitProxies );
}

void FLevelEditorModule::BroadcastTakeHighResScreenShots( )
{
	TakeHighResScreenShotsEvent.Broadcast();
}

void FLevelEditorModule::BroadcastMapChanged( UWorld* World, EMapChangeType MapChangeType )
{
	MapChangedEvent.Broadcast( World, MapChangeType );
}

void FLevelEditorModule::BroadcastComponentsEdited()
{
	ComponentsEditedEvent.Broadcast();
}

const FLevelEditorCommands& FLevelEditorModule::GetLevelEditorCommands() const
{
	return FLevelEditorCommands::Get();
}

const FLevelEditorModesCommands& FLevelEditorModule::GetLevelEditorModesCommands() const
{
	return FLevelEditorModesCommands::Get();
}

const FLevelViewportCommands& FLevelEditorModule::GetLevelViewportCommands() const
{
	return FLevelViewportCommands::Get();
}

TWeakPtr<class SLevelEditor> FLevelEditorModule::GetLevelEditorInstance() const
{
	return LevelEditorInstancePtr;
}

TWeakPtr<class SDockTab> FLevelEditorModule::GetLevelEditorInstanceTab() const
{
	return LevelEditorInstanceTabPtr;
}

TSharedPtr<FTabManager> FLevelEditorModule::GetLevelEditorTabManager() const
{
	return LevelEditorTabManager;
}

void FLevelEditorModule::SetLevelEditorInstance( TWeakPtr<SLevelEditor> LevelEditor )
{
	LevelEditorInstancePtr = LevelEditor;
	GLevelEditorModeTools().SetToolkitHost(LevelEditorInstancePtr.Pin().ToSharedRef());
}

void FLevelEditorModule::SetLevelEditorInstanceTab( TWeakPtr<SDockTab> LevelEditorTab )
{
	LevelEditorInstanceTabPtr = LevelEditorTab;
}

void FLevelEditorModule::SetLevelEditorTabManager( const TSharedPtr<SDockTab>& OwnerTab )
{
	if (LevelEditorTabManager.IsValid())
	{
		LevelEditorTabManager->UnregisterAllTabSpawners();
		LevelEditorTabManager.Reset();
	}

	if (OwnerTab.IsValid())
	{
		LevelEditorTabManager = FGlobalTabmanager::Get()->NewTabManager(OwnerTab.ToSharedRef());
		LevelEditorTabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateRaw(this, &FLevelEditorModule::HandleTabManagerPersistLayout));

		TabManagerChangedEvent.Broadcast();
	}
}

void FLevelEditorModule::StartPlayInEditorSession()
{
	TSharedPtr<ILevelViewport> ActiveLevelViewport = GetFirstActiveViewport();

	if( ActiveLevelViewport.IsValid() )
	{
		const FVector* StartLocation = NULL;
		const FRotator* StartRotation = NULL;

		// We never want to play from the camera's location at startup, because the camera could have
		// been abandoned in a strange location in the map
		if( 0 )	// @todo immersive
		{
			// If this is a perspective viewport, then we'll Play From Here
			const FLevelEditorViewportClient& LevelViewportClient = ActiveLevelViewport->GetLevelViewportClient();
			if( LevelViewportClient.IsPerspective() )
			{
				// Start PIE from the camera's location and orientation!
				StartLocation = &LevelViewportClient.GetViewLocation();
				StartRotation = &LevelViewportClient.GetViewRotation();
			}
		}

		// Queue up the PIE session
		const bool bSimulateInEditor = false;
		const bool bUseMobilePreview = false;
		GUnrealEd->RequestPlaySession( true, ActiveLevelViewport, bSimulateInEditor, StartLocation, StartRotation, -1, bUseMobilePreview );
		// Kick off the queued PIE session immediately.  This is so that at startup, we don't need to
		// wait for the next engine tick.  We want to see PIE gameplay when the editor first appears!
		GUnrealEd->StartQueuedPlayMapRequest();

		// Special case for immersive pie startup, When in immersive pie at startup we use the player start but we want to move the camera where the player
		// was at when pie ended.
		GEditor->bHasPlayWorldPlacement = true;
	}
}

void FLevelEditorModule::GoImmersiveWithActiveLevelViewport( const bool bForceGameView )
{
	TSharedPtr<ILevelViewport> ActiveLevelViewport = GetFirstActiveViewport();

	if( ActiveLevelViewport.IsValid() )
	{
		// Make sure we can find a path to the viewport.  This will fail in cases where the viewport widget
		// is in a backgrounded tab, etc.  We can't currently support starting PIE in a backgrounded tab
		// due to how PIE manages focus and requires event forwarding from the application.
		TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow( ActiveLevelViewport->AsWidget() );
		if(Window.IsValid() )
		{
			if( bForceGameView && !ActiveLevelViewport->IsInGameView() )
			{
				ActiveLevelViewport->ToggleGameView();
			}

			{
				const bool bWantImmersive = true;
				const bool bAllowAnimation = false;
				ActiveLevelViewport->MakeImmersive( bWantImmersive, bAllowAnimation );
				FVector2D WindowSize = Window->GetSizeInScreen();
				// Set the initial size of the viewport to be the size of the window. This must be done because Slate has not ticked yet so the viewport will have no initial size
				ActiveLevelViewport->GetActiveViewport()->SetInitialSize( FIntPoint( FMath::TruncToInt( WindowSize.X ), FMath::TruncToInt( WindowSize.Y ) ) );
			}
		}
	}
}

void FLevelEditorModule::ToggleImmersiveOnActiveLevelViewport()
{
	TSharedPtr< ILevelViewport > ActiveLevelViewport = GetFirstActiveViewport();
	if( ActiveLevelViewport.IsValid() )
	{
		// Toggle immersive mode (with animation!)
		const bool bAllowAnimation = true;
		ActiveLevelViewport->MakeImmersive( !ActiveLevelViewport->IsImmersive(), bAllowAnimation );
	}
}

/** @return Returns the first Level Editor that we currently know about */
TSharedPtr<ILevelEditor> FLevelEditorModule::GetFirstLevelEditor()
{
	return LevelEditorInstancePtr.Pin();
}

TSharedPtr<SDockTab> FLevelEditorModule::GetLevelEditorTab() const
{
	return LevelEditorInstanceTabPtr.Pin();
}

TSharedRef<IViewportLayoutEntity> FLevelEditorModule::FactoryViewport(FName InTypeName, const FViewportConstructionArgs& ConstructionArgs) const
{
	const FViewportTypeDefinition* Definition = CustomViewports.Find(InTypeName);
	if (Definition)
	{
		return Definition->FactoryFunction(ConstructionArgs);
	}

	return MakeShareable(new FLevelViewportLayoutEntity(ConstructionArgs));
}

void FLevelEditorModule::BindGlobalLevelEditorCommands()
{
	check( !GlobalLevelEditorActions.IsValid() );

	GlobalLevelEditorActions = MakeShareable( new FUICommandList );

	const FLevelEditorCommands& Commands = FLevelEditorCommands::Get();
	FUICommandList& ActionList = *GlobalLevelEditorActions;

	// Make a default can execute action that disables input when in debug mode
	FCanExecuteAction DefaultExecuteAction = FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::DefaultCanExecuteAction );

	ActionList.MapAction( Commands.BrowseDocumentation, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseDocumentation ) );
	ActionList.MapAction( Commands.BrowseAPIReference, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseAPIReference ) );
	ActionList.MapAction( Commands.BrowseCVars, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseCVars ) );
	ActionList.MapAction( Commands.BrowseViewportControls, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BrowseViewportControls ) );
	ActionList.MapAction( Commands.NewLevel, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::NewLevel ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::NewLevel_CanExecute ) );
	ActionList.MapAction(Commands.OpenLevel, FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OpenLevel), FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OpenLevel_CanExecute));
	ActionList.MapAction( Commands.Save, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Save ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanSaveWorld ) );
	ActionList.MapAction( Commands.SaveAs, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SaveCurrentAs ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanSaveWorld ) );
	ActionList.MapAction( Commands.SaveAllLevels, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SaveAllLevels ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanSaveWorld ) );
	ActionList.MapAction( Commands.ToggleFavorite, FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ToggleFavorite ), FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ToggleFavorite_CanExecute ), FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::ToggleFavorite_IsChecked ) );

	for( int32 CurRecentIndex = 0; CurRecentIndex < FLevelEditorCommands::MaxRecentFiles; ++CurRecentIndex )
	{
		ActionList.MapAction( Commands.OpenRecentFileCommands[ CurRecentIndex ], FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OpenRecentFile, CurRecentIndex ), DefaultExecuteAction );
	}

	for (int32 CurFavoriteIndex = 0; CurFavoriteIndex < FLevelEditorCommands::MaxFavoriteFiles; ++CurFavoriteIndex)
	{
		ActionList.MapAction(Commands.OpenFavoriteFileCommands[CurFavoriteIndex], FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OpenFavoriteFile, CurFavoriteIndex), DefaultExecuteAction);
	}

	ActionList.MapAction( 
		Commands.ToggleVR, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ToggleVR ), 
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ToggleVR_CanExecute ),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::ToggleVR_IsChecked ) );

	ActionList.MapAction(Commands.ImportScene,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ImportScene_Clicked));

	ActionList.MapAction( Commands.ExportAll,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExportAll_Clicked ) );

	ActionList.MapAction( Commands.ExportSelected,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExportSelected_Clicked ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExportSelected_CanExecute ) );

	ActionList.MapAction( Commands.Build,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Build_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Build_CanExecute ) );

	ActionList.MapAction(
		Commands.ConnectToSourceControl,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ConnectToSourceControl_Clicked)
		);

	ActionList.MapAction(
		Commands.ChangeSourceControlSettings,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ConnectToSourceControl_Clicked)
		);

	ActionList.MapAction(
		Commands.CheckOutModifiedFiles,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CheckOutModifiedFiles_Clicked),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CheckOutModifiedFiles_CanExecute)
		);

	ActionList.MapAction(
		Commands.SubmitToSourceControl,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SubmitToSourceControl_Clicked),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SubmitToSourceControl_CanExecute)
		);

	ActionList.MapAction(Commands.RecompileGameCode,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RecompileGameCode_Clicked ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Recompile_CanExecute )
		);

	ActionList.MapAction( 
		FGlobalEditorCommonCommands::Get().FindInContentBrowser, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::FindInContentBrowser_Clicked )
		);

	ActionList.MapAction( 
		FGlobalEditorCommonCommands::Get().ViewReferences, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ViewReferences_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanViewReferences )
		);

	ActionList.MapAction( 
		FGlobalEditorCommonCommands::Get().ViewSizeMap, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ViewSizeMap_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanViewSizeMap )
		);

	const FVector* NullVector = nullptr;
	ActionList.MapAction(
		Commands.GoHere,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::GoHere_Clicked, NullVector )
		);

	ActionList.MapAction( 
		Commands.SnapCameraToObject,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("CAMERA SNAP") ) )
		);

	ActionList.MapAction(
		Commands.SnapObjectToCamera,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapObjectToView_Clicked)
		);

	ActionList.MapAction(
		Commands.GoToCodeForActor, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::GoToCodeForActor_Clicked )
		);

	ActionList.MapAction( 
		Commands.GoToDocsForActor, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::GoToDocsForActor_Clicked )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Duplicate, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("DUPLICATE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Duplicate_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Delete, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("DELETE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Delete_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Rename, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Rename_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Rename_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Cut, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT CUT") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Cut_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Copy, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT COPY") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Copy_CanExecute )
		);

	ActionList.MapAction( 
		FGenericCommands::Get().Paste, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT PASTE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::Paste_CanExecute )
		);

	ActionList.MapAction( 
		Commands.PasteHere, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDIT PASTE TO=HERE") ) ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::PasteHere_CanExecute )
		);

	bool bAlign = false;
	bool bPerActor = false;
	ActionList.MapAction(
		Commands.SnapOriginToGrid,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToGrid_Clicked, bAlign, bPerActor),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bPerActor = true;
	ActionList.MapAction(
		Commands.SnapOriginToGridPerActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToGrid_Clicked, bAlign, bPerActor),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);
	
	bAlign = true;
	bPerActor = false;
	ActionList.MapAction(
		Commands.AlignOriginToGrid,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToGrid_Clicked, bAlign, bPerActor),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	ActionList.MapAction(
		Commands.SnapOriginToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToActor_Clicked, bAlign),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);
	
	bAlign = true;
	ActionList.MapAction(
		Commands.AlignOriginToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveActorToActor_Clicked, bAlign),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.SnapTo2DLayer,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapTo2DLayer_Clicked),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CanSnapTo2DLayer)
		);

	ActionList.MapAction(
		Commands.MoveSelectionUpIn2DLayers,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveSelectionToDifferent2DLayer_Clicked, /*bGoingUp=*/ true, /*bForceToTopOrBottom=*/ false),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CanMoveSelectionToDifferent2DLayer, /*bGoingUp=*/ true)
		);
	ActionList.MapAction(
		Commands.MoveSelectionDownIn2DLayers,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveSelectionToDifferent2DLayer_Clicked, /*bGoingUp=*/ false, /*bForceToTopOrBottom=*/ false),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CanMoveSelectionToDifferent2DLayer, /*bGoingUp=*/ false)
		);
	ActionList.MapAction(
		Commands.MoveSelectionToTop2DLayer,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveSelectionToDifferent2DLayer_Clicked, /*bGoingUp=*/ true, /*bForceToTopOrBottom=*/ true),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CanMoveSelectionToDifferent2DLayer, /*bGoingUp=*/ true)
		);
	ActionList.MapAction(
		Commands.MoveSelectionToBottom2DLayer,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::MoveSelectionToDifferent2DLayer_Clicked, /*bGoingUp=*/ false, /*bForceToTopOrBottom=*/ true),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CanMoveSelectionToDifferent2DLayer, /*bGoingUp=*/ false)
		);


	ActionList.MapAction(
		Commands.Select2DLayerAbove,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::Select2DLayerDeltaAway_Clicked, -1)
		);
	ActionList.MapAction(
		Commands.Select2DLayerBelow,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::Select2DLayerDeltaAway_Clicked, 1)
		);

	bAlign = false;
	bool bUseLineTrace = false;
	bool bUseBounds = false;
	bool bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.SnapPivotToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.AlignPivotToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapBottomCenterBoundsToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignBottomCenterBoundsToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.SnapPivotToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = false;
	bUsePivot = true;
	ActionList.MapAction(
		Commands.AlignPivotToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = false;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.SnapBottomCenterBoundsToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = true;
	bUseBounds = true;
	bUsePivot = false;
	ActionList.MapAction(
		Commands.AlignBottomCenterBoundsToActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapActorToActor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorsSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.DeltaTransformToActors, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::DeltaTransform ), 
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ActorSelected_CanExecute ) );

	ActionList.MapAction(
		Commands.MirrorActorX,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR MIRROR X=-1") ) ),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.MirrorActorY,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR MIRROR Y=-1") ) ),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.MirrorActorZ,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR MIRROR Z=-1") ) ),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	ActionList.MapAction(
		Commands.DetachFromParent,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::DetachActor_Clicked )
		);

	ActionList.MapAction(
		Commands.AttachSelectedActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AttachSelectedActors )
		);

	ActionList.MapAction(
		Commands.AttachActorIteractive,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AttachActorIteractive )
		);

	ActionList.MapAction(
		Commands.CreateNewOutlinerFolder,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CreateNewOutlinerFolder_Clicked )
		);

	ActionList.MapAction(
		Commands.LockActorMovement,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LockActorMovement_Clicked ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::LockActorMovement_IsChecked )
		);

	ActionList.MapAction(
		Commands.RegroupActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RegroupActor_Clicked )
		);

	ActionList.MapAction(
		Commands.UngroupActors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::UngroupActor_Clicked )
		);

	ActionList.MapAction(
		Commands.LockGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LockGroup_Clicked )
		);

	ActionList.MapAction(
		Commands.UnlockGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::UnlockGroup_Clicked )
		);

	ActionList.MapAction(
		Commands.AddActorsToGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::AddActorsToGroup_Clicked )
		);

	ActionList.MapAction(
		Commands.RemoveActorsFromGroup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::RemoveActorsFromGroup_Clicked )
		);
	
	ActionList.MapAction(
		Commands.ShowAll,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE ALL") ) ) 
		);

	ActionList.MapAction(
		Commands.ShowSelectedOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnShowOnlySelectedActors )
		);

	ActionList.MapAction(
		Commands.ShowSelected,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE SELECTED") ) )
		);

	ActionList.MapAction(
		Commands.HideSelected,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR HIDE SELECTED") ) )
		);

	ActionList.MapAction(
		Commands.ShowAllStartup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE ALL STARTUP") ) )
		);

	ActionList.MapAction(
		Commands.ShowSelectedStartup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNHIDE SELECTED STARTUP") ) )
		);

	ActionList.MapAction(
		Commands.HideSelectedStartup,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR HIDE SELECTED STARTUP") ) )
		);

	ActionList.MapAction(
		Commands.CycleNavigationDataDrawn,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("CYCLENAVDRAWN") ) )
		);

	ActionList.MapAction(
		FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT ALL") ) )
		);

	ActionList.MapAction(
		Commands.SelectNone,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("SELECT NONE") ) )
		);

	ActionList.MapAction(
		Commands.InvertSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT INVERT") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllActorsOfSameClass,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllActorsOfClass, (bool)false )
		);

	ActionList.MapAction(
		Commands.SelectAllActorsOfSameClassWithArchetype,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllActorsOfClass, (bool)true )
		);

	ActionList.MapAction(
		Commands.SelectComponentOwnerActor,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectComponentOwnerActor ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanSelectComponentOwnerActor )
		);

	ActionList.MapAction(
		Commands.SelectRelevantLights,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT RELEVANTLIGHTS") ) )
		);

	ActionList.MapAction(
		Commands.SelectStaticMeshesOfSameClass,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSTATICMESH") ) )
		);

	ActionList.MapAction(
		Commands.SelectStaticMeshesAllClasses,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSTATICMESH ALLCLASSES") ) )
		);

	ActionList.MapAction(
		Commands.SelectOwningHierarchicalLODCluster,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OnSelectOwningHLODCluster)
		);

	ActionList.MapAction(
		Commands.SelectSkeletalMeshesOfSameClass,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSKELETALMESH") ) )
		);

	ActionList.MapAction(
		Commands.SelectSkeletalMeshesAllClasses,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGSKELETALMESH ALLCLASSES") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllWithSameMaterial,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGMATERIAL") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllActorsControlledByMatinee,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllActorsControlledByMatinee )
		);

	ActionList.MapAction(
		Commands.SelectMatchingEmitter,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR SELECT MATCHINGEMITTER") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllLights,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectAllLights )
		);

	ActionList.MapAction(
		Commands.SelectStationaryLightsExceedingOverlap,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectStationaryLightsExceedingOverlap )
		);

	ActionList.MapAction(
		Commands.SelectAllAddditiveBrushes,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SELECT ADDS") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllSubtractiveBrushes,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SELECT SUBTRACTS") ) )
		);

	ActionList.MapAction(
		Commands.SelectAllSurfaces,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ALL") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllMatchingBrush,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MATCHING BRUSH") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllMatchingTexture,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MATCHING TEXTURE") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacents,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT ALL") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentCoplanars,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT COPLANARS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentWalls,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT WALLS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentFloors,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT FLOORS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAllAdjacentSlants,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT ADJACENT SLANTS") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectReverse,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT REVERSE") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectMemorize,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY SET") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectRecall,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY RECALL") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectOr,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY INTERSECTION") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectAnd,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY UNION") ) )
		);

	ActionList.MapAction( 
		Commands.SurfSelectXor,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("POLY SELECT MEMORY XOR") ) )
		);

	ActionList.MapAction( 
		Commands.SurfUnalign,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_Default )
		);

	ActionList.MapAction( 
		Commands.SurfAlignPlanarAuto,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_PlanarAuto )
		);

	ActionList.MapAction( 
		Commands.SurfAlignPlanarWall,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_PlanarWall )
		);

	ActionList.MapAction( 
		Commands.SurfAlignPlanarFloor,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_PlanarFloor )
		);

	ActionList.MapAction( 
		Commands.SurfAlignBox,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_Box )
		);

	ActionList.MapAction(
		Commands.SurfAlignFit,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSurfaceAlignment, TEXALIGN_Fit )
		);


	ActionList.MapAction(
		Commands.ApplyMaterialToSurface,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnApplyMaterialToSurface )
		);

	ActionList.MapAction(
		Commands.SavePivotToPrePivot,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR BAKEPREPIVOT") ) )
		);

	ActionList.MapAction(
		Commands.ResetPivot,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR RESET PIVOT") ) )
		);

	ActionList.MapAction(
		Commands.ResetPrePivot,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR UNBAKEPREPIVOT") ) )
		);

	ActionList.MapAction(
		Commands.MovePivotHere,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PIVOT HERE") ) )
		);

	ActionList.MapAction(
		Commands.MovePivotHereSnapped,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PIVOT SNAPPED") ) )
		);

	ActionList.MapAction(
		Commands.MovePivotToCenter,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PIVOT CENTERSELECTION") ) )
		);

	ActionList.MapAction(
		Commands.ConvertToAdditive,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf(TEXT("MAP SETBRUSH BRUSHTYPE=%d"), (int32)Brush_Add) )
		);

	ActionList.MapAction(
		Commands.ConvertToSubtractive,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf(TEXT("MAP SETBRUSH BRUSHTYPE=%d"), (int32)Brush_Subtract) )
		);

	ActionList.MapAction(
		Commands.OrderFirst,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SENDTO FIRST") ) )
		);

	ActionList.MapAction(
		Commands.OrderLast,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MAP SENDTO LAST") ) )
		);

	ActionList.MapAction(
		Commands.MakeSolid,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), PF_Semisolid + PF_NotSolid, 0 ) )
		);

	ActionList.MapAction(
		Commands.MakeSemiSolid,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), (int32)(PF_Semisolid + PF_NotSolid), (int32)PF_Semisolid ) )
		);

	ActionList.MapAction(
		Commands.MakeNonSolid,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString::Printf( TEXT("MAP SETBRUSH CLEARFLAGS=%d SETFLAGS=%d"), (int32)(PF_Semisolid + PF_NotSolid), (int32)PF_NotSolid ) )
		);

	ActionList.MapAction(
		Commands.MergePolys,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("BRUSH MERGEPOLYS") ) )
		);

	ActionList.MapAction(
		Commands.SeparatePolys,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("BRUSH SEPARATEPOLYS") ) )
		);

	ActionList.MapAction(
		Commands.CreateBoundingBoxVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_BOUNDINGBOX SnapToGrid=1") ) )
		);


	ActionList.MapAction(
		Commands.CreateHeavyConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.01 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.CreateNormalConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.15 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.CreateLightConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=.5 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.CreateRoughConvexVolume,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("ACTOR CREATE_BV_CONVEXVOLUME NORMALTOLERANCE=0.75 SnapToGrid=1") ) )
		);

	ActionList.MapAction(
		Commands.KeepSimulationChanges,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnKeepSimulationChanges ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::CanExecuteKeepSimulationChanges )
		);


#if WITH_FLEX

	ActionList.MapAction(
		Commands.KeepFlexSimulationChanges,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OnKeepFlexSimulationChanges)
		);

	ActionList.MapAction(
		Commands.ClearFlexSimulationChanges,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OnClearFlexSimulationChanges)
		);

#endif

	ActionList.MapAction( 
		Commands.MakeActorLevelCurrent,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnMakeSelectedActorLevelCurrent )
		);

	ActionList.MapAction(
		Commands.MoveSelectedToCurrentLevel,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnMoveSelectedToCurrentLevel )
		);

	ActionList.MapAction(
		Commands.FindActorLevelInContentBrowser,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OnFindActorLevelInContentBrowser),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::CanExecuteFindActorLevelInContentBrowser)
		);

	ActionList.MapAction(
		Commands.FindLevelsInLevelBrowser,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnFindLevelsInLevelBrowser )
		);

	ActionList.MapAction(
		Commands.AddLevelsToSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnSelectLevelInLevelBrowser )
		);

	ActionList.MapAction(
		Commands.RemoveLevelsFromSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnDeselectLevelInLevelBrowser )
		);

	ActionList.MapAction(
		Commands.FindActorInLevelScript,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnFindActorInLevelScript )
		);

	ActionList.MapAction( Commands.BuildAndSubmitToSourceControl,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildAndSubmitToSourceControl_Execute ) );

	ActionList.MapAction( Commands.BuildLightingOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildLightingOnly_Execute ),
		FCanExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildLighting_CanExecute ) );

	ActionList.MapAction( Commands.BuildReflectionCapturesOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildReflectionCapturesOnly_Execute ) );

	ActionList.MapAction( Commands.BuildLightingOnly_VisibilityOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildLightingOnly_VisibilityOnly_Execute ) );

	ActionList.MapAction( Commands.LightingBuildOptions_UseErrorColoring,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_UseErrorColoring_Toggled ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_UseErrorColoring_IsChecked ) );

	ActionList.MapAction( Commands.LightingBuildOptions_ShowLightingStats,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_ShowLightingStats_Toggled ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::LightingBuildOptions_ShowLightingStats_IsChecked ) );

	ActionList.MapAction( Commands.BuildGeometryOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildGeometryOnly_Execute ) );

	ActionList.MapAction( Commands.BuildGeometryOnly_OnlyCurrentLevel,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildGeometryOnly_OnlyCurrentLevel_Execute ) );

	ActionList.MapAction( Commands.BuildPathsOnly,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::BuildPathsOnly_Execute ) );

	ActionList.MapAction(Commands.BuildLODsOnly,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::BuildLODsOnly_Execute));
	
	ActionList.MapAction(Commands.BuildTextureStreamingOnly,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::BuildTextureStreamingOnly_Execute));

	ActionList.MapAction( 
		Commands.LightingQuality_Production, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_Production ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_Production ) );
	ActionList.MapAction( 
		Commands.LightingQuality_High, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_High ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_High ) );
	ActionList.MapAction( 
		Commands.LightingQuality_Medium, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_Medium ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_Medium ) );
	ActionList.MapAction( 
		Commands.LightingQuality_Preview, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingQuality, (ELightingBuildQuality)Quality_Preview ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingQualityChecked, (ELightingBuildQuality)Quality_Preview) );

	ActionList.MapAction( 
		Commands.LightingDensity_RenderGrayscale, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingDensityRenderGrayscale ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingDensityRenderGrayscaleChecked ) );

	ActionList.MapAction( 
		Commands.LightingResolution_CurrentLevel, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionLevel, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Current ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionLevelChecked, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Current ) );
	ActionList.MapAction( 
		Commands.LightingResolution_SelectedLevels, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionLevel, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Selected ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionLevelChecked, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::Selected ) );
	ActionList.MapAction( 
		Commands.LightingResolution_AllLoadedLevels, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionLevel, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::AllLoaded ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionLevelChecked, (FLightmapResRatioAdjustSettings::AdjustLevels)FLightmapResRatioAdjustSettings::AllLoaded ) );
	ActionList.MapAction( 
		Commands.LightingResolution_SelectedObjectsOnly, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetLightingResolutionSelectedObjectsOnly ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsLightingResolutionSelectedObjectsOnlyChecked ) );

	ActionList.MapAction( 
		Commands.LightingStaticMeshInfo, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ShowLightingStaticMeshInfo ) );

	ActionList.MapAction( 
		Commands.SceneStats,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ShowSceneStats ) );

	ActionList.MapAction( 
		Commands.TextureStats,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ShowTextureStats ) );

	ActionList.MapAction( 
		Commands.MapCheck,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::MapCheck_Execute ) );

	ActionList.MapAction(
		Commands.ShowTransformWidget,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleTransformWidgetVisibility ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnGetTransformWidgetVisibility )
		);

	ActionList.MapAction(
		Commands.AllowTranslucentSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnAllowTranslucentSelection ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsAllowTranslucentSelectionEnabled ) 
		);


	ActionList.MapAction(
		Commands.AllowGroupSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnAllowGroupSelection ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsAllowGroupSelectionEnabled ) 
		);

	ActionList.MapAction(
		Commands.StrictBoxSelect,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleStrictBoxSelect ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsStrictBoxSelectEnabled ) 
		);

	ActionList.MapAction(
		Commands.TransparentBoxSelect,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleTransparentBoxSelect ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsTransparentBoxSelectEnabled ) 
		);

	ActionList.MapAction(
		Commands.DrawBrushMarkerPolys,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnDrawBrushMarkerPolys ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsDrawBrushMarkerPolysEnabled ) 
		);

	ActionList.MapAction(
		Commands.OnlyLoadVisibleInPIE,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleOnlyLoadVisibleInPIE ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsOnlyLoadVisibleInPIEEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleSocketSnapping,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleSocketSnapping ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsSocketSnappingEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleParticleSystemLOD,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleParticleSystemLOD ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsParticleSystemLODEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleFreezeParticleSimulation,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleFreezeParticleSimulation ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsParticleSimulationFrozen )
		);

	ActionList.MapAction(
		Commands.ToggleParticleSystemHelpers,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleParticleSystemHelpers ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsParticleSystemHelpersEnabled ) 
		);

	ActionList.MapAction(
		Commands.ToggleLODViewLocking,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleLODViewLocking ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsLODViewLockingEnabled ) 
		);

	ActionList.MapAction(
		Commands.LevelStreamingVolumePrevis,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleLevelStreamingVolumePrevis ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsLevelStreamingVolumePrevisEnabled ) 
		);

	ActionList.MapAction(
		Commands.EnableActorSnap,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnEnableActorSnap ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsActorSnapEnabled ) 
		);

	ActionList.MapAction(
		Commands.EnableVertexSnap,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnEnableVertexSnap ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::OnIsVertexSnapEnabled ) 
		);

	ActionList.MapAction(
		Commands.ShowSelectedDetails,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("EDCALLBACK SELECTEDPROPS" ) ) ) 
		);

	//if (FParse::Param( FCommandLine::Get(), TEXT( "editortoolbox" ) ))
	//{
	//	ActionList.MapAction(
	//		Commands.BspMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE BSP") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_Bsp ) 
	//		);

	//	ActionList.MapAction(
	//		Commands.MeshPaintMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE MESHPAINT") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_MeshPaint ) 
	//		);

	//	ActionList.MapAction(
	//		Commands.LandscapeMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE LANDSCAPE") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_Landscape ) 
	//		);

	//	ActionList.MapAction(
	//		Commands.FoliageMode,
	//		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("MODE FOLIAGE") ) ),
	//		FCanExecuteAction(),
	//		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsEditorModeActive, FBuiltinEditorModes::EM_Foliage ) 
	//		);
	//}

	ActionList.MapAction(
		Commands.RecompileShaders,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("RECOMPILESHADERS CHANGED" ) ) )
		);

	ActionList.MapAction(
		Commands.ProfileGPU,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PROFILEGPU") ) )
		);

	ActionList.MapAction(
		Commands.ResetAllParticleSystems,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PARTICLE RESET ALL") ) )
		);

	ActionList.MapAction(
		Commands.ResetSelectedParticleSystem,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("PARTICLE RESET SELECTED") ) )
		);

	ActionList.MapAction(
		FEditorViewportCommands::Get().LocationGridSnap,
		FExecuteAction::CreateStatic(FLevelEditorActionCallbacks::LocationGridSnap_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(FLevelEditorActionCallbacks::LocationGridSnap_IsChecked)
		);
	ActionList.MapAction(
		FEditorViewportCommands::Get().RotationGridSnap,
		FExecuteAction::CreateStatic(FLevelEditorActionCallbacks::RotationGridSnap_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(FLevelEditorActionCallbacks::RotationGridSnap_IsChecked)
		);
	ActionList.MapAction(
		FEditorViewportCommands::Get().ScaleGridSnap,
		FExecuteAction::CreateStatic(FLevelEditorActionCallbacks::ScaleGridSnap_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(FLevelEditorActionCallbacks::ScaleGridSnap_IsChecked)
		);
	ActionList.MapAction(
		Commands.ToggleHideViewportUI,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnToggleHideViewportUI ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsViewportUIHidden ) 
		);
	ActionList.MapAction(
		Commands.AddMatinee,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::OnAddMatinee )
		);

	ActionList.MapAction( 
		Commands.MaterialQualityLevel_Low, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetMaterialQualityLevel, (EMaterialQualityLevel::Type)EMaterialQualityLevel::Low ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsMaterialQualityLevelChecked, (EMaterialQualityLevel::Type)EMaterialQualityLevel::Low ) );
	ActionList.MapAction(
		Commands.MaterialQualityLevel_Medium,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetMaterialQualityLevel, (EMaterialQualityLevel::Type)EMaterialQualityLevel::Medium),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsMaterialQualityLevelChecked, (EMaterialQualityLevel::Type)EMaterialQualityLevel::Medium));
	ActionList.MapAction( 
		Commands.MaterialQualityLevel_High, 
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::SetMaterialQualityLevel, (EMaterialQualityLevel::Type)EMaterialQualityLevel::High ),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic( &FLevelEditorActionCallbacks::IsMaterialQualityLevelChecked, (EMaterialQualityLevel::Type)EMaterialQualityLevel::High ) );

	ActionList.MapAction(
		Commands.PreviewPlatformOverride_DefaultES2,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetPreviewPlatform, FName(), ERHIFeatureLevel::ES2),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsPreviewPlatformChecked, FName(), ERHIFeatureLevel::ES2));
	ActionList.MapAction(
		Commands.PreviewPlatformOverride_AndroidGLES2,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetPreviewPlatform, LegacyShaderPlatformToShaderFormat(SP_OPENGL_ES2_ANDROID), ERHIFeatureLevel::ES2),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsPreviewPlatformChecked, LegacyShaderPlatformToShaderFormat(SP_OPENGL_ES2_ANDROID), ERHIFeatureLevel::ES2));

	ActionList.MapAction(
		Commands.PreviewPlatformOverride_DefaultES31,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetPreviewPlatform, FName(), ERHIFeatureLevel::ES3_1),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsPreviewPlatformChecked, FName(), ERHIFeatureLevel::ES3_1));
	ActionList.MapAction(
		Commands.PreviewPlatformOverride_AndroidGLES31,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetPreviewPlatform, LegacyShaderPlatformToShaderFormat(SP_OPENGL_ES3_1_ANDROID), ERHIFeatureLevel::ES3_1),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsPreviewPlatformChecked, LegacyShaderPlatformToShaderFormat(SP_OPENGL_ES3_1_ANDROID), ERHIFeatureLevel::ES3_1));
	ActionList.MapAction(
		Commands.PreviewPlatformOverride_AndroidVulkanES31,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetPreviewPlatform, LegacyShaderPlatformToShaderFormat(SP_VULKAN_ES3_1_ANDROID), ERHIFeatureLevel::ES3_1),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsPreviewPlatformChecked, LegacyShaderPlatformToShaderFormat(SP_VULKAN_ES3_1_ANDROID), ERHIFeatureLevel::ES3_1));
	ActionList.MapAction(
		Commands.PreviewPlatformOverride_IOSMetalES31,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetPreviewPlatform, LegacyShaderPlatformToShaderFormat(SP_METAL), ERHIFeatureLevel::ES3_1),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsPreviewPlatformChecked, LegacyShaderPlatformToShaderFormat(SP_METAL), ERHIFeatureLevel::ES3_1));

	ActionList.MapAction(
		Commands.OpenMergeActor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::OpenMergeActor_Clicked)
	);

	for (int32 i = 0; i < ERHIFeatureLevel::Num; ++i)
	{
		ActionList.MapAction(
			Commands.FeatureLevelPreview[i],
			FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SetFeatureLevelPreview, (ERHIFeatureLevel::Type)i),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&FLevelEditorActionCallbacks::IsFeatureLevelPreviewChecked, (ERHIFeatureLevel::Type)i));
	}
}
	
#undef LOCTEXT_NAMESPACE
