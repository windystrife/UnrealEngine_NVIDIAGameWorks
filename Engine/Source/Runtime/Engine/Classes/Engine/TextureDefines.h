// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "TextureDefines.generated.h"

/**
 * @warning: if this is changed:
 *     update BaseEngine.ini [SystemSettings]
 *     you might have to update the update Game's DefaultEngine.ini [SystemSettings]
 *     order and actual name can never change (order is important!)
 *
 * TEXTUREGROUP_Cinematic: should be used for Cinematics which will be baked out
 *                         and want to have the highest settings
 */
UENUM()
enum TextureGroup
{
	TEXTUREGROUP_World UMETA(DisplayName="World"),
	TEXTUREGROUP_WorldNormalMap UMETA(DisplayName="WorldNormalMap"),
	TEXTUREGROUP_WorldSpecular UMETA(DisplayName="WorldSpecular"),
	TEXTUREGROUP_Character UMETA(DisplayName="Character"),
	TEXTUREGROUP_CharacterNormalMap UMETA(DisplayName="CharacterNormalMap"),
	TEXTUREGROUP_CharacterSpecular UMETA(DisplayName="CharacterSpecular"),
	TEXTUREGROUP_Weapon UMETA(DisplayName="Weapon"),
	TEXTUREGROUP_WeaponNormalMap UMETA(DisplayName="WeaponNormalMap"),
	TEXTUREGROUP_WeaponSpecular UMETA(DisplayName="WeaponSpecular"),
	TEXTUREGROUP_Vehicle UMETA(DisplayName="Vehicle"),
	TEXTUREGROUP_VehicleNormalMap UMETA(DisplayName="VehicleNormalMap"),
	TEXTUREGROUP_VehicleSpecular UMETA(DisplayName="VehicleSpecular"),
	TEXTUREGROUP_Cinematic UMETA(DisplayName="Cinematic"),
	TEXTUREGROUP_Effects UMETA(DisplayName="Effects"),
	TEXTUREGROUP_EffectsNotFiltered UMETA(DisplayName="EffectsNotFiltered"),
	TEXTUREGROUP_Skybox UMETA(DisplayName="Skybox"),
	TEXTUREGROUP_UI UMETA(DisplayName="UI"),
	TEXTUREGROUP_Lightmap UMETA(DisplayName="Lightmap"),
	TEXTUREGROUP_RenderTarget UMETA(DisplayName="RenderTarget"),
	TEXTUREGROUP_MobileFlattened UMETA(DisplayName="MobileFlattened"),
	/** Obsolete - kept for backwards compatibility. */
	TEXTUREGROUP_ProcBuilding_Face UMETA(DisplayName = "ProcBuilding_Face"),
	/** Obsolete - kept for backwards compatibility. */
	TEXTUREGROUP_ProcBuilding_LightMap UMETA(DisplayName="ProcBuilding_LightMap"),
	TEXTUREGROUP_Shadowmap UMETA(DisplayName="Shadowmap"),
	/** No compression, no mips. */
	TEXTUREGROUP_ColorLookupTable UMETA(DisplayName="ColorLookupTable"),
	TEXTUREGROUP_Terrain_Heightmap UMETA(DisplayName="Terrain_Heightmap"),
	TEXTUREGROUP_Terrain_Weightmap UMETA(DisplayName="Terrain_Weightmap"),
	/** Using this TextureGroup triggers special mip map generation code only useful for the BokehDOF post process. */
	TEXTUREGROUP_Bokeh UMETA(DisplayName="Bokeh"),
	/** No compression, created on import of a .IES file. */
	TEXTUREGROUP_IESLightProfile UMETA(DisplayName="IESLightProfile"),
	/** Non-filtered, useful for 2D rendering. */
	TEXTUREGROUP_Pixels2D UMETA(DisplayName="2D Pixels (unfiltered)"),
	/** Hierarchical LOD generated textures*/
	TEXTUREGROUP_HierarchicalLOD UMETA(DisplayName = "Hierarchical LOD"),
	TEXTUREGROUP_MAX,
};

UENUM()
enum TextureMipGenSettings
{
	/** Default for the "texture". */
	TMGS_FromTextureGroup UMETA(DisplayName="FromTextureGroup"),
	/** 2x2 average, default for the "texture group". */
	TMGS_SimpleAverage UMETA(DisplayName="SimpleAverage"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen0 UMETA(DisplayName="Sharpen0"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen1 UMETA(DisplayName = "Sharpen1"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen2 UMETA(DisplayName = "Sharpen2"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen3 UMETA(DisplayName = "Sharpen3"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen4 UMETA(DisplayName = "Sharpen4"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen5 UMETA(DisplayName = "Sharpen5"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen6 UMETA(DisplayName = "Sharpen6"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen7 UMETA(DisplayName = "Sharpen7"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen8 UMETA(DisplayName = "Sharpen8"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen9 UMETA(DisplayName = "Sharpen9"),
	/** 8x8 with sharpening: 0=no sharpening but better quality which is softer, 1=little, 5=medium, 10=extreme. */
	TMGS_Sharpen10 UMETA(DisplayName = "Sharpen10"),
	TMGS_NoMipmaps UMETA(DisplayName="NoMipmaps"),
	/** Do not touch existing mip chain as it contains generated data. */
	TMGS_LeaveExistingMips UMETA(DisplayName="LeaveExistingMips"),
	/** Blur further (useful for image based reflections). */
	TMGS_Blur1 UMETA(DisplayName="Blur1"),
	TMGS_Blur2 UMETA(DisplayName="Blur2"),
	TMGS_Blur3 UMETA(DisplayName="Blur3"),
	TMGS_Blur4 UMETA(DisplayName="Blur4"),
	TMGS_Blur5 UMETA(DisplayName="Blur5"),
	TMGS_MAX,

	// Note: These are serialized as as raw values in the texture DDC key, so additional entries
	// should be added at the bottom; reordering or removing entries will require changing the GUID
	// in the texture compressor DDC key
};

/** Options for texture padding mode. */
UENUM()
namespace ETexturePowerOfTwoSetting
{
	enum Type
	{
		/** Do not modify the texture dimensions. */
		None,

		/** Pad the texture to the nearest power of two size. */
		PadToPowerOfTwo,

		/** Pad the texture to the nearest square power of two size. */
		PadToSquarePowerOfTwo

		// Note: These are serialized as as raw values in the texture DDC key, so additional entries
		// should be added at the bottom; reordering or removing entries will require changing the GUID
		// in the texture compressor DDC key
	};
}

// Must match enum ESamplerFilter in RHIDefinitions.h
UENUM()
enum class ETextureSamplerFilter : uint8
{
	Point,
	Bilinear,
	Trilinear,
	AnisotropicPoint,
	AnisotropicLinear,
};
