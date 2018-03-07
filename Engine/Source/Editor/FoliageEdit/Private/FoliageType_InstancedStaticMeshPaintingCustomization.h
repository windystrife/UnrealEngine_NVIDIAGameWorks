// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

struct FAssetData;
class FEdModeFoliage;
class IDetailLayoutBuilder;

/////////////////////////////////////////////////////
// FFoliageTypePaintingCustomization
class FFoliageType_InstancedStaticMeshPaintingCustomization : public IDetailCustomization
{
public:
	// Makes a new instance of this detail layout class
	static TSharedRef<IDetailCustomization> MakeInstance(FEdModeFoliage* InFoliageEditMode);

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
	// End of IDetailCustomization interface

private:
	FFoliageType_InstancedStaticMeshPaintingCustomization(FEdModeFoliage* InFoliageEditMode);
	bool OnShouldFilterAsset(const FAssetData& AssetData) const;

	/** Pointer to the foliage edit mode. */
	FEdModeFoliage* FoliageEditMode;

	/** The static meshes that are not available to local foliage types (because they are already assigned to a local foliage type) */
	TArray<FName> UnavailableMeshNames;
};
