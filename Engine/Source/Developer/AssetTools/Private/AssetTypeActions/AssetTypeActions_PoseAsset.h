// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions/AssetTypeActions_AnimationAsset.h"
#include "Animation/PoseAsset.h"

class FAssetTypeActions_PoseAsset : public FAssetTypeActions_AnimationAsset
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PoseAsset", "Pose Asset"); }
	virtual FColor GetTypeColor() const override { return FColor(237, 28, 36); }
	virtual UClass* GetSupportedClass() const override { return UPoseAsset::StaticClass(); }
	virtual bool CanFilter() override { return true; }
};
