// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// Scene - script exposed scene enums
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptInterface.h"
#include "Engine/BlendableInterface.h"
#include "Scene.generated.h"

// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
#include "GFSDK_VXGI.h"
#endif
// NVCHANGE_END: Add VXGI

struct FPostProcessSettings;


/** Used by FPostProcessSettings Depth of Fields */
UENUM()
enum EDepthOfFieldMethod
{
	DOFM_BokehDOF UMETA(DisplayName="BokehDOF"),
	DOFM_Gaussian UMETA(DisplayName="GaussianDOF"),
	DOFM_CircleDOF UMETA(DisplayName="CircleDOF"),
	DOFM_MAX,
};

/** Used by rendering project settings. */
UENUM()
enum EAntiAliasingMethod
{
	AAM_None UMETA(DisplayName="None"),
	AAM_FXAA UMETA(DisplayName="FXAA"),
	AAM_TemporalAA UMETA(DisplayName="TemporalAA"),
	/** Only supported with forward shading.  MSAA sample count is controlled by r.MSAACount. */
	AAM_MSAA UMETA(DisplayName="MSAA"),
	AAM_MAX,
};

/** Used by FPostProcessSettings Auto Exposure */
UENUM()
enum EAutoExposureMethod
{
	/** Not supported on mobile, requires compute shader to construct 64 bin histogram */
	AEM_Histogram  UMETA(DisplayName = "Auto Exposure Histogram"),
	/** Not supported on mobile, faster method that computes single value by downsampling */
	AEM_Basic      UMETA(DisplayName = "Auto Exposure Basic"),
	AEM_MAX,
};

UENUM()
enum EBloomMethod
{
	/** Sum of Gaussian formulation */
	BM_SOG  UMETA(DisplayName = "Standard"),
	/** Fast Fourier Transform Image based convolution, intended for cinematics (too expensive for games)  */
	BM_FFT  UMETA(DisplayName = "Convolution"),
	BM_MAX,
};

USTRUCT(BlueprintType)
struct FColorGradePerRangeSettings
{
	GENERATED_USTRUCT_BODY()


	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "saturation", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", DisplayName = "Saturation"))
	FVector4 Saturation;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "contrast", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", DisplayName = "Contrast"))
	FVector4 Contrast;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gamma", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", DisplayName = "Gamma"))
	FVector4 Gamma;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gain", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", DisplayName = "Gain"))
	FVector4 Gain;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "-1.0", UIMax = "1.0", Delta = "0.001", ColorGradingMode = "offset", ShiftMouseMovePixelPerDelta = "20", SupportDynamicSliderMaxValue = "true", SupportDynamicSliderMinValue = "true", DisplayName = "Offset"))
	FVector4 Offset;


	FColorGradePerRangeSettings()
	{
		Saturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Contrast = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Gamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Gain = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		Offset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
};

USTRUCT(BlueprintType)
struct FColorGradingSettings
{
	GENERATED_USTRUCT_BODY()


	UPROPERTY(Interp,BlueprintReadWrite, Category = "Color Grading")
	FColorGradePerRangeSettings Global;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading")
	FColorGradePerRangeSettings Shadows;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading")
	FColorGradePerRangeSettings Midtones;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading")
	FColorGradePerRangeSettings Highlights;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "-1.0", UIMax = "1.0", DisplayName = "ShadowsMax"))
	float ShadowsMax;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Color Grading", meta = (UIMin = "-1.0", UIMax = "1.0", DisplayName = "HighlightsMin"))
	float HighlightsMin;


	FColorGradingSettings()
	{
		ShadowsMax = 0.09f;
		HighlightsMin = 0.5f;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FFilmStockSettings
{
	GENERATED_USTRUCT_BODY()


	UPROPERTY(Interp, BlueprintReadWrite, Category = "Film Stock", meta = (UIMin = "0.0", UIMax = "1.0", DisplayName = "Slope"))
	float Slope;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Film Stock", meta = (UIMin = "0.0", UIMax = "1.0", DisplayName = "Toe"))
	float Toe;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Film Stock", meta = (UIMin = "0.0", UIMax = "1.0", DisplayName = "Shoulder"))
	float Shoulder;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Film Stock", meta = (UIMin = "0.0", UIMax = "1.0", DisplayName = "Black clip"))
	float BlackClip;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Film Stock", meta = (UIMin = "0.0", UIMax = "1.0", DisplayName = "White clip"))
	float WhiteClip;


	FFilmStockSettings()
	{
		Slope = 0.88f;
		Toe = 0.55f;
		Shoulder = 0.26f;
		BlackClip = 0.0f;
		WhiteClip = 0.04f;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FGaussianSumBloomSettings
{
	GENERATED_USTRUCT_BODY()


	/** Multiplier for all bloom contributions >=0: off, 1(default), >1 brighter */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", meta=(ClampMin = "0.0", UIMax = "8.0", DisplayName = "Intensity"))
	float Intensity;

	/**
	 * minimum brightness the bloom starts having effect
	 * -1:all pixels affect bloom equally (physically correct, faster as a threshold pass is omitted), 0:all pixels affect bloom brights more, 1(default), >1 brighter
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", meta=(ClampMin = "-1.0", UIMax = "8.0", DisplayName = "Threshold"))
	float Threshold;

	/**
	 * Scale for all bloom sizes
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", DisplayName = "Size scale"))
	float SizeScale;

	/**
	 * Diameter size for the Bloom1 in percent of the screen width
	 * (is done in 1/2 resolution, larger values cost more performance, good for high frequency details)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "4.0", DisplayName = "#1 Size"))
	float Filter1Size;

	/**
	 * Diameter size for Bloom2 in percent of the screen width
	 * (is done in 1/4 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "8.0", DisplayName = "#2 Size"))
	float Filter2Size;

	/**
	 * Diameter size for Bloom3 in percent of the screen width
	 * (is done in 1/8 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "16.0", DisplayName = "#3 Size"))
	float Filter3Size;

	/**
	 * Diameter size for Bloom4 in percent of the screen width
	 * (is done in 1/16 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "32.0", DisplayName = "#4 Size"))
	float Filter4Size;

	/**
	 * Diameter size for Bloom5 in percent of the screen width
	 * (is done in 1/32 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", DisplayName = "#5 Size"))
	float Filter5Size;

	/**
	 * Diameter size for Bloom6 in percent of the screen width
	 * (is done in 1/64 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "128.0", DisplayName = "#6 Size"))
	float Filter6Size;

	/** Bloom1 tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(DisplayName = "#1 Tint", HideAlphaChannel))
	FLinearColor Filter1Tint;

	/** Bloom2 tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(DisplayName = "#2 Tint", HideAlphaChannel))
	FLinearColor Filter2Tint;

	/** Bloom3 tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(DisplayName = "#3 Tint", HideAlphaChannel))
	FLinearColor Filter3Tint;

	/** Bloom4 tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(DisplayName = "#4 Tint", HideAlphaChannel))
	FLinearColor Filter4Tint;

	/** Bloom5 tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(DisplayName = "#5 Tint", HideAlphaChannel))
	FLinearColor Filter5Tint;

	/** Bloom6 tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(DisplayName = "#6 Tint", HideAlphaChannel))
	FLinearColor Filter6Tint;


	FGaussianSumBloomSettings()
	{
		Intensity = 0.675f;
		Threshold = -1.0f;
		// default is 4 to maintain old settings after fixing something that caused a factor of 4
		SizeScale = 4.0;
		Filter1Tint = FLinearColor(0.3465f, 0.3465f, 0.3465f);
		Filter1Size = 0.3f;
		Filter2Tint = FLinearColor(0.138f, 0.138f, 0.138f);
		Filter2Size = 1.0f;
		Filter3Tint = FLinearColor(0.1176f, 0.1176f, 0.1176f);
		Filter3Size = 2.0f;
		Filter4Tint = FLinearColor(0.066f, 0.066f, 0.066f);
		Filter4Size = 10.0f;
		Filter5Tint = FLinearColor(0.066f, 0.066f, 0.066f);
		Filter5Size = 30.0f;
		Filter6Tint = FLinearColor(0.061f, 0.061f, 0.061f);
		Filter6Size = 64.0f;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FConvolutionBloomSettings
{
	GENERATED_USTRUCT_BODY()


	/** Texture to replace default convolution bloom kernel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Bloom", meta = (DisplayName = "Convolution Kernel"))
	class UTexture2D* Texture;

	/** Relative size of the convolution kernel image compared to the minor axis of the viewport  */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (ClampMin = "0.0", UIMax = "1.0", DisplayName = "Convolution Scale"))
	float Size;

	/** The UV location of the center of the kernel.  Should be very close to (.5,.5) */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (DisplayName = "Convolution Center"))
	FVector2D CenterUV;

	/** Boost intensity of select pixels  prior to computing bloom convolution (Min, Max, Multiplier).  Max < Min disables */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (DisplayName = "Convolution Boost Min"))
	float PreFilterMin;

	/** Boost intensity of select pixels  prior to computing bloom convolution (Min, Max, Multiplier).  Max < Min disables */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (DisplayName = "Convolution Boost Max"))
	float PreFilterMax;

	/** Boost intensity of select pixels  prior to computing bloom convolution (Min, Max, Multiplier).  Max < Min disables */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (DisplayName = "Convolution Boost Mult"))
	float PreFilterMult;

	/** Implicit buffer region as a fraction of the screen size to insure the bloom does not wrap across the screen.  Larger sizes have perf impact.*/
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (ClampMin = "0.0", UIMax = "1.0", DisplayName = "Convolution Buffer"))
	float BufferScale;


	FConvolutionBloomSettings()
	{
		Texture = nullptr;
		Size = 1.f;
		CenterUV = FVector2D(0.5f, 0.5f);
		PreFilterMin = 7.f;
		PreFilterMax = 15000.f;
		PreFilterMult = 15.f;
		BufferScale = 0.133f;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FLensBloomSettings
{
	GENERATED_USTRUCT_BODY()

	/** Bloom gaussian sum method specific settings. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Gaussian Sum Method")
	FGaussianSumBloomSettings GaussianSum;

	/** Bloom convolution method specific settings. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Convolution Method")
	FConvolutionBloomSettings Convolution;

	/** Bloom algorithm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Bloom")
	TEnumAsByte<enum EBloomMethod> Method;


	FLensBloomSettings()
	{
		Method = BM_SOG;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FLensImperfectionSettings
{
	GENERATED_USTRUCT_BODY()
	
	/**
	 * Texture that defines the dirt on the camera lens where the light of very bright objects is scattered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens|Dirt Mask", meta=(DisplayName = "Dirt Mask Texture"))
	class UTexture* DirtMask;	
	
	/** BloomDirtMask intensity */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Dirt Mask", meta=(ClampMin = "0.0", UIMax = "8.0", DisplayName = "Dirt Mask Intensity"))
	float DirtMaskIntensity;

	/** BloomDirtMask tint color */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Lens|Dirt Mask", meta=(DisplayName = "Dirt Mask Tint", HideAlphaChannel))
	FLinearColor DirtMaskTint;


	FLensImperfectionSettings()
	{
		DirtMask = nullptr;
		DirtMaskIntensity = 0.0f;
		DirtMaskTint = FLinearColor(0.5f, 0.5f, 0.5f);
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FLensSettings
{
	GENERATED_USTRUCT_BODY()


	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens")
	FLensBloomSettings Bloom;

	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens")
	FLensImperfectionSettings Imperfections;

	/** in percent, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Lens", meta = (UIMin = "0.0", UIMax = "5.0"))
	float ChromaticAberration;


	FLensSettings()
	{
		ChromaticAberration = 0.0f;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FCameraExposureSettings
{
	GENERATED_USTRUCT_BODY()


	/** Luminance computation method */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exposure", meta=(DisplayName = "Method"))
    TEnumAsByte<enum EAutoExposureMethod> Method;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 70 .. 80
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", DisplayName = "Low Percent"))
	float LowPercent;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 80 .. 95
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", DisplayName = "High Percent"))
	float HighPercent;

	/**
	 * A good value should be positive near 0. This is the minimum brightness the auto exposure can adapt to.
	 * It should be tweaked in a dark lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", meta=(ClampMin = "0.0", UIMax = "10.0", DisplayName = "Min Brightness"))
	float MinBrightness;

	/**
	 * A good value should be positive (2 is a good value). This is the maximum brightness the auto exposure can adapt to.
	 * It should be tweaked in a bright lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", meta=(ClampMin = "0.0", UIMax = "10.0", DisplayName = "Max Brightness"))
	float MaxBrightness;

	/** >0 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", meta=(ClampMin = "0.02", UIMax = "20.0", DisplayName = "Speed Up"))
	float SpeedUp;

	/** >0 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", meta=(ClampMin = "0.02", UIMax = "20.0", DisplayName = "Speed Down"))
	float SpeedDown;

	/**
	 * Logarithmic adjustment for the exposure. Only used if a tonemapper is specified.
	 * 0: no adjustment, -1:2x darker, -2:4x darker, 1:2x brighter, 2:4x brighter, ...
	 */
	UPROPERTY(Interp, BlueprintReadWrite, Category = "Exposure", meta = (UIMin = "-8.0", UIMax = "8.0", DisplayName = "Exposure Bias"))
	float Bias;

	/** temporary exposed until we found good values, -8: 1/256, -10: 1/1024 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", AdvancedDisplay, meta=(UIMin = "-16", UIMax = "0.0"))
	float HistogramLogMin;

	/** temporary exposed until we found good values 4: 16, 8: 256 */
	UPROPERTY(Interp, BlueprintReadWrite, Category="Exposure", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "16.0"))
	float HistogramLogMax;


	FCameraExposureSettings()
	{
		// next value might get overwritten by r.DefaultFeature.AutoExposure.Method
		Method = AEM_Histogram;
		LowPercent = 80.0f;
		HighPercent = 98.3f;
		// next value might get overwritten by r.DefaultFeature.AutoExposure
		MinBrightness = 0.03f;
		// next value might get overwritten by r.DefaultFeature.AutoExposure
		MaxBrightness = 2.0f;
		SpeedUp = 3.0f;
		SpeedDown = 1.0f;
		Bias = 0.0f;
		HistogramLogMin = -8.0f;
		HistogramLogMax = 4.0f;
	}

	/* Exports to post process settings with overrides. */
	ENGINE_API void ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const;
};

USTRUCT(BlueprintType)
struct FWeightedBlendable
{
	GENERATED_USTRUCT_BODY()

	/** 0:no effect .. 1:full effect */
	UPROPERTY(interp, BlueprintReadWrite, Category=FWeightedBlendable, meta=(ClampMin = "0.0", ClampMax = "1.0", Delta = "0.01"))
	float Weight;

	/** should be of the IBlendableInterface* type but UProperties cannot express that */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=FWeightedBlendable, meta=( AllowedClasses="BlendableInterface", Keywords="PostProcess" ))
	UObject* Object;

	// default constructor
	FWeightedBlendable()
		: Weight(-1)
		, Object(0)
	{
	}

	// constructor
	// @param InWeight -1 is used to hide the weight and show the "Choose" UI, 0:no effect .. 1:full effect
	FWeightedBlendable(float InWeight, UObject* InObject)
		: Weight(InWeight)
		, Object(InObject)
	{
	}
};

// for easier detail customization, needed?
USTRUCT(BlueprintType)
struct FWeightedBlendables
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PostProcessSettings", meta=( Keywords="PostProcess" ))
	TArray<FWeightedBlendable> Array;
};

// NVCHANGE_BEGIN: Add VXGI
/** used by FPostProcessSettings VXGI */
UENUM()
enum EVxgiSpecularTracingFilter
{
	VXGISTF_None UMETA(DisplayName = "None"),
	VXGISTF_Temporal UMETA(DisplayName = "Temporal filter"),
	VXGISTF_Simple UMETA(DisplayName = "Bilateral box filter"),
	VXGISTF_MAX,
};
// NVCHANGE_END: Add VXGI

// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
UENUM()
namespace EFogMode
{
	enum Type
	{
		FOG_NONE UMETA(DisplayName = "None"),
		FOG_NOSKY UMETA(DisplayName = "Exclude Sky"),
		FOG_FULL UMETA(DisplayName = "Full"),
	};
}
// NVCHANGE_END: Nvidia Volumetric Lighting

// NVCHANGE_BEGIN: Add HBAO+

UENUM()
enum EHBAOBlurRadius
{
	AOBR_BlurRadius0 UMETA(DisplayName = "Disabled"),
	AOBR_BlurRadius2 UMETA(DisplayName = "2 pixels"),
	AOBR_BlurRadius4 UMETA(DisplayName = "4 pixels"),
	AOBR_MAX,
};

// NVCHANGE_END: Add HBAO+

/** To be able to use struct PostProcessSettings. */
// Each property consists of a bool to enable it (by default off),
// the variable declaration and further down the default value for it.
// The comment should include the meaning and usable range.
USTRUCT(BlueprintType, meta=(HiddenByDefault))
struct FPostProcessSettings
{
	GENERATED_USTRUCT_BODY()

	// first all bOverride_... as they get grouped together into bitfields

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_WhiteTemp:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_WhiteTint:1;

	// Color Correction controls
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturation:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrast:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGamma:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGain:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffset:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturationShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrastShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGammaShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGainShadows : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffsetShadows : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturationMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrastMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGammaMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGainMidtones : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffsetMidtones : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorSaturationHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorContrastHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGammaHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGainHighlights : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorOffsetHighlights : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorCorrectionShadowsMax : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorCorrectionHighlightsMin : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmWhitePoint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmSaturation:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmChannelMixerRed:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmChannelMixerGreen:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmChannelMixerBlue:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmContrast:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmDynamicRange:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmHealAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmToeAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShadowTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShadowTintBlend:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShadowTintAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmSlope:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmToe:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmShoulder:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmBlackClip:1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_FilmWhiteClip:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_SceneColorTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_SceneFringeIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientCubemapTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientCubemapIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomMethod : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom1Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom1Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom2Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom2Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom3Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom3Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom4Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom4Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom5Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom5Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom6Tint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_Bloom6Size:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomSizeScale:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionTexture : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionSize : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionCenterUV : 1;

	UPROPERTY()
	uint32 bOverride_BloomConvolutionPreFilter_DEPRECATED : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionPreFilterMin : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionPreFilterMax : 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionPreFilterMult : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomConvolutionBufferScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomDirtMaskIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomDirtMaskTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_BloomDirtMask:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
    uint32 bOverride_AutoExposureMethod:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureLowPercent:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureHighPercent:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureMinBrightness:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureMaxBrightness:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureSpeedUp:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureSpeedDown:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AutoExposureBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_HistogramLogMin:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_HistogramLogMax:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareTint:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareTints:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareBokehSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareBokehShape:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LensFlareThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VignetteIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_GrainIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_GrainJitter:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionStaticFraction:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionRadius:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionFadeDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionFadeRadius:1;

	// NVCHANGE_BEGIN: Add HBAO+
    UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOPowerExponent : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAORadius : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOBias : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOSmallScaleAO : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOBlurRadius : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOBlurSharpness : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOForegroundAOEnable : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOForegroundAODistance : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOBackgroundAOEnable : 1;

	UPROPERTY(BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault))
	uint32 bOverride_HBAOBackgroundAODistance : 1;
	// NVCHANGE_END: Add HBAO+

	UPROPERTY()
	uint32 bOverride_AmbientOcclusionDistance_DEPRECATED:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionRadiusInWS:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionPower:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionQuality:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionMipBlend:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionMipScale:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_AmbientOcclusionMipThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVIntensity:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionRadius:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDiffuseOcclusionExponent:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVSpecularOcclusionExponent:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVDiffuseOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, Category=Overrides, meta=(InlineEditConditionToggle))
	uint32 bOverride_LPVSpecularOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSecondaryOcclusionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVSecondaryBounceIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVGeometryVolumeBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVVplInjectionBias:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVEmissiveInjectionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVFadeRange : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_LPVDirectionalOcclusionFadeRange : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_IndirectLightingColor:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_IndirectLightingIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGradingIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ColorGradingLUT:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFocalDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFstop:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldSensorWidth:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldDepthBlurRadius:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldDepthBlurAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFocalRegion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldNearTransitionRegion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFarTransitionRegion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldScale:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldMaxBokehSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldNearBlurSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldFarBlurSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldMethod:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MobileHQGaussian:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldBokehShape:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldOcclusion:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldColorThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldSizeThreshold:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldSkyFocusDistance:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_DepthOfFieldVignetteSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MotionBlurAmount:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MotionBlurMax:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_MotionBlurPerObjectSize:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenPercentage:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionIntensity:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionQuality:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionMaxRoughness:1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Overrides, meta=(PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_ScreenSpaceReflectionRoughnessScale:1;



	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_RayleighTransmittance : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_MieBlendFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_MieColor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_MieTransmittance : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_AbsorptionColor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_AbsorptionTransmittance : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_HGColor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_HGTransmittance : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_HGEccentricity1 : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_HGEccentricity2 : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_HGEccentricityRatio : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_IsotropicColor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_IsotropicTransmittance : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_FogMode : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_FogIntensity : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_FogColor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
		uint32 bOverride_FogTransmittance : 1;
	// NVCHANGE_END: Nvidia Volumetric Lighting


	// NVCHANGE_BEGIN: Add VXGI

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingIntensity : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingIntensity : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiMultiBounceIrradianceScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingSparsity : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingNumCones : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_bVxgiDiffuseTracingAutoAngle : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingConeAngle : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingConeNormalGroupingFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingMaxSamples : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingMaxSamples : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingStep : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingTracingStep : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingOpacityCorrectionFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingOpacityCorrectionFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_bVxgiDiffuseTracingConeRotation : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_bVxgiDiffuseTracingRandomConeOffsets : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingNormalOffsetFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingEnvironmentMapTint : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingEnvironmentMap : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingInitialOffsetBias : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingInitialOffsetDistanceFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientColor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientRange : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientBias : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientPowerExponent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientDistanceDarkening : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiAmbientMixIntensity : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingInitialOffsetBias : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingInitialOffsetDistanceFactor : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingEnvironmentMapTint : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingFilter : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingEnvironmentMap : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiSpecularTracingTangentJitterScale : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_bVxgiDiffuseTracingTemporalReprojectionEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingTemporalReprojectionPreviousFrameWeight : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingTemporalReprojectionMaxDistanceInVoxels : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_VxgiDiffuseTracingTemporalReprojectionNormalWeightExponent : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_bVxgiDiffuseTracingRefinementEnabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (PinHiddenByDefault, InlineEditConditionToggle))
	uint32 bOverride_bVxgiDiffuseTracingFlipOpacityDirections : 1;

	// NVCHANGE_END: Add VXGI

	// -----------------------------------------------------------------------

	UPROPERTY(interp, BlueprintReadWrite, Category="Color Grading|WhiteBalance", meta=(UIMin = "1500.0", UIMax = "15000.0", editcondition = "bOverride_WhiteTemp", DisplayName = "Temp"))
	float WhiteTemp;
	UPROPERTY(interp, BlueprintReadWrite, Category="Color Grading|WhiteBalance", meta=(UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_WhiteTint", DisplayName = "Tint"))
	float WhiteTint;

	// Color Correction controls
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "saturation", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorSaturation", DisplayName = "Saturation"))
	FVector4 ColorSaturation;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "contrast", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorContrast", DisplayName = "Contrast"))
	FVector4 ColorContrast;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gamma", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGamma", DisplayName = "Gamma"))
	FVector4 ColorGamma;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gain", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGain", DisplayName = "Gain"))
	FVector4 ColorGain;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Global", meta = (UIMin = "-1.0", UIMax = "1.0", Delta = "0.001", ColorGradingMode = "offset", ShiftMouseMovePixelPerDelta = "20", SupportDynamicSliderMaxValue = "true", SupportDynamicSliderMinValue = "true", editcondition = "bOverride_ColorOffset", DisplayName = "Offset"))
	FVector4 ColorOffset;

	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "saturation", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorSaturationShadows", DisplayName = "Saturation"))
	FVector4 ColorSaturationShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "contrast", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorContrastShadows", DisplayName = "Contrast"))
	FVector4 ColorContrastShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gamma", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGammaShadows", DisplayName = "Gamma"))
	FVector4 ColorGammaShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Shadows", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gain", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGainShadows", DisplayName = "Gain"))
	FVector4 ColorGainShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Shadows", meta = (UIMin = "-1.0", UIMax = "1.0", Delta = "0.001", ColorGradingMode = "offset", ShiftMouseMovePixelPerDelta = "20", SupportDynamicSliderMaxValue = "true", SupportDynamicSliderMinValue = "true", editcondition = "bOverride_ColorOffsetShadows", DisplayName = "Offset"))
	FVector4 ColorOffsetShadows;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Shadows", meta = (UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_ColorCorrectionShadowsMax", DisplayName = "ShadowsMax"))
	float ColorCorrectionShadowsMax;

	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "saturation", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorSaturationMidtones", DisplayName = "Saturation"))
	FVector4 ColorSaturationMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "contrast", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorContrastMidtones", DisplayName = "Contrast"))
	FVector4 ColorContrastMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gamma", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGammaMidtones", DisplayName = "Gamma"))
	FVector4 ColorGammaMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Midtones", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gain", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGainMidtones", DisplayName = "Gain"))
	FVector4 ColorGainMidtones;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Midtones", meta = (UIMin = "-1.0", UIMax = "1.0", Delta = "0.001", ColorGradingMode = "offset", ShiftMouseMovePixelPerDelta = "20", SupportDynamicSliderMaxValue = "true", SupportDynamicSliderMinValue = "true", editcondition = "bOverride_ColorOffsetMidtones", DisplayName = "Offset"))
	FVector4 ColorOffsetMidtones;

	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "saturation", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorSaturationHighlights", DisplayName = "Saturation"))
	FVector4 ColorSaturationHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "contrast", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorContrastHighlights", DisplayName = "Contrast"))
	FVector4 ColorContrastHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gamma", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGammaHighlights", DisplayName = "Gamma"))
	FVector4 ColorGammaHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Highlights", meta = (UIMin = "0.0", UIMax = "2.0", Delta = "0.01", ColorGradingMode = "gain", ShiftMouseMovePixelPerDelta = "10", SupportDynamicSliderMaxValue = "true", editcondition = "bOverride_ColorGainHighlights", DisplayName = "Gain"))
	FVector4 ColorGainHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Highlights", meta = (UIMin = "-1.0", UIMax = "1.0", Delta = "0.001", ColorGradingMode = "offset", ShiftMouseMovePixelPerDelta = "20", SupportDynamicSliderMaxValue = "true", SupportDynamicSliderMinValue = "true", editcondition = "bOverride_ColorOffsetHighlights", DisplayName = "Offset"))
	FVector4 ColorOffsetHighlights;
	UPROPERTY(interp, BlueprintReadWrite, Category = "Color Grading|Highlights", meta = (UIMin = "-1.0", UIMax = "1.0", editcondition = "bOverride_ColorCorrectionHighlightsMin", DisplayName = "HighlightsMin"))
	float ColorCorrectionHighlightsMin;

	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmSlope", DisplayName = "Slope"))
	float FilmSlope;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmToe", DisplayName = "Toe"))
	float FilmToe;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShoulder", DisplayName = "Shoulder"))
	float FilmShoulder;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmBlackClip", DisplayName = "Black clip"))
	float FilmBlackClip;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmWhiteClip", DisplayName = "White clip"))
	float FilmWhiteClip;

	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(editcondition = "bOverride_FilmWhitePoint", DisplayName = "Tint", HideAlphaChannel, LegacyTonemapper))
	FLinearColor FilmWhitePoint;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(editcondition = "bOverride_FilmShadowTint", DisplayName = "Tint Shadow", HideAlphaChannel, LegacyTonemapper))
	FLinearColor FilmShadowTint;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShadowTintBlend", DisplayName = "Tint Shadow Blend", LegacyTonemapper))
	float FilmShadowTintBlend;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmShadowTintAmount", DisplayName = "Tint Shadow Amount", LegacyTonemapper))
	float FilmShadowTintAmount;

	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "2.0", editcondition = "bOverride_FilmSaturation", DisplayName = "Saturation", LegacyTonemapper))
	float FilmSaturation;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerRed", DisplayName = "Channel Mixer Red", HideAlphaChannel, LegacyTonemapper))
	FLinearColor FilmChannelMixerRed;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerGreen", DisplayName = "Channel Mixer Green", HideAlphaChannel, LegacyTonemapper))
	FLinearColor FilmChannelMixerGreen;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(editcondition = "bOverride_FilmChannelMixerBlue", DisplayName = " Channel Mixer Blue", HideAlphaChannel, LegacyTonemapper))
	FLinearColor FilmChannelMixerBlue;

	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmContrast", DisplayName = "Contrast", LegacyTonemapper))
	float FilmContrast;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmToeAmount", DisplayName = "Crush Shadows", LegacyTonemapper))
	float FilmToeAmount;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_FilmHealAmount", DisplayName = "Crush Highlights", LegacyTonemapper))
	float FilmHealAmount;
	UPROPERTY(interp, BlueprintReadWrite, Category="Tonemapper", AdvancedDisplay, meta=(UIMin = "1.0", UIMax = "4.0", editcondition = "bOverride_FilmDynamicRange", DisplayName = "Dynamic Range", LegacyTonemapper))
	float FilmDynamicRange;

	/** Scene tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Color Grading|Global", meta=(editcondition = "bOverride_SceneColorTint", HideAlphaChannel))
	FLinearColor SceneColorTint;
	
	/** in percent, Scene chromatic aberration / color fringe (camera imperfection) to simulate an artifact that happens in real-world lens, mostly visible in the image corners. */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Image Effects", meta=(UIMin = "0.0", UIMax = "5.0", editcondition = "bOverride_SceneFringeIntensity", DisplayName = "Chromatic Aberration"))
	float SceneFringeIntensity;


	/** Bloom algorithm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Bloom", meta = (editcondition = "bOverride_BloomMethod", DisplayName = "Method"))
	TEnumAsByte<enum EBloomMethod> BloomMethod;

	/** Multiplier for all bloom contributions >=0: off, 1(default), >1 brighter */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomIntensity", DisplayName = "Intensity"))
	float BloomIntensity;

	/**
	 * minimum brightness the bloom starts having effect
	 * -1:all pixels affect bloom equally (physically correct, faster as a threshold pass is omitted), 0:all pixels affect bloom brights more, 1(default), >1 brighter
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", meta=(ClampMin = "-1.0", UIMax = "8.0", editcondition = "bOverride_BloomThreshold", DisplayName = "Threshold"))
	float BloomThreshold;

	/**
	 * Scale for all bloom sizes
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", editcondition = "bOverride_BloomSizeScale", DisplayName = "Size scale"))
	float BloomSizeScale;

	/**
	 * Diameter size for the Bloom1 in percent of the screen width
	 * (is done in 1/2 resolution, larger values cost more performance, good for high frequency details)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_Bloom1Size", DisplayName = "#1 Size"))
	float Bloom1Size;
	/**
	 * Diameter size for Bloom2 in percent of the screen width
	 * (is done in 1/4 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_Bloom2Size", DisplayName = "#2 Size"))
	float Bloom2Size;
	/**
	 * Diameter size for Bloom3 in percent of the screen width
	 * (is done in 1/8 resolution, larger values cost more performance)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "16.0", editcondition = "bOverride_Bloom3Size", DisplayName = "#3 Size"))
	float Bloom3Size;
	/**
	 * Diameter size for Bloom4 in percent of the screen width
	 * (is done in 1/16 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "32.0", editcondition = "bOverride_Bloom4Size", DisplayName = "#4 Size"))
	float Bloom4Size;
	/**
	 * Diameter size for Bloom5 in percent of the screen width
	 * (is done in 1/32 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "64.0", editcondition = "bOverride_Bloom5Size", DisplayName = "#5 Size"))
	float Bloom5Size;
	/**
	 * Diameter size for Bloom6 in percent of the screen width
	 * (is done in 1/64 resolution, larger values cost more performance, best for wide contributions)
	 * >=0: can be clamped because of shader limitations
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "128.0", editcondition = "bOverride_Bloom6Size", DisplayName = "#6 Size"))
	float Bloom6Size;

	/** Bloom1 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(editcondition = "bOverride_Bloom1Tint", DisplayName = "#1 Tint", HideAlphaChannel))
	FLinearColor Bloom1Tint;
	/** Bloom2 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(editcondition = "bOverride_Bloom2Tint", DisplayName = "#2 Tint", HideAlphaChannel))
	FLinearColor Bloom2Tint;
	/** Bloom3 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(editcondition = "bOverride_Bloom3Tint", DisplayName = "#3 Tint", HideAlphaChannel))
	FLinearColor Bloom3Tint;
	/** Bloom4 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(editcondition = "bOverride_Bloom4Tint", DisplayName = "#4 Tint", HideAlphaChannel))
	FLinearColor Bloom4Tint;
	/** Bloom5 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(editcondition = "bOverride_Bloom5Tint", DisplayName = "#5 Tint", HideAlphaChannel))
	FLinearColor Bloom5Tint;
	/** Bloom6 tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Bloom", AdvancedDisplay, meta=(editcondition = "bOverride_Bloom6Tint", DisplayName = "#6 Tint", HideAlphaChannel))
	FLinearColor Bloom6Tint;

	/** Texture to replace default convolution bloom kernel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Bloom", meta = (editcondition = "bOverride_BloomConvolutionTexture", DisplayName = "Convolution Kernel"))
	class UTexture2D* BloomConvolutionTexture;

	/** Relative size of the convolution kernel image compared to the minor axis of the viewport  */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (ClampMin = "0.0", UIMax = "1.0", editcondition = "bOverride_BloomConvolutionSize", DisplayName = "Convolution Scale"))
	float BloomConvolutionSize;

	/** The UV location of the center of the kernel.  Should be very close to (.5,.5) */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (editcondition = "bOverride_BloomConvolutionCenterUV", DisplayName = "Convolution Center"))
	FVector2D BloomConvolutionCenterUV;

	UPROPERTY()
	FVector BloomConvolutionPreFilter_DEPRECATED;
	
	/** Boost intensity of select pixels  prior to computing bloom convolution (Min, Max, Multiplier).  Max < Min disables */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (editcondition = "bOverride_BloomConvolutionPreFilterMin", DisplayName = "Convolution Boost Min"))
	float BloomConvolutionPreFilterMin;

	/** Boost intensity of select pixels  prior to computing bloom convolution (Min, Max, Multiplier).  Max < Min disables */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (editcondition = "bOverride_BloomConvolutionPreFilterMax", DisplayName = "Convolution Boost Max"))
	float BloomConvolutionPreFilterMax;

	/** Boost intensity of select pixels  prior to computing bloom convolution (Min, Max, Multiplier).  Max < Min disables */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (editcondition = "bOverride_BloomConvolutionPreFilterMult", DisplayName = "Convolution Boost Mult"))
	float BloomConvolutionPreFilterMult;

	/** Implicit buffer region as a fraction of the screen size to insure the bloom does not wrap across the screen.  Larger sizes have perf impact.*/
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Bloom", AdvancedDisplay, meta = (ClampMin = "0.0", UIMax = "1.0", editcondition = "bOverride_BloomConvolutionBufferScale", DisplayName = "Convolution Buffer"))
	float BloomConvolutionBufferScale;
	
	/**
	 * Texture that defines the dirt on the camera lens where the light of very bright objects is scattered.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens|Dirt Mask", meta=(editcondition = "bOverride_BloomDirtMask", DisplayName = "Dirt Mask Texture"))
	class UTexture* BloomDirtMask;	
	
	/** BloomDirtMask intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Dirt Mask", meta=(ClampMin = "0.0", UIMax = "8.0", editcondition = "bOverride_BloomDirtMaskIntensity", DisplayName = "Dirt Mask Intensity"))
	float BloomDirtMaskIntensity;

	/** BloomDirtMask tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Dirt Mask", meta=(editcondition = "bOverride_BloomDirtMaskTint", DisplayName = "Dirt Mask Tint", HideAlphaChannel))
	FLinearColor BloomDirtMaskTint;

	/** AmbientCubemap tint color */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Cubemap", meta=(editcondition = "bOverride_AmbientCubemapTint", DisplayName = "Tint", HideAlphaChannel))
	FLinearColor AmbientCubemapTint;

	/**
	 * To scale the Ambient cubemap brightness
	 * >=0: off, 1(default), >1 brighter
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Cubemap", meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_AmbientCubemapIntensity", DisplayName = "Intensity"))
	float AmbientCubemapIntensity;

	/** The Ambient cubemap (Affects diffuse and specular shading), blends additively which if different from all other settings here */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering Features|Ambient Cubemap", meta=(DisplayName = "Cubemap Texture"))
	class UTextureCube* AmbientCubemap;

	/** Luminance computation method */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens|Auto Exposure", meta=(editcondition = "bOverride_AutoExposureMethod", DisplayName = "Method"))
    TEnumAsByte<enum EAutoExposureMethod> AutoExposureMethod;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 70 .. 80
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_AutoExposureLowPercent", DisplayName = "Low Percent"))
	float AutoExposureLowPercent;

	/**
	 * The eye adaptation will adapt to a value extracted from the luminance histogram of the scene color.
	 * The value is defined as having x percent below this brightness. Higher values give bright spots on the screen more priority
	 * but can lead to less stable results. Lower values give the medium and darker values more priority but might cause burn out of
	 * bright spots.
	 * >0, <100, good values are in the range 80 .. 95
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_AutoExposureHighPercent", DisplayName = "High Percent"))
	float AutoExposureHighPercent;

	/**
	 * A good value should be positive near 0. This is the minimum brightness the auto exposure can adapt to.
	 * It should be tweaked in a dark lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AutoExposureMinBrightness", DisplayName = "Min Brightness"))
	float AutoExposureMinBrightness;

	/**
	 * A good value should be positive (2 is a good value). This is the maximum brightness the auto exposure can adapt to.
	 * It should be tweaked in a bright lighting situation (too small: image appears too bright, too large: image appears too dark).
	 * Note: Tweaking emissive materials and lights or tweaking auto exposure can look the same. Tweaking auto exposure has global
	 * effect and defined the HDR range - you don't want to change that late in the project development.
	 * Eye Adaptation is disabled if MinBrightness = MaxBrightness
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AutoExposureMaxBrightness", DisplayName = "Max Brightness"))
	float AutoExposureMaxBrightness;

	/** >0 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", meta=(ClampMin = "0.02", UIMax = "20.0", editcondition = "bOverride_AutoExposureSpeedUp", DisplayName = "Speed Up"))
	float AutoExposureSpeedUp;

	/** >0 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", meta=(ClampMin = "0.02", UIMax = "20.0", editcondition = "bOverride_AutoExposureSpeedDown", DisplayName = "Speed Down"))
	float AutoExposureSpeedDown;

	/**
	 * Logarithmic adjustment for the exposure. Only used if a tonemapper is specified.
	 * 0: no adjustment, -1:2x darker, -2:4x darker, 1:2x brighter, 2:4x brighter, ...
	 */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Auto Exposure", meta = (UIMin = "-8.0", UIMax = "8.0", editcondition = "bOverride_AutoExposureBias", DisplayName = "Exposure Bias"))
	float AutoExposureBias;

	/** temporary exposed until we found good values, -8: 1/256, -10: 1/1024 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", AdvancedDisplay, meta=(UIMin = "-16", UIMax = "0.0", editcondition = "bOverride_HistogramLogMin"))
	float HistogramLogMin;

	/** temporary exposed until we found good values 4: 16, 8: 256 */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Auto Exposure", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "16.0", editcondition = "bOverride_HistogramLogMax"))
	float HistogramLogMax;

	/** Brightness scale of the image cased lens flares (linear) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Lens Flares", meta=(UIMin = "0.0", UIMax = "16.0", editcondition = "bOverride_LensFlareIntensity", DisplayName = "Intensity"))
	float LensFlareIntensity;

	/** Tint color for the image based lens flares. */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Lens Flares", meta=(editcondition = "bOverride_LensFlareTint", DisplayName = "Tint", HideAlphaChannel))
	FLinearColor LensFlareTint;

	/** Size of the Lens Blur (in percent of the view width) that is done with the Bokeh texture (note: performance cost is radius*radius) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Lens Flares", meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_LensFlareBokehSize", DisplayName = "BokehSize"))
	float LensFlareBokehSize;

	/** Minimum brightness the lens flare starts having effect (this should be as high as possible to avoid the performance cost of blurring content that is too dark too see) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Lens Flares", meta=(UIMin = "0.1", UIMax = "32.0", editcondition = "bOverride_LensFlareThreshold", DisplayName = "Threshold"))
	float LensFlareThreshold;

	/** Defines the shape of the Bokeh when the image base lens flares are blurred, cannot be blended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens|Lens Flares", meta=(editcondition = "bOverride_LensFlareBokehShape", DisplayName = "BokehShape"))
	class UTexture* LensFlareBokehShape;

	/** RGB defines the lens flare color, A it's position. This is a temporary solution. */
	UPROPERTY(EditAnywhere, Category="Lens|Lens Flares", meta=(editcondition = "bOverride_LensFlareTints", DisplayName = "Tints"))
	FLinearColor LensFlareTints[8];

	/** 0..1 0=off/no vignette .. 1=strong vignette */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Image Effects", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_VignetteIntensity"))
	float VignetteIntensity;

	/** 0..1 grain jitter */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Lens|Image Effects", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_GrainJitter"))
	float GrainJitter;

	/** 0..1 grain intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Image Effects", meta=(UIMin = "0.0", UIMax = "1.0", editcondition = "bOverride_GrainIntensity"))
	float GrainIntensity;

	// NVCHANGE_BEGIN: Add HBAO+
	/** 0..4 >0 to enable HBAO+ (DX11/Windows only) .. the greater this parameter, the darker is the HBAO */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_HBAOPowerExponent", DisplayName = "Power Exponent"))
	float HBAOPowerExponent;

	/** 0..2 in meters, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.1", UIMax = "2.0", editcondition = "bOverride_HBAORadius", DisplayName = "Radius"))
	float HBAORadius;

	/** 0.0..0.2 increase to hide tesselation artifacts */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.0", UIMax = "0.2", editcondition = "bOverride_HBAOBias", DisplayName = "Bias"))
	float HBAOBias;

	/** 0..1 strength of the low-range occlusion .. set to 0.0 to improve performance */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.0", UIMax = "1.0", editcondition = "bOverride_HBAOSmallScaleAO", DisplayName = "SmallScale AO"))
	float HBAOSmallScaleAO;

	/** The HBAO blur is needed to hide noise artifacts .. Blur radius = 4 pixels is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (editcondition = "bOverride_HBAOBlurRadius", DisplayName = "Blur Radius"))
	TEnumAsByte<enum EHBAOBlurRadius> HBAOBlurRadius;

	/** 0..32 the larger, the more the HBAO blur preserves edges */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.0", UIMax = "32.0", editcondition = "bOverride_HBAOBlurSharpness", DisplayName = "Blur Sharpness"))
	float HBAOBlurSharpness;

	/** Enables clamping of AO radius for foreground objects */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (editcondition = "bOverride_HBAOForegroundAOEnable", DisplayName = "Clamp Foreground AO"))
	uint32 HBAOForegroundAOEnable : 1;
	
	/** Distance from camera at which the foreground AO radius should be clamped */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.0", UIMax = "1000.0", editcondition = "bOverride_HBAOForegroundAODistance", DisplayName = "Foreground AO Distance"))
	float HBAOForegroundAODistance;

	/** Enables clamping of AO radius for background objects */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (editcondition = "bOverride_HBAOBackgroundAOEnable", DisplayName = "Clamp Background AO"))
	uint32 HBAOBackgroundAOEnable : 1;

	/** Distance from camera at which the background AO radius should be clamped */
	UPROPERTY(interp, BlueprintReadWrite, Category = "HBAO+", meta = (ClampMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_HBAOBackgroundAODistance", DisplayName = "Background AO Distance"))
	float HBAOBackgroundAODistance;
	// NVCHANGE_END: Add HBAO+

	/** 0..1 0=off/no ambient occlusion .. 1=strong ambient occlusion, defines how much it affects the non direct lighting after base pass */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AmbientOcclusionIntensity", DisplayName = "Intensity"))
	float AmbientOcclusionIntensity;

	/** 0..1 0=no effect on static lighting .. 1=AO affects the stat lighting, 0 is free meaning no extra rendering pass */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AmbientOcclusionStaticFraction", DisplayName = "Static Fraction"))
	float AmbientOcclusionStaticFraction;

	/** >0, in unreal units, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", meta=(ClampMin = "0.1", UIMax = "500.0", editcondition = "bOverride_AmbientOcclusionRadius", DisplayName = "Radius"))
	float AmbientOcclusionRadius;

	/** true: AO radius is in world space units, false: AO radius is locked the view space in 400 units */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(editcondition = "bOverride_AmbientOcclusionRadiusInWS", DisplayName = "Radius in WorldSpace"))
	uint32 AmbientOcclusionRadiusInWS:1;

	/** >0, in unreal units, at what distance the AO effect disppears in the distance (avoding artifacts and AO effects on huge object) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "20000.0", editcondition = "bOverride_AmbientOcclusionFadeDistance", DisplayName = "Fade Out Distance"))
	float AmbientOcclusionFadeDistance;
	
	/** >0, in unreal units, how many units before AmbientOcclusionFadeOutDistance it starts fading out */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "20000.0", editcondition = "bOverride_AmbientOcclusionFadeRadius", DisplayName = "Fade Out Radius"))
	float AmbientOcclusionFadeRadius;

	/** >0, in unreal units, how wide the ambient occlusion effect should affect the geometry (in depth), will be removed - only used for non normal method which is not exposed */
	UPROPERTY()
	float AmbientOcclusionDistance_DEPRECATED;

	/** >0, in unreal units, bigger values means even distant surfaces affect the ambient occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.1", UIMax = "8.0", editcondition = "bOverride_AmbientOcclusionPower", DisplayName = "Power"))
	float AmbientOcclusionPower;

	/** >0, in unreal units, default (3.0) works well for flat surfaces but can reduce details */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "10.0", editcondition = "bOverride_AmbientOcclusionBias", DisplayName = "Bias"))
	float AmbientOcclusionBias;

	/** 0=lowest quality..100=maximum quality, only a few quality levels are implemented, no soft transition */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_AmbientOcclusionQuality", DisplayName = "Quality"))
	float AmbientOcclusionQuality;

	/** Affects the blend over the multiple mips (lower resolution versions) , 0:fully use full resolution, 1::fully use low resolution, around 0.6 seems to be a good value */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.1", UIMax = "1.0", editcondition = "bOverride_AmbientOcclusionMipBlend", DisplayName = "Mip Blend"))
	float AmbientOcclusionMipBlend;

	/** Affects the radius AO radius scale over the multiple mips (lower resolution versions) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.5", UIMax = "4.0", editcondition = "bOverride_AmbientOcclusionMipScale", DisplayName = "Mip Scale"))
	float AmbientOcclusionMipScale;

	/** to tweak the bilateral upsampling when using multiple mips (lower resolution versions) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Ambient Occlusion", AdvancedDisplay, meta=(ClampMin = "0.0", UIMax = "0.1", editcondition = "bOverride_AmbientOcclusionMipThreshold", DisplayName = "Mip Threshold"))
	float AmbientOcclusionMipThreshold;

	/** Adjusts indirect lighting color. (1,1,1) is default. (0,0,0) to disable GI. The show flag 'Global Illumination' must be enabled to use this property. */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Global Illumination", meta=(editcondition = "bOverride_IndirectLightingColor", DisplayName = "Indirect Lighting Color", HideAlphaChannel))
	FLinearColor IndirectLightingColor;

	/** Scales the indirect lighting contribution. A value of 0 disables GI. Default is 1. The show flag 'Global Illumination' must be enabled to use this property. */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Global Illumination", meta=(ClampMin = "0", UIMax = "4.0", editcondition = "bOverride_IndirectLightingIntensity", DisplayName = "Indirect Lighting Intensity"))
	float IndirectLightingIntensity;

	/** Color grading lookup table intensity. 0 = no intensity, 1=full intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category="Color Grading|Global", meta=(ClampMin = "0", ClampMax = "1.0", editcondition = "bOverride_ColorGradingIntensity", DisplayName = "Color Grading LUT Intensity"))
	float ColorGradingIntensity;

	/** Look up table texture to use or none of not used*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Color Grading|Global", meta=(editcondition = "bOverride_ColorGradingLUT", DisplayName = "Color Grading LUT"))
	class UTexture* ColorGradingLUT;

	/** BokehDOF, Simple gaussian, ... Mobile supports Gaussian only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(editcondition = "bOverride_DepthOfFieldMethod", DisplayName = "Method"))
	TEnumAsByte<enum EDepthOfFieldMethod> DepthOfFieldMethod;

	/** Enable HQ Gaussian on high end mobile platforms. (ES3_1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens|Depth of Field", meta = (editcondition = "bOverride_MobileHQGaussian", DisplayName = "High Quality Gaussian DoF on Mobile"))
	uint32 bMobileHQGaussian : 1;

	/** CircleDOF only: Defines the opening of the camera lens, Aperture is 1/fstop, typical lens go down to f/1.2 (large opening), larger numbers reduce the DOF effect */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(ClampMin = "1.0", ClampMax = "32.0", editcondition = "bOverride_DepthOfFieldFstop", DisplayName = "Aperture F-stop"))
	float DepthOfFieldFstop;

	/** Width of the camera sensor to assume, in mm. */
	UPROPERTY(BlueprintReadWrite, Category="Lens|Depth of Field", meta=(ForceUnits=mm, ClampMin = "0.1", UIMin="0.1", UIMax= "1000.0", editcondition = "bOverride_DepthOfFieldSensorWidth", DisplayName = "Sensor Width (mm)"))
	float DepthOfFieldSensorWidth;

	/** Distance in which the Depth of Field effect should be sharp, in unreal units (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(ClampMin = "0.0", UIMin = "1.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFocalDistance", DisplayName = "Focal Distance"))
	float DepthOfFieldFocalDistance;

	/** CircleDOF only: Depth blur km for 50% */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(ClampMin = "0.000001", ClampMax = "100.0", editcondition = "bOverride_DepthOfFieldDepthBlurAmount", DisplayName = "Depth Blur km for 50%"))
	float DepthOfFieldDepthBlurAmount;

	/** CircleDOF only: Depth blur radius in pixels at 1920x */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(ClampMin = "0.0", UIMax = "4.0", editcondition = "bOverride_DepthOfFieldDepthBlurRadius", DisplayName = "Depth Blur Radius"))
	float DepthOfFieldDepthBlurRadius;

	/** Artificial region where all content is in focus, starting after DepthOfFieldFocalDistance, in unreal units  (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFocalRegion", DisplayName = "Focal Region"))
	float DepthOfFieldFocalRegion;

	/** To define the width of the transition region next to the focal region on the near side (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldNearTransitionRegion", DisplayName = "Near Transition Region"))
	float DepthOfFieldNearTransitionRegion;

	/** To define the width of the transition region next to the focal region on the near side (cm) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(UIMin = "0.0", UIMax = "10000.0", editcondition = "bOverride_DepthOfFieldFarTransitionRegion", DisplayName = "Far Transition Region"))
	float DepthOfFieldFarTransitionRegion;

	/** SM5: BokehDOF only: To amplify the depth of field effect (like aperture)  0=off 
	    ES2: Used to blend DoF. 0=off
	*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(ClampMin = "0.0", ClampMax = "2.0", editcondition = "bOverride_DepthOfFieldScale", DisplayName = "Scale"))
	float DepthOfFieldScale;

	/** BokehDOF only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size*size) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldMaxBokehSize", DisplayName = "Max Bokeh Size"))
	float DepthOfFieldMaxBokehSize;

	/** Gaussian only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldNearBlurSize", DisplayName = "Near Blur Size"))
	float DepthOfFieldNearBlurSize;

	/** Gaussian only: Maximum size of the Depth of Field blur (in percent of the view width) (note: performance cost scales with size) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", meta=(UIMin = "0.0", UIMax = "32.0", editcondition = "bOverride_DepthOfFieldFarBlurSize", DisplayName = "Far Blur Size"))
	float DepthOfFieldFarBlurSize;

	/** Defines the shape of the Bokeh when object get out of focus, cannot be blended */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="Lens|Depth of Field", meta=(editcondition = "bOverride_DepthOfFieldBokehShape", DisplayName = "Shape"))
	class UTexture* DepthOfFieldBokehShape;

	/** Occlusion tweak factor 1 (0.18 to get natural occlusion, 0.4 to solve layer color leaking issues) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_DepthOfFieldOcclusion", DisplayName = "Occlusion"))
	float DepthOfFieldOcclusion;
	
	/** Color threshold to do full quality DOF (BokehDOF only) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "10.0", editcondition = "bOverride_DepthOfFieldColorThreshold", DisplayName = "Color Threshold"))
	float DepthOfFieldColorThreshold;

	/** Size threshold to do full quality DOF (BokehDOF only) */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_DepthOfFieldSizeThreshold", DisplayName = "Size Threshold"))
	float DepthOfFieldSizeThreshold;
	
	/** Artificial distance to allow the skybox to be in focus (e.g. 200000), <=0 to switch the feature off, only for GaussianDOF, can cost performance */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", AdvancedDisplay, meta=(ClampMin = "0.0", ClampMax = "200000.0", editcondition = "bOverride_DepthOfFieldSkyFocusDistance", DisplayName = "Sky Distance"))
	float DepthOfFieldSkyFocusDistance;

	/** Artificial circular mask to (near) blur content outside the radius, only for GaussianDOF, diameter in percent of screen width, costs performance if the mask is used, keep Feather can Radius on default to keep it off */
	UPROPERTY(interp, BlueprintReadWrite, Category="Lens|Depth of Field", AdvancedDisplay, meta=(UIMin = "0.0", UIMax = "100.0", editcondition = "bOverride_DepthOfFieldVignetteSize", DisplayName = "Vignette Size"))
	float DepthOfFieldVignetteSize;

	/** Strength of motion blur, 0:off, should be renamed to intensity */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Motion Blur", meta=(ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_MotionBlurAmount", DisplayName = "Amount"))
	float MotionBlurAmount;
	/** max distortion caused by motion blur, in percent of the screen width, 0:off */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Motion Blur", meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_MotionBlurMax", DisplayName = "Max"))
	float MotionBlurMax;
	/** The minimum projected screen radius for a primitive to be drawn in the velocity pass, percentage of screen width. smaller numbers cause more draw calls, default: 4% */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Motion Blur", meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_MotionBlurPerObjectSize", DisplayName = "Per Object Size"))
	float MotionBlurPerObjectSize;

	/** How strong the dynamic GI from the LPV should be. 0.0 is off, 1.0 is the "normal" value, but higher values can be used to boost the effect*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", meta=(editcondition = "bOverride_LPVIntensity", UIMin = "0", UIMax = "20", DisplayName = "Intensity"))
	float LPVIntensity;

	/** Bias applied to light injected into the LPV in cell units. Increase to reduce bleeding through thin walls*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVVplInjectionBias", UIMin = "0", UIMax = "2", DisplayName = "Light Injection Bias"))
	float LPVVplInjectionBias;

	/** The size of the LPV volume, in Unreal units*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", meta=(editcondition = "bOverride_LPVSize", UIMin = "100", UIMax = "20000", DisplayName = "Size"))
	float LPVSize;

	/** Secondary occlusion strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", meta=(editcondition = "bOverride_LPVSecondaryOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Occlusion Intensity"))
	float LPVSecondaryOcclusionIntensity;

	/** Secondary bounce light strength (bounce light shadows). Set to 0 to disable*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVSecondaryBounceIntensity", UIMin = "0", UIMax = "1", DisplayName = "Secondary Bounce Intensity"))
	float LPVSecondaryBounceIntensity;

	/** Bias applied to the geometry volume in cell units. Increase to reduce darkening due to secondary occlusion */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVGeometryVolumeBias", UIMin = "0", UIMax = "2", DisplayName = "Geometry Volume Bias"))
	float LPVGeometryVolumeBias;

	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVEmissiveInjectionIntensity", UIMin = "0", UIMax = "20", DisplayName = "Emissive Injection Intensity"))
	float LPVEmissiveInjectionIntensity;

	/** Controls the amount of directional occlusion. Requires LPV. Values very close to 1.0 are recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", meta=(editcondition = "bOverride_LPVDirectionalOcclusionIntensity", UIMin = "0", UIMax = "1", DisplayName = "Occlusion Intensity"))
	float LPVDirectionalOcclusionIntensity;

	/** Occlusion Radius - 16 is recommended for most scenes */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVDirectionalOcclusionRadius", UIMin = "1", UIMax = "16", DisplayName = "Occlusion Radius"))
	float LPVDirectionalOcclusionRadius;

	/** Diffuse occlusion exponent - increase for more contrast. 1 to 2 is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", meta=(editcondition = "bOverride_LPVDiffuseOcclusionExponent", UIMin = "0.5", UIMax = "5", DisplayName = "Diffuse occlusion exponent"))
	float LPVDiffuseOcclusionExponent;

	/** Specular occlusion exponent - increase for more contrast. 6 to 9 is recommended */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", meta=(editcondition = "bOverride_LPVSpecularOcclusionExponent", UIMin = "1", UIMax = "16", DisplayName = "Specular occlusion exponent"))
	float LPVSpecularOcclusionExponent;

	/** Diffuse occlusion intensity - higher values provide increased diffuse occlusion.*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVDiffuseOcclusionIntensity", UIMin = "0", UIMax = "4", DisplayName = "Diffuse occlusion intensity"))
	float LPVDiffuseOcclusionIntensity;

	/** Specular occlusion intensity - higher values provide increased specular occlusion.*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Light Propagation Volume", AdvancedDisplay, meta=(editcondition = "bOverride_LPVSpecularOcclusionIntensity", UIMin = "0", UIMax = "4", DisplayName = "Specular occlusion intensity"))
	float LPVSpecularOcclusionIntensity;

	/** Enable/Fade/disable the Screen Space Reflection feature, in percent, avoid numbers between 0 and 1 fo consistency */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Screen Space Reflections", meta=(ClampMin = "0.0", ClampMax = "100.0", editcondition = "bOverride_ScreenSpaceReflectionIntensity", DisplayName = "Intensity"))
	float ScreenSpaceReflectionIntensity;

	/** 0=lowest quality..100=maximum quality, only a few quality levels are implemented, no soft transition, 50 is the default for better performance. */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Screen Space Reflections", meta=(ClampMin = "0.0", UIMax = "100.0", editcondition = "bOverride_ScreenSpaceReflectionQuality", DisplayName = "Quality"))
	float ScreenSpaceReflectionQuality;

	/** Until what roughness we fade the screen space reflections, 0.8 works well, smaller can run faster */
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Screen Space Reflections", meta=(ClampMin = "0.01", ClampMax = "1.0", editcondition = "bOverride_ScreenSpaceReflectionMaxRoughness", DisplayName = "Max Roughness"))
	float ScreenSpaceReflectionMaxRoughness;

	/** LPV Fade range - increase to fade more gradually towards the LPV edges.*/
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Light Propagation Volume", AdvancedDisplay, meta = (editcondition = "bOverride_LPVFadeRange", UIMin = "0", UIMax = "9", DisplayName = "Fade range"))
	float LPVFadeRange;

	/** LPV Directional Occlusion Fade range - increase to fade more gradually towards the LPV edges.*/
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Light Propagation Volume", AdvancedDisplay, meta = (editcondition = "bOverride_LPVDirectionalOcclusionFadeRange", UIMin = "0", UIMax = "9", DisplayName = "DO Fade range"))
	float LPVDirectionalOcclusionFadeRange;

	/**
	* To render with lower or high resolution than it is presented,
	* controlled by console variable,
	* 100:off, needs to be <99 to get upsampling and lower to get performance,
	* >100 for super sampling (slower but higher quality),
	* only applied in game
	*/
	UPROPERTY(interp, BlueprintReadWrite, Category="Rendering Features|Misc", meta=(ClampMin = "0.0", ClampMax = "400.0", editcondition = "bOverride_ScreenPercentage"))
	float ScreenPercentage;

	// NVCHANGE_BEGIN: Add VXGI
	/** To toggle VXGI Diffuse Tracing (adds fully-dynamic diffuse indirect lighting to the direct lighting) */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_VxgiDiffuseTracingEnabled"), DisplayName = "Enable Diffuse Tracing")
	uint32 VxgiDiffuseTracingEnabled : 1;

	/** Intensity multiplier for the diffuse component. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiDiffuseTracingIntensity"), DisplayName = "Indirect Lighting Intensity")
	float VxgiDiffuseTracingIntensity;

	/** Intensity multiplier for multi-bounce tracing. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", ClampMax = "2.0", editcondition = "bOverride_VxgiMultiBounceIrradianceScale"), DisplayName = "Multi-Bounce Irradiance Scale")
	float VxgiMultiBounceIrradianceScale;

	/** Number of diffuse cones to trace for each fragment, 4 or more. Balances Quality (more cones) vs Performance. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "1", ClampMax = "128", editcondition = "bOverride_VxgiDiffuseTracingNumCones"), DisplayName = "Number of Cones")
	int32 VxgiDiffuseTracingNumCones;

	/** Automatic diffuse angle computation based on the number of cones. Overrides the value set in DiffuseConeAngle. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_bVxgiDiffuseTracingAutoAngle"), DisplayName = "Auto Cone Angle")
	uint32 bVxgiDiffuseTracingAutoAngle : 1;

	/** Tracing sparsity. 1 = dense tracing, 2 or 3 = sparse tracing. Using sparse tracing greatly improves performance in exchange for fine detail quality. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "1", ClampMax = "4", editcondition = "bOverride_VxgiDiffuseTracingSparsity"), DisplayName = "Tracing Sparsity")
	int32 VxgiDiffuseTracingSparsity;

	/** Cone angle for GI diffuse component evaluation. This value has no effect if autoDiffuseAngle == true. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "1", ClampMax = "60", editcondition = "bOverride_VxgiDiffuseTracingConeAngle"), DisplayName = "Cone Angle")
	float VxgiDiffuseTracingConeAngle;

	/** Random per-pixel rotation of the diffuse cone set - it helps reduce banding but costs some performance. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_bVxgiDiffuseTracingConeRotation"), DisplayName = "Cone Rotation")
	uint32 bVxgiDiffuseTracingConeRotation : 1;

	/** Enables a second tracing pass to fill holes in the sparse diffuse tracing results */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_bVxgiDiffuseTracingRefinementEnabled"), DisplayName = "Refine Sparse Tracing")
	uint32 bVxgiDiffuseTracingRefinementEnabled : 1;

	/** Enables a second tracing pass to fill holes in the sparse diffuse tracing results */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_bVxgiDiffuseTracingFlipOpacityDirections"), DisplayName = "Flip Opacity Directions")
	uint32 bVxgiDiffuseTracingFlipOpacityDirections : 1;

	/** Random per-pixel adjustment of initial tracing offsets for diffuse tracing, also helps reduce banding. This flag is only effective if enableConeRotation == true. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_bVxgiDiffuseTracingRandomConeOffsets"), DisplayName = "Random Cone Offsets")
	uint32 bVxgiDiffuseTracingRandomConeOffsets : 1;
	/** Maximum number of samples that can be fetched for each diffuse cone. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "16", ClampMax = "1024", editcondition = "bOverride_VxgiDiffuseTracingMaxSamples"), DisplayName = "Max Sample Count")
	int32 VxgiDiffuseTracingMaxSamples;

	/** Tracing step for diffuse component. Reasonable values [0.5, 1]. Sampling with lower step produces more stable results at Performance cost. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.01", ClampMax = "2.0", editcondition = "bOverride_VxgiDiffuseTracingStep"), DisplayName = "Tracing Step")
	float VxgiDiffuseTracingStep;

	/** Opacity correction factor for diffuse component. Reasonable values [0.1, 10]. Higher values produce more contrast rendering, overall picture looks darker. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.01", ClampMax = "10.0", editcondition = "bOverride_VxgiDiffuseTracingOpacityCorrectionFactor"), DisplayName = "Opacity Correction Factor")
	float VxgiDiffuseTracingOpacityCorrectionFactor;

	/** A factor that controls linear interpolation between smoothNormal and ray direction. Accepted values are [0, 1]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_VxgiDiffuseTracingNormalOffsetFactor"), DisplayName = "Cone Offset Along Normal")
	float VxgiDiffuseTracingNormalOffsetFactor;

	/** Bigger factor would move the diffuse cones closer to the surface normal. Reasonable values in [0, 1]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0", ClampMax = "1", editcondition = "bOverride_VxgiDiffuseTracingConeNormalGroupingFactor"), DisplayName = "Cone Grouping Around Normal")
	float VxgiDiffuseTracingConeNormalGroupingFactor;

	/** Environment map to use for diffuse lighting of non-occluded surfaces. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_VxgiDiffuseTracingEnvironmentMap"), DisplayName = "Environment Map")
	class UTextureCube* VxgiDiffuseTracingEnvironmentMap;

	/** Multiplier for environment map lighting in the diffuse channel. The environment map is multiplied by diffuse cones' final transmittance factors. Default value = 0.0. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_VxgiDiffuseTracingEnvironmentMapTint", HideAlphaChannel), DisplayName = "Environment Map Tint")
	FLinearColor VxgiDiffuseTracingEnvironmentMapTint;

	/** Uniform bias to reduce false occlusion for diffuse tracing. Reasonable values in [1.0, 4.0]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiDiffuseTracingInitialOffsetBias"), DisplayName = "Initial Offset Bias")
	float VxgiDiffuseTracingInitialOffsetBias;

	/** Bias factor to reduce false occlusion for diffuse tracing linearly with distance. Reasonable values in [1.0, 4.0]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiDiffuseTracingInitialOffsetDistanceFactor"), DisplayName = "Initial Offset Distance Factor")
	float VxgiDiffuseTracingInitialOffsetDistanceFactor;

	/** Enables reuse of diffuse tracing results from the previous frame, to reduce any flickering artifacts. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (editcondition = "bOverride_bVxgiDiffuseTracingTemporalReprojectionEnabled"), DisplayName = "Use Temporal Filtering")
	uint32 bVxgiDiffuseTracingTemporalReprojectionEnabled : 1;

	/**
	* Weight of the reprojected irradiance data relative to newly computed data, Reasonable values in [0.5, 0.9].
	* where 0 means do not use reprojection, and values closer to 1 mean accumulate data over more previous frames.
	*/
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_VxgiDiffuseTracingTemporalReprojectionPreviousFrameWeight"), DisplayName = "Temporal Filter Previous Frame Weight")
	float VxgiDiffuseTracingTemporalReprojectionPreviousFrameWeight;

	/** Maximum distance between two samples for which they're still considered to be the same surface, expressed in voxels. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiDiffuseTracingTemporalReprojectionMaxDistanceInVoxels"), DisplayName = "Temporal Filter Max Surface Distance")
	float VxgiDiffuseTracingTemporalReprojectionMaxDistanceInVoxels;

	/** The exponent used for the dot product of old and new normals in the temporal reprojection filter. Set to 0.0 to disable this weight factor (default). */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Diffuse", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiDiffuseTracingTemporalReprojectionNormalWeightExponent"), DisplayName = "Temporal Filter Normal Difference Exponent")
	float VxgiDiffuseTracingTemporalReprojectionNormalWeightExponent;

	/** Optional color for adding occluded directional ambient lighting to diffuse tracing results. Does not apply in VXAO mode. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (editcondition = "bOverride_VxgiAmbientColor"), DisplayName = "Diffuse Color")
	FLinearColor VxgiAmbientColor;

	/** World-space distance at which the contribution of geometry to ambient occlusion will be 10x smaller than near the surface. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiAmbientRange"), DisplayName = "Range")
	float VxgiAmbientRange;

	/** Multiplier for VXAO ambient term, applied before gamma correction. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (ClampMin = "0.0", ClampMax = "10.0", editcondition = "bOverride_VxgiAmbientScale"), DisplayName = "Scale")
	float VxgiAmbientScale;

	/** Bias for VXAO ambient term, applied before gamma correction. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_VxgiAmbientBias"), DisplayName = "Bias")
	float VxgiAmbientBias;

	/** Gamma correction factor for VXAO ambient term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (ClampMin = "0.01", ClampMax = "10.0", editcondition = "bOverride_VxgiAmbientPowerExponent"), DisplayName = "Power Exponent")
	float VxgiAmbientPowerExponent;

	/** Controls how much darker to make ambient occlusion at distance. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_VxgiAmbientDistanceDarkening"), DisplayName = "Distance Darkening")
	float VxgiAmbientDistanceDarkening;

	/** Intensity for mixing VXAO effect on top of engine SSAO effect. You can set this to 0 and use the "VXGI Diffuse" channel in a post-process material instead. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Ambient", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_VxgiAmbientMixIntensity"), DisplayName = "Mix Intensity")
	float VxgiAmbientMixIntensity;

	/** To toggle VXGI Specular Tracing (replaces any Reflection Environment or SSR with VXGI Specular Tracing) */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (editcondition = "bOverride_VxgiSpecularTracingEnabled"), DisplayName = "Enable Specular Tracing")
	uint32 VxgiSpecularTracingEnabled : 1;

	/** Intensity multiplier for the specular component. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (editcondition = "bOverride_VxgiSpecularTracingIntensity"), DisplayName = "Indirect Lighting Intensity")
	float VxgiSpecularTracingIntensity;

	/** Maximum number of samples that can be fetched for each specular cone. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (ClampMin = "16", ClampMax = "1024", editcondition = "bOverride_VxgiSpecularTracingMaxSamples"), DisplayName = "Max Sample Count")
	int32 VxgiSpecularTracingMaxSamples;

	/** Tracing step for specular component. Reasonable values [0.5, 1]. Sampling with lower step produces more stable results at Performance cost. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (ClampMin = "0.01", ClampMax = "2.0", editcondition = "bOverride_VxgiSpecularTracingTracingStep"), DisplayName = "Tracing Step")
	float VxgiSpecularTracingTracingStep;

	/** Opacity correction factor for specular component. Reasonable values [0.1, 10]. Higher values prevent specular cones from tracing through solid objects. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (ClampMin = "0.01", ClampMax = "10.0", editcondition = "bOverride_VxgiSpecularTracingOpacityCorrectionFactor"), DisplayName = "Opacity Correction Factor")
	float VxgiSpecularTracingOpacityCorrectionFactor;

	/** Uniform bias to avoid false occlusion for specular tracing. Reasonable values in [1.0, 4.0]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiSpecularTracingInitialOffsetBias"), DisplayName = "Initial Offset Bias")
	float VxgiSpecularTracingInitialOffsetBias;

	/** Bias factor to reduce false occlusion for specular tracing linearly with distance. Reasonable values in [1.0, 4.0]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (ClampMin = "0.0", editcondition = "bOverride_VxgiSpecularTracingInitialOffsetDistanceFactor"), DisplayName = "Initial Offset Distance Factor")
	float VxgiSpecularTracingInitialOffsetDistanceFactor;

	/** Enable simple filtering on the specular surface after tracing in order to reduce noise introduced by cone jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VXGI Specular", meta = (editcondition = "bOverride_VxgiSpecularTracingFilter"), DisplayName = "Filter")
	TEnumAsByte<enum EVxgiSpecularTracingFilter> VxgiSpecularTracingFilter;

	/** Environment map to use when specular cones don't hit any geometry. Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VXGI Specular", meta = (editcondition = "bOverride_VxgiSpecularTracingEnvironmentMap"), DisplayName = "Environment Map")
	class UTextureCube* VxgiSpecularTracingEnvironmentMap;

	/** Multiplier for environment map reflections in the specular channel. The environment map will only be visible on pixels that do not reflect any solid geometry. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (editcondition = "bOverride_VxgiSpecularTracingEnvironmentMapTint", HideAlphaChannel), DisplayName = "Environment Map Tint")
	FLinearColor VxgiSpecularTracingEnvironmentMapTint;

	/** [Experimental] Scale of the jitter that can be added to specular sample positions to reduce blockiness of the reflections, in the range [0,1]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "VXGI Specular", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_VxgiSpecularTracingTangentJitterScale"), DisplayName = "Sample Position Jitter")
	float VxgiSpecularTracingTangentJitterScale;
	// NVCHANGE_END: Add VXGI

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	/** Absorpsive component of the medium. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (HideAlphaChannel, editcondition = "bOverride_AbsorptionColor"))
		FLinearColor AbsorptionColor;

	/** Transmittance for absorpsive component. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_AbsorptionTransmittance"))
		float AbsorptionTransmittance;

	/** Rayleigh term. Rayleigh color is locked as [5.8f, 13.6f, 33.1f]. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_RayleighTransmittance"))
		float RayleighTransmittance;

	/** No Mie effect (0) to a Mie-Hazy effect (0.5) to a fully Mie-Murky effect (1). */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_MieBlendFactor"))
		float MieBlendFactor;

	/** Color distribution for Mie term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (HideAlphaChannel, editcondition = "bOverride_MieColor"))
		FLinearColor MieColor;

	/** Transmittance for Mie term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_MieTransmittance"))
		float MieTransmittance;

	/** Color distribution for Henyey-Greenstein term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (HideAlphaChannel, editcondition = "bOverride_HGColor"))
		FLinearColor HGColor;

	/** Transmittance for Henyey-Greenstein term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_HGTransmittance"))
		float HGTransmittance;

	/** Eccentricity for the first Henyey-Greenstein term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "-1.0", ClampMax = "1.0", editcondition = "bOverride_HGEccentricity1"))
		float HGEccentricity1;

	/** Eccentricity for the second Henyey-Greenstein term. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "-1.0", ClampMax = "1.0", editcondition = "bOverride_HGEccentricity2"))
		float HGEccentricity2;

	/** the ratio of the optical thickness that each term represents
	* (where 0 would mean it's all applied to the first HG term, 1 meaning it's all applied to the second term, and 0.5 meaning it's split evenly between the two).
	*/
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_HGEccentricityRatio"))
		float HGEccentricityRatio;

	/** Color distribution for Isotropic scattering. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (HideAlphaChannel, editcondition = "bOverride_IsotropicColor"))
		FLinearColor IsotropicColor;

	/** Transmittance for Isotropic scattering. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_IsotropicTransmittance"))
		float IsotropicTransmittance;

	/** Fog mode based on the scattering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (editcondition = "bOverride_FogMode"))
		TEnumAsByte<EFogMode::Type> FogMode;

	/** Brightness multiplier of the fog. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (UIMin = "0.0", UIMax = "100000.0", editcondition = "bOverride_FogIntensity"))
		float FogIntensity;

	/** Filter color of the fog. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (HideAlphaChannel, editcondition = "bOverride_FogColor"))
		FLinearColor FogColor;

	/** Transmittance for the fog. */
	UPROPERTY(interp, BlueprintReadWrite, Category = "Rendering Features|Nvidia Volumetric Lighting", meta = (ClampMin = "0.0", ClampMax = "1.0", editcondition = "bOverride_FogTransmittance"))
		float FogTransmittance;
	// NVCHANGE_END: Nvidia Volumetric Lighting

	// Note: Adding properties before this line require also changes to the OverridePostProcessSettings() function and 
	// FPostProcessSettings constructor and possibly the SetBaseValues() method.
	// -----------------------------------------------------------------------
	
	/**
	 * Allows custom post process materials to be defined, using a MaterialInstance with the same Material as its parent to allow blending.
	 * For materials this needs to be the "PostProcess" domain type. This can be used for any UObject object implementing the IBlendableInterface (e.g. could be used to fade weather settings).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rendering Features", meta=( Keywords="PostProcess", DisplayName = "Post Process Materials" ))
	FWeightedBlendables WeightedBlendables;

	// for backwards compatibility
	UPROPERTY()
	TArray<UObject*> Blendables_DEPRECATED;

	// for backwards compatibility
	void OnAfterLoad()
	{
		for(int32 i = 0, count = Blendables_DEPRECATED.Num(); i < count; ++i)
		{
			if(Blendables_DEPRECATED[i])
			{
				FWeightedBlendable Element(1.0f, Blendables_DEPRECATED[i]);
				WeightedBlendables.Array.Add(Element);
			}
		}
		Blendables_DEPRECATED.Empty();

		if (bOverride_BloomConvolutionPreFilter_DEPRECATED)
		{
			bOverride_BloomConvolutionPreFilterMin  = bOverride_BloomConvolutionPreFilter_DEPRECATED;
			bOverride_BloomConvolutionPreFilterMax  = bOverride_BloomConvolutionPreFilter_DEPRECATED;
			bOverride_BloomConvolutionPreFilterMult = bOverride_BloomConvolutionPreFilter_DEPRECATED;
		}
		if (BloomConvolutionPreFilter_DEPRECATED.X > -1.f)
		{
			BloomConvolutionPreFilterMin = BloomConvolutionPreFilter_DEPRECATED.X;
			BloomConvolutionPreFilterMax = BloomConvolutionPreFilter_DEPRECATED.Y;
			BloomConvolutionPreFilterMult = BloomConvolutionPreFilter_DEPRECATED.Z;
		}
	}

	// Adds an Blendable (implements IBlendableInterface) to the array of Blendables (if it doesn't exist) and update the weight
	// @param InBlendableObject silently ignores if no object is referenced
	// @param 0..1 InWeight, values outside of the range get clampled later in the pipeline
	void AddBlendable(TScriptInterface<IBlendableInterface> InBlendableObject, float InWeight)
	{
		// update weight, if the Blendable is already in the array
		if(UObject* Object = InBlendableObject.GetObject())
		{
			for (int32 i = 0, count = WeightedBlendables.Array.Num(); i < count; ++i)
			{
				if (WeightedBlendables.Array[i].Object == Object)
				{
					WeightedBlendables.Array[i].Weight = InWeight;
					// We assumes we only have one
					return;
				}
			}

			// add in the end
			WeightedBlendables.Array.Add(FWeightedBlendable(InWeight, Object));
		}
	}

	// removes one or multiple blendables from the array
	void RemoveBlendable(TScriptInterface<IBlendableInterface> InBlendableObject)
	{
		if(UObject* Object = InBlendableObject.GetObject())
		{
			for (int32 i = 0, count = WeightedBlendables.Array.Num(); i < count; ++i)
			{
				if (WeightedBlendables.Array[i].Object == Object)
				{
					// this might remove multiple
					WeightedBlendables.Array.RemoveAt(i);
					--i;
					--count;
				}
			}
		}
	}

	// good start values for a new volume, by default no value is overriding
	ENGINE_API FPostProcessSettings();

	/**
		* Used to define the values before any override happens.
		* Should be as neutral as possible.
		*/		
	void SetBaseValues()
	{
		*this = FPostProcessSettings();

		AmbientCubemapIntensity = 0.0f;
		ColorGradingIntensity = 0.0f;
	}
};

UCLASS()
class UScene : public UObject
{
	GENERATED_UCLASS_BODY()


	/** bits needed to store DPG value */
	#define SDPG_NumBits	3
};
