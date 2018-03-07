// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTools/MediaPlaylistActions.h"
#include "MediaPlaylist.h"
#include "Toolkits/MediaPlaylistEditorToolkit.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FMediaPlaylistActions constructors
 *****************************************************************************/

FMediaPlaylistActions::FMediaPlaylistActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FMediaPlaylistActions::CanFilter()
{
	return true;
}


uint32 FMediaPlaylistActions::GetCategories()
{
	return EAssetTypeCategories::Media;
}


FText FMediaPlaylistActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MediaPlaylist", "Media Playlist");
}


UClass* FMediaPlaylistActions::GetSupportedClass() const
{
	return UMediaPlaylist::StaticClass();
}


FColor FMediaPlaylistActions::GetTypeColor() const
{
	return FColor::Yellow;
}


void FMediaPlaylistActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MediaPlaylist = Cast<UMediaPlaylist>(*ObjIt);

		if (MediaPlaylist != nullptr)
		{
			TSharedRef<FMediaPlaylistEditorToolkit> EditorToolkit = MakeShareable(new FMediaPlaylistEditorToolkit(Style));
			EditorToolkit->Initialize(MediaPlaylist, Mode, EditWithinLevelEditor);
		}
	}
}


#undef LOCTEXT_NAMESPACE
