// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#define TEXTURE_COMPRESSOR_MODULENAME "TextureCompressor"

/**
 * Compressed image data.
 */
struct FCompressedImage2D
{
	TArray<uint8> RawData;
	int32 SizeX;
	int32 SizeY;
	uint8 PixelFormat; // EPixelFormat, opaque to avoid dependencies on Engine headers.
};

/**
 * Color adjustment parameters.
 */
struct FColorAdjustmentParameters 
{
	/** Brightness adjustment (scales HSV value) */
	float AdjustBrightness;

	/** Curve adjustment (raises HSV value to the specified power) */
	float AdjustBrightnessCurve;

	/** Saturation adjustment (scales HSV saturation) */
	float AdjustSaturation;

	/** "Vibrance" adjustment (HSV saturation algorithm adjustment) */
	float AdjustVibrance;

	/** RGB curve adjustment (raises linear-space RGB color to the specified power) */
	float AdjustRGBCurve;

	/** Hue adjustment (offsets HSV hue by value in degrees) */
	float AdjustHue;

	/** Remaps the alpha to the specified min/max range  (Non-destructive; Requires texture source art to be available.) */
	float AdjustMinAlpha;

	/** Remaps the alpha to the specified min/max range  (Non-destructive; Requires texture source art to be available.) */
	float AdjustMaxAlpha;

	/** Constructor */
	FColorAdjustmentParameters()
		: AdjustBrightness( 1.0f ),
		  AdjustBrightnessCurve( 1.0f ),
		  AdjustSaturation( 1.0f ),
		  AdjustVibrance( 0.0f ),
		  AdjustRGBCurve( 1.0f ),
		  AdjustHue( 0.0f ),
		  AdjustMinAlpha( 0.0f ),
		  AdjustMaxAlpha( 1.0f )
	{
	}
};

/**
 * Texture build settings.
 */
struct FTextureBuildSettings
{
	/** Color adjustment parameters. */
	FColorAdjustmentParameters ColorAdjustment;
	/** Channel values to compare to when preserving alpha coverage. */
	FVector4 AlphaCoverageThresholds;
	/** The desired amount of mip sharpening. */
	float MipSharpening;
	/** For angular filtered cubemaps, the mip level which contains convolution with the diffuse cosine lobe. */
	uint32 DiffuseConvolveMipLevel;
	/** The size of the kernel with which mips should be sharpened. 2 for 2x2, 4 for 4x4, 6 for 6x6, 8 for 8x8 */
	uint32 SharpenMipKernelSize;
	/** For maximum resolution. */
	uint32 MaxTextureResolution;
	/** Format of the compressed texture, used to choose a compression DLL. */
	FName TextureFormatName;
	/** Mipmap generation settings. */
	uint8 MipGenSettings; // TextureMipGenSettings, opaque to avoid dependencies on engine headers.
	/** Whether the texture being built is a cubemap. */
	uint32 bCubemap : 1;
	/** Whether the texture being built from long/lat source to cubemap. */
	uint32 bLongLatSource : 1;
	/** Whether the texture contains color data in the sRGB colorspace. */
	uint32 bSRGB : 1;
	/** Whether the texture should use the legacy gamma space for converting to sRGB */
	uint32 bUseLegacyGamma : 1;
	/** Whether the border of the image should be maintained during mipmap generation. */
	uint32 bPreserveBorder : 1;
	/** Whether the alpha channel should contain a dithered alpha value. */
	uint32 bDitherMipMapAlpha : 1;
	/** Whether bokeh alpha values should be computed for the texture. */
	uint32 bComputeBokehAlpha : 1;
	/** Whether the contents of the red channel should be replicated to all channels. */
	uint32 bReplicateRed : 1;
	/** Whether the contents of the alpha channel should be replicated to all channels. */
	uint32 bReplicateAlpha : 1;
	/** Whether each mip should use the downsampled-with-average result instead of the sharpened result. */
	uint32 bDownsampleWithAverage : 1;
	/** Whether sharpening should prevent color shifts. */
	uint32 bSharpenWithoutColorShift : 1;
	/** Whether the border color should be black. */
	uint32 bBorderColorBlack : 1;
	/** Whether the green channel should be flipped. Typically only done on normal maps. */
	uint32 bFlipGreenChannel : 1;
	/** 1:apply mip sharpening/blurring kernel to top mip as well (at half the kernel size), 0:don't */
	uint32 bApplyKernelToTopMip : 1;
	/** 1: renormalizes the top mip (only useful for normal maps, prevents artists errors and adds quality) 0:don't */
	uint32 bRenormalizeTopMip : 1;
	/** e.g. CTM_RoughnessFromNormalAlpha */
	uint8 CompositeTextureMode;	// ECompositeTextureMode, opaque to avoid dependencies on engine headers.
	/* default 1, high values result in a stronger effect */
	float CompositePower;
	/** The source texture's final LOD bias (i.e. includes LODGroup based biases) */
	uint32 LODBias;
	/** The texture's top mip size without LODBias applied, should be moved into a separate struct together with bImageHasAlphaChannel */
	mutable FIntPoint TopMipSize;
	/** Can the texture be streamed */
	uint32 bStreamable : 1;
	/** Whether to chroma key the image, replacing any pixels that match ChromaKeyColor with transparent black */
	uint32 bChromaKeyTexture : 1;
	/** How to stretch or pad the texture to a power of 2 size (if necessary); ETexturePowerOfTwoSetting::Type, opaque to avoid dependencies on Engine headers. */
	uint8 PowerOfTwoMode;
	/** The color used to pad the texture out if it is resized due to PowerOfTwoMode */
	FColor PaddingColor;
	/** The color that will be replaced with transparent black if chroma keying is enabled */
	FColor ChromaKeyColor;
	/** The threshold that components have to match for the texel to be considered equal to the ChromaKeyColor when chroma keying (<=, set to 0 to require a perfect exact match) */
	float ChromaKeyThreshold;
	/** The quality of the compression algorithm (min 0 - lowest quality, highest cook speed, 4 - highest quality, lowest cook speed)*/
	int32 CompressionQuality;

	/** Default settings. */
	FTextureBuildSettings()
		: AlphaCoverageThresholds(0, 0, 0, 0)
		, MipSharpening(0.0f)
		, DiffuseConvolveMipLevel(0)
		, SharpenMipKernelSize(2)
		, MaxTextureResolution(TNumericLimits<uint32>::Max())
		, MipGenSettings(1 /*TMGS_SimpleAverage*/)
		, bCubemap(false)
		, bLongLatSource(false)
		, bSRGB(false)
		, bUseLegacyGamma(false)
		, bPreserveBorder(false)
		, bDitherMipMapAlpha(false)
		, bComputeBokehAlpha(false)
		, bReplicateRed(false)
		, bReplicateAlpha(false)
		, bDownsampleWithAverage(false)
		, bSharpenWithoutColorShift(false)
		, bBorderColorBlack(false)
		, bFlipGreenChannel(false)
		, bApplyKernelToTopMip(false)
		, bRenormalizeTopMip(false)
		, CompositeTextureMode(0 /*CTM_Disabled*/)
		, CompositePower(1.0f)
		, LODBias(0)
		, TopMipSize(0, 0)
		, bStreamable(false)
		, bChromaKeyTexture(false)
		, PowerOfTwoMode(0 /*ETexturePowerOfTwoSetting::None*/)
		, PaddingColor(FColor::Black)
		, ChromaKeyColor(FColorList::Magenta)
		, ChromaKeyThreshold(1.0f / 255.0f)
		, CompressionQuality(-1)
	{
	}

	FORCEINLINE EGammaSpace GetGammaSpace() const
	{
		return bSRGB ? ( bUseLegacyGamma ? EGammaSpace::Pow22 : EGammaSpace::sRGB ) : EGammaSpace::Linear;
	}
};

/**
 * Texture compression module interface.
 */
class ITextureCompressorModule : public IModuleInterface
{
public:

	/**
	 * Builds a texture from source images.
	 * @param SourceMips - The input mips.
	 * @param BuildSettings - Build settings.
	 * @param OutCompressedMips - The compressed mips built by the compressor.
	 * @returns true on success
	 */
	virtual bool BuildTexture(
		const TArray<struct FImage>& SourceMips,
		const TArray<struct FImage>& AssociatedNormalSourceMips,
		const FTextureBuildSettings& BuildSettings,
		TArray<FCompressedImage2D>& OutTextureMips
		) = 0;
};
