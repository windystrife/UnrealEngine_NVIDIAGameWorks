// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTools/MediaSourceActions.h"
#include "AssetData.h"
#include "MediaSource.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FMediaSourceActions::CanFilter()
{
	return false;
}


FText FMediaSourceActions::GetAssetDescription(const struct FAssetData& AssetData) const
{
	auto MediaSource = Cast<UMediaSource>(AssetData.GetAsset());

	if (MediaSource != nullptr)
	{
		FString Url = MediaSource->GetUrl();

		if (Url.IsEmpty())
		{
			return LOCTEXT("AssetTypeActions_MediaSourceMissing", "Warning: Missing settings detected!");
		}

		if (!MediaSource->Validate())
		{
			return LOCTEXT("AssetTypeActions_MediaSourceInvalid", "Warning: Invalid settings detected!");
		}
	}

	return FText::GetEmpty();
}


uint32 FMediaSourceActions::GetCategories()
{
	return EAssetTypeCategories::Media;
}


FText FMediaSourceActions::GetName() const
{
	return FText::GetEmpty(); // let factory return sanitized class name
}


UClass* FMediaSourceActions::GetSupportedClass() const
{
	return UMediaSource::StaticClass();
}


FColor FMediaSourceActions::GetTypeColor() const
{
	return FColor::White;
}


TSharedPtr<class SWidget> FMediaSourceActions::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	return nullptr;
/*	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetNoBrush())
		.Visibility(EVisibility::HitTestInvisible)
		.Padding(3.0f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			SNew(SImage)
				.Image(FEditorStyle::GetBrush("Icons.Error"))
		];*/
}


#undef LOCTEXT_NAMESPACE
