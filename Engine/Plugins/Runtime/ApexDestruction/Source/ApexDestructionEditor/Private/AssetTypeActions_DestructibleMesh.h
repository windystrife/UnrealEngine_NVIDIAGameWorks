// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_DestructibleMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DestructibleMesh", "Destructible Mesh"); }
	virtual FColor GetTypeColor() const override { return FColor(200,128,128); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Physics; }
	virtual bool HasActions( const TArray<UObject*>& InObjects ) const override { return false; }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool IsImportedAsset() const override { return false; }

	static void ExecuteCreateDestructibleMeshes(TArray<FAssetData> AssetData);
};
