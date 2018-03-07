// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DestructibleMesh.h"
#include "DestructibleMesh.h"
#include "ApexDestructionEditorModule.h"
#include "Engine/StaticMesh.h"
#include "SNotificationList.h"
#include "NotificationManager.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_DestructibleMesh::GetSupportedClass() const
{
	return UDestructibleMesh::StaticClass();
}

void FAssetTypeActions_DestructibleMesh::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Mesh = Cast<UDestructibleMesh>(*ObjIt);
		if (Mesh != NULL)
		{
			FDestructibleMeshEditorModule& DestructibleMeshEditorModule = FModuleManager::LoadModuleChecked<FDestructibleMeshEditorModule>( "ApexDestructionEditor" );
			TSharedRef< IDestructibleMeshEditor > NewDestructibleMeshEditor = DestructibleMeshEditorModule.CreateDestructibleMeshEditor( EToolkitMode::Standalone, EditWithinLevelEditor, Mesh );
		}
	}
}

void FAssetTypeActions_DestructibleMesh::ExecuteCreateDestructibleMeshes(TArray<FAssetData> AssetData)
{
	TArray<UDestructibleMesh*> NewAssets;
	NewAssets.Reserve(AssetData.Num());

	for(const FAssetData& Asset : AssetData)
	{
		if(Asset.AssetClass == UStaticMesh::StaticClass()->GetFName())
		{
			if(UStaticMesh* StaticMesh = Cast<UStaticMesh>(Asset.GetAsset()))
			{
				FText ErrorMsg;
				FDestructibleMeshEditorModule& DestructibleMeshEditorModule = FModuleManager::LoadModuleChecked<FDestructibleMeshEditorModule>("ApexDestructionEditor");
				UDestructibleMesh* DestructibleMesh = DestructibleMeshEditorModule.CreateDestructibleMeshFromStaticMesh(StaticMesh->GetOuter(), StaticMesh, NAME_None, StaticMesh->GetFlags(), ErrorMsg);
				if (DestructibleMesh)
				{
					FAssetEditorManager::Get().OpenEditorForAsset(DestructibleMesh);
					NewAssets.Add(DestructibleMesh);
				}
				else if (!ErrorMsg.IsEmpty())
				{
					FNotificationInfo ErrorNotification(ErrorMsg);
					FSlateNotificationManager::Get().AddNotification(ErrorNotification);
				}
			}
		}
	}
	
	if (NewAssets.Num() > 0)
	{
		//FAssetTools::Get().SyncBrowserToAssets(NewAssets);
	}
}

#undef LOCTEXT_NAMESPACE
