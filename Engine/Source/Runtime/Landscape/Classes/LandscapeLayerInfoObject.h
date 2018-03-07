// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "LandscapeLayerInfoObject.generated.h"

class UPhysicalMaterial;
struct FPropertyChangedEvent;

UCLASS(MinimalAPI)
class ULandscapeLayerInfoObject : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(VisibleAnywhere, Category=LandscapeLayerInfoObject, AssetRegistrySearchable)
	FName LayerName;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	UPhysicalMaterial* PhysMaterial;

	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	float Hardness;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=LandscapeLayerInfoObject)
	uint32 bNoWeightBlend:1;

	UPROPERTY(NonTransactional, Transient)
	bool IsReferencedFromLoadedData;
#endif // WITH_EDITORONLY_DATA

	/* The color to use for layer usage debug */
	UPROPERTY(EditAnywhere, Category=LandscapeLayerInfoObject)
	FLinearColor LayerUsageDebugColor;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	//~ End UObject Interface
#endif
};
