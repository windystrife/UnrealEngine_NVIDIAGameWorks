// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;

class FAssetTypeActions_StaticMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_StaticMesh", "Static Mesh"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 255, 255); }
	virtual UClass* GetSupportedClass() const override { return UStaticMesh::StaticClass(); }
	virtual bool HasActions( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	// End IAssetTypeActions

private:

	/** Handler for when CopyLODDData is selected */
	void ExecuteCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects);
	
	/** Whether there is a valid static mesh to copy LOD from */
	bool CanCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const;

	/** Handler for when PasteLODDData is selected */
	void ExecutePasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Whether there is a valid static meshes to copy LOD to*/
	bool CanPasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const;

	/** Handler for when SaveGeneratedLODsInPackage is selected */
	void ExecuteSaveGeneratedLODsInPackage(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler for when RemoveVertexColors is selected */
	void ExecuteRemoveVertexColors(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler to provide the list of LODs that can be imported or reimported */
	void GetImportLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler to provide the LOD sub-menu. Hides away LOD actions - includes Import LOD sub menu */
	void GetLODMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Meshes);

	/** Handler for calling import methods */
	static void ExecuteImportMeshLOD(UObject* Mesh, int32 LOD);
private:

	TWeakObjectPtr<UStaticMesh> LODCopyMesh;	
};
