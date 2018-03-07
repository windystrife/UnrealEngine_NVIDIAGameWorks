// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/MediaPlayerEditorToolkit.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "SlateOptMacros.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "MediaPlaylist.h"
#include "Widgets/Docking/SDockTab.h"
#include "Models/MediaPlayerEditorCommands.h"
#include "Widgets/SMediaPlayerEditorDetails.h"
#include "Widgets/SMediaPlayerEditorInfo.h"
#include "Widgets/SMediaPlayerEditorMedia.h"
#include "Widgets/SMediaPlayerEditorPlaylist.h"
#include "Widgets/SMediaPlayerEditorStats.h"
#include "MediaPlayer.h"
#include "Widgets/SMediaPlayerEditorViewer.h"


#define LOCTEXT_NAMESPACE "FMediaPlayerEditorToolkit"


namespace MediaPlayerEditorToolkit
{
	static const FName AppIdentifier("MediaPlayerEditorApp");
	static const FName DetailsTabId("Details");
	static const FName InfoTabId("Info");
	static const FName MediaTabId("Media");
	static const FName PlaylistTabId("Playlist");
	static const FName StatsTabId("Stats");
	static const FName ViewerTabId("Viewer");
}


/* FMediaPlayerEditorToolkit structors
 *****************************************************************************/

FMediaPlayerEditorToolkit::FMediaPlayerEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: MediaPlayer(nullptr)
	, Style(InStyle)
{ }


FMediaPlayerEditorToolkit::~FMediaPlayerEditorToolkit()
{
	FReimportManager::Instance()->OnPreReimport().RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->UnregisterForUndo(this);
}


/* FMediaPlayerEditorToolkit interface
 *****************************************************************************/

void FMediaPlayerEditorToolkit::Initialize(UMediaPlayer* InMediaPlayer, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost)
{
	MediaPlayer = InMediaPlayer;

	if (MediaPlayer == nullptr)
	{
		return;
	}

	// support undo/redo
	MediaPlayer->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);

	BindCommands();

	// create tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_MediaPlayerEditor_v10")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.66f)
						->Split
						(
							// tool bar
							FTabManager::NewStack()
								->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.1f)
								
						)
						->Split
						(
							// viewer
							FTabManager::NewStack()
								->AddTab(MediaPlayerEditorToolkit::ViewerTabId, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.6f)
						)
						->Split
						(
							// media library
							FTabManager::NewStack()
								->AddTab(MediaPlayerEditorToolkit::MediaTabId, ETabState::OpenedTab)
								->SetHideTabWell(true)
								->SetSizeCoefficient(0.3f)
						)
				)
				->Split
				(
					FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.33f)
						->Split
						(
							// playlist
							FTabManager::NewStack()
								->AddTab(MediaPlayerEditorToolkit::PlaylistTabId, ETabState::OpenedTab)
								->SetSizeCoefficient(0.5f)
						)
						->Split
						(
							// details, info, stats
							FTabManager::NewStack()
								->AddTab(MediaPlayerEditorToolkit::DetailsTabId, ETabState::OpenedTab)
								->AddTab(MediaPlayerEditorToolkit::InfoTabId, ETabState::OpenedTab)
								->AddTab(MediaPlayerEditorToolkit::StatsTabId, ETabState::ClosedTab)
								->SetForegroundTab(MediaPlayerEditorToolkit::DetailsTabId)
								->SetSizeCoefficient(0.5f)
						)
				)
		);

	FAssetEditorToolkit::InitAssetEditor(
		InMode,
		InToolkitHost,
		MediaPlayerEditorToolkit::AppIdentifier,
		Layout,
		true /*bCreateDefaultStandaloneMenu*/,
		true /*bCreateDefaultToolbar*/,
		InMediaPlayer
	);
	
//	IMediaPlayerEditorModule* MediaPlayerEditorModule = &FModuleManager::LoadModuleChecked<IMediaPlayerEditorModule>("MediaPlayerEditor");
//	AddMenuExtender(MediaPlayerEditorModule->GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	ExtendToolBar();
	RegenerateMenusAndToolbars();
}


/* FAssetEditorToolkit interface
 *****************************************************************************/

FString FMediaPlayerEditorToolkit::GetDocumentationLink() const
{
	return FString(TEXT("Engine/Content/Types/MediaAssets/Properties/Interface"));
}


bool FMediaPlayerEditorToolkit::OnRequestClose()
{
	if (MediaPlayer != nullptr)
	{
		MediaPlayer->Close();
	}

	return FAssetEditorToolkit::OnRequestClose();
}


void FMediaPlayerEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MediaPlayerEditor", "Media Player Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(MediaPlayerEditorToolkit::DetailsTabId, FOnSpawnTab::CreateSP(this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, MediaPlayerEditorToolkit::DetailsTabId))
		.SetDisplayName(LOCTEXT("DetailsTabName", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(MediaPlayerEditorToolkit::InfoTabId, FOnSpawnTab::CreateSP(this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, MediaPlayerEditorToolkit::InfoTabId))
		.SetDisplayName(LOCTEXT("InfoTabName", "Info"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.Tabs.Info"));

	InTabManager->RegisterTabSpawner(MediaPlayerEditorToolkit::MediaTabId, FOnSpawnTab::CreateSP(this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, MediaPlayerEditorToolkit::MediaTabId))
		.SetDisplayName(LOCTEXT("MediaTabName", "Media Library"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.Tabs.Media"));

	InTabManager->RegisterTabSpawner(MediaPlayerEditorToolkit::ViewerTabId, FOnSpawnTab::CreateSP(this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, MediaPlayerEditorToolkit::ViewerTabId))
		.SetDisplayName(LOCTEXT("PlayerTabName", "Player"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.Tabs.Player"));

	InTabManager->RegisterTabSpawner(MediaPlayerEditorToolkit::PlaylistTabId, FOnSpawnTab::CreateSP(this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, MediaPlayerEditorToolkit::PlaylistTabId))
		.SetDisplayName(LOCTEXT("PlaylistTabName", "Playlist"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.Tabs.Playlist"));

	InTabManager->RegisterTabSpawner(MediaPlayerEditorToolkit::StatsTabId, FOnSpawnTab::CreateSP(this, &FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab, MediaPlayerEditorToolkit::StatsTabId))
		.SetDisplayName(LOCTEXT("StatsTabName", "Stats"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.Tabs.Stats"));
}


void FMediaPlayerEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(MediaPlayerEditorToolkit::ViewerTabId);
	InTabManager->UnregisterTabSpawner(MediaPlayerEditorToolkit::StatsTabId);
	InTabManager->UnregisterTabSpawner(MediaPlayerEditorToolkit::PlaylistTabId);
	InTabManager->UnregisterTabSpawner(MediaPlayerEditorToolkit::MediaTabId);
	InTabManager->UnregisterTabSpawner(MediaPlayerEditorToolkit::InfoTabId);
	InTabManager->UnregisterTabSpawner(MediaPlayerEditorToolkit::DetailsTabId);
}


/* IToolkit interface
 *****************************************************************************/

FText FMediaPlayerEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Media Player Editor");
}


FName FMediaPlayerEditorToolkit::GetToolkitFName() const
{
	return FName("MediaPlayerEditor");
}


FLinearColor FMediaPlayerEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}


FString FMediaPlayerEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "MediaPlayer ").ToString();
}


/* FGCObject interface
 *****************************************************************************/

void FMediaPlayerEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MediaPlayer);
}


/* FEditorUndoClient interface
*****************************************************************************/

void FMediaPlayerEditorToolkit::PostUndo(bool bSuccess)
{
	// do nothing
}


void FMediaPlayerEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}


/* FMediaPlayerEditorToolkit implementation
 *****************************************************************************/

void FMediaPlayerEditorToolkit::BindCommands()
{
	const FMediaPlayerEditorCommands& Commands = FMediaPlayerEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.CloseMedia,
		FExecuteAction::CreateLambda([this] { MediaPlayer->Close(); }),
		FCanExecuteAction::CreateLambda([this] { return !MediaPlayer->GetUrl().IsEmpty(); })
	);

	ToolkitCommands->MapAction(
		Commands.ForwardMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->SetRate(GetForwardRate()); }),
		FCanExecuteAction::CreateLambda([this]{ return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(GetForwardRate(), false); })
	);

	ToolkitCommands->MapAction(
		Commands.NextMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->Next(); }),
		FCanExecuteAction::CreateLambda([this]{ return (MediaPlayer->GetPlaylistRef().Num() > 1); })
	);

	ToolkitCommands->MapAction(
		Commands.PauseMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->Pause(); }),
		FCanExecuteAction::CreateLambda([this]{ return MediaPlayer->CanPause() && !MediaPlayer->IsPaused(); })
	);

	ToolkitCommands->MapAction(
		Commands.PlayMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->Play(); }),
		FCanExecuteAction::CreateLambda([this]{ return MediaPlayer->IsReady() && (!MediaPlayer->IsPlaying() || (MediaPlayer->GetRate() != 1.0f)); })
	);

	ToolkitCommands->MapAction(
		Commands.PreviousMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->Previous(); }),
		FCanExecuteAction::CreateLambda([this]{ return (MediaPlayer->GetPlaylistRef().Num() > 1); })
	);

	ToolkitCommands->MapAction(
		Commands.ReverseMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->SetRate(GetReverseRate()); } ),
		FCanExecuteAction::CreateLambda([this]{ return MediaPlayer->IsReady() && MediaPlayer->SupportsRate(GetReverseRate(), false); })
	);

	ToolkitCommands->MapAction(
		Commands.RewindMedia,
		FExecuteAction::CreateLambda([this]{ MediaPlayer->Rewind(); }),
		FCanExecuteAction::CreateLambda([this]{ return MediaPlayer->IsReady() && MediaPlayer->SupportsSeeking() && MediaPlayer->GetTime() > FTimespan::Zero(); })
	);
}


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FMediaPlayerEditorToolkit::ExtendToolBar()
{
	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, const TSharedRef<FUICommandList> ToolkitCommands)
		{
			ToolbarBuilder.BeginSection("PlaybackControls");
			{
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().PreviousMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().RewindMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().ReverseMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().PlayMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().PauseMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().ForwardMedia);
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().NextMedia);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("MediaControls");
			{
				ToolbarBuilder.AddToolBarButton(FMediaPlayerEditorCommands::Get().CloseMedia);
			}
			ToolbarBuilder.EndSection();
		}
	};


	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, GetToolkitCommands())
	);

	AddToolbarExtender(ToolbarExtender);
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION


float FMediaPlayerEditorToolkit::GetForwardRate() const
{
	float Rate = MediaPlayer->GetRate();

	if (Rate < 1.0f)
	{
		Rate = 1.0f;
	}

	return 2.0f * Rate;
}


float FMediaPlayerEditorToolkit::GetReverseRate() const
{
	float Rate = MediaPlayer->GetRate();

	if (Rate > -1.0f)
	{
		return -1.0f;
	}

	return 2.0f * Rate;
}


/* FMediaPlayerEditorToolkit callbacks
 *****************************************************************************/

TSharedRef<SDockTab> FMediaPlayerEditorToolkit::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier)
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (TabIdentifier == MediaPlayerEditorToolkit::DetailsTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorDetails, *MediaPlayer, Style);
	}
	else if (TabIdentifier == MediaPlayerEditorToolkit::InfoTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorInfo, *MediaPlayer, Style);
	}
	else if (TabIdentifier == MediaPlayerEditorToolkit::MediaTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorMedia, *MediaPlayer, Style);
	}
	else if (TabIdentifier == MediaPlayerEditorToolkit::PlaylistTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorPlaylist, *MediaPlayer, Style);
	}
	else if (TabIdentifier == MediaPlayerEditorToolkit::StatsTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorStats, *MediaPlayer, Style);
	}
	else if (TabIdentifier == MediaPlayerEditorToolkit::ViewerTabId)
	{
		TabWidget = SNew(SMediaPlayerEditorViewer, *MediaPlayer, Style);
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
