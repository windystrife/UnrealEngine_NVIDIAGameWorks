// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ProceduralFoliageSpawner.h"
#include "Settings/EditorExperimentalSettings.h"
#include "ProceduralFoliageSpawner.h"

UClass* FAssetTypeActions_ProceduralFoliageSpawner::GetSupportedClass() const
{
	return UProceduralFoliageSpawner::StaticClass();
}

bool FAssetTypeActions_ProceduralFoliageSpawner::CanFilter()
{
	return GetDefault<UEditorExperimentalSettings>()->bProceduralFoliage;
}
