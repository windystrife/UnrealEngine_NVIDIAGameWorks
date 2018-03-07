// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/TextureDefines.h"
#include "TextureLODSettings.generated.h"

class UTexture;

/** LOD settings for a single texture group. */
USTRUCT()
struct ENGINE_API FTextureLODGroup
{
	GENERATED_USTRUCT_BODY()

	FTextureLODGroup()
		: MinLODMipCount(0)
		, MaxLODMipCount(12)
		, LODBias(0)
		, NumStreamedMips(-1)
		, MipGenSettings(TextureMipGenSettings::TMGS_SimpleAverage)
		, MinLODSize(1)
		, MaxLODSize(4096)
		, MinMagFilter(NAME_Aniso)
		, MipFilter(NAME_Point)
	{
		SetupGroup();
	}

	/** Minimum LOD mip count below which the code won't bias.						*/
	UPROPERTY()
	TEnumAsByte<TextureGroup> Group;

	/** Minimum LOD mip count below which the code won't bias.						*/
	int32 MinLODMipCount;
	
	/** Maximum LOD mip count. Bias will be adjusted so texture won't go above.		*/
	int32 MaxLODMipCount;
	
	/** Group LOD bias.																*/
	UPROPERTY()
	int32 LODBias;
	
	/** Sampler filter state.														*/
	ETextureSamplerFilter Filter;
	
	/** Number of mip-levels that can be streamed. -1 means all mips can stream.	*/
	UPROPERTY()
	int32 NumStreamedMips;

	/** Defines how the the mip-map generation works, e.g. sharpening				*/
	UPROPERTY()
	TEnumAsByte<TextureMipGenSettings> MipGenSettings;

	UPROPERTY()
	int32 MinLODSize;

	UPROPERTY()
	int32 MaxLODSize;

	UPROPERTY()
	FName MinMagFilter;

	UPROPERTY()
	FName MipFilter;

	void SetupGroup();
};

/**
 * Structure containing all information related to an LOD group and providing helper functions to calculate
 * the LOD bias of a given group.
 */
UCLASS(config=DeviceProfiles, perObjectConfig)
class ENGINE_API UTextureLODSettings : public UObject
{
	GENERATED_UCLASS_BODY()

public:


	/**
	 * Calculates and returns the LOD bias based on texture LOD group, LOD bias and maximum size.
	 *
	 * @param	Texture				Texture object to calculate LOD bias for.
	 * @param	bIncCinematicMips	If true, cinematic mips will also be included in consideration
	 * @return	LOD bias
	 */
	int32 CalculateLODBias(const UTexture* Texture, bool bIncCinematicMips = true) const;

	/**
	 * Calculates and returns the LOD bias based on the information provided.
	 *
	 * @param	Width						Width of the texture
	 * @param	Height						Height of the texture
	 * @param	LODGroup					Which LOD group the texture belongs to
	 * @param	LODBias						LOD bias to include in the calculation
	 * @param	NumCinematicMipLevels		The texture cinematic mip levels to include in the calculation
	 * @param	MipGenSetting				Mip generation setting
	 * @return	LOD bias
	 */
	int32 CalculateLODBias( int32 Width, int32 Height, int32 LODGroup, int32 LODBias, int32 NumCinematicMipLevels, TextureMipGenSettings MipGenSetting ) const;


#if WITH_EDITORONLY_DATA
	void GetMipGenSettings( const UTexture& Texture, TextureMipGenSettings& OutMipGenSettings, float& OutSharpen, uint32& OutKernelSize, bool& bOutDownsampleWithAverage, bool& bOutSharpenWithoutColorShift, bool &bOutBorderColorBlack ) const;
#endif // #if WITH_EDITORONLY_DATA

	/**
	 * Will return the LODBias for a passed in LODGroup
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	LODBias
	 */
	int32 GetTextureLODGroupLODBias( int32 InLODGroup ) const;

	/**
	 * Returns the LODGroup setting for number of streaming mip-levels.
	 * -1 means that all mip-levels are allowed to stream.
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	Number of streaming mip-levels for textures in the specified LODGroup
	 */
	int32 GetNumStreamedMips( int32 InLODGroup ) const;

	int32 GetMinLODMipCount( int32 InLODGroup ) const;
	int32 GetMaxLODMipCount( int32 InLODGroup ) const;

	/**
	 * Returns the filter state that should be used for the passed in texture, taking
	 * into account other system settings.
	 *
	 * @param	Texture		Texture to retrieve filter state for, must not be 0
	 * @return	Filter sampler state for passed in texture
	 */
	ETextureSamplerFilter GetSamplerFilter(const UTexture* Texture) const;

	ETextureSamplerFilter GetSamplerFilter(int32 InLODGroup) const;

	/**
	 * Returns the LODGroup mip gen settings
	 *
	 * @param	InLODGroup		The LOD Group ID 
	 * @return	TextureMipGenSettings for lod group
	 */
	const TextureMipGenSettings GetTextureMipGenSettings( int32 InLODGroup ) const; 


	/**
	 * Returns the texture group names, sorted like enum.
	 *
	 * @return array of texture group names
	 */
	static TArray<FString> GetTextureGroupNames();

	/**
	 * TextureLODGroups access with bounds check
	 *
	 * @param   GroupIndex      usually from Texture.LODGroup
	 * @return                  A handle to the indexed LOD group. 
	 */
	FTextureLODGroup& GetTextureLODGroup(TextureGroup GroupIndex);

	/**
	* TextureLODGroups access with bounds check
	*
	* @param   GroupIndex      usually from Texture.LODGroup
	* @return                  A handle to the indexed LOD group.
	*/
	const FTextureLODGroup& GetTextureLODGroup(TextureGroup GroupIndex) const;

protected:
	void SetupLODGroup(int32 GroupId);

public:

	/** Array of LOD settings with entries per group. */
	UPROPERTY(EditAnywhere, config, Category="Texture LOD Settings")
	TArray<FTextureLODGroup> TextureLODGroups;
};
