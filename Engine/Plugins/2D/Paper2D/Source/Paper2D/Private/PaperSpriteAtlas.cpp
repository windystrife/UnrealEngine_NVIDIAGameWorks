// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriteAtlas.h"

//////////////////////////////////////////////////////////////////////////
// UPaperSpriteAtlas

UPaperSpriteAtlas::UPaperSpriteAtlas(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
	, MaxWidth(2048)
	, MaxHeight(2048)
	, MipCount(1)
	, PaddingType(EPaperSpriteAtlasPadding::DilateBorder)
	, Padding(1)
	, CompressionSettings(TextureCompressionSettings::TC_Default)
	, Filter(TextureFilter::TF_Bilinear)
	, bRebuildAtlas(false)
#endif
{
}

void UPaperSpriteAtlas::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

#if WITH_EDITORONLY_DATA
	OutTags.Add(FAssetRegistryTag(GET_MEMBER_NAME_CHECKED(UPaperSpriteAtlas, AtlasDescription), AtlasDescription, FAssetRegistryTag::TT_Hidden));
#endif
}

#if WITH_EDITORONLY_DATA

void UPaperSpriteAtlas::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	FPlatformMisc::CreateGuid(AtlasGUID);
}

void UPaperSpriteAtlas::PostInitProperties()
{
	Super::PostInitProperties();
	FPlatformMisc::CreateGuid(AtlasGUID);
}

#endif
