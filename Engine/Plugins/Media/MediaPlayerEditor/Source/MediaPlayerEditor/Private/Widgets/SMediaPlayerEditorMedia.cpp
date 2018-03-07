// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlayerEditorMedia.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Containers/ArrayBuilder.h"
#include "Toolkits/AssetEditorManager.h"
#include "FileMediaSource.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "MediaSource.h"

#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SMediaPlayerEditorMedia"


/* SMediaPlayerEditorMedia interface
 *****************************************************************************/

void SMediaPlayerEditorMedia::Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer, const TSharedRef<ISlateStyle>& InStyle)
{
	MediaPlayer = &InMediaPlayer;
	Style = InStyle;

	// initialize asset picker
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.Filter.ClassNames.Add(UMediaPlaylist::StaticClass()->GetFName());
		AssetPickerConfig.Filter.ClassNames.Add(UMediaSource::StaticClass()->GetFName());
		AssetPickerConfig.Filter.bRecursiveClasses = true;

		AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoMediaSourcesFound", "No media sources or play lists found.");
		AssetPickerConfig.bAllowDragging = false;
		AssetPickerConfig.bAutohideSearchBar = true;
		AssetPickerConfig.bCanShowClasses = false;
		AssetPickerConfig.bCanShowDevelopersFolder = true;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::Column;
		AssetPickerConfig.ThumbnailScale = 0.1f;

		AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SMediaPlayerEditorMedia::HandleAssetPickerAssetDoubleClicked);
		AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateSP(this, &SMediaPlayerEditorMedia::HandleAssetPickerAssetEnterPressed);
		AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SMediaPlayerEditorMedia::HandleAssetPickerGetAssetContextMenu);
	}

	auto& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	ChildSlot
	[
		SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.ToolTipText(LOCTEXT("DoubleClickToAddToolTip", "Double-click a media source or playlist to open it in the player."))
			[
				ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
			]
	];
}


/* SMediaPlayerEditorMedia implementation
 *****************************************************************************/

void SMediaPlayerEditorMedia::OpenMediaAsset(UObject* Asset)
{
	UMediaSource* MediaSource = Cast<UMediaSource>(Asset);

	if (MediaSource != nullptr)
	{
		if (!MediaPlayer->OpenSource(MediaSource))
		{
			ShowMediaOpenFailedMessage();
		}

		return;
	}

	UMediaPlaylist* MediaPlaylist = Cast<UMediaPlaylist>(Asset);

	if (MediaPlaylist != nullptr)
	{
		if (!MediaPlayer->OpenPlaylist(MediaPlaylist))
		{
			ShowMediaOpenFailedMessage();
		}
	}
}


void SMediaPlayerEditorMedia::ShowMediaOpenFailedMessage()
{
	FNotificationInfo NotificationInfo(LOCTEXT("MediaOpenFailedError", "The media failed to open. Check Output Log for details!"));
	{
		NotificationInfo.ExpireDuration = 2.0f;
	}

	FSlateNotificationManager::Get().AddNotification(NotificationInfo)->SetCompletionState(SNotificationItem::CS_Fail);
}


/* SMediaPlayerEditorMedia callbacks
 *****************************************************************************/

void SMediaPlayerEditorMedia::HandleAssetPickerAssetDoubleClicked(const struct FAssetData& AssetData)
{
	OpenMediaAsset(AssetData.GetAsset());
}


void SMediaPlayerEditorMedia::HandleAssetPickerAssetEnterPressed(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.Num() > 0)
	{
		OpenMediaAsset(SelectedAssets[0].GetAsset());
	}
}


TSharedPtr<SWidget> SMediaPlayerEditorMedia::HandleAssetPickerGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets)
{
	if (SelectedAssets.Num() <= 0)
	{
		return nullptr;
	}

	UObject* SelectedAsset = SelectedAssets[0].GetAsset();

	if (SelectedAsset == nullptr)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true /*bInShouldCloseWindowAfterMenuSelection=*/, nullptr);
	{
		// media section
		MenuBuilder.BeginSection("MediaSection", LOCTEXT("MediaSection", "Media"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("EditMenuAction", "Edit..."),
				LOCTEXT("EditMenuActionTooltip", "Opens the selected asset for edit."),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "ContentBrowser.AssetActions.Edit"),
				FUIAction(
					FExecuteAction::CreateLambda([=]() {
						FAssetEditorManager::Get().OpenEditorForAsset(SelectedAsset);
					})
				)
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("OpenMenuAction", "Open"),
				LOCTEXT("OpenMenuActionTooltip", "Open this media asset in the player"),
				FSlateIcon(Style->GetStyleSetName(), "MediaPlayerEditor.NextMedia.Small"),
				FUIAction(
					FExecuteAction::CreateLambda([=]() {
						OpenMediaAsset(SelectedAsset);
					})
				)
			);
		}
		MenuBuilder.EndSection();

		// asset section
		MenuBuilder.BeginSection("AssetSection", LOCTEXT("AssetSection", "Asset"));
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("FindInCbMenuAction", "Find in Content Browser"),
				LOCTEXT("FindInCbMenuActionTooltip", "Summons the Content Browser and navigates to the selected asset"),
				FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
				FUIAction(
					FExecuteAction::CreateLambda([=]() {
						TArray<UObject*> AssetsToSync = TArrayBuilder<UObject*>().Add(SelectedAsset);
						GEditor->SyncBrowserToObjects(AssetsToSync);
					})
				)
			);

			auto FileMediaSource = Cast<UFileMediaSource>(SelectedAsset);

			if (FileMediaSource != nullptr)
			{
				FFormatNamedArguments Args;
				{
					Args.Add(TEXT("FileManagerName"), FPlatformMisc::GetFileManagerName());
				}

				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("OpenInFileManager", "Show Media File in {FileManagerName}"), Args),
					LOCTEXT("OpenInFileManagerTooltip", "Finds the media file that this asset points to on disk"),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "SystemWideCommands.FindInContentBrowser"),
					FUIAction(
						FExecuteAction::CreateLambda([=]() {
							FPlatformProcess::ExploreFolder(*FileMediaSource->GetFullPath());
						}),
						FCanExecuteAction::CreateLambda([=]() -> bool {
							return FileMediaSource->Validate();
						})
					)
				);
			}
		}
		MenuBuilder.EndSection();
	}

	return MenuBuilder.MakeWidget();
}


#undef LOCTEXT_NAMESPACE
