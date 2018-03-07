// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionLandscapeLayerCoords.generated.h"

UENUM()
enum ETerrainCoordMappingType
{
	TCMT_Auto,
	TCMT_XY,
	TCMT_XZ,
	TCMT_YZ,
	TCMT_MAX,
};

UENUM()
enum ELandscapeCustomizedCoordType
{
	/** Don't use customized UV, just use original UV. */
	LCCT_None,
	LCCT_CustomUV0,
	LCCT_CustomUV1,
	LCCT_CustomUV2,
	/** Use original WeightMapUV, which could not be customized. */
	LCCT_WeightMapUV,
	LCCT_MAX,
};

UCLASS(collapsecategories, hidecategories=Object)
class LANDSCAPE_API UMaterialExpressionLandscapeLayerCoords : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()

	/** Determines the mapping place to use on the terrain. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	TEnumAsByte<enum ETerrainCoordMappingType> MappingType;

	/** Determines the mapping place to use on the terrain. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	TEnumAsByte<enum ELandscapeCustomizedCoordType> CustomUVType;

	/** Uniform scale to apply to the mapping. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingScale;

	/** Rotation to apply to the mapping. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingRotation;

	/** Offset to apply to the mapping along U. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingPanU;

	/** Offset to apply to the mapping along V. */
	UPROPERTY(EditAnywhere, Category=MaterialExpressionLandscapeLayerCoords)
	float MappingPanV;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface

	//~ Begin UObject Interface
	virtual bool NeedsLoadForClient() const override;
	//~ End UObject Interface
};



