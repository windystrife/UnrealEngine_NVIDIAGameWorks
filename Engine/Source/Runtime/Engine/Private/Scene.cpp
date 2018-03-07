// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/Scene.h"


void FColorGradingSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_ColorSaturation = true;
	OutPostProcessSettings->bOverride_ColorContrast = true;
	OutPostProcessSettings->bOverride_ColorGamma = true;
	OutPostProcessSettings->bOverride_ColorGain = true;
	OutPostProcessSettings->bOverride_ColorOffset = true;

	OutPostProcessSettings->bOverride_ColorSaturationShadows = true;
	OutPostProcessSettings->bOverride_ColorContrastShadows = true;
	OutPostProcessSettings->bOverride_ColorGammaShadows = true;
	OutPostProcessSettings->bOverride_ColorGainShadows = true;
	OutPostProcessSettings->bOverride_ColorOffsetShadows = true;

	OutPostProcessSettings->bOverride_ColorSaturationMidtones = true;
	OutPostProcessSettings->bOverride_ColorContrastMidtones = true;
	OutPostProcessSettings->bOverride_ColorGammaMidtones = true;
	OutPostProcessSettings->bOverride_ColorGainMidtones = true;
	OutPostProcessSettings->bOverride_ColorOffsetMidtones = true;

	OutPostProcessSettings->bOverride_ColorSaturationHighlights = true;
	OutPostProcessSettings->bOverride_ColorContrastHighlights = true;
	OutPostProcessSettings->bOverride_ColorGammaHighlights = true;
	OutPostProcessSettings->bOverride_ColorGainHighlights = true;
	OutPostProcessSettings->bOverride_ColorOffsetHighlights = true;

	OutPostProcessSettings->bOverride_ColorCorrectionShadowsMax = true;
	OutPostProcessSettings->bOverride_ColorCorrectionHighlightsMin = true;

	OutPostProcessSettings->ColorSaturation = Global.Saturation;
	OutPostProcessSettings->ColorContrast = Global.Contrast;
	OutPostProcessSettings->ColorGamma = Global.Gamma;
	OutPostProcessSettings->ColorGain = Global.Gain;
	OutPostProcessSettings->ColorOffset = Global.Offset;

	OutPostProcessSettings->ColorSaturationShadows = Shadows.Saturation;
	OutPostProcessSettings->ColorContrastShadows = Shadows.Contrast;
	OutPostProcessSettings->ColorGammaShadows = Shadows.Gamma;
	OutPostProcessSettings->ColorGainShadows = Shadows.Gain;
	OutPostProcessSettings->ColorOffsetShadows = Shadows.Offset;

	OutPostProcessSettings->ColorSaturationMidtones = Midtones.Saturation;
	OutPostProcessSettings->ColorContrastMidtones = Midtones.Contrast;
	OutPostProcessSettings->ColorGammaMidtones = Midtones.Gamma;
	OutPostProcessSettings->ColorGainMidtones = Midtones.Gain;
	OutPostProcessSettings->ColorOffsetMidtones = Midtones.Offset;

	OutPostProcessSettings->ColorSaturationHighlights = Highlights.Saturation;
	OutPostProcessSettings->ColorContrastHighlights = Highlights.Contrast;
	OutPostProcessSettings->ColorGammaHighlights = Highlights.Gamma;
	OutPostProcessSettings->ColorGainHighlights = Highlights.Gain;
	OutPostProcessSettings->ColorOffsetHighlights = Highlights.Offset;

	OutPostProcessSettings->ColorCorrectionShadowsMax = ShadowsMax;
	OutPostProcessSettings->ColorCorrectionHighlightsMin = HighlightsMin;
}

void FFilmStockSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_FilmSlope = true;
	OutPostProcessSettings->bOverride_FilmToe = true;
	OutPostProcessSettings->bOverride_FilmShoulder = true;
	OutPostProcessSettings->bOverride_FilmBlackClip = true;
	OutPostProcessSettings->bOverride_FilmWhiteClip = true;

	OutPostProcessSettings->FilmSlope = Slope;
	OutPostProcessSettings->FilmToe = Toe;
	OutPostProcessSettings->FilmShoulder = Shoulder;
	OutPostProcessSettings->FilmBlackClip = BlackClip;
	OutPostProcessSettings->FilmWhiteClip = WhiteClip;
}

void FGaussianSumBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomIntensity = true;
	OutPostProcessSettings->bOverride_BloomThreshold = true;
	OutPostProcessSettings->bOverride_BloomSizeScale = true;
	OutPostProcessSettings->bOverride_Bloom1Tint = true;
	OutPostProcessSettings->bOverride_Bloom1Size = true;
	OutPostProcessSettings->bOverride_Bloom2Tint = true;
	OutPostProcessSettings->bOverride_Bloom2Size = true;
	OutPostProcessSettings->bOverride_Bloom3Tint = true;
	OutPostProcessSettings->bOverride_Bloom3Size = true;
	OutPostProcessSettings->bOverride_Bloom4Tint = true;
	OutPostProcessSettings->bOverride_Bloom4Size = true;
	OutPostProcessSettings->bOverride_Bloom5Tint = true;
	OutPostProcessSettings->bOverride_Bloom5Size = true;
	OutPostProcessSettings->bOverride_Bloom6Tint = true;
	OutPostProcessSettings->bOverride_Bloom6Size = true;

	OutPostProcessSettings->BloomIntensity = Intensity;
	OutPostProcessSettings->BloomThreshold = Threshold;
	OutPostProcessSettings->BloomSizeScale = SizeScale;
	OutPostProcessSettings->Bloom1Tint = Filter1Tint;
	OutPostProcessSettings->Bloom1Size = Filter1Size;
	OutPostProcessSettings->Bloom2Tint = Filter2Tint;
	OutPostProcessSettings->Bloom2Size = Filter2Size;
	OutPostProcessSettings->Bloom3Tint = Filter3Tint;
	OutPostProcessSettings->Bloom3Size = Filter3Size;
	OutPostProcessSettings->Bloom4Tint = Filter4Tint;
	OutPostProcessSettings->Bloom4Size = Filter4Size;
	OutPostProcessSettings->Bloom5Tint = Filter5Tint;
	OutPostProcessSettings->Bloom5Size = Filter5Size;
	OutPostProcessSettings->Bloom6Tint = Filter6Tint;
	OutPostProcessSettings->Bloom6Size = Filter6Size;
}

void FConvolutionBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomConvolutionTexture = true;
	OutPostProcessSettings->bOverride_BloomConvolutionSize = true;
	OutPostProcessSettings->bOverride_BloomConvolutionCenterUV = true;
	OutPostProcessSettings->bOverride_BloomConvolutionPreFilterMin = true;
	OutPostProcessSettings->bOverride_BloomConvolutionPreFilterMax = true;
	OutPostProcessSettings->bOverride_BloomConvolutionPreFilterMult = true;
	OutPostProcessSettings->bOverride_BloomConvolutionBufferScale = true;

	OutPostProcessSettings->BloomConvolutionTexture = Texture;
	OutPostProcessSettings->BloomConvolutionSize = Size;
	OutPostProcessSettings->BloomConvolutionCenterUV = CenterUV;
	OutPostProcessSettings->BloomConvolutionPreFilterMin = PreFilterMin;
	OutPostProcessSettings->BloomConvolutionPreFilterMax = PreFilterMax;
	OutPostProcessSettings->BloomConvolutionPreFilterMult = PreFilterMult;
	OutPostProcessSettings->BloomConvolutionBufferScale = BufferScale;
}

void FLensBloomSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	GaussianSum.ExportToPostProcessSettings(OutPostProcessSettings);
	Convolution.ExportToPostProcessSettings(OutPostProcessSettings);

	OutPostProcessSettings->bOverride_BloomMethod = true;
	OutPostProcessSettings->BloomMethod = Method;
}

void FLensImperfectionSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_BloomDirtMask = true;
	OutPostProcessSettings->bOverride_BloomDirtMaskIntensity = true;
	OutPostProcessSettings->bOverride_BloomDirtMaskTint = true;

	OutPostProcessSettings->BloomDirtMask = DirtMask;
	OutPostProcessSettings->BloomDirtMaskIntensity = DirtMaskIntensity;
	OutPostProcessSettings->BloomDirtMaskTint = DirtMaskTint;
}

void FLensSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	Bloom.ExportToPostProcessSettings(OutPostProcessSettings);
	Imperfections.ExportToPostProcessSettings(OutPostProcessSettings);

	OutPostProcessSettings->bOverride_SceneFringeIntensity = true;
	OutPostProcessSettings->SceneFringeIntensity = ChromaticAberration;
}

void FCameraExposureSettings::ExportToPostProcessSettings(FPostProcessSettings* OutPostProcessSettings) const
{
	OutPostProcessSettings->bOverride_AutoExposureMethod = true;
	OutPostProcessSettings->bOverride_AutoExposureLowPercent = true;
	OutPostProcessSettings->bOverride_AutoExposureHighPercent = true;
	OutPostProcessSettings->bOverride_AutoExposureMinBrightness = true;
	OutPostProcessSettings->bOverride_AutoExposureMaxBrightness = true;
	OutPostProcessSettings->bOverride_AutoExposureSpeedUp = true;
	OutPostProcessSettings->bOverride_AutoExposureSpeedDown = true;
	OutPostProcessSettings->bOverride_AutoExposureBias = true;
	OutPostProcessSettings->bOverride_HistogramLogMin = true;
	OutPostProcessSettings->bOverride_HistogramLogMax = true;

	OutPostProcessSettings->AutoExposureLowPercent = LowPercent;
	OutPostProcessSettings->AutoExposureHighPercent = HighPercent;
	OutPostProcessSettings->AutoExposureMinBrightness = MinBrightness;
	OutPostProcessSettings->AutoExposureMaxBrightness = MaxBrightness;
	OutPostProcessSettings->AutoExposureSpeedUp = SpeedUp;
	OutPostProcessSettings->AutoExposureSpeedDown = SpeedDown;
	OutPostProcessSettings->AutoExposureBias = Bias;
	OutPostProcessSettings->HistogramLogMin = HistogramLogMin;
	OutPostProcessSettings->HistogramLogMax = HistogramLogMax;
}


// Check there is no divergence between FPostProcessSettings and the smaller settings structures.
#if DO_CHECK && WITH_EDITOR

static void VerifyPostProcessingProperties(
	const FString& PropertyPrefix,
	const TArray<const UStruct*>& NewStructs,
	const TMap<FString, FString>& RenameMap)
{
	const UStruct* LegacyStruct = FPostProcessSettings::StaticStruct();

	TMap<FString, const UProperty*> NewPropertySet;

	// Walk new struct and build list of property name.
	for (const UStruct* NewStruct : NewStructs)
	{
		for (UProperty* Property = NewStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			// Make sure there is no duplicate.
			check(!NewPropertySet.Contains(Property->GetNameCPP()));
			NewPropertySet.Add(Property->GetNameCPP(), Property);
		}
	}

	// Walk FPostProcessSettings.
	for (UProperty* Property = LegacyStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		if (!Property->GetNameCPP().StartsWith(PropertyPrefix))
		{
			continue;
		}

		FString OldPropertyName = Property->GetName();
		FString NewPropertyName = OldPropertyName.Mid(PropertyPrefix.Len());

		if (RenameMap.Contains(Property->GetNameCPP()))
		{
			if (RenameMap.FindChecked(Property->GetNameCPP()) == TEXT(""))
			{
				// This property is part of deprecated feature (such as legacy tonemapper).
				check(!NewPropertySet.Contains(NewPropertyName));
				continue;
			}

			NewPropertyName = RenameMap.FindChecked(Property->GetNameCPP());
		}

		if (Property->GetNameCPP().EndsWith(TEXT("_DEPRECATED")))
		{
			check(!NewPropertySet.Contains(NewPropertyName));
		}
		else
		{
			check(Property->SameType(NewPropertySet.FindChecked(NewPropertyName)));
		}
	}
}

static void DoPostProcessSettingsSanityCheck()
{
	{
		TMap<FString, FString> RenameMap;
		RenameMap.Add(TEXT("Bloom1Size"), TEXT("Filter1Size"));
		RenameMap.Add(TEXT("Bloom2Size"), TEXT("Filter2Size"));
		RenameMap.Add(TEXT("Bloom3Size"), TEXT("Filter3Size"));
		RenameMap.Add(TEXT("Bloom4Size"), TEXT("Filter4Size"));
		RenameMap.Add(TEXT("Bloom5Size"), TEXT("Filter5Size"));
		RenameMap.Add(TEXT("Bloom6Size"), TEXT("Filter6Size"));
		RenameMap.Add(TEXT("Bloom1Tint"), TEXT("Filter1Tint"));
		RenameMap.Add(TEXT("Bloom2Tint"), TEXT("Filter2Tint"));
		RenameMap.Add(TEXT("Bloom3Tint"), TEXT("Filter3Tint"));
		RenameMap.Add(TEXT("Bloom4Tint"), TEXT("Filter4Tint"));
		RenameMap.Add(TEXT("Bloom5Tint"), TEXT("Filter5Tint"));
		RenameMap.Add(TEXT("Bloom6Tint"), TEXT("Filter6Tint"));

		RenameMap.Add(TEXT("BloomConvolutionTexture"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionSize"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionCenterUV"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionPreFilterMin"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionPreFilterMax"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionPreFilterMult"), TEXT(""));
		RenameMap.Add(TEXT("BloomConvolutionBufferScale"), TEXT(""));

		VerifyPostProcessingProperties(TEXT("Bloom"),
			TArray<const UStruct*>({
				FGaussianSumBloomSettings::StaticStruct(),
				FLensBloomSettings::StaticStruct(),
				FLensImperfectionSettings::StaticStruct()}),
			RenameMap);
	}
	
	{
		TMap<FString, FString> RenameMap;
		VerifyPostProcessingProperties(TEXT("BloomConvolution"),
			TArray<const UStruct*>({FConvolutionBloomSettings::StaticStruct()}),
			RenameMap);
	}

	{
		TMap<FString, FString> RenameMap;
		VerifyPostProcessingProperties(TEXT("AutoExposure"),
			TArray<const UStruct*>({
				FCameraExposureSettings::StaticStruct()}),
			RenameMap);
	}

	{
		TMap<FString, FString> RenameMap;
		// Old tonemapper parameters are ignored.
		RenameMap.Add(TEXT("FilmWhitePoint"), TEXT(""));
		RenameMap.Add(TEXT("FilmSaturation"), TEXT(""));
		RenameMap.Add(TEXT("FilmChannelMixerRed"), TEXT(""));
		RenameMap.Add(TEXT("FilmChannelMixerGreen"), TEXT(""));
		RenameMap.Add(TEXT("FilmChannelMixerBlue"), TEXT(""));
		RenameMap.Add(TEXT("FilmContrast"), TEXT(""));
		RenameMap.Add(TEXT("FilmDynamicRange"), TEXT(""));
		RenameMap.Add(TEXT("FilmHealAmount"), TEXT(""));
		RenameMap.Add(TEXT("FilmToeAmount"), TEXT(""));
		RenameMap.Add(TEXT("FilmShadowTint"), TEXT(""));
		RenameMap.Add(TEXT("FilmShadowTintBlend"), TEXT(""));
		RenameMap.Add(TEXT("FilmShadowTintAmount"), TEXT(""));
		VerifyPostProcessingProperties(TEXT("Film"),
			TArray<const UStruct*>({FFilmStockSettings::StaticStruct()}),
			RenameMap);
	}
}

#endif // DO_CHECK


FPostProcessSettings::FPostProcessSettings()
{
	// to set all bOverride_.. by default to false
	FMemory::Memzero(this, sizeof(FPostProcessSettings));

	WhiteTemp = 6500.0f;
	WhiteTint = 0.0f;

	// Color Correction controls
	ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGain = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	ColorSaturationShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrastShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGammaShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGainShadows = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffsetShadows = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	ColorSaturationMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrastMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGammaMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGainMidtones = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffsetMidtones = FVector4(0.f, 0.0f, 0.0f, 0.0f);

	ColorSaturationHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorContrastHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGammaHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorGainHighlights = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
	ColorOffsetHighlights = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

	ColorCorrectionShadowsMax = 0.09f;
	ColorCorrectionHighlightsMin = 0.5f;

	// default values:
	FilmWhitePoint = FLinearColor(1.0f, 1.0f, 1.0f);
	FilmSaturation = 1.0f;
	FilmChannelMixerRed = FLinearColor(1.0f, 0.0f, 0.0f);
	FilmChannelMixerGreen = FLinearColor(0.0f, 1.0f, 0.0f);
	FilmChannelMixerBlue = FLinearColor(0.0f, 0.0f, 1.0f);
	FilmContrast = 0.03f;
	FilmDynamicRange = 4.0f;
	FilmHealAmount = 1.0f;
	FilmToeAmount = 1.0f;
	FilmShadowTint = FLinearColor(1.0f, 1.0f, 1.0f);
	FilmShadowTintBlend = 0.5;
	FilmShadowTintAmount = 0.0;

	// ACES settings
	FilmSlope = 0.88f;
	FilmToe = 0.55f;
	FilmShoulder = 0.26f;
	FilmBlackClip = 0.0f;
	FilmWhiteClip = 0.04f;

	SceneColorTint = FLinearColor(1, 1, 1);
	SceneFringeIntensity = 0.0f;
	BloomMethod = BM_SOG;
	// next value might get overwritten by r.DefaultFeature.Bloom
	BloomIntensity = 0.675f;
	BloomThreshold = -1.0f;
	// default is 4 to maintain old settings after fixing something that caused a factor of 4
	BloomSizeScale = 4.0;
	Bloom1Tint = FLinearColor(0.3465f, 0.3465f, 0.3465f);
	Bloom1Size = 0.3f;
	Bloom2Tint = FLinearColor(0.138f, 0.138f, 0.138f);
	Bloom2Size = 1.0f;
	Bloom3Tint = FLinearColor(0.1176f, 0.1176f, 0.1176f);
	Bloom3Size = 2.0f;
	Bloom4Tint = FLinearColor(0.066f, 0.066f, 0.066f);
	Bloom4Size = 10.0f;
	Bloom5Tint = FLinearColor(0.066f, 0.066f, 0.066f);
	Bloom5Size = 30.0f;
	Bloom6Tint = FLinearColor(0.061f, 0.061f, 0.061f);
	Bloom6Size = 64.0f;
	BloomConvolutionSize = 1.f;
	BloomConvolutionCenterUV = FVector2D(0.5f, 0.5f);
	BloomConvolutionPreFilter_DEPRECATED = FVector(-1.f, -1.f, -1.f);
	BloomConvolutionPreFilterMin = 7.f;
	BloomConvolutionPreFilterMax = 15000.f;
	BloomConvolutionPreFilterMult = 15.f;
	BloomConvolutionBufferScale = 0.133f;
	BloomDirtMaskIntensity = 0.0f;
	BloomDirtMaskTint = FLinearColor(0.5f, 0.5f, 0.5f);
	AmbientCubemapIntensity = 1.0f;
	AmbientCubemapTint = FLinearColor(1, 1, 1);
	LPVIntensity = 1.0f;
	LPVSize = 5312.0f;
	LPVSecondaryOcclusionIntensity = 0.0f;
	LPVSecondaryBounceIntensity = 0.0f;
	LPVVplInjectionBias = 0.64f;
	LPVGeometryVolumeBias = 0.384f;
	LPVEmissiveInjectionIntensity = 1.0f;
	// next value might get overwritten by r.DefaultFeature.AutoExposure.Method
	AutoExposureMethod = AEM_Histogram;
	AutoExposureLowPercent = 80.0f;
	AutoExposureHighPercent = 98.3f;
	// next value might get overwritten by r.DefaultFeature.AutoExposure
	AutoExposureMinBrightness = 0.03f;
	// next value might get overwritten by r.DefaultFeature.AutoExposure
	AutoExposureMaxBrightness = 2.0f;
	AutoExposureBias = 0.0f;
	AutoExposureSpeedUp = 3.0f;
	AutoExposureSpeedDown = 1.0f;
	LPVDirectionalOcclusionIntensity = 0.0f;
	LPVDirectionalOcclusionRadius = 8.0f;
	LPVDiffuseOcclusionExponent = 1.0f;
	LPVSpecularOcclusionExponent = 7.0f;
	LPVDiffuseOcclusionIntensity = 1.0f;
	LPVSpecularOcclusionIntensity = 1.0f;
	LPVFadeRange = 0.0f;
	LPVDirectionalOcclusionFadeRange = 0.0f;
	HistogramLogMin = -8.0f;
	HistogramLogMax = 4.0f;
	// next value might get overwritten by r.DefaultFeature.LensFlare
	LensFlareIntensity = 1.0f;
	LensFlareTint = FLinearColor(1.0f, 1.0f, 1.0f);
	LensFlareBokehSize = 3.0f;
	LensFlareThreshold = 8.0f;
	VignetteIntensity = 0.4f;
	GrainIntensity = 0.0f;
	GrainJitter = 0.0f;
	// next value might get overwritten by r.DefaultFeature.AmbientOcclusion
	AmbientOcclusionIntensity = .5f;
	// next value might get overwritten by r.DefaultFeature.AmbientOcclusionStaticFraction
	AmbientOcclusionStaticFraction = 1.0f;
	AmbientOcclusionRadius = 200.0f;
	AmbientOcclusionDistance_DEPRECATED = 80.0f;
	AmbientOcclusionFadeDistance = 8000.0f;
	AmbientOcclusionFadeRadius = 5000.0f;
	AmbientOcclusionPower = 2.0f;
	AmbientOcclusionBias = 3.0f;
	AmbientOcclusionQuality = 50.0f;
	AmbientOcclusionMipBlend = 0.6f;
	AmbientOcclusionMipScale = 1.7f;
	AmbientOcclusionMipThreshold = 0.01f;
	AmbientOcclusionRadiusInWS = false;
	IndirectLightingColor = FLinearColor(1.0f, 1.0f, 1.0f);
	IndirectLightingIntensity = 1.0f;
	ColorGradingIntensity = 1.0f;
	DepthOfFieldFocalDistance = 1000.0f;
	DepthOfFieldFstop = 4.0f;
	DepthOfFieldSensorWidth = 24.576f;			// APS-C
	DepthOfFieldDepthBlurAmount = 1.0f;
	DepthOfFieldDepthBlurRadius = 0.0f;
	DepthOfFieldFocalRegion = 0.0f;
	DepthOfFieldNearTransitionRegion = 300.0f;
	DepthOfFieldFarTransitionRegion = 500.0f;
	DepthOfFieldScale = 0.0f;
	DepthOfFieldMaxBokehSize = 15.0f;
	DepthOfFieldNearBlurSize = 15.0f;
	DepthOfFieldFarBlurSize = 15.0f;
	DepthOfFieldOcclusion = 0.4f;
	DepthOfFieldColorThreshold = 1.0f;
	DepthOfFieldSizeThreshold = 0.08f;
	DepthOfFieldSkyFocusDistance = 0.0f;
	// 200 should be enough even for extreme aspect ratios to give the default no effect
	DepthOfFieldVignetteSize = 200.0f;
	LensFlareTints[0] = FLinearColor(1.0f, 0.8f, 0.4f, 0.6f);
	LensFlareTints[1] = FLinearColor(1.0f, 1.0f, 0.6f, 0.53f);
	LensFlareTints[2] = FLinearColor(0.8f, 0.8f, 1.0f, 0.46f);
	LensFlareTints[3] = FLinearColor(0.5f, 1.0f, 0.4f, 0.39f);
	LensFlareTints[4] = FLinearColor(0.5f, 0.8f, 1.0f, 0.31f);
	LensFlareTints[5] = FLinearColor(0.9f, 1.0f, 0.8f, 0.27f);
	LensFlareTints[6] = FLinearColor(1.0f, 0.8f, 0.4f, 0.22f);
	LensFlareTints[7] = FLinearColor(0.9f, 0.7f, 0.7f, 0.15f);
	// next value might get overwritten by r.DefaultFeature.MotionBlur
	MotionBlurAmount = 0.5f;
	MotionBlurMax = 5.0f;
	MotionBlurPerObjectSize = 0.5f;
	ScreenPercentage = 100.0f;
	ScreenSpaceReflectionIntensity = 100.0f;
	ScreenSpaceReflectionQuality = 50.0f;
	ScreenSpaceReflectionMaxRoughness = 0.6f;
	bMobileHQGaussian = false;

	// NVCHANGE_BEGIN: Nvidia Volumetric Lighting
	RayleighTransmittance = 1.0f;
	MieBlendFactor = 0.0f;
	MieColor = FLinearColor::Black;
	MieTransmittance = 1.0f;
	AbsorptionColor = FLinearColor::Black;
	AbsorptionTransmittance = 1.0f;
	HGColor = FLinearColor::Black;
	HGTransmittance = 1.0f;
	HGEccentricity1 = 0.0f;
	HGEccentricity2 = 0.0f;
	HGEccentricityRatio = 0.0f;
	IsotropicColor = FLinearColor::Black;
	IsotropicTransmittance = 1.0f;
	FogMode = EFogMode::FOG_NONE;
	FogIntensity = 0.0f;
	FogColor = FLinearColor::Black;
	FogTransmittance = 1.0f;
	// NVCHANGE_END: Nvidia Volumetric Lighting


	// NVCHANGE_BEGIN: Add HBAO+
#if WITH_GFSDK_SSAO
	{
		HBAOPowerExponent = 2.f;
		HBAORadius = 2.f;
		HBAOBias = 0.1f;
		HBAOSmallScaleAO = 1.f;
		HBAOBlurRadius = AOBR_BlurRadius2;
		HBAOBlurSharpness = 16.f;
		HBAOForegroundAOEnable = false;
		HBAOForegroundAODistance = 100.f;
		HBAOBackgroundAOEnable = false;
		HBAOBackgroundAODistance = 1000.f;
	}
#endif
	// NVCHANGE_END: Add HBAO+


	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	{
		VXGI::DiffuseTracingParameters DefaultParams;
		VxgiDiffuseTracingEnabled = false;
		VxgiDiffuseTracingIntensity = DefaultParams.irradianceScale;
		VxgiDiffuseTracingNumCones = DefaultParams.numCones;
		bVxgiDiffuseTracingAutoAngle = DefaultParams.autoConeAngle;
		VxgiDiffuseTracingSparsity = DefaultParams.tracingSparsity;
		VxgiDiffuseTracingConeAngle = DefaultParams.coneAngle;
		bVxgiDiffuseTracingConeRotation = DefaultParams.enableConeRotation;
		bVxgiDiffuseTracingRandomConeOffsets = DefaultParams.enableRandomConeOffsets;
		VxgiDiffuseTracingConeNormalGroupingFactor = DefaultParams.coneNormalGroupingFactor;
		VxgiDiffuseTracingMaxSamples = DefaultParams.maxSamples;
		VxgiDiffuseTracingStep = DefaultParams.tracingStep;
		VxgiDiffuseTracingOpacityCorrectionFactor = DefaultParams.opacityCorrectionFactor;
		VxgiDiffuseTracingNormalOffsetFactor = DefaultParams.normalOffsetFactor;
		VxgiDiffuseTracingInitialOffsetBias = DefaultParams.initialOffsetBias;
		VxgiDiffuseTracingInitialOffsetDistanceFactor = DefaultParams.initialOffsetDistanceFactor;
		bVxgiDiffuseTracingTemporalReprojectionEnabled = DefaultParams.enableTemporalReprojection;
		VxgiDiffuseTracingTemporalReprojectionPreviousFrameWeight = DefaultParams.temporalReprojectionWeight;
		VxgiDiffuseTracingTemporalReprojectionMaxDistanceInVoxels = 1.f;
		VxgiDiffuseTracingTemporalReprojectionNormalWeightExponent = 0.f;
		VxgiDiffuseTracingEnvironmentMapTint = FLinearColor(1.f, 1.f, 1.f, 1.f);
		VxgiDiffuseTracingEnvironmentMap = NULL;
		bVxgiDiffuseTracingRefinementEnabled = DefaultParams.enableSparseTracingRefinement;
		bVxgiDiffuseTracingFlipOpacityDirections = DefaultParams.flipOpacityDirections;

		VxgiAmbientColor = FLinearColor(0.f, 0.f, 0.f);
		VxgiAmbientRange = DefaultParams.ambientRange;
		VxgiAmbientScale = DefaultParams.ambientScale;
		VxgiAmbientBias = DefaultParams.ambientBias;
		VxgiAmbientPowerExponent = DefaultParams.ambientPower;
		VxgiAmbientDistanceDarkening = DefaultParams.ambientDistanceDarkening;
		VxgiAmbientMixIntensity = 1.0f;

		VXGI::UpdateVoxelizationParameters DefaultUpdateVoxelizationParams;
		VxgiMultiBounceIrradianceScale = DefaultUpdateVoxelizationParams.indirectIrradianceMapTracingParameters.irradianceScale;
	}
	{
		VXGI::SpecularTracingParameters DefaultParams;
		VxgiSpecularTracingEnabled = false;
		VxgiSpecularTracingIntensity = DefaultParams.irradianceScale;
		VxgiSpecularTracingMaxSamples = DefaultParams.maxSamples;
		VxgiSpecularTracingTracingStep = DefaultParams.tracingStep;
		VxgiSpecularTracingOpacityCorrectionFactor = DefaultParams.opacityCorrectionFactor;
		VxgiSpecularTracingInitialOffsetBias = DefaultParams.initialOffsetBias;
		VxgiSpecularTracingInitialOffsetDistanceFactor = DefaultParams.initialOffsetDistanceFactor;
		VxgiSpecularTracingFilter = (EVxgiSpecularTracingFilter)DefaultParams.filter;
		VxgiSpecularTracingEnvironmentMapTint = FLinearColor(1.f, 1.f, 1.f, 1.f);
		VxgiSpecularTracingEnvironmentMap = NULL;
	}
#endif
	// NVCHANGE_END: Add VXGI

#if DO_CHECK && WITH_EDITOR
	static bool bCheckedMembers = false;
	if (!bCheckedMembers)
	{
		bCheckedMembers = true;
		DoPostProcessSettingsSanityCheck();
	}
#endif // DO_CHECK
}

UScene::UScene(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}
