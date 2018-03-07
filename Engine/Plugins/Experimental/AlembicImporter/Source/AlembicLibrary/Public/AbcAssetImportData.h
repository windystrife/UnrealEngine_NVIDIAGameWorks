// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "EditorFramework/AssetImportData.h"
#include "AbcAssetImportData.generated.h"

/**
* Base class for import data and options used when importing any asset from Alembic
*/
UCLASS()
class ALEMBICLIBRARY_API UAbcAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	TArray<FString> TrackNames;
};
