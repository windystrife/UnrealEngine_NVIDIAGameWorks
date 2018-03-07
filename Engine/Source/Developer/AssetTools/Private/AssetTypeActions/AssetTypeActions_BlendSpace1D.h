// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Animation/BlendSpace1D.h"

class FAssetTypeActions_BlendSpace1D : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_BlendSpace1D", "Blend Space 1D"); }
	virtual FColor GetTypeColor() const override { return FColor(255,180,130); }
	virtual UClass* GetSupportedClass() const override { return UBlendSpace1D::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
