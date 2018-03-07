// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Toolkits/MediaPlaylistEditorToolkit.h"
#include "Editor.h"
#include "EditorStyleSet.h"
#include "EditorReimportHandler.h"
#include "MediaPlaylist.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SMediaPlaylistEditorDetails.h"
#include "Widgets/SMediaPlaylistEditorMedia.h"


#define LOCTEXT_NAMESPACE "FMediaPlaylistEditorToolkit"


namespace MediaPlaylistEditorToolkit
{
	static const FName AppIdentifier("MediaPlaylistEditorApp");
	static const FName DetailsTabId("Details");
	static const FName MediaTabId("Media");
}


/* FMediaPlaylistEditorToolkit structors
 *****************************************************************************/

FMediaPlaylistEditorToolkit::FMediaPlaylistEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: MediaPlaylist(nullptr)
	, Style(InStyle)
{ }


FMediaPlaylistEditorToolkit::~FMediaPlaylistEditorToolkit()
{
	FReimportManager::Instance()->OnPreReimport().RemoveAll(this);
	FReimportManager::Instance()->OnPostReimport().RemoveAll(this);

	GEditor->UnregisterForUndo(this);
}


/* FMediaPlaylistEditorToolkit interface
 *****************************************************************************/

void FMediaPlaylistEditorToolkit::Initialize(UMediaPlaylist* InMediaPlaylist, const EToolkitMode::Type InMode, const TSharedPtr<IToolkitHost>& InToolkitHost)
{
	MediaPlaylist = InMediaPlaylist;

	if (MediaPlaylist == nullptr)
	{
		return;
	}

	// Support undo/redo
	MediaPlaylist->SetFlags(RF_Transactional);
	GEditor->RegisterForUndo(this);

	// create tab layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("Standalone_MediaPlaylistEditor_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
				->SetOrientation(Orient_Vertical)
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
					// details
					FTabManager::NewStack()
						->AddTab(MediaPlaylistEditorToolkit::DetailsTabId, ETabState::OpenedTab)
						->SetSizeCoefficient(0.45f)
				)
				->Split
				(
					// media library
					FTabManager::NewStack()
						->AddTab(MediaPlaylistEditorToolkit::MediaTabId, ETabState::OpenedTab)
						->SetHideTabWell(true)
						->SetSizeCoefficient(0.45f)
				)
		);

	FAssetEditorToolkit::InitAssetEditor(
		InMode,
		InToolkitHost,
		MediaPlaylistEditorToolkit::AppIdentifier,
		Layout,
		true /*bCreateDefaultStandaloneMenu*/,
		true /*bCreateDefaultToolbar*/,
		InMediaPlaylist
	);
	
	RegenerateMenusAndToolbars();
}


/* FAssetEditorToolkit interface
 *****************************************************************************/

FString FMediaPlaylistEditorToolkit::GetDocumentationLink() const
{
	return FString(TEXT("Engine/Content/Types/MediaAssets/Properties/Interface"));
}


void FMediaPlaylistEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MediaPlaylistEditor", "Media Player Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(MediaPlaylistEditorToolkit::DetailsTabId, FOnSpawnTab::CreateSP(this, &FMediaPlaylistEditorToolkit::HandleTabManagerSpawnTab, MediaPlaylistEditorToolkit::DetailsTabId))
		.SetDisplayName(LOCTEXT("DetailsTabName", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(MediaPlaylistEditorToolkit::MediaTabId, FOnSpawnTab::CreateSP(this, &FMediaPlaylistEditorToolkit::HandleTabManagerSpawnTab, MediaPlaylistEditorToolkit::MediaTabId))
		.SetDisplayName(LOCTEXT("MediaTabName", "Media Library"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.Tabs.Media"));
}


void FMediaPlaylistEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(MediaPlaylistEditorToolkit::MediaTabId);	
	InTabManager->UnregisterTabSpawner(MediaPlaylistEditorToolkit::DetailsTabId);
}


/* IToolkit interface
 *****************************************************************************/

FText FMediaPlaylistEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Media Playlist Editor");
}


FName FMediaPlaylistEditorToolkit::GetToolkitFName() const
{
	return FName("MediaPlaylistEditor");
}


FLinearColor FMediaPlaylistEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}


FString FMediaPlaylistEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "MediaPlaylist ").ToString();
}


/* FGCObject interface
 *****************************************************************************/

void FMediaPlaylistEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(MediaPlaylist);
}


/* FEditorUndoClient interface
*****************************************************************************/

void FMediaPlaylistEditorToolkit::PostUndo(bool bSuccess)
{
}


void FMediaPlaylistEditorToolkit::PostRedo(bool bSuccess)
{
	PostUndo(bSuccess);
}


/* FMediaPlaylistEditorToolkit callbacks
 *****************************************************************************/

TSharedRef<SDockTab> FMediaPlaylistEditorToolkit::HandleTabManagerSpawnTab(const FSpawnTabArgs& Args, FName TabIdentifier)
{
	TSharedPtr<SWidget> TabWidget = SNullWidget::NullWidget;

	if (TabIdentifier == MediaPlaylistEditorToolkit::DetailsTabId)
	{
		TabWidget = SNew(SMediaPlaylistEditorDetails, *MediaPlaylist, Style);
	}
	else if (TabIdentifier == MediaPlaylistEditorToolkit::MediaTabId)
	{
		TabWidget = SNew(SMediaPlaylistEditorMedia, *MediaPlaylist, Style);
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::PanelTab)
		[
			TabWidget.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
