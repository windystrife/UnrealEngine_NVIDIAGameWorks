// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/EnumAsByte.h" 
#include "Engine/MaterialMerging.h"
#include "SimplygonSDK.h"

#include "SimplygonTypes.generated.h"

//@third party code BEGIN SIMPLYGON
#define USE_USER_OPACITY_CHANNEL 1
#if USE_USER_OPACITY_CHANNEL
static const char* USER_MATERIAL_CHANNEL_OPACITY = "UserOpacity";
static const char* USER_MATERIAL_CHANNEL_OPACITY_MASK = "UserOpacityMask";
#endif
//@third party code END SIMPLYGON

// User defined material channel used for baking out sub surface colours
static const char* USER_MATERIAL_CHANNEL_SUBSURFACE_COLOR = "UserSubSurfaceColor";

UENUM()
namespace ESimplygonMaterialChannel
{
	enum Type
	{
		SG_MATERIAL_CHANNEL_AMBIENT UMETA(DisplayName = "Ambient", DisplayValue = "Ambient"),
		SG_MATERIAL_CHANNEL_DIFFUSE UMETA(DisplayName = "Diffuse", DisplayValue = "Diffuse"),
		SG_MATERIAL_CHANNEL_SPECULAR UMETA(DisplayName = "Specular", DisplayValue = "Specular"),
		SG_MATERIAL_CHANNEL_OPACITY UMETA(DisplayName = "Opacity", DisplayValue = "Opacity"),
		SG_MATERIAL_CHANNEL_OPACITYMASK UMETA(DisplayName = "OpacityMask", DisplayValue = "OpacityMask"),
		SG_MATERIAL_CHANNEL_EMISSIVE UMETA(DisplayName = "Emissive", DisplayValue = "Emissive"),
		SG_MATERIAL_CHANNEL_NORMALS UMETA(DisplayName = "Normals", DisplayValue = "Normals"),
		SG_MATERIAL_CHANNEL_DISPLACEMENT UMETA(DisplayName = "Displacement", DisplayValue = "Displacement"),
		SG_MATERIAL_CHANNEL_BASECOLOR UMETA(DisplayName = "Basecolor", DisplayValue = "Basecolor"),
		SG_MATERIAL_CHANNEL_ROUGHNESS UMETA(DisplayName = "Roughness", DisplayValue = "Roughness"),
		SG_MATERIAL_CHANNEL_METALLIC UMETA(DisplayName = "Metallic", DisplayValue = "Metallic"),
		SG_MATERIAL_CHANNEL_SUBSURFACE UMETA(DisplayName = "SubsurfaceColor", DisplayValue = "SubsurfaceColor")
	};

}

static const char* GetSimplygonMaterialChannel(ESimplygonMaterialChannel::Type channel)
{
	if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_BASECOLOR)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_BASECOLOR;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_SPECULAR)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_SPECULAR;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_ROUGHNESS)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_ROUGHNESS;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_METALLIC)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_METALNESS;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_NORMALS)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_NORMALS;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_OPACITY)
#if USE_USER_OPACITY_CHANNEL
		return USER_MATERIAL_CHANNEL_OPACITY;
#else
		return SimplygonSDK::SG_MATERIAL_CHANNEL_OPACITY;
#endif
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_OPACITYMASK)
#if USE_USER_OPACITY_CHANNEL
		return USER_MATERIAL_CHANNEL_OPACITY_MASK;
#else
		return SimplygonSDK::SG_MATERIAL_CHANNEL_OPACITY;
#endif
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_EMISSIVE)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_EMISSIVE;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_SUBSURFACE)
		return USER_MATERIAL_CHANNEL_SUBSURFACE_COLOR;
	else if (channel == ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_AMBIENT)
		return SimplygonSDK::SG_MATERIAL_CHANNEL_AMBIENT;
	else
	{
		check(0);
		return 0;
	}

}

UENUM()
namespace ESimplygonLODType
{
	enum Type
	{
		Reduction,
		Remeshing
	};
}

/*
*	Material LOD Type
*/
UENUM()
namespace EMaterialLODType
{
	enum Type
	{
		Off,				//no material lod
		BakeTexture,		//combine materials and cast new textures
		BakeVertexColor,	//combine materials and cast textures into vertex color field instead of baking new textures
		Replace
	};
}

/*
*	Texture Stretch
*/
UENUM()
namespace ESimplygonTextureStrech
{
	enum Type
	{
		None,
		VerySmall,
		Small,
		Medium,
		Large,
		VeryLarge
	};
}

/*
*	Type of caster to use Stretch
*/
UENUM()
namespace ESimplygonCasterType
{
	enum Type
	{
		None,
		Color,				//use Color caster
		Normals,			//use Normals caster
		Opacity,			//use Opacity caster
	};
}

/*
*	Texture Sampling Quality
*/
UENUM()
namespace ESimplygonTextureSamplingQuality
{
	enum Type
	{
		Poor,
		Low,
		Medium,
		High
	};
}

UENUM()
namespace ESimplygonColorChannels
{
	enum Type
	{
		RGBA,
		RGB,
		L
	};
}


UENUM()
namespace ESimplygonTextureResolution
{
	enum Type
	{
		TextureResolution_64 UMETA(DisplayName = "64"),
		TextureResolution_128 UMETA(DisplayName = "128"),
		TextureResolution_256 UMETA(DisplayName = "256"),
		TextureResolution_512 UMETA(DisplayName = "512"),
		TextureResolution_1024 UMETA(DisplayName = "1024"),
		TextureResolution_2048 UMETA(DisplayName = "2048"),
		TextureResolution_4096 UMETA(DisplayName = "4096"),
		TextureResolution_8192 UMETA(DisplayName = "8192")
	};
}

/*
* Desc : The following class stores settings for the simplygon caster.
*/
USTRUCT()
struct  FSimplygonChannelCastingSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = SimplygonChannelCasterSettings)
	TEnumAsByte<ESimplygonMaterialChannel::Type> MaterialChannel;

	UPROPERTY(EditAnywhere, Category = SimplygonChannelCasterSettings)
	TEnumAsByte<ESimplygonCasterType::Type> Caster;

	UPROPERTY(EditAnywhere, Category = SimplygonChannelCasterSettings)
	bool bActive;

	UPROPERTY(EditAnywhere, Category = SimplygonChannelCasterSettings)
	TEnumAsByte<ESimplygonColorChannels::Type> ColorChannels;

	UPROPERTY(EditAnywhere, Category = SimplygonChannelCasterSettings)
	int32 BitsPerChannel;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bUseSRGB;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bBakeVertexColors;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bFlipBackfacingNormals;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bUseTangentSpaceNormals;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bFlipGreenChannel;

	FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::Type channel, ESimplygonCasterType::Type caster, ESimplygonColorChannels::Type colorChannels)
		: MaterialChannel(channel)
		, Caster(caster)
		, bActive(false)
		, ColorChannels(colorChannels)
		, BitsPerChannel(8)
		, bUseSRGB(true)
		, bBakeVertexColors(false)
		, bFlipBackfacingNormals(false)
		, bUseTangentSpaceNormals(true)
		, bFlipGreenChannel(false)
	{
	}

	FSimplygonChannelCastingSettings()
		: MaterialChannel(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_BASECOLOR)
		, Caster(ESimplygonCasterType::Color)
		, bActive(false)
		, ColorChannels(ESimplygonColorChannels::RGB)
		, BitsPerChannel(8)
		, bUseSRGB(true)
		, bBakeVertexColors(false)
		, bFlipBackfacingNormals(false)
		, bUseTangentSpaceNormals(true)
		, bFlipGreenChannel(false)
	{
	}

	FSimplygonChannelCastingSettings(const FSimplygonChannelCastingSettings& Other)
		: MaterialChannel(Other.MaterialChannel)
		, Caster(Other.Caster)
		, bActive(Other.bActive)
		, ColorChannels(Other.ColorChannels)
		, BitsPerChannel(Other.BitsPerChannel)
		, bUseSRGB(Other.bUseSRGB)
		, bBakeVertexColors(Other.bBakeVertexColors)
		, bFlipBackfacingNormals(Other.bFlipBackfacingNormals)
		, bUseTangentSpaceNormals(Other.bUseTangentSpaceNormals)
		, bFlipGreenChannel(Other.bFlipGreenChannel)
	{
	}

	bool operator==(const FSimplygonChannelCastingSettings& Other) const
	{
		if (bActive == false && Other.bActive == false)
		{
			// Ignore other fields when both objects are inactive
			return true;
		}

		return MaterialChannel == Other.MaterialChannel
			&& Caster == Other.Caster
			&& bActive == Other.bActive
			&& ColorChannels == Other.ColorChannels
			&& BitsPerChannel == Other.BitsPerChannel
			&& bUseSRGB == Other.bUseSRGB
			&& bBakeVertexColors == Other.bBakeVertexColors
			&& bFlipBackfacingNormals == Other.bFlipBackfacingNormals
			&& bUseTangentSpaceNormals == Other.bUseTangentSpaceNormals
			&& bFlipGreenChannel == Other.bFlipGreenChannel;
	}

	bool operator!=(const FSimplygonChannelCastingSettings& Other) const
	{
		return !(*this == Other);
	}
};


static const ESimplygonTextureResolution::Type GetResolutionEnum(const int32 InSize)
{
	switch (InSize)
	{
		case 64:
		{
			return ESimplygonTextureResolution::TextureResolution_64;
		}

		case 128:
		{
			return ESimplygonTextureResolution::TextureResolution_128;
		}

		case 256:
		{
			return ESimplygonTextureResolution::TextureResolution_256;
		}

		case 512:
		{
			return ESimplygonTextureResolution::TextureResolution_512;

		}
		case 1024:
		{
			return ESimplygonTextureResolution::TextureResolution_1024;
		}
		case 2048:
		{
			return ESimplygonTextureResolution::TextureResolution_2048;
		}
		case 4096:
		{
			return ESimplygonTextureResolution::TextureResolution_4096;
		}
		case 8192:
		{
			return ESimplygonTextureResolution::TextureResolution_8192;
		}

		default:
		{
			check(false);
			return ESimplygonTextureResolution::TextureResolution_64;
		}
	}

	return ESimplygonTextureResolution::TextureResolution_64;
}

/*
* Desc : The following class stores settings for the simplygon material LOD. Specifically the mapping image
*/
USTRUCT()
struct FSimplygonMaterialLODSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bActive;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	TEnumAsByte<EMaterialLODType::Type> MaterialLODType;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bUseAutomaticSizes;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	TEnumAsByte<ESimplygonTextureResolution::Type> TextureWidth;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	TEnumAsByte<ESimplygonTextureResolution::Type> TextureHeight;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	TEnumAsByte<ESimplygonTextureSamplingQuality::Type> SamplingQuality;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	int32 GutterSpace;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	TEnumAsByte<ESimplygonTextureStrech::Type> TextureStrech;

	UPROPERTY(EditAnywhere, Category = SimplygonMaterialLODSettings)
	bool bReuseExistingCharts;

	UPROPERTY()
	TArray<struct FSimplygonChannelCastingSettings> ChannelsToCast;
	
	FSimplygonMaterialLODSettings()
		: bActive(false)
		, MaterialLODType(EMaterialLODType::BakeTexture)
		, bUseAutomaticSizes(false)
		, TextureWidth(ESimplygonTextureResolution::Type::TextureResolution_512)
		, TextureHeight(ESimplygonTextureResolution::Type::TextureResolution_512)
		, SamplingQuality(ESimplygonTextureSamplingQuality::Low)
		, GutterSpace(4)
		, TextureStrech(ESimplygonTextureStrech::Medium)
		, bReuseExistingCharts(false)
	{
		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_BASECOLOR, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_SPECULAR, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_ROUGHNESS, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_METALLIC, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_NORMALS, ESimplygonCasterType::Normals, ESimplygonColorChannels::RGB));
	}

	FSimplygonMaterialLODSettings(const FSimplygonMaterialLODSettings& Other)
		: bActive(Other.bActive)
		, MaterialLODType(Other.MaterialLODType)
		, bUseAutomaticSizes(Other.bUseAutomaticSizes)
		, TextureWidth(Other.TextureWidth)
		, TextureHeight(Other.TextureHeight)
		, SamplingQuality(Other.SamplingQuality)
		, GutterSpace(Other.GutterSpace)
		, TextureStrech(Other.TextureStrech)
		, bReuseExistingCharts(Other.bReuseExistingCharts)
	{
		ChannelsToCast.Empty();
		for (int ItemIndex = 0; ItemIndex < Other.ChannelsToCast.Num(); ItemIndex++)
		{
			ChannelsToCast.Add(Other.ChannelsToCast[ItemIndex]);

		}
	}



	FSimplygonMaterialLODSettings(const FMaterialProxySettings& Settings)
		: bActive(true)
		, MaterialLODType(EMaterialLODType::BakeTexture)
		, bUseAutomaticSizes( Settings.TextureSizingType == TextureSizingType_UseSimplygonAutomaticSizing )
		, TextureWidth(GetResolutionEnum(Settings.TextureSize.X))
		, TextureHeight(GetResolutionEnum(Settings.TextureSize.Y))
		, SamplingQuality(ESimplygonTextureSamplingQuality::High)
		, GutterSpace( Settings.GutterSpace )
		, TextureStrech(ESimplygonTextureStrech::Medium)
		, bReuseExistingCharts(false)
	{
		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_BASECOLOR, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = true;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_SPECULAR, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bSpecularMap;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_ROUGHNESS, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bRoughnessMap;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_METALLIC, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bMetallicMap;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_NORMALS, ESimplygonCasterType::Normals, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bNormalMap;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_EMISSIVE, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bEmissiveMap;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_OPACITY, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bOpacityMap;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_OPACITYMASK, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bOpacityMaskMap;

		// TODO, properly render out sub surface data/values
		//ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_SUBSURFACE, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		//ChannelsToCast.Last().bUseSRGB = false;
		//ChannelsToCast.Last().bActive = true;

		ChannelsToCast.Add(FSimplygonChannelCastingSettings(ESimplygonMaterialChannel::SG_MATERIAL_CHANNEL_AMBIENT, ESimplygonCasterType::Color, ESimplygonColorChannels::RGB));
		ChannelsToCast.Last().bUseSRGB = false;
		ChannelsToCast.Last().bActive = Settings.bAmbientOcclusionMap;
	}

	static int32 GetTextureResolutionFromEnum(ESimplygonTextureResolution::Type InResolution)
	{
		switch (InResolution)
		{
		case ESimplygonTextureResolution::TextureResolution_64:
			return 64;
		case ESimplygonTextureResolution::TextureResolution_128:
			return 128;
		case ESimplygonTextureResolution::TextureResolution_256:
			return 256;
		case ESimplygonTextureResolution::TextureResolution_512:
			return 512;
		case ESimplygonTextureResolution::TextureResolution_1024:
			return 1024;
		case ESimplygonTextureResolution::TextureResolution_2048:
			return 2048;
		case ESimplygonTextureResolution::TextureResolution_4096:
			return 4096;
		case ESimplygonTextureResolution::TextureResolution_8192:
			return 8192;

		}
		return 64;
	}

	bool operator==(const FSimplygonMaterialLODSettings& Other) const
	{
		if (bActive == false && Other.bActive == false)
		{
			// Ignore other fields when both objects are inactive
			return true;
		}

		bool areAllElementsEqual = ChannelsToCast.Num() == Other.ChannelsToCast.Num() ? true : false;

		if (areAllElementsEqual)
		{
			for (int ItemIndex = 0; ItemIndex < ChannelsToCast.Num(); ItemIndex++)
			{
				if (ChannelsToCast[ItemIndex] != Other.ChannelsToCast[ItemIndex])
				{
					areAllElementsEqual = false;
					break;
				}
				if (!areAllElementsEqual)
					break;
			}
		}

		return  bActive == Other.bActive
			&& MaterialLODType == Other.MaterialLODType
			&& bUseAutomaticSizes == Other.bUseAutomaticSizes
			&& TextureWidth == Other.TextureWidth
			&& TextureHeight == Other.TextureHeight
			&& SamplingQuality == Other.SamplingQuality
			&& GutterSpace == Other.GutterSpace
			&& TextureStrech == Other.TextureStrech
			&& bReuseExistingCharts == Other.bReuseExistingCharts
			&& areAllElementsEqual == true;
	}

	bool operator!=(const FSimplygonMaterialLODSettings& Other) const
	{
		return !(*this == Other);
	}
};