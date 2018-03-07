// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessCombineLUTs.cpp: Post processing tone mapping implementation.
=============================================================================*/

#include "PostProcess/PostProcessCombineLUTs.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "TranslucentRendering.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessing.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"

// CVars
static TAutoConsoleVariable<float> CVarColorMin(
	TEXT("r.Color.Min"),
	0.0f,
	TEXT("Allows to define where the value 0 in the color channels is mapped to after color grading.\n")
	TEXT("The value should be around 0, positive: a gray scale is added to the darks, negative: more dark values become black, Default: 0"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarColorMid(
	TEXT("r.Color.Mid"),
	0.5f,
	TEXT("Allows to define where the value 0.5 in the color channels is mapped to after color grading (This is similar to a gamma correction).\n")
	TEXT("Value should be around 0.5, smaller values darken the mid tones, larger values brighten the mid tones, Default: 0.5"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarColorMax(
	TEXT("r.Color.Max"),
	1.0f,
	TEXT("Allows to define where the value 1.0 in the color channels is mapped to after color grading.\n")
	TEXT("Value should be around 1, smaller values darken the highlights, larger values move more colors towards white, Default: 1"),
	ECVF_RenderThreadSafe);

int32 GLUTSize = 32;
static FAutoConsoleVariableRef CVarLUTSize(
	TEXT("r.LUT.Size"),
	GLUTSize,
	TEXT("Size of film LUT"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarTonemapperFilm(
	TEXT("r.TonemapperFilm"),
	1,
	TEXT("Use new film tone mapper"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarMobileTonemapperFilm(
	TEXT("r.Mobile.TonemapperFilm"),
	0,
	TEXT("Whether mobile platforms should use new film tone mapper"),
	ECVF_RenderThreadSafe
	);

// false:use 256x16 texture / true:use volume texture (faster, requires geometry shader)
// USE_VOLUME_LUT: needs to be the same for C++ and HLSL
bool UseVolumeTextureLUT(EShaderPlatform Platform) 
{
	return (IsFeatureLevelSupported(Platform,ERHIFeatureLevel::SM4) && GSupportsVolumeTextureRendering && (RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform)));
}

// including the neutral one at index 0
const uint32 GMaxLUTBlendCount = 5;

const int32 GCombineLUTsComputeTileSize = 8;

struct FColorTransform
{
	FColorTransform()
	{
		Reset();
	}
	
	float			MinValue;
	float			MidValue;
	float			MaxValue;

	void Reset()
	{
		MinValue = 0.0f;
		MidValue = 0.5f;
		MaxValue = 1.0f;
	}
};

/*-----------------------------------------------------------------------------
FCombineLUTsShaderParameters
-----------------------------------------------------------------------------*/

template <uint32 BlendCount>
class FCombineLUTsShaderParameters
{
public:
	FCombineLUTsShaderParameters() {}

	FCombineLUTsShaderParameters(const FShaderParameterMap& ParameterMap)
		: ColorRemapShaderParameters(ParameterMap)
	{
		// Suppress static code analysis warnings about a potentially ill-defined loop. BlendCount > 0 is valid.
		CA_SUPPRESS(6294)

		// starts as 1 as 0 is the neutral one
		for(uint32 i = 1; i < BlendCount; ++i)
		{
			FString Name = FString::Printf(TEXT("Texture%d"), i);

			TextureParameter[i].Bind(ParameterMap, *Name);
			TextureParameterSampler[i].Bind(ParameterMap, *(Name + TEXT("Sampler")));
		}

		WeightsParameter.Bind(	ParameterMap, TEXT("LUTWeights"));
		ColorScale.Bind(		ParameterMap,TEXT("ColorScale"));
		OverlayColor.Bind(		ParameterMap,TEXT("OverlayColor"));
		InverseGamma.Bind(		ParameterMap,TEXT("InverseGamma"));

		WhiteTemp.Bind( ParameterMap,TEXT("WhiteTemp") );
		WhiteTint.Bind( ParameterMap,TEXT("WhiteTint") );

		ColorSaturation.Bind(	ParameterMap,TEXT("ColorSaturation") );
		ColorContrast.Bind(		ParameterMap,TEXT("ColorContrast") );
		ColorGamma.Bind(		ParameterMap,TEXT("ColorGamma") );
		ColorGain.Bind(			ParameterMap,TEXT("ColorGain") );
		ColorOffset.Bind(		ParameterMap,TEXT("ColorOffset") );

		ColorSaturationShadows.Bind(ParameterMap, TEXT("ColorSaturationShadows"));
		ColorContrastShadows.Bind(	ParameterMap, TEXT("ColorContrastShadows"));
		ColorGammaShadows.Bind(		ParameterMap, TEXT("ColorGammaShadows"));
		ColorGainShadows.Bind(		ParameterMap, TEXT("ColorGainShadows"));
		ColorOffsetShadows.Bind(	ParameterMap, TEXT("ColorOffsetShadows"));

		ColorSaturationMidtones.Bind(	ParameterMap, TEXT("ColorSaturationMidtones"));
		ColorContrastMidtones.Bind(		ParameterMap, TEXT("ColorContrastMidtones"));
		ColorGammaMidtones.Bind(		ParameterMap, TEXT("ColorGammaMidtones"));
		ColorGainMidtones.Bind(			ParameterMap, TEXT("ColorGainMidtones"));
		ColorOffsetMidtones.Bind(		ParameterMap, TEXT("ColorOffsetMidtones"));

		ColorSaturationHighlights.Bind(	ParameterMap, TEXT("ColorSaturationHighlights"));
		ColorContrastHighlights.Bind(	ParameterMap, TEXT("ColorContrastHighlights"));
		ColorGammaHighlights.Bind(		ParameterMap, TEXT("ColorGammaHighlights"));
		ColorGainHighlights.Bind(		ParameterMap, TEXT("ColorGainHighlights"));
		ColorOffsetHighlights.Bind(		ParameterMap, TEXT("ColorOffsetHighlights"));

		ColorCorrectionShadowsMax.Bind(		ParameterMap, TEXT("ColorCorrectionShadowsMax"));
		ColorCorrectionHighlightsMin.Bind(	ParameterMap, TEXT("ColorCorrectionHighlightsMin"));

		FilmSlope.Bind(		ParameterMap,TEXT("FilmSlope") );
		FilmToe.Bind(		ParameterMap,TEXT("FilmToe") );
		FilmShoulder.Bind(	ParameterMap,TEXT("FilmShoulder") );
		FilmBlackClip.Bind(	ParameterMap,TEXT("FilmBlackClip") );
		FilmWhiteClip.Bind(	ParameterMap,TEXT("FilmWhiteClip") );

		OutputDevice.Bind(	ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(	ParameterMap, TEXT("OutputGamut"));

		ColorMatrixR_ColorCurveCd1.Bind(		ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(		ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(		ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(	ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(				ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(					ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(					ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(					ParameterMap, TEXT("ColorShadow_Tint2"));
	}
	
	template <typename TRHICmdList, typename TRHIShader>
	void Set(TRHICmdList& RHICmdList, const TRHIShader ShaderRHI, const FSceneView& View, FTexture* Textures[BlendCount], float Weights[BlendCount])
	{
		const FPostProcessSettings& Settings = View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(View.Family);

		for(uint32 i = 0; i < BlendCount; ++i)
		{
			// we don't need to set the neutral one
			if(i != 0)
			{
				// don't use texture asset sampler as it might have anisotropic filtering enabled
				FSamplerStateRHIParamRef Sampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI();

				SetTextureParameter(RHICmdList, ShaderRHI, TextureParameter[i], TextureParameterSampler[i], Sampler, Textures[i]->TextureRHI);
			}

			SetShaderValue(RHICmdList, ShaderRHI, WeightsParameter, Weights[i], i);
		}

		SetShaderValue(RHICmdList, ShaderRHI, ColorScale, View.ColorScale);
		SetShaderValue(RHICmdList, ShaderRHI, OverlayColor, View.OverlayColor);

		ColorRemapShaderParameters.Set(RHICmdList, ShaderRHI);

		// White balance
		SetShaderValue(RHICmdList, ShaderRHI, WhiteTemp, Settings.WhiteTemp );
		SetShaderValue(RHICmdList, ShaderRHI, WhiteTint, Settings.WhiteTint );

		// Color grade
		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturation,	Settings.ColorSaturation );
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrast,	Settings.ColorContrast );
		SetShaderValue(RHICmdList, ShaderRHI, ColorGamma,		Settings.ColorGamma );
		SetShaderValue(RHICmdList, ShaderRHI, ColorGain,		Settings.ColorGain );
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffset,		Settings.ColorOffset );

		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturationShadows,	Settings.ColorSaturationShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrastShadows,		Settings.ColorContrastShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGammaShadows,		Settings.ColorGammaShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGainShadows,			Settings.ColorGainShadows);
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffsetShadows,		Settings.ColorOffsetShadows);

		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturationMidtones,	Settings.ColorSaturationMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrastMidtones,	Settings.ColorContrastMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGammaMidtones,		Settings.ColorGammaMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGainMidtones,		Settings.ColorGainMidtones);
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffsetMidtones,		Settings.ColorOffsetMidtones);

		SetShaderValue(RHICmdList, ShaderRHI, ColorSaturationHighlights,	Settings.ColorSaturationHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorContrastHighlights,		Settings.ColorContrastHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGammaHighlights,			Settings.ColorGammaHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorGainHighlights,			Settings.ColorGainHighlights);
		SetShaderValue(RHICmdList, ShaderRHI, ColorOffsetHighlights,		Settings.ColorOffsetHighlights);

		SetShaderValue(RHICmdList, ShaderRHI, ColorCorrectionShadowsMax,	Settings.ColorCorrectionShadowsMax);
		SetShaderValue(RHICmdList, ShaderRHI, ColorCorrectionHighlightsMin,	Settings.ColorCorrectionHighlightsMin);

		// Film
		SetShaderValue(RHICmdList, ShaderRHI, FilmSlope,		Settings.FilmSlope);
		SetShaderValue(RHICmdList, ShaderRHI, FilmToe,			Settings.FilmToe);
		SetShaderValue(RHICmdList, ShaderRHI, FilmShoulder,		Settings.FilmShoulder);
		SetShaderValue(RHICmdList, ShaderRHI, FilmBlackClip,	Settings.FilmBlackClip);
		SetShaderValue(RHICmdList, ShaderRHI, FilmWhiteClip,	Settings.FilmWhiteClip);

		{
			static TConsoleVariableData<int32>* CVarOutputDevice = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.OutputDevice"));
			static TConsoleVariableData<float>* CVarOutputGamma = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));

			int32 OutputDeviceValue = CVarOutputDevice->GetValueOnRenderThread();
			float Gamma = CVarOutputGamma->GetValueOnRenderThread();

			if (PLATFORM_APPLE && Gamma == 0.0f)
			{
				Gamma = 2.2f;
			}
	
			if (Gamma > 0.0f)
			{
				// Enforce user-controlled ramp over sRGB or Rec709
				OutputDeviceValue = FMath::Max(OutputDeviceValue, 2);
			}

			SetShaderValue(RHICmdList, ShaderRHI, OutputDevice, OutputDeviceValue);

			static TConsoleVariableData<int32>* CVarOutputGamut = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.HDR.Display.ColorGamut"));
			int32 OutputGamutValue = CVarOutputGamut->GetValueOnRenderThread();
			SetShaderValue(RHICmdList, ShaderRHI, OutputGamut, OutputGamutValue);

			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Z = 1.0f / FMath::Max(Gamma, 1.0f);
			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			// Legacy tone mapper
			// TODO remove

			// Must insure inputs are in correct range (else possible generation of NaNs).
			float InExposure = 1.0f;
			FVector InWhitePoint(Settings.FilmWhitePoint);
			float InSaturation = FMath::Clamp(Settings.FilmSaturation, 0.0f, 2.0f);
			FVector InLuma = FVector(1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f);
			FVector InMatrixR(Settings.FilmChannelMixerRed);
			FVector InMatrixG(Settings.FilmChannelMixerGreen);
			FVector InMatrixB(Settings.FilmChannelMixerBlue);
			float InContrast = FMath::Clamp(Settings.FilmContrast, 0.0f, 1.0f) + 1.0f;
			float InDynamicRange = powf(2.0f, FMath::Clamp(Settings.FilmDynamicRange, 1.0f, 4.0f));
			float InToe = (1.0f - FMath::Clamp(Settings.FilmToeAmount, 0.0f, 1.0f)) * 0.18f;
			InToe = FMath::Clamp(InToe, 0.18f/8.0f, 0.18f * (15.0f/16.0f));
			float InHeal = 1.0f - (FMath::Max(1.0f/32.0f, 1.0f - FMath::Clamp(Settings.FilmHealAmount, 0.0f, 1.0f)) * (1.0f - 0.18f)); 
			FVector InShadowTint(Settings.FilmShadowTint);
			float InShadowTintBlend = FMath::Clamp(Settings.FilmShadowTintBlend, 0.0f, 1.0f) * 64.0f;

			// Shadow tint amount enables turning off shadow tinting.
			float InShadowTintAmount = FMath::Clamp(Settings.FilmShadowTintAmount, 0.0f, 1.0f);
			InShadowTint = InWhitePoint + (InShadowTint - InWhitePoint) * InShadowTintAmount;

			// Make sure channel mixer inputs sum to 1 (+ smart dealing with all zeros).
			InMatrixR.X += 1.0f / (256.0f*256.0f*32.0f);
			InMatrixG.Y += 1.0f / (256.0f*256.0f*32.0f);
			InMatrixB.Z += 1.0f / (256.0f*256.0f*32.0f);
			InMatrixR *= 1.0f / FVector::DotProduct(InMatrixR, FVector(1.0f));
			InMatrixG *= 1.0f / FVector::DotProduct(InMatrixG, FVector(1.0f));
			InMatrixB *= 1.0f / FVector::DotProduct(InMatrixB, FVector(1.0f));

			// Conversion from linear rgb to luma (using HDTV coef).
			FVector LumaWeights = FVector(0.2126f, 0.7152f, 0.0722f);

			// Make sure white point has 1.0 as luma (so adjusting white point doesn't change exposure).
			// Make sure {0.0,0.0,0.0} inputs do something sane (default to white).
			InWhitePoint += FVector(1.0f / (256.0f*256.0f*32.0f));
			InWhitePoint *= 1.0f / FVector::DotProduct(InWhitePoint, LumaWeights);
			InShadowTint += FVector(1.0f / (256.0f*256.0f*32.0f));
			InShadowTint *= 1.0f / FVector::DotProduct(InShadowTint, LumaWeights);

			// Grey after color matrix is applied.
			FVector ColorMatrixLuma = FVector(
			FVector::DotProduct(InLuma.X * FVector(InMatrixR.X, InMatrixG.X, InMatrixB.X), FVector(1.0f)),
			FVector::DotProduct(InLuma.Y * FVector(InMatrixR.Y, InMatrixG.Y, InMatrixB.Y), FVector(1.0f)),
			FVector::DotProduct(InLuma.Z * FVector(InMatrixR.Z, InMatrixG.Z, InMatrixB.Z), FVector(1.0f)));

			FVector OutMatrixR = FVector(0.0f);
			FVector OutMatrixG = FVector(0.0f);
			FVector OutMatrixB = FVector(0.0f);
			FVector OutColorShadow_Luma = LumaWeights * InShadowTintBlend;
			FVector OutColorShadow_Tint1 = InWhitePoint;
			FVector OutColorShadow_Tint2 = InShadowTint - InWhitePoint;

			// Final color matrix effected by saturation and exposure.
			OutMatrixR = (ColorMatrixLuma + ((InMatrixR - ColorMatrixLuma) * InSaturation)) * InExposure;
			OutMatrixG = (ColorMatrixLuma + ((InMatrixG - ColorMatrixLuma) * InSaturation)) * InExposure;
			OutMatrixB = (ColorMatrixLuma + ((InMatrixB - ColorMatrixLuma) * InSaturation)) * InExposure;

			// Line for linear section.
			float FilmLineOffset = 0.18f - 0.18f*InContrast;
			float FilmXAtY0 = -FilmLineOffset/InContrast;
			float FilmXAtY1 = (1.0f - FilmLineOffset) / InContrast;
			float FilmXS = FilmXAtY1 - FilmXAtY0;

			// Coordinates of linear section.
			float FilmHiX = FilmXAtY0 + InHeal*FilmXS;
			float FilmHiY = FilmHiX*InContrast + FilmLineOffset;
			float FilmLoX = FilmXAtY0 + InToe*FilmXS;
			float FilmLoY = FilmLoX*InContrast + FilmLineOffset;
			// Supported exposure range before clipping.
			float FilmHeal = InDynamicRange - FilmHiX;
			// Intermediates.
			float FilmMidXS = FilmHiX - FilmLoX;
			float FilmMidYS = FilmHiY - FilmLoY;
			float FilmSlopeS = FilmMidYS / (FilmMidXS);
			float FilmHiYS = 1.0f - FilmHiY;
			float FilmLoYS = FilmLoY;
			float FilmToeVal = FilmLoX;
			float FilmHiG = (-FilmHiYS + (FilmSlopeS*FilmHeal)) / (FilmSlopeS*FilmHeal);
			float FilmLoG = (-FilmLoYS + (FilmSlopeS*FilmToeVal)) / (FilmSlopeS*FilmToeVal);

			// Constants.
			float OutColorCurveCh1 = FilmHiYS/FilmHiG;
			float OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
			float OutColorCurveCh3 = FilmHiYS/(FilmSlopeS*FilmHiG) - FilmHiX;
			float OutColorCurveCh0Cm1 = FilmHiX;
			float OutColorCurveCm2 = FilmSlopeS;
			float OutColorCurveCm0Cd0 = FilmLoX;
			float OutColorCurveCd3Cm3 = FilmLoY - FilmLoX*FilmSlopeS;
			float OutColorCurveCd1 = 0.0f;
			float OutColorCurveCd2 = 1.0f;
			// Handle these separate in case of FilmLoG being 0.
			if(FilmLoG != 0.0f)
			{
				OutColorCurveCd1 = -FilmLoYS/FilmLoG;
				OutColorCurveCd2 = FilmLoYS/(FilmSlopeS*FilmLoG);
			}
			else
			{
				// FilmLoG being zero means dark region is a linear segment (so just continue the middle section).
				OutColorCurveCm0Cd0 = 0.0f;
				OutColorCurveCd3Cm3 = 0.0f;
			}

			FVector4 Constants[8];
			Constants[0] = FVector4(OutMatrixR, OutColorCurveCd1);
			Constants[1] = FVector4(OutMatrixG, OutColorCurveCd3Cm3);
			Constants[2] = FVector4(OutMatrixB, OutColorCurveCm2); 
			Constants[3] = FVector4(OutColorCurveCm0Cd0, OutColorCurveCd2, OutColorCurveCh0Cm1, OutColorCurveCh3); 
			Constants[4] = FVector4(OutColorCurveCh1, OutColorCurveCh2, 0.0f, 0.0f);
			Constants[5] = FVector4(OutColorShadow_Luma, 0.0f);
			Constants[6] = FVector4(OutColorShadow_Tint1, 0.0f);
			Constants[7] = FVector4(OutColorShadow_Tint2, CVarTonemapperFilm.GetValueOnRenderThread());

			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}
	}

	friend FArchive& operator<<(FArchive& Ar,FCombineLUTsShaderParameters& P)
	{
		for(uint32 i = 0; i < BlendCount; ++i)
		{
			Ar << P.TextureParameter[i];
			Ar << P.TextureParameterSampler[i];
		}

		Ar << P.WeightsParameter << P.ColorScale << P.OverlayColor;
		Ar << P.ColorRemapShaderParameters << P.InverseGamma;
		Ar << P.WhiteTemp << P.WhiteTint;
		Ar << P.ColorSaturation << P.ColorContrast << P.ColorGamma << P.ColorGain << P.ColorOffset;
		Ar << P.ColorSaturationShadows  << P.ColorContrastShadows << P.ColorGammaShadows << P.ColorGainShadows << P.ColorOffsetShadows;
		Ar << P.ColorSaturationMidtones << P.ColorContrastMidtones << P.ColorGammaMidtones << P.ColorGainMidtones << P.ColorOffsetMidtones;
		Ar << P.ColorSaturationHighlights << P.ColorContrastHighlights << P.ColorGammaHighlights  << P.ColorGainHighlights << P.ColorOffsetHighlights;
		Ar << P.ColorCorrectionShadowsMax << P.ColorCorrectionHighlightsMin;
		Ar << P.OutputDevice << P.OutputGamut;
		Ar << P.FilmSlope << P.FilmToe << P.FilmShoulder << P.FilmBlackClip << P.FilmWhiteClip;
		Ar << P.ColorMatrixR_ColorCurveCd1 << P.ColorMatrixG_ColorCurveCd3Cm3 << P.ColorMatrixB_ColorCurveCm2;
		Ar << P.ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << P.ColorCurve_Ch1_Ch2 << P.ColorShadow_Luma << P.ColorShadow_Tint1 << P.ColorShadow_Tint2;

		return Ar;
	}

	// [0] is not used as it's the neutral one we do in the shader
	FShaderResourceParameter TextureParameter[GMaxLUTBlendCount];
	FShaderResourceParameter TextureParameterSampler[GMaxLUTBlendCount];
	FShaderParameter WeightsParameter;
	FShaderParameter ColorScale;
	FShaderParameter OverlayColor;
	FShaderParameter InverseGamma;
	FColorRemapShaderParameters ColorRemapShaderParameters;

	FShaderParameter WhiteTemp;
	FShaderParameter WhiteTint;

	FShaderParameter ColorSaturation;
	FShaderParameter ColorContrast;
	FShaderParameter ColorGamma;
	FShaderParameter ColorGain;
	FShaderParameter ColorOffset;

	FShaderParameter ColorSaturationShadows;
	FShaderParameter ColorContrastShadows;
	FShaderParameter ColorGammaShadows;
	FShaderParameter ColorGainShadows;
	FShaderParameter ColorOffsetShadows;

	FShaderParameter ColorSaturationMidtones;
	FShaderParameter ColorContrastMidtones;
	FShaderParameter ColorGammaMidtones;
	FShaderParameter ColorGainMidtones;
	FShaderParameter ColorOffsetMidtones;

	FShaderParameter ColorSaturationHighlights;
	FShaderParameter ColorContrastHighlights;
	FShaderParameter ColorGammaHighlights;
	FShaderParameter ColorGainHighlights;
	FShaderParameter ColorOffsetHighlights;

	FShaderParameter ColorCorrectionShadowsMax;
	FShaderParameter ColorCorrectionHighlightsMin;

	FShaderParameter FilmSlope;
	FShaderParameter FilmToe;
	FShaderParameter FilmShoulder;
	FShaderParameter FilmBlackClip;
	FShaderParameter FilmWhiteClip;

	FShaderParameter OutputDevice;
	FShaderParameter OutputGamut;

	// Legacy
	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;
};

/*-----------------------------------------------------------------------------
FColorRemapShaderParameters
-----------------------------------------------------------------------------*/

FColorRemapShaderParameters::FColorRemapShaderParameters(const FShaderParameterMap& ParameterMap)
{
	MappingPolynomial.Bind(ParameterMap, TEXT("MappingPolynomial"));
}

void FColorRemapShaderParameters::Set(FRHICommandList& RHICmdList, const FPixelShaderRHIParamRef ShaderRHI)
{
	FColorTransform ColorTransform;
	ColorTransform.MinValue = FMath::Clamp(CVarColorMin.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MidValue = FMath::Clamp(CVarColorMid.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MaxValue = FMath::Clamp(CVarColorMax.GetValueOnRenderThread(), -10.0f, 10.0f);
	
	{
		// x is the input value, y the output value
		// RGB = a, b, c where y = a * x*x + b * x + c

		float c = ColorTransform.MinValue;
		float b = 4 * ColorTransform.MidValue - 3 * ColorTransform.MinValue - ColorTransform.MaxValue;
		float a = ColorTransform.MaxValue - ColorTransform.MinValue - b;

		SetShaderValue(RHICmdList, ShaderRHI, MappingPolynomial, FVector(a, b, c));
	}
}

template <typename TRHICmdList>
void FColorRemapShaderParameters::Set(TRHICmdList& RHICmdList, const FComputeShaderRHIParamRef ShaderRHI)
{
	FColorTransform ColorTransform;
	ColorTransform.MinValue = FMath::Clamp(CVarColorMin.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MidValue = FMath::Clamp(CVarColorMid.GetValueOnRenderThread(), -10.0f, 10.0f);
	ColorTransform.MaxValue = FMath::Clamp(CVarColorMax.GetValueOnRenderThread(), -10.0f, 10.0f);
	
	{
		// x is the input value, y the output value
		// RGB = a, b, c where y = a * x*x + b * x + c

		float c = ColorTransform.MinValue;
		float b = 4 * ColorTransform.MidValue - 3 * ColorTransform.MinValue - ColorTransform.MaxValue;
		float a = ColorTransform.MaxValue - ColorTransform.MinValue - b;

		SetShaderValue(RHICmdList, ShaderRHI, MappingPolynomial, FVector(a, b, c));
	}
}

FArchive& operator<<(FArchive& Ar, FColorRemapShaderParameters& P)
{
	Ar << P.MappingPolynomial;
	return Ar;
}

/**
* A pixel shader for blending multiple LUT to one
*
* @param BlendCount >0
*/
template<uint32 BlendCount>
class FLUTBlenderPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLUTBlenderPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}

	FLUTBlenderPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer) 
		, CombineLUTsShaderParameters(Initializer.ParameterMap)
	{
	}
	
	template <typename TRHICommandList>
	void SetParameters(TRHICommandList& RHICmdList, const FSceneView& View, FTexture* Textures[BlendCount], float Weights[BlendCount])
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		CombineLUTsShaderParameters.Set(RHICmdList, ShaderRHI, View, Textures, Weights);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("BLENDCOUNT"), BlendCount);
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeTextureLUT(Platform));
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << CombineLUTsShaderParameters;
		return bShaderHasOutdatedParameters;
	}

private: // ---------------------------------------------------
	FLUTBlenderPS() {}

	FCombineLUTsShaderParameters<BlendCount> CombineLUTsShaderParameters;
};


IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<1>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<2>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<3>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<4>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainPS"),SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderPS<5>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainPS"),SF_Pixel);

/**
* A compute shader for blending multiple LUTs together
* @param BlendCount [1 - 5]
*/
template<uint32 BlendCount>
class FLUTBlenderCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FLUTBlenderCS,Global);

public:
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// CS params
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GCombineLUTsComputeTileSize);
		
		// PS params
		OutEnvironment.SetDefine(TEXT("BLENDCOUNT"), BlendCount);
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"), UseVolumeTextureLUT(Platform));
	}

	FLUTBlenderCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer) 
		, CombineLUTsShaderParameters(Initializer.ParameterMap)
	{
		// CS params
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
		CombineLUTsComputeParams.Bind(Initializer.ParameterMap, TEXT("CombineLUTsComputeParams"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& DestSize, FUnorderedAccessViewRHIParamRef DestUAV, FTexture* Textures[BlendCount], float Weights[BlendCount])
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		// CS params
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		OutComputeTex.SetTexture(RHICmdList, ShaderRHI, nullptr, DestUAV);		
		
		FVector4 CombineLUTsComputeValues(0, 0, 1.f / (float)DestSize.X, 1.f / (float)DestSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, CombineLUTsComputeParams, CombineLUTsComputeValues);

		// PS params
		CombineLUTsShaderParameters.Set(RHICmdList, ShaderRHI, Context.View, Textures, Weights);
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		OutComputeTex.UnsetUAV(RHICmdList, ShaderRHI);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		// CS params
		Ar << OutComputeTex << CombineLUTsComputeParams;
		// PS params
		Ar << CombineLUTsShaderParameters;
		return bShaderHasOutdatedParameters;
	}

private: // ---------------------------------------------------
	FLUTBlenderCS() {}

	// CS params
	FRWShaderParameter OutComputeTex;
	FShaderParameter CombineLUTsComputeParams;

	// PS params
	FCombineLUTsShaderParameters<BlendCount> CombineLUTsShaderParameters;
};

IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderCS<1>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderCS<2>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderCS<3>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderCS<4>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>,FLUTBlenderCS<5>,TEXT("/Engine/Private/PostProcessCombineLUTs.usf"),TEXT("MainCS"),SF_Compute);

template <typename TRHICommandList>
static void SetLUTBlenderShader(FRenderingCompositePassContext& Context, TRHICommandList& RHICmdList, uint32 BlendCount, FTexture* Texture[], float Weights[], const FVolumeBounds& VolumeBounds)
{
	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	check(BlendCount > 0);

	FShader* LocalPixelShader = 0;
	const FSceneView& View = Context.View;

	const auto FeatureLevel = Context.GetFeatureLevel();
	auto ShaderMap = Context.GetShaderMap();

	// A macro to handle setting the filter shader for a specific number of samples.
#define CASE_COUNT(BlendCount) \
	case BlendCount: \
	{ \
		TShaderMapRef<FLUTBlenderPS<BlendCount> > PixelShader(ShaderMap); \
		LocalPixelShader = *PixelShader;\
	}; \
	break;

	switch(BlendCount)
	{
		// starts at 1 as we always have at least the neutral one
		CASE_COUNT(1);
		CASE_COUNT(2);
		CASE_COUNT(3);
		CASE_COUNT(4);
		CASE_COUNT(5);
		//	default:
		//		UE_LOG(LogRenderer, Fatal,TEXT("Invalid number of samples: %u"),BlendCount);
	}
#undef CASE_COUNT
	if(UseVolumeTextureLUT(Context.View.GetShaderPlatform()))
	{
		TShaderMapRef<FWriteToSliceVS> VertexShader(ShaderMap);
		TOptionalShaderMapRef<FWriteToSliceGS> GeometryShader(ShaderMap);

		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GScreenVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(LocalPixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(RHICmdList, VolumeBounds, FIntVector(VolumeBounds.MaxX - VolumeBounds.MinX));
		if(GeometryShader.IsValid())
		{
			GeometryShader->SetParameters(RHICmdList, VolumeBounds.MinZ);
		}
	}
	else
	{
		TShaderMapRef<FPostProcessVS> VertexShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(LocalPixelShader);
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		VertexShader->SetParameters(Context);
	}
#define CASE_COUNT(BlendCount) \
	case BlendCount: \
	{ \
	TShaderMapRef<FLUTBlenderPS<BlendCount> > PixelShader(ShaderMap); \
	PixelShader->SetParameters(RHICmdList, View, Texture, Weights); \
	}; \
	break;

	switch(BlendCount)
	{
		// starts at 1 as we always have at least the neutral one
		CASE_COUNT(1);
		CASE_COUNT(2);
		CASE_COUNT(3);
		CASE_COUNT(4);
		CASE_COUNT(5);
		//	default:
		//		UE_LOG(LogRenderer, Fatal,TEXT("Invalid number of samples: %u"),BlendCount);
	}
#undef CASE_COUNT
}


uint32 FRCPassPostProcessCombineLUTs::FindIndex(const FFinalPostProcessSettings& Settings, UTexture* Tex) const
{
	for(uint32 i = 0; i < (uint32)Settings.ContributingLUTs.Num(); ++i)
	{
		if(Settings.ContributingLUTs[i].LUTTexture == Tex)
		{
			return i;
		}
	}

	return 0xffffffff;
}

uint32 FRCPassPostProcessCombineLUTs::GenerateFinalTable(const FFinalPostProcessSettings& Settings, FTexture* OutTextures[], float OutWeights[], uint32 MaxCount) const
{
	// find the n strongest contributors, drop small contributors
	// (inefficient implementation for many items but count should be small)

	uint32 LocalCount = 1;

	// add the neutral one (done in the shader) as it should be the first and always there
	OutTextures[0] = 0;
	{
		uint32 NeutralIndex = FindIndex(Settings, 0);

		OutWeights[0] = NeutralIndex == 0xffffffff ? 0.0f : Settings.ContributingLUTs[NeutralIndex].Weight;
	}

	float OutWeightsSum = OutWeights[0];
	for(; LocalCount < MaxCount; ++LocalCount)
	{
		uint32 BestIndex = 0xffffffff;
		// find the one with the strongest weight, add until full
		for(uint32 i = 0; i < (uint32)Settings.ContributingLUTs.Num(); ++i)
		{
			bool AlreadyInArray = false;
			{
				UTexture* LUTTexture = Settings.ContributingLUTs[i].LUTTexture; 
				FTexture* Internal = LUTTexture ? LUTTexture->Resource : 0; 
				for(uint32 e = 0; e < LocalCount; ++e)
				{
					if(Internal == OutTextures[e])
					{
						AlreadyInArray = true;
						break;
					}
				}
			}

			if(AlreadyInArray)
			{
				// we already have this one
				continue;
			}

			if(BestIndex != 0xffffffff
				&& Settings.ContributingLUTs[BestIndex].Weight > Settings.ContributingLUTs[i].Weight)
			{
				// we have a better ones, maybe add next time
				continue;
			}

			BestIndex = i;
		}

		if(BestIndex == 0xffffffff)
		{
			// no more elements to process 
			break;
		}

		float BestWeight = Settings.ContributingLUTs[BestIndex].Weight;

		if(BestWeight < 1.0f / 512.0f)
		{
			// drop small contributor 
			break;
		}

		UTexture* BestLUTTexture = Settings.ContributingLUTs[BestIndex].LUTTexture; 
		FTexture* BestInternal = BestLUTTexture ? BestLUTTexture->Resource : 0; 

		OutTextures[LocalCount] = BestInternal;
		OutWeights[LocalCount] = BestWeight;
		OutWeightsSum += BestWeight;
	}

	// normalize
	if(OutWeightsSum > 0.001f)
	{
		float InvOutWeightsSum = 1.0f / OutWeightsSum;

		for(uint32 i = 0; i < LocalCount; ++i)
		{
			OutWeights[i] *= InvOutWeightsSum;
		}
	}
	else
	{
		// neutral only is fully utilized
		OutWeights[0] = 1.0f;
		LocalCount = 1;
	}

	return LocalCount;
}

void FRCPassPostProcessCombineLUTs::Process(FRenderingCompositePassContext& Context)
{
	FTexture* LocalTextures[GMaxLUTBlendCount];
	float LocalWeights[GMaxLUTBlendCount];
	AsyncEndFence = FComputeFenceRHIRef();

	const FSceneView& View = Context.View;
	const FSceneViewFamily& ViewFamily = *(View.Family);

	uint32 LocalCount = 1;

	// set defaults for no LUT
	LocalTextures[0] = 0;
	LocalWeights[0] = 1.0f;

	if(ViewFamily.EngineShowFlags.ColorGrading)
	{
		LocalCount = GenerateFinalTable(Context.View.FinalPostProcessSettings, LocalTextures, LocalWeights, GMaxLUTBlendCount);
	}

	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessCombineLUTs, TEXT("PostProcessCombineLUTs%s [%d] %dx%dx%d"),
		bIsComputePass?TEXT("Compute"):TEXT(""), LocalCount, GLUTSize, GLUTSize, GLUTSize);

	const bool bUseVolumeTextureLUT = UseVolumeTextureLUT(ShaderPlatform);
	// for a 3D texture, the viewport is 16x16 (per slice), for a 2D texture, it's unwrapped to 256x16
	FIntPoint DestSize(bUseVolumeTextureLUT ? GLUTSize : GLUTSize * GLUTSize, GLUTSize);

	// The view owns this texture.  For stereo rendering the combine LUT pass should only be executed for eSSP_LEFT_EYE
	// and the result is reused by eSSP_RIGHT_EYE.   Eye-adaptation for stereo works in a similar way.
	// Fundamentally, this relies on the fact that the view is recycled when doing stereo rendering and the LEFT eye is done first.
	const FSceneRenderTargetItem* DestRenderTarget = !bAllocateOutput ?
		Context.View.GetTonemappingLUTRenderTarget(Context.RHICmdList, GLUTSize, bUseVolumeTextureLUT, bIsComputePass) :
		&PassOutputs[0].RequestSurface(Context);
	
	check(DestRenderTarget);
	static auto* RenderPassCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHIRenderPasses"));
	if (bIsComputePass)
	{
		FIntRect DestRect(0, 0, bUseVolumeTextureLUT ? GLUTSize : GLUTSize * GLUTSize, GLUTSize);
	
		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncCombineLUTsEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncCombineLUTs);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget->UAV);
				DispatchCS(RHICmdListComputeImmediate, Context, DestRect, DestRenderTarget->UAV, LocalCount, LocalTextures, LocalWeights);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget->UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget->UAV);
			DispatchCS(Context.RHICmdList, Context, DestRect, DestRenderTarget->UAV, LocalCount, LocalTextures, LocalWeights);			
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget->UAV, AsyncEndFence);
		}
	}
	else
	{
		// Set the view family's render target/viewport.
		if (IsMobilePlatform(ShaderPlatform))
		{
			// Full clear to avoid restore
			SetRenderTarget(Context.RHICmdList, DestRenderTarget->TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
		}
		else
		{
			SetRenderTarget(Context.RHICmdList, DestRenderTarget->TargetableTexture, FTextureRHIRef(), ESimpleRenderTargetMode::EUninitializedColorAndDepth);
		}
		
		Context.SetViewportAndCallRHI(0, 0, 0.0f, DestSize.X, DestSize.Y, 1.0f );

		const FVolumeBounds VolumeBounds(GLUTSize);

		SetLUTBlenderShader(Context, Context.RHICmdList, LocalCount, LocalTextures, LocalWeights, VolumeBounds);

		if (bUseVolumeTextureLUT)
		{
			// use volume texture 16x16x16
			RasterizeToVolumeTexture(Context.RHICmdList, VolumeBounds);
		}
		else
		{
			// use unwrapped 2d texture 256x16
			TShaderMapRef<FPostProcessVS> VertexShader(Context.GetShaderMap());

			DrawRectangle( 
				Context.RHICmdList,
				0, 0,										// XY
				GLUTSize * GLUTSize, GLUTSize,				// SizeXY
				0, 0,										// UV
				GLUTSize * GLUTSize, GLUTSize,				// SizeUV
				FIntPoint(GLUTSize * GLUTSize, GLUTSize),	// TargetSize
				FIntPoint(GLUTSize * GLUTSize, GLUTSize),	// TextureSize
				*VertexShader,
				EDRF_UseTriangleOptimization);
		}

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget->TargetableTexture, DestRenderTarget->ShaderResourceTexture, false, FResolveParams());
	}

	Context.View.SetValidTonemappingLUT();
}

template <typename TRHICmdList>
void FRCPassPostProcessCombineLUTs::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, int32 BlendCount, FTexture* Textures[], float Weights[])
{
	auto ShaderMap = Context.GetShaderMap();

	FIntPoint DestSize(DestRect.Width(), DestRect.Height());
	uint32 GroupSizeXY = FMath::DivideAndRoundUp(DestSize.X, GCombineLUTsComputeTileSize);
	uint32 GroupSizeZ = UseVolumeTextureLUT(ShaderPlatform) ? GroupSizeXY : 1;

#define DISPATCH_CASE(A)																			\
	case A:																							\
	{																								\
		TShaderMapRef<FLUTBlenderCS<A>> ComputeShader(ShaderMap);									\
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());								\
		ComputeShader->SetParameters(RHICmdList, Context, DestSize, DestUAV, Textures, Weights);	\
		DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeXY, GroupSizeXY, GroupSizeZ);	\
		ComputeShader->UnsetParameters(RHICmdList);													\
	}																								\
	break;

	switch(BlendCount)
	{
	DISPATCH_CASE(1)
	DISPATCH_CASE(2)
	DISPATCH_CASE(3)
	DISPATCH_CASE(4)
	DISPATCH_CASE(5)
	default:
		check(0);
	}

#undef DISPATCH_CASE
}

FPooledRenderTargetDesc FRCPassPostProcessCombineLUTs::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	// Specify invalid description to avoid the creation of an intermediate rendertargets.
	// We want to use ViewState->GetTonemappingLUTRT instead.
	FPooledRenderTargetDesc Ret;
	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	
	if (!bAllocateOutput)
	{
		Ret.DebugName = TEXT("DummyLUT");
	}
	else
	{
		EPixelFormat LUTPixelFormat = PF_A2B10G10R10;
		if (!GPixelFormats[LUTPixelFormat].Supported)
		{
			LUTPixelFormat = PF_R8G8B8A8;
		}
		
		Ret = FPooledRenderTargetDesc::Create2DDesc(FIntPoint(GLUTSize * GLUTSize, GLUTSize), LUTPixelFormat, FClearValueBinding::Transparent, TexCreate_None, TexCreate_RenderTargetable | TexCreate_ShaderResource, false);
		
		if(UseVolumeTextureLUT(ShaderPlatform))
		{
			Ret.Extent = FIntPoint(GLUTSize, GLUTSize);
			Ret.Depth = GLUTSize;
		}
		Ret.Flags |= GFastVRamConfig.CombineLUTs;
		Ret.DebugName = TEXT("CombineLUTs");
	}
	Ret.ClearValue = FClearValueBinding::Transparent;

	return Ret;
}
