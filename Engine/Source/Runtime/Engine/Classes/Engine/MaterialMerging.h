// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineTypes.h"
#include "MaterialMerging.generated.h"

UENUM()
enum ETextureSizingType
{
	TextureSizingType_UseSingleTextureSize UMETA(DisplayName = "Use TextureSize for all material properties"),
	TextureSizingType_UseAutomaticBiasedSizes UMETA(DisplayName = "Use automatically biased texture sizes based on TextureSize"),
	TextureSizingType_UseManualOverrideTextureSize UMETA(DisplayName = "Use per property manually overriden texture sizes"),
	TextureSizingType_UseSimplygonAutomaticSizing UMETA(DisplayName = "Use Simplygon's automatic texture sizing"),
	TextureSizingType_MAX,
};

UENUM()
enum EMaterialMergeType
{
	MaterialMergeType_Default,
	MaterialMergeType_Simplygon
};

USTRUCT(Blueprintable)
struct FMaterialProxySettings
{
	GENERATED_USTRUCT_BODY()

	// Size of generated BaseColor map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta =(ClampMin = "1", UIMin = "1"))
	FIntPoint TextureSize;

	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	TEnumAsByte<ETextureSizingType> TextureSizingType;

	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere)
	float GutterSpace;

	// Whether to generate normal map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bNormalMap;

	// Whether to generate metallic map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bMetallicMap;

	// Metallic constant
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bMetallicMap"))
	float MetallicConstant;

	// Whether to generate roughness map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bRoughnessMap;

	// Roughness constant
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bRoughnessMap"))
	float RoughnessConstant;

	// Whether to generate specular map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bSpecularMap;

	// Specular constant
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bSpecularMap"))
	float SpecularConstant;

	// Whether to generate emissive map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bEmissiveMap;

	// Whether to generate opacity map
	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bOpacityMap;

	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bOpacityMap"))
	float OpacityConstant;

	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bOpacityMaskMap;

	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bOpacityMaskMap"))
	float OpacityMaskConstant;

	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere)
	bool bAmbientOcclusionMap;

	UPROPERTY(Category = Material, BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", editcondition = "!bAmbientOcclusionMap"))
	float AmbientOcclusionConstant;

	// Override diffuse map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint DiffuseTextureSize;

	// Override normal map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint NormalTextureSize;

	// Override metallic map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint MetallicTextureSize;

	// Override roughness map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint RoughnessTextureSize;

	// Override specular map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint SpecularTextureSize;

	// Override emissive map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint EmissiveTextureSize;

	// Override opacity map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint OpacityTextureSize;
	
	// Override opacity map size
	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint OpacityMaskTextureSize;

	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere, meta = (ClampMin = "1", UIMin = "1"))
	FIntPoint AmbientOcclusionTextureSize;

	UPROPERTY()
	TEnumAsByte<EMaterialMergeType> MaterialMergeType;

	UPROPERTY(Category = Material, BlueprintReadWrite, AdvancedDisplay, EditAnywhere)
	TEnumAsByte<EBlendMode> BlendMode;

	FMaterialProxySettings()
		: TextureSize(1024, 1024)
		, TextureSizingType(TextureSizingType_UseSingleTextureSize)
		, GutterSpace(4.0f)
		, bNormalMap(true)
		, bMetallicMap(false)
		, MetallicConstant(0.0f)
		, bRoughnessMap(false)
		, RoughnessConstant(0.5f)
		, bSpecularMap(false)
		, SpecularConstant(0.5f)
		, bEmissiveMap(false)
		, bOpacityMap(false)
		, OpacityConstant(1.0f)
		, bOpacityMaskMap(false)
		, OpacityMaskConstant(1.0f)
		, bAmbientOcclusionMap(false)
		, AmbientOcclusionConstant(1.0f)
		, DiffuseTextureSize(1024, 1024)
		, NormalTextureSize(1024, 1024)
		, MetallicTextureSize(1024, 1024)
		, RoughnessTextureSize(1024, 1024)
		, SpecularTextureSize(1024, 1024)
		, EmissiveTextureSize(1024, 1024)
		, OpacityTextureSize(1024, 1024)
		, OpacityMaskTextureSize(1024, 1024)
		, AmbientOcclusionTextureSize(1024, 1024)
		, MaterialMergeType( EMaterialMergeType::MaterialMergeType_Default )
		, BlendMode(BLEND_Opaque)
	{
	}

	bool operator == (const FMaterialProxySettings& Other) const
	{
		return TextureSize == Other.TextureSize
			&& TextureSizingType == Other.TextureSizingType
			&& GutterSpace == Other.GutterSpace
			&& bNormalMap == Other.bNormalMap
			&& MetallicConstant == Other.MetallicConstant
			&& bMetallicMap == Other.bMetallicMap
			&& RoughnessConstant == Other.RoughnessConstant
			&& bRoughnessMap == Other.bRoughnessMap
			&& SpecularConstant == Other.SpecularConstant
			&& bSpecularMap == Other.bSpecularMap
			&& bEmissiveMap == Other.bEmissiveMap
			&& bOpacityMap == Other.bOpacityMap
			&& bOpacityMaskMap == Other.bOpacityMaskMap
			&& bAmbientOcclusionMap == Other.bAmbientOcclusionMap
			&& AmbientOcclusionConstant == Other.AmbientOcclusionConstant
			&& DiffuseTextureSize == Other.DiffuseTextureSize
			&& NormalTextureSize == Other.NormalTextureSize
			&& MetallicTextureSize == Other.MetallicTextureSize
			&& RoughnessTextureSize == Other.RoughnessTextureSize
			&& EmissiveTextureSize == Other.EmissiveTextureSize
			&& OpacityTextureSize == Other.OpacityTextureSize
			&& OpacityMaskTextureSize == Other.OpacityMaskTextureSize
			&& AmbientOcclusionTextureSize == Other.AmbientOcclusionTextureSize;
	}
};
