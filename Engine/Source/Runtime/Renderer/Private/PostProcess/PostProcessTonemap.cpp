// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessTonemap.cpp: Post processing tone mapping implementation.
=============================================================================*/

#include "PostProcess/PostProcessTonemap.h"
#include "EngineGlobals.h"
#include "ScenePrivate.h"
#include "RendererModule.h"
#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/PostProcessCombineLUTs.h"
#include "PostProcess/PostProcessMobile.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"

static TAutoConsoleVariable<float> CVarTonemapperSharpen(
	TEXT("r.Tonemapper.Sharpen"),
	0,
	TEXT("Sharpening in the tonemapper (not for ES2), actual implementation is work in progress, clamped at 10\n")
	TEXT("   0: off(default)\n")
	TEXT(" 0.5: half strength\n")
	TEXT("   1: full strength"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// Note: Enables or disables HDR support for a project. Typically this would be set on a per-project/per-platform basis in defaultengine.ini
static TAutoConsoleVariable<int32> CVarAllowHDR(
	TEXT("r.AllowHDR"),
	0,
	TEXT("Creates an HDR compatible swap-chain and enables HDR display output.")
	TEXT("0: Disabled (default)\n")
	TEXT("1: Allow HDR, if supported by the platform and display \n"),
	ECVF_ReadOnly
);

// Note: These values are directly referenced in code. They are set in code at runtime and therefore cannot be set via ini files
// Please update all paths if changing
static TAutoConsoleVariable<int32> CVarDisplayColorGamut(
	TEXT("r.HDR.Display.ColorGamut"),
	0,
	TEXT("Color gamut of the output display:\n")
	TEXT("0: Rec709 / sRGB, D65 (default)\n")
	TEXT("1: DCI-P3, D65\n")
	TEXT("2: Rec2020 / BT2020, D65\n")
	TEXT("3: ACES, D60\n")
	TEXT("4: ACEScg, D60\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

// Note: These values are directly referenced in code, please update all paths if changing
static TAutoConsoleVariable<int32> CVarDisplayOutputDevice(
	TEXT("r.HDR.Display.OutputDevice"),
	0,
	TEXT("Device format of the output display:\n")
	TEXT("0: sRGB (LDR)\n")
	TEXT("1: Rec709 (LDR)\n")
	TEXT("2: Explicit gamma mapping (LDR)\n")
	TEXT("3: ACES 1000 nit ST-2084 (Dolby PQ) (HDR)\n")
	TEXT("4: ACES 2000 nit ST-2084 (Dolby PQ) (HDR)\n")
	TEXT("5: ACES 1000 nit ScRGB (HDR)\n")
	TEXT("6: ACES 2000 nit ScRGB (HDR)\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);
	
static TAutoConsoleVariable<int32> CVarHDROutputEnabled(
	TEXT("r.HDR.EnableHDROutput"),
	0,
	TEXT("Creates an HDR compatible swap-chain and enables HDR display output.")
	TEXT("0: Disabled (default)\n")
	TEXT("1: Enable hardware-specific implementation\n"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<float> CVarTonemapperGamma(
	TEXT("r.TonemapperGamma"),
	0.0f,
	TEXT("0: Default behavior\n")
	TEXT("#: Use fixed gamma # instead of sRGB or Rec709 transform"),
	ECVF_Scalability | ECVF_RenderThreadSafe);	

const int32 GTonemapComputeTileSizeX = 8;
const int32 GTonemapComputeTileSizeY = 8;

//
// TONEMAPPER PERMUTATION CONTROL
//

// Tonemapper option bitmask.
// Adjusting this requires adjusting TonemapperCostTab[].
typedef enum {
	TonemapperGammaOnly         = (1<<0),
	TonemapperColorMatrix       = (1<<1),
	TonemapperShadowTint        = (1<<2),
	TonemapperContrast          = (1<<3),
	TonemapperGrainJitter       = (1<<4),
	TonemapperGrainIntensity    = (1<<5),
	TonemapperGrainQuantization = (1<<6),
	TonemapperBloom             = (1<<7),
	TonemapperDOF               = (1<<8),
	TonemapperVignette          = (1<<9),
	TonemapperLightShafts       = (1<<10),
	Tonemapper32BPPHDR          = (1<<11),
	TonemapperColorFringe       = (1<<12),
	TonemapperMsaa              = (1<<13),
	TonemapperSharpen           = (1<<14),
} TonemapperOption;

// Tonemapper option cost (0 = no cost, 255 = max cost).
// Suggested cost should be 
// These need a 1:1 mapping with TonemapperOption enum,
static uint8 TonemapperCostTab[] = { 
	1, //TonemapperGammaOnly
	1, //TonemapperColorMatrix
	1, //TonemapperShadowTint
	1, //TonemapperContrast
	1, //TonemapperGrainJitter
	1, //TonemapperGrainIntensity
	1, //TonemapperGrainQuantization
	1, //TonemapperBloom
	1, //TonemapperDOF
	1, //TonemapperVignette
	1, //TonemapperLightShafts
	1, //TonemapperMosaic
	1, //TonemapperColorFringe
	1, //TonemapperMsaa
	1, //TonemapperSharpen
};

// Edit the following to add and remove configurations.
// This is a white list of the combinations which are compiled.
// Place most common first (faster when searching in TonemapperFindLeastExpensive()).

// List of configurations compiled for PC.
static uint32 TonemapperConfBitmaskPC[10] = {

	TonemapperBloom +
	TonemapperGrainJitter +
	TonemapperGrainIntensity +
	TonemapperGrainQuantization +
	TonemapperVignette +
	TonemapperColorFringe +
	TonemapperSharpen +
	0,

	TonemapperBloom +
	TonemapperGrainJitter +
	TonemapperGrainIntensity +
	TonemapperGrainQuantization +
	TonemapperVignette +
	TonemapperSharpen +
	0,

	TonemapperBloom +
	TonemapperGrainJitter +
	TonemapperGrainIntensity +
	TonemapperGrainQuantization +
	TonemapperVignette +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperVignette +
	TonemapperGrainQuantization +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperVignette +
	TonemapperGrainQuantization +
	0,

	TonemapperBloom +
	TonemapperSharpen +
	0,

	// same without TonemapperGrainQuantization

	TonemapperBloom +
	TonemapperGrainJitter +
	TonemapperGrainIntensity +
	TonemapperVignette +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperVignette +
	TonemapperColorFringe +
	0,

	TonemapperBloom + 
	TonemapperVignette +
	0,

	//

	TonemapperGammaOnly +
	0,
};

// List of configurations compiled for Mobile.
static uint32 TonemapperConfBitmaskMobile[39] = { 

	// 
	//  15 for NON-MOSAIC 
	//

	TonemapperGammaOnly +
	0,

	// Not supporting grain jitter or grain quantization on mobile.

	// Bloom, LightShafts, Vignette all off.
	TonemapperContrast +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	0,

	// Bloom, LightShafts, Vignette, and Vignette Color all use the same shader code in the tonemapper.
	TonemapperContrast +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	0,

	// DOF enabled.
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	0,

	// Same with grain.

	TonemapperContrast +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	// DOF enabled.
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	TonemapperGrainIntensity +
	0,

	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperLightShafts +
	TonemapperVignette +
	TonemapperDOF +
	TonemapperGrainIntensity +
	0,


	//
	// 14 for 32 bit HDR PATH
	//

	// This is 32bpp hdr without film post.
	Tonemapper32BPPHDR +
	TonemapperGammaOnly +
	0,

	Tonemapper32BPPHDR + 
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperVignette +
	TonemapperBloom +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperVignette +
	TonemapperBloom +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperVignette +
	TonemapperBloom +
	0,

	// With grain

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperGrainIntensity +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperGrainIntensity +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperGrainIntensity +
	TonemapperBloom +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperVignette +
	TonemapperGrainIntensity +
	TonemapperBloom +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperVignette +
	TonemapperGrainIntensity +
	TonemapperBloom +
	0,

	Tonemapper32BPPHDR + 
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperVignette +
	TonemapperGrainIntensity +
	TonemapperBloom +
	0,


	// 
	//  10 for MSAA
	//

	TonemapperMsaa +
	TonemapperContrast +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperBloom +
	TonemapperVignette +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperVignette +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperVignette +
	0,

	// Same with grain.
	TonemapperMsaa +
	TonemapperContrast +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperBloom +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperBloom +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

	TonemapperMsaa +
	TonemapperContrast +
	TonemapperColorMatrix +
	TonemapperShadowTint +
	TonemapperBloom +
	TonemapperVignette +
	TonemapperGrainIntensity +
	0,

};

// Returns 1 if option is defined otherwise 0.
static uint32 TonemapperIsDefined(uint32 ConfigBitmask, TonemapperOption Option)
{
	return (ConfigBitmask & Option) ? 1 : 0;
}

// This finds the least expensive configuration which supports all selected options in bitmask.
static uint32 TonemapperFindLeastExpensive(uint32* RESTRICT Table, uint32 TableEntries, uint8* RESTRICT CostTable, uint32 RequiredOptionsBitmask) 
{
	// Custom logic to insure fail cases do not happen.
	uint32 MustNotHaveBitmask = 0;
	MustNotHaveBitmask += ((RequiredOptionsBitmask & TonemapperDOF) == 0) ? TonemapperDOF : 0;
	MustNotHaveBitmask += ((RequiredOptionsBitmask & Tonemapper32BPPHDR) == 0) ? Tonemapper32BPPHDR : 0;
	MustNotHaveBitmask += ((RequiredOptionsBitmask & TonemapperMsaa) == 0) ? TonemapperMsaa : 0;

	// Search for exact match first.
	uint32 Index;
	for(Index = 0; Index < TableEntries; ++Index) 
	{
		if(Table[Index] == RequiredOptionsBitmask) 
		{
			return Index;
		}
	}
	// Search through list for best entry.
	uint32 BestIndex = TableEntries;
	uint32 BestCost = ~0;
	uint32 NotRequiredOptionsBitmask = ~RequiredOptionsBitmask;
	for(Index = 0; Index < TableEntries; ++Index) 
	{
		uint32 Bitmask = Table[Index];
		if((Bitmask & MustNotHaveBitmask) != 0)
		{
			continue; 
		}
		if((Bitmask & RequiredOptionsBitmask) != RequiredOptionsBitmask) 
		{
			// A match requires a minimum set of bits set.
			continue;
		}
		uint32 BitExtra = Bitmask & NotRequiredOptionsBitmask;
		uint32 Cost = 0;
		while(BitExtra) 
		{
			uint32 Bit = FMath::FloorLog2(BitExtra);
			Cost += CostTable[Bit];
			if(Cost > BestCost)
			{
				// Poor match.
				goto PoorMatch;
			}
			BitExtra &= ~(1<<Bit);
		}
		// Better match.
		BestCost = Cost;
		BestIndex = Index;
	PoorMatch:
		;
	}
	// Fail returns 0, the gamma only shader.
	if(BestIndex == TableEntries) BestIndex = 0;
	return BestIndex;
}

// Common conversion of engine settings into a bitmask which describes the shader options required.
static uint32 TonemapperGenerateBitmask(const FViewInfo* RESTRICT View, bool bGammaOnly, bool bMobile)
{
	check(View);

	const FSceneViewFamily* RESTRICT Family = View->Family;
	if(
		bGammaOnly ||
		(Family->EngineShowFlags.Tonemapper == 0) || 
		(Family->EngineShowFlags.PostProcessing == 0))
	{
		return TonemapperGammaOnly;
	}

	uint32 Bitmask = 0;

	const FPostProcessSettings* RESTRICT Settings = &(View->FinalPostProcessSettings);

	FVector MixerR(Settings->FilmChannelMixerRed);
	FVector MixerG(Settings->FilmChannelMixerGreen);
	FVector MixerB(Settings->FilmChannelMixerBlue);
	uint32 useColorMatrix = 0;
	if(
		(Settings->FilmSaturation != 1.0f) || 
		((MixerR - FVector(1.0f,0.0f,0.0f)).GetAbsMax() != 0.0f) || 
		((MixerG - FVector(0.0f,1.0f,0.0f)).GetAbsMax() != 0.0f) || 
		((MixerB - FVector(0.0f,0.0f,1.0f)).GetAbsMax() != 0.0f))
	{
		Bitmask += TonemapperColorMatrix;
	}

	FVector Tint(Settings->FilmWhitePoint);
	FVector TintShadow(Settings->FilmShadowTint);

	float Sharpen = CVarTonemapperSharpen.GetValueOnRenderThread();

	Bitmask += (Settings->FilmShadowTintAmount > 0.0f) ? TonemapperShadowTint     : 0;	
	Bitmask += (Settings->FilmContrast > 0.0f)         ? TonemapperContrast       : 0;
	Bitmask += (Settings->GrainIntensity > 0.0f)       ? TonemapperGrainIntensity : 0;
	Bitmask += (Settings->VignetteIntensity > 0.0f)    ? TonemapperVignette       : 0;
	Bitmask += (Sharpen > 0.0f)                        ? TonemapperSharpen        : 0;

	return Bitmask;
}

// Common post.
// These are separated because mosiac mode doesn't support them.
static uint32 TonemapperGenerateBitmaskPost(const FViewInfo* RESTRICT View)
{
	const FPostProcessSettings* RESTRICT Settings = &(View->FinalPostProcessSettings);
	const FSceneViewFamily* RESTRICT Family = View->Family;

	uint32 Bitmask = (Settings->GrainJitter > 0.0f) ? TonemapperGrainJitter : 0;

	Bitmask += (Settings->BloomIntensity > 0.0f) ? TonemapperBloom : 0;

	return Bitmask;
}

// PC only.
static uint32 TonemapperGenerateBitmaskPC(const FViewInfo* RESTRICT View, bool bGammaOnly)
{
	uint32 Bitmask = TonemapperGenerateBitmask(View, bGammaOnly, false);

	// PC doesn't support these
	Bitmask &= ~TonemapperContrast;
	Bitmask &= ~TonemapperColorMatrix;
	Bitmask &= ~TonemapperShadowTint;

	// Must early exit if gamma only.
	if(Bitmask == TonemapperGammaOnly) 
	{
		return Bitmask;
	}

	// Grain Quantization
	{
		static TConsoleVariableData<int32>* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Tonemapper.GrainQuantization"));
		int32 Value = CVar->GetValueOnRenderThread();

		if(Value > 0)
		{
			Bitmask |= TonemapperGrainQuantization;
		}
	}

	if( View->FinalPostProcessSettings.SceneFringeIntensity > 0.01f)
	{
		Bitmask |= TonemapperColorFringe;
	}

	return Bitmask + TonemapperGenerateBitmaskPost(View);
}

// Mobile only.
static uint32 TonemapperGenerateBitmaskMobile(const FViewInfo* RESTRICT View, bool bGammaOnly)
{
	check(View);

	uint32 Bitmask = TonemapperGenerateBitmask(View, bGammaOnly, true);

	bool bUse32BPPHDR = IsMobileHDR32bpp();
	bool bUseMosaic = IsMobileHDRMosaic();

	// Must early exit if gamma only.
	if(Bitmask == TonemapperGammaOnly)
	{
		return Bitmask + (bUse32BPPHDR ? Tonemapper32BPPHDR : 0);
	}

	if (bUseMosaic)
	{
		return Bitmask + Tonemapper32BPPHDR;
	}

	static const auto CVarMobileMSAA = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileMSAA"));
	const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[View->GetFeatureLevel()];
	if ((GSupportsShaderFramebufferFetch && (ShaderPlatform == SP_METAL || ShaderPlatform == SP_VULKAN_PCES3_1)) && (CVarMobileMSAA ? CVarMobileMSAA->GetValueOnAnyThread() > 1 : false))
	{
		Bitmask += TonemapperMsaa;
	}

	if (bUse32BPPHDR)
	{
		// add limited post for 32 bit encoded hdr.
		Bitmask += Tonemapper32BPPHDR;
		Bitmask += TonemapperGenerateBitmaskPost(View);
	}
	else if (GSupportsRenderTargetFormat_PF_FloatRGBA)
	{
		// add full mobile post if FP16 is supported.
		Bitmask += TonemapperGenerateBitmaskPost(View);

		bool bUseDof = GetMobileDepthOfFieldScale(*View) > 0.0f && (!View->FinalPostProcessSettings.bMobileHQGaussian || (View->GetFeatureLevel() < ERHIFeatureLevel::ES3_1));

		Bitmask += (bUseDof)					? TonemapperDOF : 0;
		Bitmask += (View->bLightShaftUse)		? TonemapperLightShafts : 0;
	}

	// Mobile is not supporting grain quantization and grain jitter currently.
	Bitmask &= ~(TonemapperGrainQuantization | TonemapperGrainJitter);
	return Bitmask;
}

void GrainPostSettings(FVector* RESTRICT const Constant, const FPostProcessSettings* RESTRICT const Settings)
{
	float GrainJitter = Settings->GrainJitter;
	float GrainIntensity = Settings->GrainIntensity;
	Constant->X = GrainIntensity;
	Constant->Y = 1.0f + (-0.5f * GrainIntensity);
	Constant->Z = GrainJitter;
}

// This code is shared by PostProcessTonemap and VisualizeHDR.
void FilmPostSetConstants(FVector4* RESTRICT const Constants, const uint32 ConfigBitmask, const FPostProcessSettings* RESTRICT const FinalPostProcessSettings, bool bMobile)
{
	uint32 UseColorMatrix = TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix);
	uint32 UseShadowTint  = TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint);
	uint32 UseContrast    = TonemapperIsDefined(ConfigBitmask, TonemapperContrast);

	// Must insure inputs are in correct range (else possible generation of NaNs).
	float InExposure = 1.0f;
	FVector InWhitePoint(FinalPostProcessSettings->FilmWhitePoint);
	float InSaturation = FMath::Clamp(FinalPostProcessSettings->FilmSaturation, 0.0f, 2.0f);
	FVector InLuma = FVector(1.0f/3.0f, 1.0f/3.0f, 1.0f/3.0f);
	FVector InMatrixR(FinalPostProcessSettings->FilmChannelMixerRed);
	FVector InMatrixG(FinalPostProcessSettings->FilmChannelMixerGreen);
	FVector InMatrixB(FinalPostProcessSettings->FilmChannelMixerBlue);
	float InContrast = FMath::Clamp(FinalPostProcessSettings->FilmContrast, 0.0f, 1.0f) + 1.0f;
	float InDynamicRange = powf(2.0f, FMath::Clamp(FinalPostProcessSettings->FilmDynamicRange, 1.0f, 4.0f));
	float InToe = (1.0f - FMath::Clamp(FinalPostProcessSettings->FilmToeAmount, 0.0f, 1.0f)) * 0.18f;
	InToe = FMath::Clamp(InToe, 0.18f/8.0f, 0.18f * (15.0f/16.0f));
	float InHeal = 1.0f - (FMath::Max(1.0f/32.0f, 1.0f - FMath::Clamp(FinalPostProcessSettings->FilmHealAmount, 0.0f, 1.0f)) * (1.0f - 0.18f)); 
	FVector InShadowTint(FinalPostProcessSettings->FilmShadowTint);
	float InShadowTintBlend = FMath::Clamp(FinalPostProcessSettings->FilmShadowTintBlend, 0.0f, 1.0f) * 64.0f;

	// Shadow tint amount enables turning off shadow tinting.
	float InShadowTintAmount = FMath::Clamp(FinalPostProcessSettings->FilmShadowTintAmount, 0.0f, 1.0f);
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

	if(UseColorMatrix)
	{
		// Final color matrix effected by saturation and exposure.
		OutMatrixR = (ColorMatrixLuma + ((InMatrixR - ColorMatrixLuma) * InSaturation)) * InExposure;
		OutMatrixG = (ColorMatrixLuma + ((InMatrixG - ColorMatrixLuma) * InSaturation)) * InExposure;
		OutMatrixB = (ColorMatrixLuma + ((InMatrixB - ColorMatrixLuma) * InSaturation)) * InExposure;
		if(UseShadowTint == 0)
		{
			OutMatrixR = OutMatrixR * InWhitePoint.X;
			OutMatrixG = OutMatrixG * InWhitePoint.Y;
			OutMatrixB = OutMatrixB * InWhitePoint.Z;
		}
	}
	else
	{
		// No color matrix fast path.
		if(UseShadowTint == 0)
		{
			OutMatrixB = InExposure * InWhitePoint;
		}
		else
		{
			// Need to drop exposure in.
			OutColorShadow_Luma *= InExposure;
			OutColorShadow_Tint1 *= InExposure;
			OutColorShadow_Tint2 *= InExposure;
		}
	}

	// Curve constants.
	float OutColorCurveCh3;
	float OutColorCurveCh0Cm1;
	float OutColorCurveCd2;
	float OutColorCurveCm0Cd0;
	float OutColorCurveCh1;
	float OutColorCurveCh2;
	float OutColorCurveCd1;
	float OutColorCurveCd3Cm3;
	float OutColorCurveCm2;

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
	float FilmSlope = FilmMidYS / (FilmMidXS);
	float FilmHiYS = 1.0f - FilmHiY;
	float FilmLoYS = FilmLoY;
	float FilmToe = FilmLoX;
	float FilmHiG = (-FilmHiYS + (FilmSlope*FilmHeal)) / (FilmSlope*FilmHeal);
	float FilmLoG = (-FilmLoYS + (FilmSlope*FilmToe)) / (FilmSlope*FilmToe);

	if(UseContrast)
	{
		// Constants.
		OutColorCurveCh1 = FilmHiYS/FilmHiG;
		OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
		OutColorCurveCh3 = FilmHiYS/(FilmSlope*FilmHiG) - FilmHiX;
		OutColorCurveCh0Cm1 = FilmHiX;
		OutColorCurveCm2 = FilmSlope;
		OutColorCurveCm0Cd0 = FilmLoX;
		OutColorCurveCd3Cm3 = FilmLoY - FilmLoX*FilmSlope;
		// Handle these separate in case of FilmLoG being 0.
		if(FilmLoG != 0.0f)
		{
			OutColorCurveCd1 = -FilmLoYS/FilmLoG;
			OutColorCurveCd2 = FilmLoYS/(FilmSlope*FilmLoG);
		}
		else
		{
			// FilmLoG being zero means dark region is a linear segment (so just continue the middle section).
			OutColorCurveCd1 = 0.0f;
			OutColorCurveCd2 = 1.0f;
			OutColorCurveCm0Cd0 = 0.0f;
			OutColorCurveCd3Cm3 = 0.0f;
		}
	}
	else
	{
		// Simplified for no dark segment.
		OutColorCurveCh1 = FilmHiYS/FilmHiG;
		OutColorCurveCh2 = -FilmHiX*(FilmHiYS/FilmHiG);
		OutColorCurveCh3 = FilmHiYS/(FilmSlope*FilmHiG) - FilmHiX;
		OutColorCurveCh0Cm1 = FilmHiX;
		// Not used.
		OutColorCurveCm2 = 0.0f;
		OutColorCurveCm0Cd0 = 0.0f;
		OutColorCurveCd3Cm3 = 0.0f;
		OutColorCurveCd1 = 0.0f;
		OutColorCurveCd2 = 0.0f;
	}

	Constants[0] = FVector4(OutMatrixR, OutColorCurveCd1);
	Constants[1] = FVector4(OutMatrixG, OutColorCurveCd3Cm3);
	Constants[2] = FVector4(OutMatrixB, OutColorCurveCm2); 
	Constants[3] = FVector4(OutColorCurveCm0Cd0, OutColorCurveCd2, OutColorCurveCh0Cm1, OutColorCurveCh3); 
	Constants[4] = FVector4(OutColorCurveCh1, OutColorCurveCh2, 0.0f, 0.0f);
	Constants[5] = FVector4(OutColorShadow_Luma, 0.0f);
	Constants[6] = FVector4(OutColorShadow_Tint1, 0.0f);
	Constants[7] = FVector4(OutColorShadow_Tint2, 0.0f);
}


BEGIN_UNIFORM_BUFFER_STRUCT(FBloomDirtMaskParameters,)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER(FVector4,Tint)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_TEXTURE(Texture2D,Mask)
	DECLARE_UNIFORM_BUFFER_STRUCT_MEMBER_SAMPLER(SamplerState,MaskSampler)
END_UNIFORM_BUFFER_STRUCT(FBloomDirtMaskParameters)
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FBloomDirtMaskParameters,TEXT("BloomDirtMask"));


static TAutoConsoleVariable<float> CVarGamma(
	TEXT("r.Gamma"),
	1.0f,
	TEXT("Gamma on output"),
	ECVF_RenderThreadSafe);

/*-----------------------------------------------------------------------------
FPostProcessTonemapShaderParameters
-----------------------------------------------------------------------------*/

template<uint32 ConfigIndex>
class FPostProcessTonemapShaderParameters
{
public:
	FPostProcessTonemapShaderParameters() {}

	FPostProcessTonemapShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		ColorScale0.Bind(ParameterMap, TEXT("ColorScale0"));
		ColorScale1.Bind(ParameterMap, TEXT("ColorScale1"));
		NoiseTexture.Bind(ParameterMap,TEXT("NoiseTexture"));
		NoiseTextureSampler.Bind(ParameterMap,TEXT("NoiseTextureSampler"));
		TexScale.Bind(ParameterMap, TEXT("TexScale"));
		TonemapperParams.Bind(ParameterMap, TEXT("TonemapperParams"));
		GrainScaleBiasJitter.Bind(ParameterMap, TEXT("GrainScaleBiasJitter"));
		ColorGradingLUT.Bind(ParameterMap, TEXT("ColorGradingLUT"));
		ColorGradingLUTSampler.Bind(ParameterMap, TEXT("ColorGradingLUTSampler"));
		InverseGamma.Bind(ParameterMap,TEXT("InverseGamma"));

		ColorMatrixR_ColorCurveCd1.Bind(ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(ParameterMap, TEXT("ColorShadow_Tint2"));
		
		OverlayColor.Bind(ParameterMap, TEXT("OverlayColor"));

		OutputDevice.Bind(ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(ParameterMap, TEXT("OutputGamut"));
		EncodeHDROutput.Bind(ParameterMap, TEXT("EncodeHDROutput"));
	}
	
	template <typename TRHICmdList, typename TRHIShader>
	void Set(TRHICmdList& RHICmdList, const TRHIShader ShaderRHI, const FRenderingCompositePassContext& Context, const TShaderUniformBufferParameter<FBloomDirtMaskParameters>& BloomDirtMaskParam)
	{
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		SetShaderValue(RHICmdList, ShaderRHI, OverlayColor, Context.View.OverlayColor);

		{
			FLinearColor Col = Settings.SceneColorTint;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale0, ColorScale);
		}
		
		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}

		{
			UTexture2D* NoiseTextureValue = GEngine->HighFrequencyNoiseTexture;

			SetTextureParameter(RHICmdList, ShaderRHI, NoiseTexture, NoiseTextureSampler, TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(), NoiseTextureValue->Resource->TextureRHI);
		}

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			// we assume the this pass runs in 1:1 pixel
			FVector2D TexScaleValue = FVector2D(InputDesc->Extent) / FVector2D(Context.View.ViewRect.Size());

			SetShaderValue(RHICmdList, ShaderRHI, TexScale, TexScaleValue);
		}
		
		{
			float Sharpen = FMath::Clamp(CVarTonemapperSharpen.GetValueOnRenderThread(), 0.0f, 10.0f);

			// /6.0 is to save one shader instruction
			FVector2D Value(Settings.VignetteIntensity, Sharpen / 6.0f);

			SetShaderValue(RHICmdList, ShaderRHI, TonemapperParams, Value);
		}

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

			// Display format
			int32 OutputGamutValue = CVarDisplayColorGamut.GetValueOnRenderThread();
			SetShaderValue(RHICmdList, ShaderRHI, OutputGamut, OutputGamutValue);

			// ScRGB output encoding
			int32 HDROutputEncodingValue = (CVarHDROutputEnabled.GetValueOnRenderThread() != 0 && (OutputDeviceValue == 5 || OutputDeviceValue == 6)) ? 1 : 0;
			SetShaderValue(RHICmdList, ShaderRHI, EncodeHDROutput, HDROutputEncodingValue);
		}

		FVector GrainValue;
		GrainPostSettings(&GrainValue, &Settings);
		SetShaderValue(RHICmdList, ShaderRHI, GrainScaleBiasJitter, GrainValue);

		if (BloomDirtMaskParam.IsBound())
		{
			FBloomDirtMaskParameters BloomDirtMaskParams;
			
			FLinearColor Col = Settings.BloomDirtMaskTint * Settings.BloomDirtMaskIntensity;
			BloomDirtMaskParams.Tint = FVector4(Col.R, Col.G, Col.B, 0.f /*unused*/);

			BloomDirtMaskParams.Mask = GSystemTextures.BlackDummy->GetRenderTargetItem().TargetableTexture;
			if(Settings.BloomDirtMask && Settings.BloomDirtMask->Resource)
			{
				BloomDirtMaskParams.Mask = Settings.BloomDirtMask->Resource->TextureRHI;
			}
			BloomDirtMaskParams.MaskSampler = TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI();

			FUniformBufferRHIRef BloomDirtMaskUB = TUniformBufferRef<FBloomDirtMaskParameters>::CreateUniformBufferImmediate(BloomDirtMaskParams, UniformBuffer_SingleDraw);
			SetUniformBufferParameter(RHICmdList, ShaderRHI, BloomDirtMaskParam, BloomDirtMaskUB);
		}

		{
			FRenderingCompositeOutputRef* OutputRef = Context.Pass->GetInput(ePId_Input3);

			const FTextureRHIRef* SrcTexture = Context.View.GetTonemappingLUTTexture();
			bool bShowErrorLog = false;
			// Use a provided tonemaping LUT (provided by a CombineLUTs pass). 
			if (!SrcTexture)
			{
				if (OutputRef && OutputRef->IsValid())
				{
					FRenderingCompositeOutput* Input = OutputRef->GetOutput();
					
					if(Input)
					{
						TRefCountPtr<IPooledRenderTarget> InputPooledElement = Input->RequestInput();
						
						if(InputPooledElement)
						{
							check(!InputPooledElement->IsFree());
							
							SrcTexture = &InputPooledElement->GetRenderTargetItem().ShaderResourceTexture;
						}
					}

					// Indicates the Tonemapper combined LUT node was nominally in the network, so error if it's not found
					bShowErrorLog = true;
				}
			}	

			if (SrcTexture && *SrcTexture)
			{
				SetTextureParameter(RHICmdList, ShaderRHI, ColorGradingLUT, ColorGradingLUTSampler, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), *SrcTexture);
			}
			else if (bShowErrorLog)
			{
				UE_LOG(LogRenderer, Error, TEXT("No Color LUT texture to sample: output will be invalid."));
			}
		}

		{
			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			{
				static TConsoleVariableData<float>* CVar = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.TonemapperGamma"));
				float Value = CVar->GetValueOnRenderThread();
				if(Value < 1.0f)
				{
					Value = 1.0f;
				}
				InvDisplayGammaValue.Z = 1.0f / Value;
			}
			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];
			FilmPostSetConstants(Constants, TonemapperConfBitmaskPC[ConfigIndex], &Context.View.FinalPostProcessSettings, false);
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

	friend FArchive& operator<<(FArchive& Ar,FPostProcessTonemapShaderParameters& P)
	{
		Ar << P.ColorScale0 << P.ColorScale1 << P.InverseGamma << P.NoiseTexture << P.NoiseTextureSampler;
		Ar << P.TexScale << P.TonemapperParams << P.GrainScaleBiasJitter;
		Ar << P.ColorGradingLUT << P.ColorGradingLUTSampler;
		Ar << P.ColorMatrixR_ColorCurveCd1 << P.ColorMatrixG_ColorCurveCd3Cm3 << P.ColorMatrixB_ColorCurveCm2 << P.ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << P.ColorCurve_Ch1_Ch2 << P.ColorShadow_Luma << P.ColorShadow_Tint1 << P.ColorShadow_Tint2;
		Ar << P.OverlayColor;
		Ar << P.OutputDevice << P.OutputGamut << P.EncodeHDROutput;

		return Ar;
	}

	FShaderParameter ColorScale0;
	FShaderParameter ColorScale1;
	FShaderResourceParameter NoiseTexture;
	FShaderResourceParameter NoiseTextureSampler;
	FShaderParameter TexScale;
	FShaderParameter TonemapperParams;
	FShaderParameter GrainScaleBiasJitter;
	FShaderResourceParameter ColorGradingLUT;
	FShaderResourceParameter ColorGradingLUTSampler;
	FShaderParameter InverseGamma;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	//@HACK
	FShaderParameter OverlayColor;

	FShaderParameter OutputDevice;
	FShaderParameter OutputGamut;
	FShaderParameter EncodeHDROutput;
};

/**
 * Encapsulates the post processing tonemapper pixel shader.
 */
template<uint32 ConfigIndex>
class FPostProcessTonemapPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapPS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::ES2);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		uint32 ConfigBitmask = TonemapperConfBitmaskPC[ConfigIndex];

		OutEnvironment.SetDefine(TEXT("USE_GAMMA_ONLY"),         TonemapperIsDefined(ConfigBitmask, TonemapperGammaOnly));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"),       TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix));
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"),        TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint));
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"),           TonemapperIsDefined(ConfigBitmask, TonemapperContrast));
		OutEnvironment.SetDefine(TEXT("USE_BLOOM"),              TonemapperIsDefined(ConfigBitmask, TonemapperBloom));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_JITTER"),       TonemapperIsDefined(ConfigBitmask, TonemapperGrainJitter));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_INTENSITY"),    TonemapperIsDefined(ConfigBitmask, TonemapperGrainIntensity));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_QUANTIZATION"), TonemapperIsDefined(ConfigBitmask, TonemapperGrainQuantization));
		OutEnvironment.SetDefine(TEXT("USE_VIGNETTE"),           TonemapperIsDefined(ConfigBitmask, TonemapperVignette));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_FRINGE"),		 TonemapperIsDefined(ConfigBitmask, TonemapperColorFringe));
		OutEnvironment.SetDefine(TEXT("USE_SHARPEN"),	         TonemapperIsDefined(ConfigBitmask, TonemapperSharpen));
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"),		 UseVolumeTextureLUT(Platform));
	}

	/** Default constructor. */
	FPostProcessTonemapPS() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FPostProcessTonemapShaderParameters<ConfigIndex> PostProcessTonemapShaderParameters;

	/** Initialization constructor. */
	FPostProcessTonemapPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
		, PostProcessTonemapShaderParameters(Initializer.ParameterMap)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter;
		Ar << PostProcessTonemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetPS(const FRenderingCompositePassContext& Context)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		{
			// filtering can cost performance so we use point where possible, we don't want anisotropic sampling
			FSamplerStateRHIParamRef Filters[] =
			{
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),		// todo: could be SF_Point if fringe is disabled
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
			};

			PostprocessParameter.SetPS(Context.RHICmdList, ShaderRHI, Context, 0, eFC_0000, Filters);
		}

		PostProcessTonemapShaderParameters.Set(Context.RHICmdList, ShaderRHI, Context, GetUniformBufferParameter<FBloomDirtMaskParameters>());
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessTonemap.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS");
	}
};

#define VARIATION1(A)																	\
	typedef FPostProcessTonemapPS<A> FPostProcessTonemapPS##A; 							\
	IMPLEMENT_SHADER_TYPE2(FPostProcessTonemapPS##A, SF_Pixel);

	VARIATION1(0)  VARIATION1(1)  VARIATION1(2)  VARIATION1(3)  VARIATION1(4)  VARIATION1(5) VARIATION1(6) VARIATION1(7) VARIATION1(8)
	VARIATION1(9)

#undef VARIATION1


// Vertex Shader permutations based on bool AutoExposure.
IMPLEMENT_SHADER_TYPE(template<>, TPostProcessTonemapVS<true>, TEXT("/Engine/Private/PostProcessTonemap.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, TPostProcessTonemapVS<false>, TEXT("/Engine/Private/PostProcessTonemap.usf"), TEXT("MainVS"), SF_Vertex);

/** Encapsulates the post processing tonemap compute shader. */
template<uint32 ConfigIndex, bool bDoEyeAdaptation>
class FPostProcessTonemapCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapCS, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}	
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		// CS params
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEX"), GTonemapComputeTileSizeX);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZEY"), GTonemapComputeTileSizeY);

		// VS params
		OutEnvironment.SetDefine(TEXT("EYEADAPTATION_EXPOSURE_FIX"), bDoEyeAdaptation ? 1 : 0);

		// PS params
		uint32 ConfigBitmask = TonemapperConfBitmaskPC[ConfigIndex];
		OutEnvironment.SetDefine(TEXT("USE_GAMMA_ONLY"),         TonemapperIsDefined(ConfigBitmask, TonemapperGammaOnly));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"),       TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix));
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"),        TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint));
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"),           TonemapperIsDefined(ConfigBitmask, TonemapperContrast));
		OutEnvironment.SetDefine(TEXT("USE_BLOOM"),              TonemapperIsDefined(ConfigBitmask, TonemapperBloom));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_JITTER"),       TonemapperIsDefined(ConfigBitmask, TonemapperGrainJitter));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_INTENSITY"),    TonemapperIsDefined(ConfigBitmask, TonemapperGrainIntensity));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_QUANTIZATION"), TonemapperIsDefined(ConfigBitmask, TonemapperGrainQuantization));
		OutEnvironment.SetDefine(TEXT("USE_VIGNETTE"),           TonemapperIsDefined(ConfigBitmask, TonemapperVignette));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_FRINGE"),		 TonemapperIsDefined(ConfigBitmask, TonemapperColorFringe));
		OutEnvironment.SetDefine(TEXT("USE_SHARPEN"),	         TonemapperIsDefined(ConfigBitmask, TonemapperSharpen));
		OutEnvironment.SetDefine(TEXT("USE_VOLUME_LUT"),		 UseVolumeTextureLUT(Platform));
	}

	/** Default constructor. */
	FPostProcessTonemapCS() {}

public:
	// CS params
	FPostProcessPassParameters PostprocessParameter;
	FRWShaderParameter OutComputeTex;
	FShaderParameter TonemapComputeParams;

	// VS params
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter FringeUVParams;
	FShaderParameter DefaultEyeExposure;

	// PS params
	FPostProcessTonemapShaderParameters<ConfigIndex> PostProcessTonemapShaderParameters;

	/** Initialization constructor. */
	FPostProcessTonemapCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
		, PostProcessTonemapShaderParameters(Initializer.ParameterMap)
	{
		// CS params
		PostprocessParameter.Bind(Initializer.ParameterMap);
		OutComputeTex.Bind(Initializer.ParameterMap, TEXT("OutComputeTex"));
		TonemapComputeParams.Bind(Initializer.ParameterMap, TEXT("TonemapComputeParams"));

		// VS params
		EyeAdaptation.Bind(Initializer.ParameterMap, TEXT("EyeAdaptation"));
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		FringeUVParams.Bind(Initializer.ParameterMap, TEXT("FringeUVParams"));
		DefaultEyeExposure.Bind(Initializer.ParameterMap, TEXT("DefaultEyeExposure"));
	}

	template <typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, const FIntPoint& DestSize, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		// CS params
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);
		OutComputeTex.SetTexture(RHICmdList, ShaderRHI, nullptr, DestUAV);		
		
		FVector4 TonemapComputeValues(0, 0, 1.f / (float)DestSize.X, 1.f / (float)DestSize.Y);
		SetShaderValue(RHICmdList, ShaderRHI, TonemapComputeParams, TonemapComputeValues);

		// VS params
		FVector GrainRandomFullValue;
		{
			uint8 FrameIndexMod8 = 0;
			if (Context.View.State)
			{
				FrameIndexMod8 = Context.View.State->GetFrameIndexMod8();
			}
			GrainRandomFromFrame(&GrainRandomFullValue, FrameIndexMod8);
		}
		SetShaderValue(RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);
		
		SetTextureParameter(RHICmdList, ShaderRHI, EyeAdaptation, EyeAdaptationTex);

		// Compile time template-based conditional
		if (!bDoEyeAdaptation)
		{
			// Compute a CPU-based default.  NB: reverts to "1" if SM5 feature level is not supported
			float DefaultEyeExposureValue = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);
			// Load a default value 
			SetShaderValue(RHICmdList, ShaderRHI, DefaultEyeExposure, DefaultEyeExposureValue);
		}

		{
			// for scene color fringe
			// from percent to fraction
			float Offset = Context.View.FinalPostProcessSettings.SceneFringeIntensity * 0.01f;
			//FVector4 Value(1.0f - Offset * 0.5f, 1.0f - Offset, 0.0f, 0.0f);

			// Wavelength of primaries in nm
			const float PrimaryR = 611.3f;
			const float PrimaryG = 549.1f;
			const float PrimaryB = 464.3f;

			// Simple lens chromatic aberration is roughly linear in wavelength
			float ScaleR = 0.007f * ( PrimaryR - PrimaryB );
			float ScaleG = 0.007f * ( PrimaryG - PrimaryB );
			FVector4 Value( 1.0f / ( 1.0f + Offset * ScaleG ), 1.0f / ( 1.0f + Offset * ScaleR ), 0.0f, 0.0f);

			// we only get bigger to not leak in content from outside
			SetShaderValue(RHICmdList, ShaderRHI, FringeUVParams, Value);
		}

		// PS params
		{
			// filtering can cost performance so we use point where possible, we don't want anisotropic sampling
			FSamplerStateRHIParamRef Filters[] =
			{
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),		// todo: could be SF_Point if fringe is disabled
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp, 0, 1>::GetRHI(),
			};

			PostprocessParameter.SetCS(ShaderRHI, Context, RHICmdList, 0, eFC_0000, Filters);
		}

		PostProcessTonemapShaderParameters.Set(RHICmdList, ShaderRHI, Context, GetUniformBufferParameter<FBloomDirtMaskParameters>());
	}

	template <typename TRHICmdList>
	void UnsetParameters(TRHICmdList& RHICmdList)
	{
		const FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		OutComputeTex.UnsetUAV(RHICmdList, ShaderRHI);
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		// CS params
		Ar << PostprocessParameter << OutComputeTex << TonemapComputeParams;

		// VS params
		Ar << GrainRandomFull << EyeAdaptation << FringeUVParams << DefaultEyeExposure;

		// PS params
		Ar << PostProcessTonemapShaderParameters;
		return bShaderHasOutdatedParameters;
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessTonemap.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainCS");
	}
};

// #define avoids a lot of code duplication
#define VARIATION1(A)																\
	typedef FPostProcessTonemapCS<A,true> FPostProcessTonemapCS_EyeAdaption##A;		\
	IMPLEMENT_SHADER_TYPE2(FPostProcessTonemapCS_EyeAdaption##A, SF_Compute);		\
	typedef FPostProcessTonemapCS<A,false> FPostProcessTonemapCS_NoEyeAdaption##A;	\
	IMPLEMENT_SHADER_TYPE2(FPostProcessTonemapCS_NoEyeAdaption##A, SF_Compute);

	VARIATION1(0)  VARIATION1(1)  VARIATION1(2)  VARIATION1(3)  VARIATION1(4)  VARIATION1(5)  VARIATION1(6)  VARIATION1(7)
	VARIATION1(8)  VARIATION1(9)
#undef VARIATION1

FRCPassPostProcessTonemap::FRCPassPostProcessTonemap(const FViewInfo& InView, bool bInDoGammaOnly, bool bInDoEyeAdaptation, bool bInHDROutput, bool bInIsComputePass)
	: bDoGammaOnly(bInDoGammaOnly)
	, bDoScreenPercentageInTonemapper(false)
	, bDoEyeAdaptation(bInDoEyeAdaptation)
	, bHDROutput(bInHDROutput)
	, View(InView)
{
	uint32 ConfigBitmask = TonemapperGenerateBitmaskPC(&InView, bDoGammaOnly);
	ConfigIndexPC = TonemapperFindLeastExpensive(TonemapperConfBitmaskPC, sizeof(TonemapperConfBitmaskPC)/4, TonemapperCostTab, ConfigBitmask);

	bIsComputePass = bInIsComputePass;
	bPreferAsyncCompute = false;
}

namespace PostProcessTonemapUtil
{
	// Template implementation supports unique static BoundShaderState for each permutation of Vertex/Pixel Shaders 
	template <uint32 ConfigIndex, bool bVSDoEyeAdaptation>
	static inline void SetShaderTempl(const FRenderingCompositePassContext& Context)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		typedef TPostProcessTonemapVS<bVSDoEyeAdaptation>				VertexShaderType;
		typedef FPostProcessTonemapPS<ConfigIndex>                      PixelShaderType;

		TShaderMapRef<PixelShaderType>  PixelShader(Context.GetShaderMap());
		TShaderMapRef<VertexShaderType> VertexShader(Context.GetShaderMap());

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetVS(Context);
		PixelShader->SetPS(Context);
	}

	template <uint32 ConfigIndex>
	static inline void SetShaderTempl(const FRenderingCompositePassContext& Context, bool bDoEyeAdaptation)
	{
		if (bDoEyeAdaptation)
		{
            SetShaderTempl<ConfigIndex, true>(Context);
		}
		else
		{
			SetShaderTempl<ConfigIndex, false>(Context);
		}
	}

	template <uint32 ConfigIndex, bool bDoEyeAdaptation, typename TRHICmdList>
	static inline void DispatchComputeShaderTmpl(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
	{
		auto ShaderMap = Context.GetShaderMap();
		TShaderMapRef<FPostProcessTonemapCS<ConfigIndex, bDoEyeAdaptation>> ComputeShader(ShaderMap);
		RHICmdList.SetComputeShader(ComputeShader->GetComputeShader());		

		FIntPoint DestSize(DestRect.Width(), DestRect.Height());
		ComputeShader->SetParameters(RHICmdList, Context, DestSize, DestUAV, EyeAdaptationTex);

		uint32 GroupSizeX = FMath::DivideAndRoundUp(DestSize.X, GTonemapComputeTileSizeX);
		uint32 GroupSizeY = FMath::DivideAndRoundUp(DestSize.Y, GTonemapComputeTileSizeY);
		DispatchComputeShader(RHICmdList, *ComputeShader, GroupSizeX, GroupSizeY, 1);

		ComputeShader->UnsetParameters(RHICmdList);
	}
}

static TAutoConsoleVariable<int32> CVarTonemapperOverride(
	TEXT("r.Tonemapper.ConfigIndexOverride"),
	-1,
	TEXT("direct configindex override. Ignores all other tonemapper configuration cvars"),
	ECVF_RenderThreadSafe);

void FRCPassPostProcessTonemap::Process(FRenderingCompositePassContext& Context)
{
	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);
	AsyncEndFence = FComputeFenceRHIRef();

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}

	const FSceneViewFamily& ViewFamily = *(View.Family);
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = bDoScreenPercentageInTonemapper ? View.UnscaledViewRect : View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DestSize = PassOutputs[0].RenderTargetDesc.Extent;
	
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessTonemap, TEXT("Tonemapper#%d%s GammaOnly=%d HandleScreenPercentage=%d  %dx%d"),
		ConfigIndexPC, bIsComputePass?TEXT("Compute"):TEXT(""), bDoGammaOnly, bDoScreenPercentageInTonemapper, DestRect.Width(), DestRect.Height());

	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(Context.RHICmdList);

	if (bIsComputePass)
	{
		DestRect = {DestRect.Min, DestRect.Min + DestSize};
		
		// Common setup
		SetRenderTarget(Context.RHICmdList, nullptr, nullptr);
		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);
		
		static FName AsyncEndFenceName(TEXT("AsyncTonemapEndFence"));
		AsyncEndFence = Context.RHICmdList.CreateComputeFence(AsyncEndFenceName);

		FTextureRHIRef EyeAdaptationTex = GWhiteTexture->TextureRHI;
		if (Context.View.HasValidEyeAdaptation())
		{
			EyeAdaptationTex = Context.View.GetEyeAdaptation(Context.RHICmdList)->GetRenderTargetItem().TargetableTexture;
		}

		if (IsAsyncComputePass())
		{
			// Async path
			FRHIAsyncComputeCommandListImmediate& RHICmdListComputeImmediate = FRHICommandListExecutor::GetImmediateAsyncComputeCommandList();
			{
				SCOPED_COMPUTE_EVENT(RHICmdListComputeImmediate, AsyncTonemap);
				WaitForInputPassComputeFences(RHICmdListComputeImmediate);
					
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
				DispatchCS(RHICmdListComputeImmediate, Context, DestRect, DestRenderTarget.UAV, EyeAdaptationTex);
				RHICmdListComputeImmediate.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
			}
			FRHIAsyncComputeCommandListImmediate::ImmediateDispatch(RHICmdListComputeImmediate);
		}
		else
		{
			// Direct path
			WaitForInputPassComputeFences(Context.RHICmdList);

			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, DestRenderTarget.UAV);
			DispatchCS(Context.RHICmdList, Context, DestRect, DestRenderTarget.UAV, EyeAdaptationTex);
			Context.RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, DestRenderTarget.UAV, AsyncEndFence);
		}
	}
	else
	{
		WaitForInputPassComputeFences(Context.RHICmdList);

		const EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[Context.GetFeatureLevel()];

		if (IsMobilePlatform(ShaderPlatform))
		{
			// clear target when processing first view in case of splitscreen
			const bool bFirstView = (&View == View.Family->Views[0]);
		
			// Full clear to avoid restore
			if ((View.StereoPass == eSSP_FULL && bFirstView) || View.StereoPass == eSSP_LEFT_EYE)
			{
				SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
			}
			else
			{
				SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());
			}
		}
		else
		{
			// Set the view family's render target/viewport.
			SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef(), ESimpleRenderTargetMode::EExistingColorAndDepth);

			if (Context.HasHmdMesh() && View.StereoPass == eSSP_LEFT_EYE)
			{
				// needed when using an hmd mesh instead of a full screen quad because we don't touch all of the pixels in the render target
				DrawClearQuad(Context.RHICmdList, FLinearColor::Black);
			}
			else if (ViewFamily.RenderTarget->GetRenderTargetTexture() != DestRenderTarget.TargetableTexture)
			{
				// needed to not have PostProcessAA leaking in content (e.g. Matinee black borders), is optimized away if possible (RT size=view size, )
				DrawClearQuad(Context.RHICmdList, true, FLinearColor::Black, false, 0, false, 0, PassOutputs[0].RenderTargetDesc.Extent, DestRect);
			}
		}

		Context.SetViewportAndCallRHI(DestRect, 0.0f, 1.0f);

		const int32 ConfigOverride = CVarTonemapperOverride->GetInt();
		const uint32 FinalConfigIndex = ConfigOverride == -1 ? ConfigIndexPC : (int32)ConfigOverride;
		switch (FinalConfigIndex)
		{
		using namespace PostProcessTonemapUtil;
		case 0:	SetShaderTempl<0>(Context, bDoEyeAdaptation); break;
		case 1:	SetShaderTempl<1>(Context, bDoEyeAdaptation); break;
		case 2: SetShaderTempl<2>(Context, bDoEyeAdaptation); break;
		case 3: SetShaderTempl<3>(Context, bDoEyeAdaptation); break;
		case 4: SetShaderTempl<4>(Context, bDoEyeAdaptation); break;
		case 5: SetShaderTempl<5>(Context, bDoEyeAdaptation); break;
		case 6: SetShaderTempl<6>(Context, bDoEyeAdaptation); break;
		case 7: SetShaderTempl<7>(Context, bDoEyeAdaptation); break;
		case 8: SetShaderTempl<8>(Context, bDoEyeAdaptation); break;
		case 9: SetShaderTempl<9>(Context, bDoEyeAdaptation); break;
		default:
			check(0);
		}

		FShader* VertexShader;
		if (bDoEyeAdaptation)
		{
			// Use the vertex shader that passes on eye-adaptation values to the pixel shader
			TShaderMapRef<TPostProcessTonemapVS<true>> VertexShaderMapRef(Context.GetShaderMap());
			VertexShader = *VertexShaderMapRef;
		}
		else
		{
			TShaderMapRef<TPostProcessTonemapVS<false>> VertexShaderMapRef(Context.GetShaderMap());
			VertexShader = *VertexShaderMapRef;
		}

		DrawPostProcessPass(
			Context.RHICmdList,
			0, 0,
			DestRect.Width(), DestRect.Height(),
			View.ViewRect.Min.X, View.ViewRect.Min.Y,
			View.ViewRect.Width(), View.ViewRect.Height(),
			DestRect.Size(),
			SceneContext.GetBufferSizeXY(),
			VertexShader,
			View.StereoPass,
			Context.HasHmdMesh(),
			EDRF_UseTriangleOptimization);

		Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());

		// We only release the SceneColor after the last view was processed (SplitScreen)
		if(Context.View.Family->Views[Context.View.Family->Views.Num() - 1] == &Context.View && !GIsEditor)
		{
			// The RT should be released as early as possible to allow sharing of that memory for other purposes.
			// This becomes even more important with some limited VRam (XBoxOne).
			SceneContext.SetSceneColor(0);
		}
	}
}

template <typename TRHICmdList>
void FRCPassPostProcessTonemap::DispatchCS(TRHICmdList& RHICmdList, FRenderingCompositePassContext& Context, const FIntRect& DestRect, FUnorderedAccessViewRHIParamRef DestUAV, FTextureRHIParamRef EyeAdaptationTex)
{
#define DISPATCH_CASE(A)																				\
	case A:	bDoEyeAdaptation																			\
		? DispatchComputeShaderTmpl<A, true>(RHICmdList, Context, DestRect, DestUAV, EyeAdaptationTex)	\
		: DispatchComputeShaderTmpl<A, false>(RHICmdList, Context, DestRect, DestUAV, EyeAdaptationTex);\
	break;

	const int32 ConfigOverride = CVarTonemapperOverride->GetInt();
	const uint32 FinalConfigIndex = ConfigOverride == -1 ? ConfigIndexPC : (int32)ConfigOverride;
	switch (FinalConfigIndex)
	{
	using namespace PostProcessTonemapUtil;
	DISPATCH_CASE(0);
	DISPATCH_CASE(1);
	DISPATCH_CASE(2);
	DISPATCH_CASE(3);
	DISPATCH_CASE(4);
	DISPATCH_CASE(5);
	DISPATCH_CASE(6);
	DISPATCH_CASE(7);
	DISPATCH_CASE(8);
	DISPATCH_CASE(9);
	default: check(0);
	}

#undef DISPATCH_CASE
}

FPooledRenderTargetDesc FRCPassPostProcessTonemap::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();

	Ret.TargetableFlags &= ~(TexCreate_RenderTargetable | TexCreate_UAV);
	Ret.TargetableFlags |= bIsComputePass ? TexCreate_UAV : TexCreate_RenderTargetable;
	Ret.Format = bIsComputePass ? PF_R8G8B8A8 : PF_B8G8R8A8;

	// RGB is the color in LDR, A is the luminance for PostprocessAA
	Ret.Format = bHDROutput ? GRHIHDRDisplayOutputFormat : Ret.Format;
	Ret.DebugName = TEXT("Tonemap");
	Ret.ClearValue = FClearValueBinding(FLinearColor(0, 0, 0, 0));
	Ret.Flags |= GFastVRamConfig.Tonemap;

	// Mobile needs to override the extent
	if (bDoScreenPercentageInTonemapper && View.GetFeatureLevel() <= ERHIFeatureLevel::ES3_1)
	{
		Ret.Extent = View.UnscaledViewRect.Max;
	}
	return Ret;
}





// ES2 version

/**
 * Encapsulates the post processing tonemapper pixel shader.
 */
template<uint32 ConfigIndex>
class FPostProcessTonemapPS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapPS_ES2, Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		const uint32 ConfigBitmask = TonemapperConfBitmaskMobile[ConfigIndex];

		// Only cache for ES2/3.1 shader platforms, and only compile 32bpp shaders for Android or PC emulation
		return IsMobilePlatform(Platform) && 
			(!TonemapperIsDefined(ConfigBitmask, Tonemapper32BPPHDR) || Platform == SP_OPENGL_ES2_ANDROID || (IsMobilePlatform(Platform) && IsPCPlatform(Platform)));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform, OutEnvironment);

		const uint32 ConfigBitmask = TonemapperConfBitmaskMobile[ConfigIndex];

		OutEnvironment.SetDefine(TEXT("USE_GAMMA_ONLY"),         TonemapperIsDefined(ConfigBitmask, TonemapperGammaOnly));
		OutEnvironment.SetDefine(TEXT("USE_COLOR_MATRIX"),       TonemapperIsDefined(ConfigBitmask, TonemapperColorMatrix));
		OutEnvironment.SetDefine(TEXT("USE_SHADOW_TINT"),        TonemapperIsDefined(ConfigBitmask, TonemapperShadowTint));
		OutEnvironment.SetDefine(TEXT("USE_CONTRAST"),           TonemapperIsDefined(ConfigBitmask, TonemapperContrast));
		OutEnvironment.SetDefine(TEXT("USE_32BPP_HDR"),          TonemapperIsDefined(ConfigBitmask, Tonemapper32BPPHDR));
		OutEnvironment.SetDefine(TEXT("USE_BLOOM"),              TonemapperIsDefined(ConfigBitmask, TonemapperBloom));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_JITTER"),       TonemapperIsDefined(ConfigBitmask, TonemapperGrainJitter));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_INTENSITY"),    TonemapperIsDefined(ConfigBitmask, TonemapperGrainIntensity));
		OutEnvironment.SetDefine(TEXT("USE_GRAIN_QUANTIZATION"), TonemapperIsDefined(ConfigBitmask, TonemapperGrainQuantization));
		OutEnvironment.SetDefine(TEXT("USE_VIGNETTE"),           TonemapperIsDefined(ConfigBitmask, TonemapperVignette));
		OutEnvironment.SetDefine(TEXT("USE_LIGHT_SHAFTS"),       TonemapperIsDefined(ConfigBitmask, TonemapperLightShafts));
		OutEnvironment.SetDefine(TEXT("USE_DOF"),                TonemapperIsDefined(ConfigBitmask, TonemapperDOF));
		OutEnvironment.SetDefine(TEXT("USE_MSAA"),               TonemapperIsDefined(ConfigBitmask, TonemapperMsaa));

		//Need to hack in exposure scale for < SM5
		OutEnvironment.SetDefine(TEXT("NO_EYEADAPTATION_EXPOSURE_FIX"), 1);
	}

	/** Default constructor. */
	FPostProcessTonemapPS_ES2() {}

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderParameter ColorScale0;
	FShaderParameter ColorScale1;
	FShaderParameter TexScale;
	FShaderParameter GrainScaleBiasJitter;
	FShaderParameter InverseGamma;
	FShaderParameter TonemapperParams;

	FShaderParameter ColorMatrixR_ColorCurveCd1;
	FShaderParameter ColorMatrixG_ColorCurveCd3Cm3;
	FShaderParameter ColorMatrixB_ColorCurveCm2;
	FShaderParameter ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3;
	FShaderParameter ColorCurve_Ch1_Ch2;
	FShaderParameter ColorShadow_Luma;
	FShaderParameter ColorShadow_Tint1;
	FShaderParameter ColorShadow_Tint2;

	FShaderParameter OverlayColor;
	FShaderParameter FringeIntensity;
	FShaderParameter SRGBAwareTargetParam;
	FShaderParameter DefaultEyeExposure;

	/** Initialization constructor. */
	FPostProcessTonemapPS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		ColorScale0.Bind(Initializer.ParameterMap, TEXT("ColorScale0"));
		ColorScale1.Bind(Initializer.ParameterMap, TEXT("ColorScale1"));
		TexScale.Bind(Initializer.ParameterMap, TEXT("TexScale"));
		TonemapperParams.Bind(Initializer.ParameterMap, TEXT("TonemapperParams"));
		GrainScaleBiasJitter.Bind(Initializer.ParameterMap, TEXT("GrainScaleBiasJitter"));
		InverseGamma.Bind(Initializer.ParameterMap,TEXT("InverseGamma"));

		ColorMatrixR_ColorCurveCd1.Bind(Initializer.ParameterMap, TEXT("ColorMatrixR_ColorCurveCd1"));
		ColorMatrixG_ColorCurveCd3Cm3.Bind(Initializer.ParameterMap, TEXT("ColorMatrixG_ColorCurveCd3Cm3"));
		ColorMatrixB_ColorCurveCm2.Bind(Initializer.ParameterMap, TEXT("ColorMatrixB_ColorCurveCm2"));
		ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3"));
		ColorCurve_Ch1_Ch2.Bind(Initializer.ParameterMap, TEXT("ColorCurve_Ch1_Ch2"));
		ColorShadow_Luma.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Luma"));
		ColorShadow_Tint1.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint1"));
		ColorShadow_Tint2.Bind(Initializer.ParameterMap, TEXT("ColorShadow_Tint2"));

		OverlayColor.Bind(Initializer.ParameterMap, TEXT("OverlayColor"));
		FringeIntensity.Bind(Initializer.ParameterMap, TEXT("FringeIntensity"));

		SRGBAwareTargetParam.Bind(Initializer.ParameterMap, TEXT("SRGBAwareTarget"));

		DefaultEyeExposure.Bind(Initializer.ParameterMap, TEXT("DefaultEyeExposure"));
	}
	
	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar  << PostprocessParameter << ColorScale0 << ColorScale1 << InverseGamma
			<< TexScale << GrainScaleBiasJitter << TonemapperParams
			<< ColorMatrixR_ColorCurveCd1 << ColorMatrixG_ColorCurveCd3Cm3 << ColorMatrixB_ColorCurveCm2 << ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3 << ColorCurve_Ch1_Ch2 << ColorShadow_Luma << ColorShadow_Tint1 << ColorShadow_Tint2
			<< OverlayColor
			<< FringeIntensity
			<< SRGBAwareTargetParam
			<< DefaultEyeExposure;

		return bShaderHasOutdatedParameters;
	}

	template <typename TRHICmdList>
	void SetPS(TRHICmdList& RHICmdList, const FRenderingCompositePassContext& Context, bool bSRGBAwareTarget)
	{
		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		const FSceneViewFamily& ViewFamily = *(Context.View.Family);

		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();
		
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		const uint32 ConfigBitmask = TonemapperConfBitmaskMobile[ConfigIndex];

		if (TonemapperIsDefined(ConfigBitmask, Tonemapper32BPPHDR) && IsMobileHDRMosaic())
		{
			PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		}
		else
		{
			PostprocessParameter.SetPS(RHICmdList, ShaderRHI, Context, TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI());
		}
			
		SetShaderValue(RHICmdList, ShaderRHI, OverlayColor, Context.View.OverlayColor);
		SetShaderValue(RHICmdList, ShaderRHI, FringeIntensity, fabsf(Settings.SceneFringeIntensity) * 0.01f); // Interpreted as [0-1] percentage

		{
			FLinearColor Col = Settings.SceneColorTint;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale0, ColorScale);
		}
		
		{
			FLinearColor Col = FLinearColor::White * Settings.BloomIntensity;
			FVector4 ColorScale(Col.R, Col.G, Col.B, 0);
			SetShaderValue(RHICmdList, ShaderRHI, ColorScale1, ColorScale);
		}

		{
			const FPooledRenderTargetDesc* InputDesc = Context.Pass->GetInputDesc(ePId_Input0);

			// we assume the this pass runs in 1:1 pixel
			FVector2D TexScaleValue = FVector2D(InputDesc->Extent) / FVector2D(Context.View.ViewRect.Size());

			SetShaderValue(RHICmdList, ShaderRHI, TexScale, TexScaleValue);
		}

		{
			float Sharpen = FMath::Clamp(CVarTonemapperSharpen.GetValueOnRenderThread(), 0.0f, 10.0f);

			FVector2D Value(Settings.VignetteIntensity, Sharpen);

			SetShaderValue(RHICmdList, ShaderRHI, TonemapperParams, Value);
		}

		FVector GrainValue;
		GrainPostSettings(&GrainValue, &Settings);
		SetShaderValue(RHICmdList, ShaderRHI, GrainScaleBiasJitter, GrainValue);

		{
			FVector InvDisplayGammaValue;
			InvDisplayGammaValue.X = 1.0f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Y = 2.2f / ViewFamily.RenderTarget->GetDisplayGamma();
			InvDisplayGammaValue.Z = 1.0; // Unused on mobile.
			SetShaderValue(RHICmdList, ShaderRHI, InverseGamma, InvDisplayGammaValue);
		}

		{
			FVector4 Constants[8];
			FilmPostSetConstants(Constants, TonemapperConfBitmaskMobile[ConfigIndex], &Context.View.FinalPostProcessSettings, true);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixR_ColorCurveCd1, Constants[0]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixG_ColorCurveCd3Cm3, Constants[1]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorMatrixB_ColorCurveCm2, Constants[2]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Cm0Cd0_Cd2_Ch0Cm1_Ch3, Constants[3]); 
			SetShaderValue(RHICmdList, ShaderRHI, ColorCurve_Ch1_Ch2, Constants[4]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Luma, Constants[5]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint1, Constants[6]);
			SetShaderValue(RHICmdList, ShaderRHI, ColorShadow_Tint2, Constants[7]);
		}

		SetShaderValue(RHICmdList, ShaderRHI, SRGBAwareTargetParam, bSRGBAwareTarget ? 1.0f : 0.0f );

		float DefaultEyeExposureValue = FRCPassPostProcessEyeAdaptation::ComputeExposureScaleValue(Context.View);
		SetShaderValue(RHICmdList, ShaderRHI, DefaultEyeExposure, DefaultEyeExposureValue);
	}
	
	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/PostProcessTonemap.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("MainPS_ES2");
	}
};

// #define avoids a lot of code duplication
#define VARIATION2(A) typedef FPostProcessTonemapPS_ES2<A> FPostProcessTonemapPS_ES2##A; \
	IMPLEMENT_SHADER_TYPE2(FPostProcessTonemapPS_ES2##A, SF_Pixel);

	VARIATION2(0)  VARIATION2(1)  VARIATION2(2)  VARIATION2(3)	VARIATION2(4)  VARIATION2(5)  VARIATION2(6)  VARIATION2(7)  VARIATION2(8)  VARIATION2(9)  
	VARIATION2(10) VARIATION2(11) VARIATION2(12) VARIATION2(13)	VARIATION2(14) VARIATION2(15) VARIATION2(16) VARIATION2(17) VARIATION2(18) VARIATION2(19)  
	VARIATION2(20) VARIATION2(21) VARIATION2(22) VARIATION2(23)	VARIATION2(24) VARIATION2(25) VARIATION2(26) VARIATION2(27) VARIATION2(28) VARIATION2(29)
	VARIATION2(30) VARIATION2(31) VARIATION2(32) VARIATION2(33)	VARIATION2(34) VARIATION2(35) VARIATION2(36) VARIATION2(37) VARIATION2(38)


#undef VARIATION2
	

class FPostProcessTonemapVS_ES2 : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FPostProcessTonemapVS_ES2,Global);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return !IsConsolePlatform(Platform);
	}

	FPostProcessTonemapVS_ES2() { }

public:
	FPostProcessPassParameters PostprocessParameter;
	FShaderResourceParameter EyeAdaptation;
	FShaderParameter GrainRandomFull;
	FShaderParameter FringeIntensity;
	bool bUsedFramebufferFetch;

	FPostProcessTonemapVS_ES2(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PostprocessParameter.Bind(Initializer.ParameterMap);
		GrainRandomFull.Bind(Initializer.ParameterMap, TEXT("GrainRandomFull"));
		FringeIntensity.Bind(Initializer.ParameterMap, TEXT("FringeIntensity"));
	}

	void SetVS(const FRenderingCompositePassContext& Context)
	{
		const FVertexShaderRHIParamRef ShaderRHI = GetVertexShader();
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(Context.RHICmdList, ShaderRHI, Context.View.ViewUniformBuffer);

		PostprocessParameter.SetVS(ShaderRHI, Context, TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI());

		FVector GrainRandomFullValue;
		{
			uint8 FrameIndexMod8 = 0;
			if (Context.View.State)
			{
				FrameIndexMod8 = Context.View.State->GetFrameIndexMod8();
			}
			GrainRandomFromFrame(&GrainRandomFullValue, FrameIndexMod8);
		}

		// TODO: Don't use full on mobile with framebuffer fetch.
		GrainRandomFullValue.Z = bUsedFramebufferFetch ? 0.0f : 1.0f;
		SetShaderValue(Context.RHICmdList, ShaderRHI, GrainRandomFull, GrainRandomFullValue);

		const FPostProcessSettings& Settings = Context.View.FinalPostProcessSettings;
		SetShaderValue(Context.RHICmdList, ShaderRHI, FringeIntensity, fabsf(Settings.SceneFringeIntensity) * 0.01f); // Interpreted as [0-1] percentage
	}
	
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << PostprocessParameter << GrainRandomFull << FringeIntensity;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(,FPostProcessTonemapVS_ES2,TEXT("/Engine/Private/PostProcessTonemap.usf"),TEXT("MainVS_ES2"),SF_Vertex);

namespace PostProcessTonemap_ES2Util
{
	// Template implementation supports unique static BoundShaderState for each permutation of Pixel Shaders 
	template <uint32 ConfigIndex>
	static inline void SetShaderTemplES2(const FRenderingCompositePassContext& Context, bool bUsedFramebufferFetch, bool bSRGBAwareTarget)
	{
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		Context.RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		TShaderMapRef<FPostProcessTonemapVS_ES2> VertexShader(Context.GetShaderMap());
		TShaderMapRef<FPostProcessTonemapPS_ES2<ConfigIndex> > PixelShader(Context.GetShaderMap());

		VertexShader->bUsedFramebufferFetch = bUsedFramebufferFetch;

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(Context.RHICmdList, GraphicsPSOInit);

		VertexShader->SetVS(Context);
		PixelShader->SetPS(Context.RHICmdList, Context, bSRGBAwareTarget);
	}
}

FRCPassPostProcessTonemapES2::FRCPassPostProcessTonemapES2(const FViewInfo& InView, bool bInUsedFramebufferFetch, bool bInSRGBAwareTarget)
	: bDoScreenPercentageInTonemapper(false)
	, View(InView)
	, bUsedFramebufferFetch(bInUsedFramebufferFetch)
	, bSRGBAwareTarget(bInSRGBAwareTarget)
{
	uint32 ConfigBitmask = TonemapperGenerateBitmaskMobile(&View, false);
	ConfigIndexMobile = TonemapperFindLeastExpensive(TonemapperConfBitmaskMobile, sizeof(TonemapperConfBitmaskMobile)/4, TonemapperCostTab, ConfigBitmask);
}

void FRCPassPostProcessTonemapES2::Process(FRenderingCompositePassContext& Context)
{
	SCOPED_DRAW_EVENTF(Context.RHICmdList, PostProcessTonemap, TEXT("Tonemapper#%d%s"), ConfigIndexMobile, bUsedFramebufferFetch ? TEXT(" FramebufferFetch=0") : TEXT("FramebufferFetch=1"));

	const FPooledRenderTargetDesc* InputDesc = GetInputDesc(ePId_Input0);

	if(!InputDesc)
	{
		// input is not hooked up correctly
		return;
	}
	
	const FSceneViewFamily& ViewFamily = *(View.Family);
	const FSceneRenderTargetItem& DestRenderTarget = PassOutputs[0].RequestSurface(Context);
	const FPooledRenderTargetDesc& OutputDesc = PassOutputs[0].RenderTargetDesc;

	// no upscale if separate ren target is used.
	FIntRect SrcRect = View.ViewRect;
	FIntRect DestRect = bDoScreenPercentageInTonemapper ? View.UnscaledViewRect : View.ViewRect;
	FIntPoint SrcSize = InputDesc->Extent;
	FIntPoint DstSize = OutputDesc.Extent;

	// Set the view family's render target/viewport.
	{
		// clear target when processing first view in case of splitscreen
		const bool bFirstView = (&View == View.Family->Views[0]);
		
		// Full clear to avoid restore
		if ((View.StereoPass == eSSP_FULL && bFirstView) || View.StereoPass == eSSP_LEFT_EYE)
		{
			SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef(), ESimpleRenderTargetMode::EClearColorAndDepth);
		}
		else
		{
			SetRenderTarget(Context.RHICmdList, DestRenderTarget.TargetableTexture, FTextureRHIParamRef());
		}
	}

	Context.SetViewportAndCallRHI(DestRect);

	const int32 ConfigOverride = CVarTonemapperOverride->GetInt();
	const uint32 FinalConfigIndex = ConfigOverride == -1 ? ConfigIndexMobile : (int32)ConfigOverride;
	
	switch(FinalConfigIndex)
	{
		using namespace PostProcessTonemap_ES2Util;
		case 0:	SetShaderTemplES2<0>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 1:	SetShaderTemplES2<1>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 2:	SetShaderTemplES2<2>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 3:	SetShaderTemplES2<3>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 4:	SetShaderTemplES2<4>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 5:	SetShaderTemplES2<5>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 6:	SetShaderTemplES2<6>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 7:	SetShaderTemplES2<7>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 8:	SetShaderTemplES2<8>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 9:	SetShaderTemplES2<9>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 10: SetShaderTemplES2<10>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 11: SetShaderTemplES2<11>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 12: SetShaderTemplES2<12>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 13: SetShaderTemplES2<13>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 14: SetShaderTemplES2<14>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 15: SetShaderTemplES2<15>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 16: SetShaderTemplES2<16>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 17: SetShaderTemplES2<17>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 18: SetShaderTemplES2<18>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 19: SetShaderTemplES2<19>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 20: SetShaderTemplES2<20>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 21: SetShaderTemplES2<21>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 22: SetShaderTemplES2<22>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 23: SetShaderTemplES2<23>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 24: SetShaderTemplES2<24>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 25: SetShaderTemplES2<25>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 26: SetShaderTemplES2<26>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 27: SetShaderTemplES2<27>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 28: SetShaderTemplES2<28>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 29: SetShaderTemplES2<29>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 30: SetShaderTemplES2<30>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 31: SetShaderTemplES2<31>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 32: SetShaderTemplES2<32>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 33: SetShaderTemplES2<33>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 34: SetShaderTemplES2<34>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 35: SetShaderTemplES2<35>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 36: SetShaderTemplES2<36>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 37: SetShaderTemplES2<37>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		case 38: SetShaderTemplES2<38>(Context, bUsedFramebufferFetch, bSRGBAwareTarget); break;
		default:
			check(0);
	}

	// Draw a quad mapping scene color to the view's render target
	TShaderMapRef<FPostProcessTonemapVS_ES2> VertexShader(Context.GetShaderMap());

	DrawRectangle(
		Context.RHICmdList,
		0, 0,
		DstSize.X, DstSize.Y,
		SrcRect.Min.X, SrcRect.Min.Y,
		SrcRect.Width(), SrcRect.Height(),
		DstSize,
		SrcSize,
		*VertexShader,
		EDRF_UseTriangleOptimization);

	Context.RHICmdList.CopyToResolveTarget(DestRenderTarget.TargetableTexture, DestRenderTarget.ShaderResourceTexture, false, FResolveParams());
}

FPooledRenderTargetDesc FRCPassPostProcessTonemapES2::ComputeOutputDesc(EPassOutputId InPassOutputId) const
{
	FPooledRenderTargetDesc Ret = GetInput(ePId_Input0)->GetOutput()->RenderTargetDesc;

	Ret.Reset();
	Ret.Format = PF_B8G8R8A8;
	Ret.DebugName = TEXT("Tonemap");
	Ret.ClearValue = FClearValueBinding(FLinearColor::Black);
	if (bDoScreenPercentageInTonemapper)
	{
		Ret.Extent = View.UnscaledViewRect.Max;
	}
	return Ret;
}
