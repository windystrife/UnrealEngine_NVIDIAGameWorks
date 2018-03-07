// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

/** Struct describing a single template project */
struct FTemplateItem
{
	FText		Name;
	FText		Description;
	bool		bGenerateCode;
	FName		Type;

	FString		SortKey;
	FString		ProjectFile;

	TSharedPtr<FSlateBrush> Thumbnail;
	TSharedPtr<FSlateBrush> PreviewImage;

	FString		ClassTypes;
	FString		AssetTypes;
	FTemplateItem(FText InName, FText InDescription, bool bInGenerateCode, FName InType, FString InSortKey, FString InProjectFile, TSharedPtr<FSlateBrush> InThumbnail, TSharedPtr<FSlateBrush> InPreviewImage,FString InClassTypes, FString InAssetTypes)
		: Name(InName), Description(InDescription), bGenerateCode(bInGenerateCode), Type(InType), SortKey(MoveTemp(InSortKey)), ProjectFile(MoveTemp(InProjectFile)), Thumbnail(InThumbnail), PreviewImage(InPreviewImage)
		, ClassTypes(InClassTypes), AssetTypes(InAssetTypes)
	{}

	FTemplateItem(const FTemplateItem& InItem)
	{
		Name = InItem.Name;
		Description = InItem.Description;
		bGenerateCode = InItem.bGenerateCode;
		Type = InItem.Type;

		SortKey = InItem.SortKey;
		ProjectFile = InItem.ProjectFile;

		ClassTypes = InItem.ClassTypes;
		AssetTypes = InItem.AssetTypes;
	}
};

