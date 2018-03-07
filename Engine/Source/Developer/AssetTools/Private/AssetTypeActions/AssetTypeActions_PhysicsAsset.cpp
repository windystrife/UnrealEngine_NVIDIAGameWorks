// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_PhysicsAsset.h"
#include "Editor/PhysicsAssetEditor/Public/PhysicsAssetEditorModule.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_PhysicsAsset::GetSupportedClass() const
{
	return UPhysicsAsset::StaticClass();
}

UThumbnailInfo* FAssetTypeActions_PhysicsAsset::GetThumbnailInfo(UObject* Asset) const
{
	UPhysicsAsset* PhysicsAsset = CastChecked<UPhysicsAsset>(Asset);
	UThumbnailInfo* ThumbnailInfo = PhysicsAsset->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<USceneThumbnailInfo>(PhysicsAsset, NAME_None, RF_Transactional);
		PhysicsAsset->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

void FAssetTypeActions_PhysicsAsset::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto PhysicsAsset = Cast<UPhysicsAsset>(*ObjIt);
		if (PhysicsAsset != NULL)
		{
			
			IPhysicsAssetEditorModule* PhysicsAssetEditorModule = &FModuleManager::LoadModuleChecked<IPhysicsAssetEditorModule>( "PhysicsAssetEditor" );
			PhysicsAssetEditorModule->CreatePhysicsAssetEditor(Mode, EditWithinLevelEditor, PhysicsAsset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
