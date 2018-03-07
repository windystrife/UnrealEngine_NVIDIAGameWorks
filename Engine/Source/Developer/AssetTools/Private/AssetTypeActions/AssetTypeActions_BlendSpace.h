// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Animation/BlendSpace.h"

class FAssetTypeActions_BlendSpace : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_BlendSpace", "Blend Space"); }
	virtual FColor GetTypeColor() const override { return FColor(255,168,111); }
	virtual UClass* GetSupportedClass() const override { return UBlendSpace::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
