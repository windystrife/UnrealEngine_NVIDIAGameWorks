// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PaperSpriterImportData.h"

//////////////////////////////////////////////////////////////////////////
// UPaperSpriterImportData

UPaperSpriterImportData::UPaperSpriterImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

void UPaperSpriterImportData::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	if (AssetImportData != nullptr)
	{
		OutTags.Add( FAssetRegistryTag(SourceFileTagName(), AssetImportData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}

	Super::GetAssetRegistryTags(OutTags);
}
