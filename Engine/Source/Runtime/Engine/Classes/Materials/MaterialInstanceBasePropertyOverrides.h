// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "MaterialInstanceBasePropertyOverrides.generated.h"

/** Properties from the base material that can be overridden in material instances. */
USTRUCT()
struct ENGINE_API FMaterialInstanceBasePropertyOverrides
{
	GENERATED_USTRUCT_BODY()

	/** Enables override of the opacity mask clip value. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_OpacityMaskClipValue;

	/** Enables override of the blend mode. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_BlendMode;

	/** Enables override of the shading model. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_ShadingModel;

	/** Enables override of the dithered LOD transition property. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_DitheredLODTransition;

	/** Enables override of whether to shadow using masked opacity on translucent materials. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_CastDynamicShadowAsMasked;

	/** Enables override of the two sided property. */
	UPROPERTY(EditAnywhere, Category = Material)
	bool bOverride_TwoSided;

	// NVCHANGE_BEGIN: Add VXGI
	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiConeTracingEnabled : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_UsedWithVxgiVoxelization : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiAllowTesselationDuringVoxelization : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiOmniDirectional : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiProportionalEmittance : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiCoverageSupersampling : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiMaterialSamplingRate : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiOpacityNoiseScaleBias : 1;

	UPROPERTY(EditAnywhere, Category = Material)
	uint32 bOverride_VxgiVoxelizationThickness : 1;
	// NVCHANGE_END: Add VXGI

	/** If BlendMode is BLEND_Masked, the surface is not rendered where OpacityMask < OpacityMaskClipValue. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_OpacityMaskClipValue", NoSpinbox = true))
	float OpacityMaskClipValue;

	/** The blend mode */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_BlendMode"))
	TEnumAsByte<EBlendMode> BlendMode;

	/** The shading model */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_ShadingModel"))
	TEnumAsByte<EMaterialShadingModel> ShadingModel;

	/** Indicates that the material should be rendered without backface culling and the normal should be flipped for backfaces. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_TwoSided"))
	uint32 TwoSided : 1;

	/** Whether the material should support a dithered LOD transition when used with the foliage system. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_DitheredLODTransition"))
	uint32 DitheredLODTransition : 1;

	/** Whether the material should cast shadows as masked even though it has a translucent blend mode. */
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_CastShadowAsMasked", NoSpinbox = true))
	uint32 bCastDynamicShadowAsMasked:1;

	// NVCHANGE_BEGIN: Add VXGI
	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiConeTracingEnabled"))
	uint32 bVxgiConeTracingEnabled : 1;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_UsedWithVxgiVoxelization"))
	uint32 bUsedWithVxgiVoxelization : 1;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiAllowTesselationDuringVoxelization"))
	uint32 bVxgiAllowTesselationDuringVoxelization : 1;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiOmniDirectional"))
	uint32 bVxgiOmniDirectional : 1;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiProportionalEmittance"))
	uint32 bVxgiProportionalEmittance : 1;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiCoverageSupersampling"))
	uint32 bVxgiCoverageSupersampling : 1;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiMaterialSamplingRate"))
	TEnumAsByte<enum EVxgiMaterialSamplingRate> VxgiMaterialSamplingRate;

	UPROPERTY(EditAnywhere, Category = Material, meta = (editcondition = "bOverride_VxgiOpacityNoiseScaleBias"))
	FVector2D VxgiOpacityNoiseScaleBias;

	UPROPERTY(EditAnywhere, Category = Material, meta = (ClampMin = "0.0", ClampMax = "2.0", editcondition = "bOverride_VxgiVoxelizationThickness"))
	float VxgiVoxelizationThickness;
	// NVCHANGE_END: Add VXGI

	FMaterialInstanceBasePropertyOverrides();

	bool operator==(const FMaterialInstanceBasePropertyOverrides& Other)const;
	bool operator!=(const FMaterialInstanceBasePropertyOverrides& Other)const;
};
