// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialInstanceConstant.h"
#include "LandscapeMaterialInstanceConstant.generated.h"

UCLASS(MinimalAPI)
class ULandscapeMaterialInstanceConstant : public UMaterialInstanceConstant
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	uint32 bIsLayerThumbnail:1;

	UPROPERTY()
	uint32 bDisableTessellation:1;

	virtual FMaterialResource* AllocatePermutationResource() override;
	virtual bool HasOverridenBaseProperties() const override;
};

