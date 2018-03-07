// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Editor/PixelInspector/Private/PixelInspectorResult.h"
#include "PixelInspectorView.generated.h"

#define FinalColorContextGridSize 7

UCLASS(HideCategories = Object, MinimalAPI)
class UPixelInspectorView : public UObject
{
	GENERATED_UCLASS_BODY()

	FLinearColor FinalColorContext[FinalColorContextGridSize*FinalColorContextGridSize];

	/** Final RGBA 8bits Color after tone mapping, default value is black. */
	UPROPERTY(VisibleAnywhere, category = FinalColor)
	FLinearColor FinalColor;

	/** HDR RGB Color. */
	UPROPERTY(VisibleAnywhere, category = SceneColor)
	FLinearColor SceneColor;

	/** HDR Luminance. */
	UPROPERTY(VisibleAnywhere, category = HDR)
	float Luminance;

	/** HDR RGB Color. */
	UPROPERTY(VisibleAnywhere, category = HDR)
	FLinearColor HdrColor;

	/** From the GBufferA RGB Channels. */
	UPROPERTY(VisibleAnywhere, category = GBufferA)
	FVector Normal;

	/** From the GBufferA A Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferA)
	float PerObjectGBufferData;

	/** From the GBufferB R Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferB)
	float Metallic;

	/** From the GBufferB G Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferB)
	float Specular;

	/** From the GBufferB B Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferB)
	float Roughness;

	/** From the GBufferB A Channel encoded with SelectiveOutputMask. */
	UPROPERTY(VisibleAnywhere, category = GBufferB)
	TEnumAsByte<enum EMaterialShadingModel> MaterialShadingModel;

	/** From the GBufferB A Channel encoded with ShadingModel. */
	UPROPERTY(VisibleAnywhere, category = GBufferB)
	int32 SelectiveOutputMask;

	/** From the GBufferC RGB Channels. */
	UPROPERTY(VisibleAnywhere, category = GBufferC)
	FLinearColor BaseColor;

	/** From the GBufferC A Channel encoded with AmbientOcclusion. */
	UPROPERTY(VisibleAnywhere, category = GBufferC)
	float IndirectIrradiance;

	/** From the GBufferC A Channel encoded with IndirectIrradiance. */
	UPROPERTY(VisibleAnywhere, category = GBufferC)
	float AmbientOcclusion;

	//Custom Data section

	/** From the GBufferD RGB Channels. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	FLinearColor SubSurfaceColor;

	/** From the GBufferD RGB Channels. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	FVector SubsurfaceProfile;

	/** From the GBufferD A Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float Opacity;

	/** From the GBufferD R Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float ClearCoat;

	/** From the GBufferD G Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float ClearCoatRoughness;

	/** From the GBufferD RG Channels. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	FVector WorldNormal;

	/** From the GBufferD B Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float BackLit;

	/** From the GBufferD A Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float Cloth;

	/** From the GBufferD RG Channels. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	FVector EyeTangent;

	/** From the GBufferD B Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float IrisMask;

	/** From the GBufferD A Channel. */
	UPROPERTY(VisibleAnywhere, category = GBufferD)
	float IrisDistance;

	void SetFromResult(PixelInspector::PixelInspectorResult &Result);
	/*
	//////////////////////////////////////////////////////////////////////////
	// Per shader model Data

	//MSM_Subsurface
	//MSM_PreintegratedSkin
	//MSM_SubsurfaceProfile
	//MSM_TwoSidedFoliage
	FVector SubSurfaceColor; // GBufferD RGB
	float Opacity; // GBufferD A

	//MSM_ClearCoat
	float ClearCoat; // GBufferD R
	float ClearCoatRoughness; // GBufferD G

	//MSM_Hair
	FVector WorldNormal; // GBufferD RG
	float BackLit; // GBufferD B

	//MSM_Cloth, Use also the sub surface color
	float Cloth; // GBufferD A

	//MSM_Eye
	FVector Tangent; // GBufferD RG
	float IrisMask; // GBufferD B
	float IrisDistance; // GBufferD A
	*/
};



