// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTools/MediaPlayerActions.h"
#include "MediaPlayer.h"
#include "Misc/PackageName.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "EditorStyleSet.h"
#include "MediaTexture.h"
#include "Toolkits/MediaPlayerEditorToolkit.h"
#include "Factories/MediaTextureFactoryNew.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FMediaPlayerActions constructors
 *****************************************************************************/

FMediaPlayerActions::FMediaPlayerActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{ }


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FMediaPlayerActions::CanFilter()
{
	return true;
}


uint32 FMediaPlayerActions::GetCategories()
{
	return EAssetTypeCategories::Media;
}


FText FMediaPlayerActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MediaPlayer", "Media Player");
}


UClass* FMediaPlayerActions::GetSupportedClass() const
{
	return UMediaPlayer::StaticClass();
}


FColor FMediaPlayerActions::GetTypeColor() const
{
	return FColor::Red;
}


void FMediaPlayerActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto MediaPlayer = Cast<UMediaPlayer>(*ObjIt);

		if (MediaPlayer != nullptr)
		{
			TSharedRef<FMediaPlayerEditorToolkit> EditorToolkit = MakeShareable(new FMediaPlayerEditorToolkit(Style));
			EditorToolkit->Initialize(MediaPlayer, Mode, EditWithinLevelEditor);
		}
	}
}


#undef LOCTEXT_NAMESPACE
