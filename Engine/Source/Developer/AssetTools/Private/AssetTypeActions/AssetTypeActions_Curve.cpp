// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Curve.h"
#include "EditorFramework/AssetImportData.h"
#include "ICurveAssetEditor.h"
#include "CurveAssetEditorModule.h"

void FAssetTypeActions_Curve::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Curve = Cast<UCurveBase>(*ObjIt);
		if (Curve != nullptr)
		{
			FCurveAssetEditorModule& CurveAssetEditorModule = FModuleManager::LoadModuleChecked<FCurveAssetEditorModule>( "CurveAssetEditor" );
			TSharedRef< ICurveAssetEditor > NewCurveAssetEditor = CurveAssetEditorModule.CreateCurveAssetEditor( Mode, EditWithinLevelEditor, Curve );
		}
	}
}

void FAssetTypeActions_Curve::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto Curve = CastChecked<UCurveBase>(Asset);
		if (Curve->AssetImportData)
		{
			Curve->AssetImportData->ExtractFilenames(OutSourceFilePaths);
		}
	}
}
