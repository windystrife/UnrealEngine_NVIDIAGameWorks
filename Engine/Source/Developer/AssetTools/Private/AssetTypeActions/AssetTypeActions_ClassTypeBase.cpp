// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ClassTypeBase.h"
#include "IClassTypeActions.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

TSharedPtr<SWidget> FAssetTypeActions_ClassTypeBase::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	TSharedPtr<IClassTypeActions> ClassTypeActions = GetClassTypeActions(AssetData).Pin();
	if(ClassTypeActions.IsValid())
	{
		return ClassTypeActions->GetThumbnailOverlay(AssetData);
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
