// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Texture.h"
#include "EditorFramework/AssetImportData.h"
#include "TileMapAssetImportData.generated.h"

class UPaperTileSet;

USTRUCT()
struct FTileSetImportMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString SourceName;

	UPROPERTY()
	TWeakObjectPtr<class UPaperTileSet> ImportedTileSet;

	UPROPERTY()
	TWeakObjectPtr<class UTexture> ImportedTexture;
};

/**
 * Base class for import data and options used when importing a tile map
 */
UCLASS()
class PAPER2DEDITOR_API UTileMapAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	TArray<FTileSetImportMapping> TileSetMap;

	static UTileMapAssetImportData* GetImportDataForTileMap(class UPaperTileMap* TileMap);
};
