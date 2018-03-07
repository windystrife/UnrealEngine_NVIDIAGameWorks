// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MobileBasePassRendering.h: base pass rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "HitProxies.h"
#include "RHIStaticStates.h"
#include "SceneManagement.h"
#include "PrimitiveSceneInfo.h"
#include "DrawingPolicy.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightMapRendering.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "DebugViewModeRendering.h"
#include "FogRendering.h"
#include "EditorCompositeParams.h"
#include "PlanarReflectionRendering.h"
#include "BasePassRendering.h"

class FPlanarReflectionSceneProxy;
class FScene;

enum EOutputFormat
{
	LDR_GAMMA_32,
	HDR_LINEAR_64,
};

#define MAX_BASEPASS_DYNAMIC_POINT_LIGHTS 4

const FLightSceneInfo* GetSceneMobileDirectionalLights(FScene const* Scene, uint32 LightChannel);

/* Info for dynamic point lights rendered in base pass */
class FMobileBasePassDynamicPointLightInfo
{
public:
	FMobileBasePassDynamicPointLightInfo(const FPrimitiveSceneProxy* InSceneProxy);

	int32 NumDynamicPointLights;
	FVector4 LightPositionAndInvRadius[MAX_BASEPASS_DYNAMIC_POINT_LIGHTS];
	FVector4 LightColorAndFalloffExponent[MAX_BASEPASS_DYNAMIC_POINT_LIGHTS];
};

static bool ShouldCacheShaderByPlatformAndOutputFormat(EShaderPlatform Platform, EOutputFormat OutputFormat)
{
	bool bSupportsMobileHDR = IsMobileHDR();
	bool bShaderUsesLDR = (OutputFormat == LDR_GAMMA_32);

	// only cache this shader if the LDR/HDR output matches what we currently support.  IsMobileHDR can't change, so we don't need
	// the LDR shaders if we are doing HDR, and vice-versa.
	return (bShaderUsesLDR && !bSupportsMobileHDR) || (!bShaderUsesLDR && bSupportsMobileHDR);
}

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename VertexParametersType>
class TMobileBasePassVSPolicyParamType : public FMeshMaterialShader, public VertexParametersType
{
protected:

	TMobileBasePassVSPolicyParamType() {}
	TMobileBasePassVSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		VertexParametersType::Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		MobileMultiViewMaskParameter.Bind(Initializer.ParameterMap, TEXT("MobileMultiViewMask"));
	}

public:

	// static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		VertexParametersType::Serialize(Ar);
		Ar << HeightFogParameters;
		Ar << MobileMultiViewMaskParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FMaterial& InMaterialResource,
		const FSceneView& View,
		ESceneRenderTargetsMode::Type TextureMode
		)
	{
		HeightFogParameters.Set(RHICmdList, GetVertexShader(), &View);
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,InMaterialResource,View,View.ViewUniformBuffer,TextureMode);

		if (MobileMultiViewMaskParameter.IsBound())
		{
			// Default is no masking
			SetShaderValue(RHICmdList, GetVertexShader(), MobileMultiViewMaskParameter, -1);
		}
	}

	void SetMobileMultiViewMask(FRHICommandList& RHICmdList, const int32 EyeIndex)
	{
		if (EyeIndex >= 0 && MobileMultiViewMaskParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), MobileMultiViewMaskParameter, EyeIndex);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

private:
	FHeightFogShaderParameters HeightFogParameters;
	FShaderParameter MobileMultiViewMaskParameter;
};

template<typename LightMapPolicyType>
class TMobileBasePassVSBaseType : public TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>
{
	typedef TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType> Super;

protected:

	TMobileBasePassVSBaseType() {}
	TMobileBasePassVSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return IsMobilePlatform(Platform) && LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat >
class TMobileBasePassVS : public TMobileBasePassVSBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TMobileBasePassVS,MeshMaterial);
public:
	
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		return TMobileBasePassVSBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType) && ShouldCacheShaderByPlatformAndOutputFormat(Platform,OutputFormat);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		TMobileBasePassVSBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine( TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32 ? 1u : 0u );
	}
	
	/** Initialization constructor. */
	TMobileBasePassVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TMobileBasePassVSBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TMobileBasePassVS() {}
};


/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */

template<typename PixelParametersType, int32 NumDynamicPointLights>
class TMobileBasePassPSPolicyParamType : public FMeshMaterialShader, public PixelParametersType
{
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// We compile the point light shader combinations based on the project settings
		static auto* MobileDynamicPointLightsUseStaticBranchCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileDynamicPointLightsUseStaticBranch"));
		static auto* MobileNumDynamicPointLightsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileNumDynamicPointLights"));
		const bool bMobileDynamicPointLightsUseStaticBranch = (MobileDynamicPointLightsUseStaticBranchCVar->GetValueOnAnyThread() == 1);
		const int32 MobileNumDynamicPointLights = MobileNumDynamicPointLightsCVar->GetValueOnAnyThread();
		const bool bIsUnlit = Material->GetShadingModel() == MSM_Unlit;

		return IsMobilePlatform(Platform) &&
			(NumDynamicPointLights == 0 ||
			(!bIsUnlit && NumDynamicPointLights == INT32_MAX && bMobileDynamicPointLightsUseStaticBranch && MobileNumDynamicPointLights > 0) ||	// single shader for variable number of point lights
			(!bIsUnlit && NumDynamicPointLights <= MobileNumDynamicPointLights && !bMobileDynamicPointLightsUseStaticBranch));					// unique 1...N point light shaders
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		if (NumDynamicPointLights == INT32_MAX)
		{
			OutEnvironment.SetDefine(TEXT("MAX_DYNAMIC_POINT_LIGHTS"), (uint32)MAX_BASEPASS_DYNAMIC_POINT_LIGHTS);
			OutEnvironment.SetDefine(TEXT("VARIABLE_NUM_DYNAMIC_POINT_LIGHTS"), (uint32)1);
		}
		else
		{
			OutEnvironment.SetDefine(TEXT("MAX_DYNAMIC_POINT_LIGHTS"), (uint32)NumDynamicPointLights);
			OutEnvironment.SetDefine(TEXT("VARIABLE_NUM_DYNAMIC_POINT_LIGHTS"), (uint32)0);
			OutEnvironment.SetDefine(TEXT("NUM_DYNAMIC_POINT_LIGHTS"), (uint32)NumDynamicPointLights);
		}
		// Modify compilation environment depending upon material shader quality level settings.
		ModifyCompilationEnvironmentForQualityLevel(Platform, Material->GetQualityLevel(), OutEnvironment);
	}

	/** Initialization constructor. */
	TMobileBasePassPSPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PixelParametersType::Bind(Initializer.ParameterMap);
		ReflectionCubemap.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap"));
		ReflectionSampler.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler"));
		InvReflectionCubemapAverageBrightness.Bind(Initializer.ParameterMap, TEXT("InvReflectionCubemapAverageBrightness"));
		EditorCompositeParams.Bind(Initializer.ParameterMap);
		LightPositionAndInvRadiusParameter.Bind(Initializer.ParameterMap, TEXT("LightPositionAndInvRadius"));
		LightColorAndFalloffExponentParameter.Bind(Initializer.ParameterMap, TEXT("LightColorAndFalloffExponent"));
		if (NumDynamicPointLights == INT32_MAX)
		{
			NumDynamicPointLightsParameter.Bind(Initializer.ParameterMap, TEXT("NumDynamicPointLights"));
		}

		ReflectionPositionsAndRadii.Bind(Initializer.ParameterMap, TEXT("ReflectionPositionsAndRadii"));
		ReflectionCubemap1.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap1"));
		ReflectionSampler1.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler1"));
		ReflectionCubemap2.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemap2"));
		ReflectionSampler2.Bind(Initializer.ParameterMap, TEXT("ReflectionCubemapSampler2"));

		PlanarReflectionParams.Bind(Initializer.ParameterMap);
	}
	TMobileBasePassPSPolicyParamType() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy, const FMaterial& MaterialResource, const FSceneView* View, ESceneRenderTargetsMode::Type TextureMode, bool bEnableEditorPrimitveDepthTest)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(),MaterialRenderProxy,MaterialResource,*View,View->ViewUniformBuffer,TextureMode);
		EditorCompositeParams.SetParameters(RHICmdList, MaterialResource, View, bEnableEditorPrimitveDepthTest, GetPixelShader());
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FRHIPixelShader* PixelShader = GetPixelShader();
		FPrimitiveSceneInfo* PrimitiveSceneInfo = Proxy ? Proxy->GetPrimitiveSceneInfo() : NULL;
		// test for HQ reflection parameter existence
		if (ReflectionCubemap1.IsBound() || ReflectionCubemap2.IsBound() || ReflectionPositionsAndRadii.IsBound())
		{
			static const int32 MaxNumReflections = FPrimitiveSceneInfo::MaxCachedReflectionCaptureProxies;
			static_assert(MaxNumReflections == 3, "Update reflection array initializations to match MaxCachedReflectionCaptureProxies");

			// set high quality reflection parameters.
			const FShaderResourceParameter* ReflectionTextureParameters[MaxNumReflections] = { &ReflectionCubemap, &ReflectionCubemap1, &ReflectionCubemap2 };
			const FShaderResourceParameter* ReflectionSamplerParameters[MaxNumReflections] = { &ReflectionSampler, &ReflectionSampler1, &ReflectionSampler2 };
			FTexture* ReflectionCubemapTextures[MaxNumReflections] = { GBlackTextureCube, GBlackTextureCube, GBlackTextureCube };
			FVector4 CapturePositions[MaxNumReflections] = { FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0), FVector4(0, 0, 0, 0) };
			FVector AverageBrightness(1, 1, 1);

			if (PrimitiveSceneInfo)
			{
				for (int32 i = 0; i < MaxNumReflections; i++)
				{
					const FReflectionCaptureProxy* ReflectionProxy = PrimitiveSceneInfo->CachedReflectionCaptureProxies[i];
					if (ReflectionProxy)
					{
						CapturePositions[i] = ReflectionProxy->Position;
						CapturePositions[i].W = ReflectionProxy->InfluenceRadius;
						if (ReflectionProxy->EncodedHDRCubemap && ReflectionProxy->EncodedHDRCubemap->IsInitialized())
						{
							ReflectionCubemapTextures[i] = PrimitiveSceneInfo->CachedReflectionCaptureProxies[i]->EncodedHDRCubemap;
						}
						AverageBrightness[i] = ReflectionProxy->AverageBrightness;
					}
				}
			}

			for (int32 i = 0; i < MaxNumReflections; i++)
			{
				if (ReflectionTextureParameters[i]->IsBound())
				{
					SetTextureParameter(RHICmdList, PixelShader, *ReflectionTextureParameters[i], *ReflectionSamplerParameters[i], ReflectionCubemapTextures[i]);
				}
			}

			if (ReflectionPositionsAndRadii.IsBound())
			{
				SetShaderValueArray(RHICmdList, PixelShader, ReflectionPositionsAndRadii, CapturePositions, MaxNumReflections);
			}

			SetShaderValue(RHICmdList, PixelShader, InvReflectionCubemapAverageBrightness, FVector(1.0f / AverageBrightness.X, 1.0f / AverageBrightness.Y, 1.0f / AverageBrightness.Z));
		}
		else if (ReflectionCubemap.IsBound())
		{
			FTexture* ReflectionTexture = GBlackTextureCube;
			float AverageBrightness = 1.0f;

			if (PrimitiveSceneInfo 
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap
				&& PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap->IsInitialized())
			{
				AverageBrightness = PrimitiveSceneInfo->CachedReflectionCaptureProxy->AverageBrightness;
				ReflectionTexture = PrimitiveSceneInfo->CachedReflectionCaptureProxy->EncodedHDRCubemap;
			}

			// Set the reflection cubemap
			SetTextureParameter(RHICmdList, PixelShader, ReflectionCubemap, ReflectionSampler, ReflectionTexture);
			SetShaderValue(RHICmdList, PixelShader, InvReflectionCubemapAverageBrightness, FVector(1.0f / AverageBrightness, 0, 0));
		}

		if (NumDynamicPointLights > 0)
		{
			FMobileBasePassDynamicPointLightInfo LightInfo(Proxy);

			if (NumDynamicPointLights == INT32_MAX)
			{
				SetShaderValue(RHICmdList, PixelShader, NumDynamicPointLightsParameter, LightInfo.NumDynamicPointLights);
			}

			// Set dynamic point lights
			SetShaderValueArray(RHICmdList, PixelShader, LightPositionAndInvRadiusParameter, LightInfo.LightPositionAndInvRadius, LightInfo.NumDynamicPointLights);
			SetShaderValueArray(RHICmdList, PixelShader, LightColorAndFalloffExponentParameter, LightInfo.LightColorAndFalloffExponent, LightInfo.NumDynamicPointLights);
		}

		const FPlanarReflectionSceneProxy* CachedPlanarReflectionProxy = PrimitiveSceneInfo ? PrimitiveSceneInfo->CachedPlanarReflectionProxy : nullptr;
		PlanarReflectionParams.SetParameters(RHICmdList, PixelShader, View, CachedPlanarReflectionProxy );

		FMeshMaterialShader::SetMesh(RHICmdList, PixelShader,VertexFactory,View,Proxy,BatchElement,DrawRenderState);		
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		PixelParametersType::Serialize(Ar);
		Ar << ReflectionCubemap;
		Ar << ReflectionSampler;
		Ar << InvReflectionCubemapAverageBrightness;
		Ar << EditorCompositeParams;
		Ar << LightPositionAndInvRadiusParameter;
		Ar << LightColorAndFalloffExponentParameter;
		if (NumDynamicPointLights == INT32_MAX)
		{
			Ar << NumDynamicPointLightsParameter;
		}

		Ar << ReflectionCubemap1;
		Ar << ReflectionCubemap2;
		Ar << ReflectionPositionsAndRadii;
		Ar << ReflectionSampler1;
		Ar << ReflectionSampler2;
		Ar << PlanarReflectionParams;

		return bShaderHasOutdatedParameters;
	}

private:

	static bool ModifyCompilationEnvironmentForQualityLevel(EShaderPlatform Platform, EMaterialQualityLevel::Type QualityLevel, FShaderCompilerEnvironment& OutEnvironment);

	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionSampler;
	FShaderParameter InvReflectionCubemapAverageBrightness;
	FEditorCompositingParameters EditorCompositeParams;
	FShaderParameter LightPositionAndInvRadiusParameter;
	FShaderParameter LightColorAndFalloffExponentParameter;
	FShaderParameter NumDynamicPointLightsParameter;


	//////////////////////////////////////////////////////////////////////////
	FShaderResourceParameter ReflectionCubemap1;
	FShaderResourceParameter ReflectionSampler1;
	FShaderResourceParameter ReflectionCubemap2;
	FShaderResourceParameter ReflectionSampler2;
	FShaderParameter ReflectionPositionsAndRadii;
	//////////////////////////////////////////////////////////////////////////
	FPlanarReflectionParameters PlanarReflectionParams;
};

template<typename LightMapPolicyType, int32 NumDynamicPointLights>
class TMobileBasePassPSBaseType : public TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights>
{
	typedef TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights> Super;

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCache(Platform, Material, VertexFactoryType) 
			&& Super::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TMobileBasePassPSBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}
	TMobileBasePassPSBaseType() {}
};

template< typename LightMapPolicyType, EOutputFormat OutputFormat, bool bEnableSkyLight, int32 NumDynamicPointLights>
class TMobileBasePassPS : public TMobileBasePassPSBaseType<LightMapPolicyType, NumDynamicPointLights>
{
	DECLARE_SHADER_TYPE(TMobileBasePassPS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{		
		// Only compile skylight version for lit materials on ES2 (Metal) or higher
		const bool bShouldCacheBySkylight = !bEnableSkyLight || (Material->GetShadingModel() != MSM_Unlit);

		return TMobileBasePassPSBaseType<LightMapPolicyType, NumDynamicPointLights>::ShouldCache(Platform, Material, VertexFactoryType) && ShouldCacheShaderByPlatformAndOutputFormat(Platform, OutputFormat) && bShouldCacheBySkylight;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{		
		TMobileBasePassPSBaseType<LightMapPolicyType, NumDynamicPointLights>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bEnableSkyLight);
		OutEnvironment.SetDefine(TEXT("OUTPUT_GAMMA_SPACE"), OutputFormat == LDR_GAMMA_32);
	}
	
	/** Initialization constructor. */
	TMobileBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TMobileBasePassPSBaseType<LightMapPolicyType, NumDynamicPointLights>(Initializer)
	{}

	/** Default constructor. */
	TMobileBasePassPS() {}
};

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <typename LightMapPolicyType, int32 NumDynamicPointLights>
struct GetMobileBasePassShaders
{
	GetMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	LightMapPolicyType LightMapPolicy, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>*& VertexShader,
	TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights>*& PixelShader
	)
	{
		if (IsMobileHDR())
		{
			VertexShader = Material.GetShader<TMobileBasePassVS<LightMapPolicyType, HDR_LINEAR_64> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, HDR_LINEAR_64, true, NumDynamicPointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, HDR_LINEAR_64, false, NumDynamicPointLights> >(VertexFactoryType);
			}
		}
		else
		{
			VertexShader = Material.GetShader<TMobileBasePassVS<LightMapPolicyType, LDR_GAMMA_32> >(VertexFactoryType);

			if (bEnableSkyLight)
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, LDR_GAMMA_32, true, NumDynamicPointLights> >(VertexFactoryType);
			}
			else
			{
				PixelShader = Material.GetShader< TMobileBasePassPS<LightMapPolicyType, LDR_GAMMA_32, false, NumDynamicPointLights> >(VertexFactoryType);
			}			
		}
	}
};

// Only using a struct/class allows partial specialisation here
template <int32 NumDynamicPointLights>
struct GetMobileBasePassShaders<FUniformLightMapPolicy, NumDynamicPointLights>
{
	GetMobileBasePassShaders(
		const FMaterial& Material, 
		FVertexFactoryType* VertexFactoryType, 
		FUniformLightMapPolicy LightMapPolicy, 
		bool bEnableSkyLight,
		TMobileBasePassVSPolicyParamType<typename FUniformLightMapPolicy::VertexParametersType>*& VertexShader,
		TMobileBasePassPSPolicyParamType<typename FUniformLightMapPolicy::PixelParametersType, NumDynamicPointLights>*& PixelShader
		);
};

template <ELightMapPolicyType Policy, int32 NumDynamicPointLights>
void GetUniformMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicyShaderParametersType, NumDynamicPointLights>*& PixelShader
	)
{
	if (IsMobileHDR())
	{
		VertexShader = Material.GetShader<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, true, NumDynamicPointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, HDR_LINEAR_64, false, NumDynamicPointLights> >(VertexFactoryType);
		}	
	}
	else
	{
		VertexShader = Material.GetShader<TMobileBasePassVS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32> >(VertexFactoryType);

		if (bEnableSkyLight)
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, true, NumDynamicPointLights> >(VertexFactoryType);
		}
		else
		{
			PixelShader = Material.GetShader< TMobileBasePassPS<TUniformLightMapPolicy<Policy>, LDR_GAMMA_32, false, NumDynamicPointLights> >(VertexFactoryType);
		}			
	}
}

template <int32 NumDynamicPointLights>
GetMobileBasePassShaders<FUniformLightMapPolicy, NumDynamicPointLights>::GetMobileBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	bool bEnableSkyLight,
	TMobileBasePassVSPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TMobileBasePassPSPolicyParamType<FUniformLightMapPolicyShaderParametersType, NumDynamicPointLights>*& PixelShader
	)
{
	switch (LightMapPolicy.GetIndirectPolicy())
	{
	case LMP_LQ_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_LQ_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
			break;
	case LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	case LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP:
			GetUniformMobileBasePassShaders<LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	default:										
		check(false);
	case LMP_NO_LIGHTMAP:
		GetUniformMobileBasePassShaders<LMP_NO_LIGHTMAP, NumDynamicPointLights>(Material, VertexFactoryType, bEnableSkyLight, VertexShader, PixelShader);
		break;
	}
}

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType, int32 NumDynamicPointLights>
class TMobileBasePassDrawingPolicy : public FMeshDrawingPolicy
{
public:

	/** The data the drawing policy uses for each mesh element. */
	class ElementDataType
	{
	public:

		/** The element's light-map data. */
		typename LightMapPolicyType::ElementDataType LightMapElementData;

		/** Default constructor. */
		ElementDataType()
		{}

		/** Initialization constructor. */
		ElementDataType(const typename LightMapPolicyType::ElementDataType& InLightMapElementData)
		:	LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	TMobileBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		ESceneRenderTargetsMode::Type InSceneTextureMode,
		bool bInEnableSkyLight,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode,
		ERHIFeatureLevel::Type FeatureLevel,
		bool bInEnableEditorPrimitiveDepthTest = false,
		bool bInEnableReceiveDecalOutput = false
		):
		FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource, InOverrideSettings, InDebugViewShaderMode),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		SceneTextureMode(InSceneTextureMode),
		bEnableEditorPrimitiveDepthTest(bInEnableEditorPrimitiveDepthTest),
		bEnableReceiveDecalOutput(bInEnableReceiveDecalOutput)
	{
		GetMobileBasePassShaders<LightMapPolicyType, NumDynamicPointLights>(
					InMaterialResource, 
					VertexFactory->GetType(), 
					InLightMapPolicy, 
					bInEnableSkyLight, 
					VertexShader,
					PixelShader
					);

#if DO_GUARD_SLOW
		// Somewhat hacky
		if (SceneTextureMode == ESceneRenderTargetsMode::DontSet && !bEnableEditorPrimitiveDepthTest && InMaterialResource.IsUsedWithEditorCompositing())
		{
			SceneTextureMode = ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing;
		}
#endif
	}

	// FMeshDrawingPolicy interface.

	FDrawingPolicyMatchResult Matches(const TMobileBasePassDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(LightMapPolicy == Other.LightMapPolicy) &&
			DRAWING_POLICY_MATCH(SceneTextureMode == Other.SceneTextureMode) &&
			DRAWING_POLICY_MATCH(bEnableReceiveDecalOutput == Other.bEnableReceiveDecalOutput) &&
			DRAWING_POLICY_MATCH(UseDebugViewPS() == Other.UseDebugViewPS());
		DRAWING_POLICY_MATCH_END
	}

	void SetMobileMultiViewMask(FRHICommandList& RHICmdList, const int32 EyeIndex)
	{
		VertexShader->SetMobileMultiViewMask(RHICmdList, EyeIndex);
	}

	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& View) const
	{
		if (UseDebugViewPS())
		{
			if (View.Family->EngineShowFlags.ShaderComplexity)
			{
				if (BlendMode == BLEND_Opaque)
				{
					DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA>::GetRHI());
				}
				else
				{
					// Add complexity to existing
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
				}
			}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			// If we are in the translucent pass or rendering a masked material then override the blend mode, otherwise maintain opaque blending
			if (View.Family->EngineShowFlags.ShaderComplexity && BlendMode != BLEND_Opaque)
			{
				// Add complexity to existing, keep alpha
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One>::GetRHI());
			}
#endif
		}
		else
		{

			bool bEncodedHDR = GetMobileHDRMode() == EMobileHDRMode::EnabledRGBE && MaterialResource->GetMaterialDomain() != MD_UI;;

			static const auto CVarMonoscopicFarField = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("vr.MonoscopicFarField"));
			const bool bIsMobileMonoscopic = CVarMonoscopicFarField && (CVarMonoscopicFarField->GetValueOnRenderThread() != 0);

			if (bEncodedHDR == false)
			{
				switch (BlendMode)
				{
				default:
				case BLEND_Opaque:
					// Opaque materials are rendered together in the base pass, where the blend state is set at a higher level
					break;
				case BLEND_Masked:
					// Masked materials are rendered together in the base pass, where the blend state is set at a higher level
					break;
				case BLEND_Translucent:
					if (bIsMobileMonoscopic)
					{
						DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_One, BF_One>::GetRHI());
					}
					else
					{
						DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					}
					break;
				case BLEND_Additive:
					// Add to the existing scene color
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					break;
				case BLEND_Modulate:
					// Modulate with the existing scene color
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
					break;
				case BLEND_AlphaComposite:
					// Blend with existing scene color. New color is already pre-multiplied by alpha.
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
					break;
				};
			}
			else
			{
				DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			}
		}

		if (bEnableReceiveDecalOutput && View.bSceneHasDecals)
		{
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<
				true, CF_GreaterEqual,
				true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
				false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
				0x00, GET_STENCIL_BIT_MASK(RECEIVE_DECAL, 1)>::GetRHI());
		}
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FViewInfo* View, const ContextDataType PolicyContext) const
	{
		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, SceneTextureMode);

		if (UseDebugViewPS())
		{
			FDebugViewMode::GetPSInterface(View->ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetParameters(RHICmdList, VertexShader, PixelShader, MaterialRenderProxy, *MaterialResource, *View);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, SceneTextureMode, bEnableEditorPrimitiveDepthTest);
		}
		// Set the light-map policy.
		LightMapPolicy.Set(RHICmdList, VertexShader, !UseDebugViewPS() ? PixelShader : nullptr, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);		
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		FPixelShaderRHIParamRef PixelShaderRHIRef = PixelShader->GetPixelShader();

		if (UseDebugViewPS())
		{
			PixelShaderRHIRef = FDebugViewMode::GetPSInterface(GetGlobalShaderMap(InFeatureLevel), MaterialResource, GetDebugViewShaderMode())->GetShader()->GetPixelShader();
		}

		return FBoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			FHullShaderRHIRef(), 
			FDomainShaderRHIRef(), 
			PixelShaderRHIRef,
			FGeometryShaderRHIRef());
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		// Set the light-map policy's mesh-specific settings.
		LightMapPolicy.SetMesh(
			RHICmdList, 
			View,
			PrimitiveSceneProxy,
			VertexShader,
			!UseDebugViewPS() ? PixelShader : nullptr,
			VertexShader,
			PixelShader,
			VertexFactory,
			MaterialRenderProxy,
			ElementData.LightMapElementData);

		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
		VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

		if (UseDebugViewPS())
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FDebugViewMode::GetPSInterface(GetGlobalShaderMap(View.FeatureLevel), MaterialResource, GetDebugViewShaderMode())->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.VisualizeLODIndex, BatchElement, DrawRenderState);
#endif
		}
		else
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		}

		if (bEnableReceiveDecalOutput && View.bSceneHasDecals)
		{
			const uint8 StencilValue = (PrimitiveSceneProxy && !PrimitiveSceneProxy->ReceivesDecals() ? 0x01 : 0x00);
			RHICmdList.SetStencilRef(GET_STENCIL_BIT_MASK(RECEIVE_DECAL, StencilValue)); // we hash the stencil group because we only have 6 bits.
		}

		// Set directional light UB
		const TShaderUniformBufferParameter<FMobileDirectionalLightShaderParameters>& MobileDirectionalLightParam = PixelShader->template GetUniformBufferParameter<FMobileDirectionalLightShaderParameters>();
		if (MobileDirectionalLightParam.IsBound())
		{
			int32 UniformBufferIndex = PrimitiveSceneProxy ? GetFirstLightingChannelFromMask(PrimitiveSceneProxy->GetLightingChannelMask()) + 1 : 0;
			SetUniformBufferParameter(RHICmdList, PixelShader->GetPixelShader(), MobileDirectionalLightParam, View.MobileDirectionalLightUniformBuffers[UniformBufferIndex]);
		}
	}

	friend int32 CompareDrawingPolicy(const TMobileBasePassDrawingPolicy& A,const TMobileBasePassDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(SceneTextureMode);

		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

protected:
	TMobileBasePassVSPolicyParamType<typename LightMapPolicyType::VertexParametersType>* VertexShader;
	TMobileBasePassPSPolicyParamType<typename LightMapPolicyType::PixelParametersType, NumDynamicPointLights>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;
	ESceneRenderTargetsMode::Type SceneTextureMode;
	/** Whether or not this policy is compositing editor primitives and needs to depth test against the scene geometry in the base pass pixel shader */
	uint32 bEnableEditorPrimitiveDepthTest : 1;
	uint32 bEnableReceiveDecalOutput : 1;
};

/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FMobileBasePassOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		/** Whether or not to perform depth test in the pixel shader */
		bool bEditorCompositeDepthTest;

		ContextType(bool bInEditorCompositeDepthTest, ESceneRenderTargetsMode::Type InTextureMode) :
			bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
			TextureMode(InTextureMode)
		{}

		ESceneRenderTargetsMode::Type TextureMode;
	};

	static void AddStaticMesh(FRHICommandList& RHICmdList, FScene* Scene, FStaticMesh* StaticMesh);
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

private:
	// templated version of DrawDynamicMesh on number of point lights
	template<int32 NumDynamicPointLights>
	static void DrawDynamicMeshTempl(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FMeshBatch& Mesh,
		const FMaterial* Material,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType, int32 NumDynamicPointLights>
void ProcessMobileBasePassMesh(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	ProcessActionType&& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = Parameters.ShadingModel != MSM_Unlit;
	if (bIsLitMaterial)
	{
		const FLightMapInteraction LightMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial)
			? Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel)
			: FLightMapInteraction();

		const FScene* Scene = Action.GetScene();
		const FLightSceneInfo* MobileDirectionalLight = nullptr;
		if (Parameters.PrimitiveSceneProxy && Scene)
		{
			const int32 LightChannel = GetFirstLightingChannelFromMask(Parameters.PrimitiveSceneProxy->GetLightingChannelMask());
			MobileDirectionalLight = LightChannel >= 0 ? GetSceneMobileDirectionalLights(Scene, LightChannel) : nullptr;
		}
	
		const FReadOnlyCVARCache& ReadOnlyCVARCache = FReadOnlyCVARCache::Get();

		const bool bPrimReceivesStaticAndCSM = Action.CanReceiveStaticAndCSM(MobileDirectionalLight, Parameters.PrimitiveSceneProxy);
		const bool bUseMovableLight = MobileDirectionalLight && !MobileDirectionalLight->Proxy->HasStaticShadowing() && ReadOnlyCVARCache.bMobileAllowMovableDirectionalLights;

		const bool bUseStaticAndCSM = MobileDirectionalLight && MobileDirectionalLight->Proxy->UseCSMForDynamicObjects()
			&& bPrimReceivesStaticAndCSM
			&& ReadOnlyCVARCache.bMobileEnableStaticAndCSMShadowReceivers
			&& (ReadOnlyCVARCache.bAllReceiveDynamicCSM || (Parameters.PrimitiveSceneProxy != nullptr && Parameters.PrimitiveSceneProxy->ShouldReceiveCombinedCSMAndStaticShadowsFromStationaryLights()));

		if (LightMapInteraction.GetType() == LMIT_Texture && ReadOnlyCVARCache.bAllowStaticLighting && ReadOnlyCVARCache.bEnableLowQualityLightmaps)
		{
			// Lightmap path
			const FShadowMapInteraction ShadowMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial)
				? Parameters.Mesh.LCI->GetShadowMapInteraction()
				: FShadowMapInteraction();

			if (bUseMovableLight)
			{
				// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
				if (MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_WITH_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_WITH_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else if (bUseStaticAndCSM)
			{
				if (ShadowMapInteraction.GetType() == SMIT_Texture && MobileDirectionalLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows() && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DISTANCE_FIELD_SHADOWS_LIGHTMAP_AND_CSM), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else
			{
				if (ShadowMapInteraction.GetType() == SMIT_Texture && ReadOnlyCVARCache.bMobileAllowDistanceFieldShadows)
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DISTANCE_FIELD_SHADOWS_AND_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel) /* implies bAllowStaticLighting*/
			&& Parameters.PrimitiveSceneProxy
			// Movable objects need to get their GI from the indirect lighting cache
			&& Parameters.PrimitiveSceneProxy->IsMovable())
		{
			if (bUseMovableLight)
			{
				if (MobileDirectionalLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
			}
			else
			{
				if (bUseStaticAndCSM)
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DIRECTIONAL_LIGHT_CSM_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_DIRECTIONAL_LIGHT_AND_SH_INDIRECT), Parameters.Mesh.LCI);
				}
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
		else if (bUseMovableLight)
		{
			// final determination of whether CSMs are rendered can be view dependent, thus we always need to clear the CSMs even if we're not going to render to them based on the condition below.
			if (MobileDirectionalLight && MobileDirectionalLight->ShouldRenderViewIndependentWholeSceneShadows())
			{
				Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT_CSM), Parameters.Mesh.LCI);
			}
			else
			{
				Action.template Process<NumDynamicPointLights>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_MOBILE_MOVABLE_DIRECTIONAL_LIGHT), Parameters.Mesh.LCI);
			}

			// Exit to avoid NoLightmapPolicy
			return;
		}
	}

	// Default to NoLightmapPolicy
	Action.template Process<0>(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
}
