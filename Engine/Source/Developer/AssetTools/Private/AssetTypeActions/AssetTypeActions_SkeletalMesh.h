// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Engine/SkeletalMesh.h"

class FMenuBuilder;

class FAssetTypeActions_SkeletalMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SkeletalMesh", "Skeletal Mesh"); }
	virtual FColor GetTypeColor() const override { return FColor(255,0,255); }
	virtual UClass* GetSupportedClass() const override { return USkeletalMesh::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic | EAssetTypeCategories::Animation; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	
private:
	/** Handler for when skeletal mesh LOD import is selected */
	void LODImport(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when skeletal mesh LOD sub menu is opened */
	void GetLODMenu(FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler to create the menu for new physics assets */
	void GetPhysicsAssetMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when NewPhysicsAsset is selected */
	void ExecuteNewPhysicsAsset(TArray<TWeakObjectPtr<USkeletalMesh>> Objects, bool bSetAssetToMesh);
	
	/** Handler for when NewSkeleton is selected */
	void ExecuteNewSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when AssignSkeleton is selected */
	void ExecuteAssignSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for when FindSkeleton is selected */
	void ExecuteFindSkeleton(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	/** Handler for skeletal mesh import */
	static void ExecuteImportMeshLOD(class UObject* Mesh, int32 LOD);

	/** Handler to import clothing to a mesh */
	void ExecuteImportClothing(TArray<TWeakObjectPtr<USkeletalMesh>> Objects);

	// Helper functions
private:
	/** Assigns a skeleton to the mesh */
	void AssignSkeletonToMesh(USkeletalMesh* SkelMesh) const;

	void OnAssetCreated(TArray<UObject*> NewAssets) const;

	void FillSourceMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const;
	void FillSkeletonMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const;
	void FillCreateMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeletalMesh>> Meshes) const;
};
