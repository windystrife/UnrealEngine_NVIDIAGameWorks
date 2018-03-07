// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	BasePassRendering.h: Base pass rendering definitions.
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
#include "Materials/Material.h"
#include "DrawingPolicy.h"
#include "PostProcess/SceneRenderTargets.h"
#include "LightMapRendering.h"
#include "VelocityRendering.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "DebugViewModeRendering.h"
#include "FogRendering.h"
#include "EditorCompositeParams.h"
#include "PlanarReflectionRendering.h"
#include "UnrealEngine.h"
// WaveWorks Start
#include "WaveWorksResource.h"
// WaveWorks End

class FScene;

template<typename TBufferStruct> class TUniformBufferRef;

class FViewInfo;

/** Whether to allow the indirect lighting cache to be applied to dynamic objects. */
extern int32 GIndirectLightingCache;

/** Whether some GBuffer targets are optional. */
extern bool UseSelectiveBasePassOutputs();

class FForwardLocalLightData
{
public:
	FVector4 LightPositionAndInvRadius;
	FVector4 LightColorAndFalloffExponent;
	FVector4 LightDirectionAndShadowMapChannelMask;
	FVector4 SpotAnglesAndSourceRadiusPacked;
	FVector4 LightTangentAndSoftSourceRadius;
};

/** Parameters for computing forward lighting. */
class FForwardLightingParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		ForwardGlobalLightData.Bind(ParameterMap, TEXT("ForwardGlobalLightData"));
		ForwardLocalLightBuffer.Bind(ParameterMap, TEXT("ForwardLocalLightBuffer"));
		NumCulledLightsGrid.Bind(ParameterMap, TEXT("NumCulledLightsGrid"));
		CulledLightDataGrid.Bind(ParameterMap, TEXT("CulledLightDataGrid"));
		
		InstancedForwardGlobalLightData.Bind(ParameterMap, TEXT("InstancedForwardGlobalLightData"));
		InstancedForwardLocalLightBuffer.Bind(ParameterMap, TEXT("InstancedForwardLocalLightBuffer"));
		InstancedNumCulledLightsGrid.Bind(ParameterMap, TEXT("InstancedNumCulledLightsGrid"));
		InstancedCulledLightDataGrid.Bind(ParameterMap, TEXT("InstancedCulledLightDataGrid"));

		LightAttenuationTexture.Bind(ParameterMap, TEXT("LightAttenuationTexture"));
		LightAttenuationTextureSampler.Bind(ParameterMap, TEXT("LightAttenuationTextureSampler"));
		IndirectOcclusionTexture.Bind(ParameterMap, TEXT("IndirectOcclusionTexture"));
		IndirectOcclusionTextureSampler.Bind(ParameterMap, TEXT("IndirectOcclusionTextureSampler"));
		ReflectionCaptureBuffer.Bind(ParameterMap, TEXT("ReflectionCapture"));
		ResolvedSceneDepthTexture.Bind(ParameterMap, TEXT("ResolvedSceneDepthTexture"));
	}

	template<typename RHICommandListType, typename ShaderRHIParamRef>
	void Set(RHICommandListType& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View, const bool bIsInstancedStereo = false)
	{
		//@todo - put all of these in a shader resource table
		check(View.ForwardLightingResources->ForwardGlobalLightData.IsValid() || !ForwardGlobalLightData.IsBound());
		SetUniformBufferParameter(RHICmdList, ShaderRHI, ForwardGlobalLightData, View.ForwardLightingResources->ForwardGlobalLightData);
		SetSRVParameter(RHICmdList, ShaderRHI, ForwardLocalLightBuffer, View.ForwardLightingResources->ForwardLocalLightBuffer.SRV);
		NumCulledLightsGrid.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->NumCulledLightsGrid);
		CulledLightDataGrid.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->CulledLightDataGrid);

		if (bIsInstancedStereo)
		{
			// Bind right eye uniforms to instanced parameters
			const FSceneView& InstancedView = *View.Family->Views[1];
			check(View.ForwardLightingResources->ForwardGlobalLightData.IsValid() || !InstancedForwardGlobalLightData.IsBound());
			SetUniformBufferParameter(RHICmdList, ShaderRHI, InstancedForwardGlobalLightData, InstancedView.ForwardLightingResources->ForwardGlobalLightData);
			SetSRVParameter(RHICmdList, ShaderRHI, InstancedForwardLocalLightBuffer, InstancedView.ForwardLightingResources->ForwardLocalLightBuffer.SRV);
			InstancedNumCulledLightsGrid.SetBuffer(RHICmdList, ShaderRHI, InstancedView.ForwardLightingResources->NumCulledLightsGrid);
			InstancedCulledLightDataGrid.SetBuffer(RHICmdList, ShaderRHI, InstancedView.ForwardLightingResources->CulledLightDataGrid);
		}
		else
		{
			// Metal & Vulkan require all slots be bound even if we don't care to use them at runtime.
			check(!InstancedForwardGlobalLightData.IsBound() || View.ForwardLightingResources->ForwardGlobalLightData.IsValid());
			SetUniformBufferParameter(RHICmdList, ShaderRHI, InstancedForwardGlobalLightData, View.ForwardLightingResources->ForwardGlobalLightData);
			SetSRVParameter(RHICmdList, ShaderRHI, InstancedForwardLocalLightBuffer, View.ForwardLightingResources->ForwardLocalLightBuffer.SRV);
			InstancedNumCulledLightsGrid.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->NumCulledLightsGrid);
			InstancedCulledLightDataGrid.SetBuffer(RHICmdList, ShaderRHI, View.ForwardLightingResources->CulledLightDataGrid);
		}

		FSceneRenderTargets& SceneRenderTargets = FSceneRenderTargets::Get(RHICmdList);

		if (LightAttenuationTexture.IsBound() || IndirectOcclusionTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				LightAttenuationTexture,
				LightAttenuationTextureSampler,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				SceneRenderTargets.GetEffectiveLightAttenuationTexture(true)
				);

			IPooledRenderTarget* IndirectOcclusion = SceneRenderTargets.ScreenSpaceAO;

			if (!SceneRenderTargets.bScreenSpaceAOIsValid)
			{
				IndirectOcclusion = GSystemTextures.WhiteDummy;
			}

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				IndirectOcclusionTexture,
				IndirectOcclusionTextureSampler,
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				IndirectOcclusion->GetRenderTargetItem().ShaderResourceTexture
				);
		}

		SetUniformBufferParameter(RHICmdList, ShaderRHI, ReflectionCaptureBuffer, View.ReflectionCaptureUniformBuffer);

		if (ResolvedSceneDepthTexture.IsBound())
		{
			FTextureRHIParamRef ResolvedSceneDepthTextureValue = GSystemTextures.WhiteDummy->GetRenderTargetItem().ShaderResourceTexture;

			if (SceneRenderTargets.GetMSAACount() > 1)
			{
				ResolvedSceneDepthTextureValue = SceneRenderTargets.SceneDepthZ->GetRenderTargetItem().ShaderResourceTexture;
			}

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI,
				ResolvedSceneDepthTexture,
				ResolvedSceneDepthTextureValue
				);
		}
	}

	template<typename ShaderRHIParamRef>
	void UnsetParameters(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI, const FViewInfo& View)
	{
		NumCulledLightsGrid.UnsetUAV(RHICmdList, ShaderRHI);
		CulledLightDataGrid.UnsetUAV(RHICmdList, ShaderRHI);
		
		TArray<FUnorderedAccessViewRHIParamRef, TInlineAllocator<2>> OutUAVs;

		if (NumCulledLightsGrid.IsUAVBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->NumCulledLightsGrid.UAV);
		}

		if (CulledLightDataGrid.IsUAVBound())
		{
			OutUAVs.Add(View.ForwardLightingResources->CulledLightDataGrid.UAV);
		}

		if (OutUAVs.Num() > 0)
		{
			RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EComputeToGfx, OutUAVs.GetData(), OutUAVs.Num());
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("LOCAL_LIGHT_DATA_STRIDE"), FMath::DivideAndRoundUp<int32>(sizeof(FForwardLocalLightData), sizeof(FVector4)));
		extern int32 NumCulledLightsGridStride;
		OutEnvironment.SetDefine(TEXT("NUM_CULLED_LIGHTS_GRID_STRIDE"), NumCulledLightsGridStride);
		extern int32 NumCulledGridPrimitiveTypes;
		OutEnvironment.SetDefine(TEXT("NUM_CULLED_GRID_PRIMITIVE_TYPES"), NumCulledGridPrimitiveTypes);
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FForwardLightingParameters& P)
	{
		Ar << P.ForwardGlobalLightData;
		Ar << P.ForwardLocalLightBuffer;
		Ar << P.NumCulledLightsGrid;
		Ar << P.CulledLightDataGrid;

		Ar << P.InstancedForwardGlobalLightData;
		Ar << P.InstancedForwardLocalLightBuffer;
		Ar << P.InstancedNumCulledLightsGrid;
		Ar << P.InstancedCulledLightDataGrid;

		Ar << P.LightAttenuationTexture;
		Ar << P.LightAttenuationTextureSampler;
		Ar << P.IndirectOcclusionTexture;
		Ar << P.IndirectOcclusionTextureSampler;
		Ar << P.ReflectionCaptureBuffer;
		Ar << P.ResolvedSceneDepthTexture;
		return Ar;
	}

private:

	FShaderUniformBufferParameter ForwardGlobalLightData;
	FShaderResourceParameter ForwardLocalLightBuffer;
	FRWShaderParameter NumCulledLightsGrid;
	FRWShaderParameter CulledLightDataGrid;
	
	FShaderUniformBufferParameter InstancedForwardGlobalLightData;
	FShaderResourceParameter InstancedForwardLocalLightBuffer;
	FRWShaderParameter InstancedNumCulledLightsGrid;
	FRWShaderParameter InstancedCulledLightDataGrid;

	FShaderResourceParameter LightAttenuationTexture;
	FShaderResourceParameter LightAttenuationTextureSampler;
	FShaderResourceParameter IndirectOcclusionTexture;
	FShaderResourceParameter IndirectOcclusionTextureSampler;
	FShaderUniformBufferParameter ReflectionCaptureBuffer;

	FShaderResourceParameter ResolvedSceneDepthTexture;
};

/** Parameters needed for looking up into translucency lighting volumes. */
class FTranslucentLightingVolumeParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TranslucencyLightingVolumeAmbientInner.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientInner"));
		TranslucencyLightingVolumeAmbientInnerSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientInnerSampler"));
		TranslucencyLightingVolumeAmbientOuter.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientOuter"));
		TranslucencyLightingVolumeAmbientOuterSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeAmbientOuterSampler"));
		TranslucencyLightingVolumeDirectionalInner.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalInner"));
		TranslucencyLightingVolumeDirectionalInnerSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalInnerSampler"));
		TranslucencyLightingVolumeDirectionalOuter.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalOuter"));
		TranslucencyLightingVolumeDirectionalOuterSampler.Bind(ParameterMap, TEXT("TranslucencyLightingVolumeDirectionalOuterSampler"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef& ShaderRHI)
	{
		if (TranslucencyLightingVolumeAmbientInner.IsBound())
		{
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI, 
				TranslucencyLightingVolumeAmbientInner, 
				TranslucencyLightingVolumeAmbientInnerSampler, 
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
				SceneContext.GetTranslucencyVolumeAmbient(TVC_Inner)->GetRenderTargetItem().ShaderResourceTexture);

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI, 
				TranslucencyLightingVolumeAmbientOuter, 
				TranslucencyLightingVolumeAmbientOuterSampler, 
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
				SceneContext.GetTranslucencyVolumeAmbient(TVC_Outer)->GetRenderTargetItem().ShaderResourceTexture);

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI, 
				TranslucencyLightingVolumeDirectionalInner, 
				TranslucencyLightingVolumeDirectionalInnerSampler, 
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
				SceneContext.GetTranslucencyVolumeDirectional(TVC_Inner)->GetRenderTargetItem().ShaderResourceTexture);

			SetTextureParameter(
				RHICmdList, 
				ShaderRHI, 
				TranslucencyLightingVolumeDirectionalOuter, 
				TranslucencyLightingVolumeDirectionalOuterSampler, 
				TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), 
				SceneContext.GetTranslucencyVolumeDirectional(TVC_Outer)->GetRenderTargetItem().ShaderResourceTexture);
		}
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FTranslucentLightingVolumeParameters& P)
	{
		Ar << P.TranslucencyLightingVolumeAmbientInner;
		Ar << P.TranslucencyLightingVolumeAmbientInnerSampler;
		Ar << P.TranslucencyLightingVolumeAmbientOuter;
		Ar << P.TranslucencyLightingVolumeAmbientOuterSampler;
		Ar << P.TranslucencyLightingVolumeDirectionalInner;
		Ar << P.TranslucencyLightingVolumeDirectionalInnerSampler;
		Ar << P.TranslucencyLightingVolumeDirectionalOuter;
		Ar << P.TranslucencyLightingVolumeDirectionalOuterSampler;
		return Ar;
	}

private:

	FShaderResourceParameter TranslucencyLightingVolumeAmbientInner;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientInnerSampler;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientOuter;
	FShaderResourceParameter TranslucencyLightingVolumeAmbientOuterSampler;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalInner;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalInnerSampler;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalOuter;
	FShaderResourceParameter TranslucencyLightingVolumeDirectionalOuterSampler;
};

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without atmospheric fog.
 */

template<typename VertexParametersType>
class TBasePassVertexShaderPolicyParamType : public FMeshMaterialShader, public VertexParametersType
{
protected:

	TBasePassVertexShaderPolicyParamType() {}
	TBasePassVertexShaderPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		VertexParametersType::Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		TranslucentLightingVolumeParameters.Bind(Initializer.ParameterMap);
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
		const bool bOutputsVelocityToGBuffer = FVelocityRendering::OutputsToGBuffer();
		if (bOutputsVelocityToGBuffer)
		{
			PreviousLocalToWorldParameter.Bind(Initializer.ParameterMap, TEXT("PreviousLocalToWorld"));
//@todo-rco: Move to pixel shader
			SkipOutputVelocityParameter.Bind(Initializer.ParameterMap, TEXT("SkipOutputVelocity"));
		}

		InstancedEyeIndexParameter.Bind(Initializer.ParameterMap, TEXT("InstancedEyeIndex"));
		IsInstancedStereoParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereo"));
	}

public:

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		VertexParametersType::Serialize(Ar);
		Ar << HeightFogParameters;
		Ar << TranslucentLightingVolumeParameters;
		Ar << ForwardLightingParameters;
		Ar << PreviousLocalToWorldParameter;
		Ar << SkipOutputVelocityParameter;
		Ar << InstancedEyeIndexParameter;
		Ar << IsInstancedStereoParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FVertexFactory* VertexFactory,
		const FMaterial& InMaterialResource,
		const FViewInfo& View,
		ESceneRenderTargetsMode::Type TextureMode, 
		bool bIsInstancedStereo,
		bool bUseDownsampledTranslucencyViewUniformBuffer
		)
	{
		checkSlow(!bUseDownsampledTranslucencyViewUniformBuffer || View.DownsampledTranslucencyViewUniformBuffer);
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer = bUseDownsampledTranslucencyViewUniformBuffer ? View.DownsampledTranslucencyViewUniformBuffer : View.ViewUniformBuffer;
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, InMaterialResource, View, ViewUniformBuffer, TextureMode);

		HeightFogParameters.Set(RHICmdList, GetVertexShader(), &View);

		TranslucentLightingVolumeParameters.Set(RHICmdList, GetVertexShader());
		ForwardLightingParameters.Set(RHICmdList, GetVertexShader(), View, bIsInstancedStereo);

		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoParameter, bIsInstancedStereo);
		}

		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, 0);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy, const FMeshBatch& Mesh, const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState);

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex);

private:
	
	/** The parameters needed to calculate the fog contribution from height fog layers. */
	FHeightFogShaderParameters HeightFogParameters;
	FTranslucentLightingVolumeParameters TranslucentLightingVolumeParameters;
	FForwardLightingParameters ForwardLightingParameters;
	// When outputting from base pass, the previous transform
	FShaderParameter PreviousLocalToWorldParameter;
	FShaderParameter SkipOutputVelocityParameter;
	FShaderParameter InstancedEyeIndexParameter;
	FShaderParameter IsInstancedStereoParameter;
};




/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without atmospheric fog.
 */

template<typename LightMapPolicyType>
class TBasePassVertexShaderBaseType : public TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>
{
	typedef TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType> Super;

protected:

	TBasePassVertexShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

	TBasePassVertexShaderBaseType() {}

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

template<typename LightMapPolicyType, bool bEnableAtmosphericFog>
class TBasePassVS : public TBasePassVertexShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassVS,MeshMaterial);
	typedef TBasePassVertexShaderBaseType<LightMapPolicyType> Super;

protected:

	TBasePassVS() {}
	TBasePassVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		Super(Initializer)
	{
	}

public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto SupportAtmosphericFog = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAtmosphericFog"));
		static const auto SupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));
		const bool bForceAllPermutations = SupportAllShaderPermutations && SupportAllShaderPermutations->GetValueOnAnyThread() != 0;

		const bool bProjectAllowsAtmosphericFog = !SupportAtmosphericFog || SupportAtmosphericFog->GetValueOnAnyThread() != 0 || bForceAllPermutations;

		bool bShouldCache = Super::ShouldCache(Platform, Material, VertexFactoryType);
		bShouldCache &= (bEnableAtmosphericFog && bProjectAllowsAtmosphericFog && IsTranslucentBlendMode(Material->GetBlendMode())) || !bEnableAtmosphericFog;

		return bShouldCache
			&& (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4));
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		// @todo MetalMRT: Remove this hack and implement proper atmospheric-fog solution for Metal MRT...
		OutEnvironment.SetDefine(TEXT("BASEPASS_ATMOSPHERIC_FOG"), (Platform != SP_METAL_MRT && Platform != SP_METAL_MRT_MAC) ? bEnableAtmosphericFog : 0);
	}
};

/**
 * The base shader type for hull shaders.
 */
template<typename LightMapPolicyType, bool bEnableAtmosphericFog>
class TBasePassHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TBasePassHS,MeshMaterial);

protected:

	TBasePassHS() {}

	TBasePassHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		// Metal requires matching permutations, but no other platform should worry about this complication.
		return (bEnableAtmosphericFog == false || IsMetalPlatform(Platform))
			&& FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVS<LightMapPolicyType,bEnableAtmosphericFog>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVS<LightMapPolicyType,bEnableAtmosphericFog>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	// Don't implement : void SetParameters(...) or SetMesh() unless changing the shader reference in TBasePassDrawingPolicy
};

/**
 * The base shader type for Domain shaders.
 */
template<typename LightMapPolicyType>
class TBasePassDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TBasePassDS,MeshMaterial);

protected:

	TBasePassDS() {}

	TBasePassDS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use vertex shader gating
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TBasePassVS<LightMapPolicyType,false>::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use vertex shader compilation environment
		TBasePassVS<LightMapPolicyType,false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

public:
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FBaseDS::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	// Don't implement : void SetParameters(...) or SetMesh() unless changing the shader reference in TBasePassDrawingPolicy
};

/** Parameters needed to implement the sky light cubemap reflection. */
class FSkyLightReflectionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		SkyLightCubemap.Bind(ParameterMap, TEXT("SkyLightCubemap"));
		SkyLightCubemapSampler.Bind(ParameterMap, TEXT("SkyLightCubemapSampler"));
		SkyLightBlendDestinationCubemap.Bind(ParameterMap, TEXT("SkyLightBlendDestinationCubemap"));
		SkyLightBlendDestinationCubemapSampler.Bind(ParameterMap, TEXT("SkyLightBlendDestinationCubemapSampler"));
		SkyLightParameters.Bind(ParameterMap, TEXT("SkyLightParameters"));
		SkyLightCubemapBrightness.Bind(ParameterMap, TEXT("SkyLightCubemapBrightness"));
	}

	template<typename TParamRef, typename TRHICmdList>
	void SetParameters(TRHICmdList& RHICmdList, const TParamRef& ShaderRHI, const FScene* Scene, bool bApplySkyLight)
	{
		if (SkyLightCubemap.IsBound() || SkyLightBlendDestinationCubemap.IsBound() || SkyLightParameters.IsBound())
		{
			FTexture* SkyLightTextureResource = GBlackTextureCube;
			FTexture* SkyLightBlendDestinationTextureResource = GBlackTextureCube;
			float ApplySkyLightMask = 0;
			float SkyMipCount = 1;
			float BlendFraction = 0;
			bool bSkyLightIsDynamic = false;
			float SkyAverageBrightness = 1.0f;

			GetSkyParametersFromScene(Scene, bApplySkyLight, SkyLightTextureResource, SkyLightBlendDestinationTextureResource, ApplySkyLightMask, SkyMipCount, bSkyLightIsDynamic, BlendFraction, SkyAverageBrightness);

			SetTextureParameter(RHICmdList, ShaderRHI, SkyLightCubemap, SkyLightCubemapSampler, SkyLightTextureResource);
			SetTextureParameter(RHICmdList, ShaderRHI, SkyLightBlendDestinationCubemap, SkyLightBlendDestinationCubemapSampler, SkyLightBlendDestinationTextureResource);
			const FVector4 SkyParametersValue(SkyMipCount - 1.0f, ApplySkyLightMask, bSkyLightIsDynamic ? 1.0f : 0.0f, BlendFraction);
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightParameters, SkyParametersValue);
			SetShaderValue(RHICmdList, ShaderRHI, SkyLightCubemapBrightness, SkyAverageBrightness);
		}
	}

	friend FArchive& operator<<(FArchive& Ar,FSkyLightReflectionParameters& P)
	{
		Ar << P.SkyLightCubemap << P.SkyLightCubemapSampler << P.SkyLightParameters << P.SkyLightBlendDestinationCubemap << P.SkyLightBlendDestinationCubemapSampler << P.SkyLightCubemapBrightness;
		return Ar;
	}

private:

	FShaderResourceParameter SkyLightCubemap;
	FShaderResourceParameter SkyLightCubemapSampler;
	FShaderResourceParameter SkyLightBlendDestinationCubemap;
	FShaderResourceParameter SkyLightBlendDestinationCubemapSampler;
	FShaderParameter SkyLightParameters;
	FShaderParameter SkyLightCubemapBrightness;

	void GetSkyParametersFromScene(
		const FScene* Scene, 
		bool bApplySkyLight, 
		FTexture*& OutSkyLightTextureResource, 
		FTexture*& OutSkyLightBlendDestinationTextureResource, 
		float& OutApplySkyLightMask, 
		float& OutSkyMipCount, 
		bool& bSkyLightIsDynamic, 
		float& OutBlendFraction,
		float& OutSkyAverageBrightness);
};

/** Parameters needed for reflections, shared by multiple shaders. */
class FBasePassReflectionParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PlanarReflectionParameters.Bind(ParameterMap);
		ReflectionCubemap.Bind(ParameterMap, TEXT("ReflectionCubemap"));
		ReflectionCubemapSampler.Bind(ParameterMap, TEXT("ReflectionCubemapSampler"));
		SingleCubemapArrayIndex.Bind(ParameterMap, TEXT("SingleCubemapArrayIndex"));	
		SingleCaptureOffsetAndAverageBrightness.Bind(ParameterMap, TEXT("SingleCaptureOffsetAndAverageBrightness"));
		SingleCapturePositionAndRadius.Bind(ParameterMap, TEXT("SingleCapturePositionAndRadius"));
		SingleCaptureBrightness.Bind(ParameterMap, TEXT("SingleCaptureBrightness"));
		SkyLightReflectionParameters.Bind(ParameterMap);
	}

	void Set(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef PixelShaderRHI, const FViewInfo* View);

	void SetMesh(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef PixelShaderRH, const FSceneView& View, const FPrimitiveSceneProxy* Proxy, ERHIFeatureLevel::Type FeatureLevel);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FBasePassReflectionParameters& P)
	{
		Ar << P.PlanarReflectionParameters;
		Ar << P.ReflectionCubemap;
		Ar << P.ReflectionCubemapSampler;
		Ar << P.SingleCubemapArrayIndex;
		Ar << P.SingleCaptureOffsetAndAverageBrightness;
		Ar << P.SingleCapturePositionAndRadius;
		Ar << P.SingleCaptureBrightness;
		Ar << P.SkyLightReflectionParameters;
		return Ar;
	}

private:

	FPlanarReflectionParameters PlanarReflectionParameters;
	FShaderResourceParameter ReflectionCubemap;
	FShaderResourceParameter ReflectionCubemapSampler;

	FShaderParameter SingleCubemapArrayIndex;
	FShaderParameter SingleCaptureOffsetAndAverageBrightness;
	FShaderParameter SingleCapturePositionAndRadius;
	FShaderParameter SingleCaptureBrightness;

	FSkyLightReflectionParameters SkyLightReflectionParameters;
};

/** Parameters needed for lighting translucency, shared by multiple shaders. */
class FTranslucentLightingParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		TranslucentLightingVolumeParameters.Bind(ParameterMap);
		HZBTexture.Bind(ParameterMap, TEXT("HZBTexture"));
		HZBSampler.Bind(ParameterMap, TEXT("HZBSampler"));
		HZBUvFactorAndInvFactor.Bind(ParameterMap, TEXT("HZBUvFactorAndInvFactor"));
		PrevSceneColor.Bind(ParameterMap, TEXT("PrevSceneColor"));
		PrevSceneColorSampler.Bind(ParameterMap, TEXT("PrevSceneColorSampler"));
	}

	void Set(FRHICommandList& RHICmdList, FPixelShaderRHIParamRef PixelShaderRHI, const FViewInfo* View);

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FTranslucentLightingParameters& P)
	{
		Ar << P.TranslucentLightingVolumeParameters;
		Ar << P.HZBTexture;
		Ar << P.HZBSampler;
		Ar << P.HZBUvFactorAndInvFactor;
		Ar << P.PrevSceneColor;
		Ar << P.PrevSceneColorSampler;
		return Ar;
	}

private:

	FTranslucentLightingVolumeParameters TranslucentLightingVolumeParameters;
	FShaderResourceParameter HZBTexture;
	FShaderResourceParameter HZBSampler;
	FShaderParameter HZBUvFactorAndInvFactor;
	FShaderResourceParameter PrevSceneColor;
	FShaderResourceParameter PrevSceneColorSampler;
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename PixelParametersType>
class TBasePassPixelShaderPolicyParamType : public FMeshMaterialShader, public PixelParametersType
{
public:

	// static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);

		const bool bOutputVelocity = FVelocityRendering::OutputsToGBuffer();
		if (bOutputVelocity)
		{
			const int32 VelocityIndex = 4; // As defined in BasePassPixelShader.usf
			OutEnvironment.SetRenderTargetOutputFormat(VelocityIndex, PF_G16R16);
		}

		FForwardLightingParameters::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassPixelShaderPolicyParamType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		PixelParametersType::Bind(Initializer.ParameterMap);
		ReflectionParameters.Bind(Initializer.ParameterMap);
		TranslucentLightingParameters.Bind(Initializer.ParameterMap);
		HeightFogParameters.Bind(Initializer.ParameterMap);
		EditorCompositeParams.Bind(Initializer.ParameterMap);
		ForwardLightingParameters.Bind(Initializer.ParameterMap);
	}
	TBasePassPixelShaderPolicyParamType() {}

	// NVCHANGE_BEGIN: Add VXGI
	virtual // We need to override this method in TVXGIConeTracingPS
	// NVCHANGE_END: Add VXGI
	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial& MaterialResource, 
		const FViewInfo* View, 
		EBlendMode BlendMode, 
		bool bEnableEditorPrimitveDepthTest,
		ESceneRenderTargetsMode::Type TextureMode,
		bool bIsInstancedStereo,
		bool bUseDownsampledTranslucencyViewUniformBuffer)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		checkSlow(!bUseDownsampledTranslucencyViewUniformBuffer || View->DownsampledTranslucencyViewUniformBuffer);
		const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer = bUseDownsampledTranslucencyViewUniformBuffer ? View->DownsampledTranslucencyViewUniformBuffer : View->ViewUniformBuffer;
		FMeshMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialRenderProxy, MaterialResource, *View, ViewUniformBuffer, TextureMode);

		ReflectionParameters.Set(RHICmdList, ShaderRHI, View);

		if (IsTranslucentBlendMode(BlendMode))
		{
			TranslucentLightingParameters.Set(RHICmdList, ShaderRHI, View);
			HeightFogParameters.Set(RHICmdList, ShaderRHI, View);
		}
		
		EditorCompositeParams.SetParameters(RHICmdList, MaterialResource, View, bEnableEditorPrimitveDepthTest, GetPixelShader());

		ForwardLightingParameters.Set(RHICmdList, ShaderRHI, *View, bIsInstancedStereo);
	}

	// NVCHANGE_BEGIN: Add VXGI
	virtual // We need to override this method in TVXGIConeTracingPS
	// NVCHANGE_END: Add VXGI
	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement, const FDrawingPolicyRenderState& DrawRenderState, EBlendMode BlendMode);

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		PixelParametersType::Serialize(Ar);
		Ar << ReflectionParameters;
		Ar << TranslucentLightingParameters;
		Ar << HeightFogParameters;
		Ar << EditorCompositeParams;
		Ar << ForwardLightingParameters;
		return bShaderHasOutdatedParameters;
	}

private:
	FBasePassReflectionParameters ReflectionParameters;
	FTranslucentLightingParameters TranslucentLightingParameters;
	FHeightFogShaderParameters HeightFogParameters;
	FEditorCompositingParameters EditorCompositeParams;
	FForwardLightingParameters ForwardLightingParameters;
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TBasePassPixelShaderBaseType : public TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>
{
	typedef TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType> Super;

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		Super::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TBasePassPixelShaderBaseType(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : Super(Initializer) {}

	TBasePassPixelShaderBaseType() {}
};

/** The concrete base pass pixel shader type. */
template<typename LightMapPolicyType, bool bEnableSkyLight>
class TBasePassPS : public TBasePassPixelShaderBaseType<LightMapPolicyType>
{
	DECLARE_SHADER_TYPE(TBasePassPS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Only compile skylight version for lit materials, and if the project allows them.
		static const auto SupportStationarySkylight = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportStationarySkylight"));
		static const auto SupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));

		const bool bTranslucent = IsTranslucentBlendMode(Material->GetBlendMode());
		const bool bForceAllPermutations = SupportAllShaderPermutations && SupportAllShaderPermutations->GetValueOnAnyThread() != 0;
		const bool bProjectSupportsStationarySkylight = !SupportStationarySkylight || SupportStationarySkylight->GetValueOnAnyThread() != 0 || bForceAllPermutations;

		const bool bCacheShaders = !bEnableSkyLight
			//translucent materials need to compile skylight support to support MOVABLE skylights also.
			|| bTranslucent
			// Some lightmap policies (eg Simple Forward) always require skylight support
			|| LightMapPolicyType::RequiresSkylight()
			|| (bProjectSupportsStationarySkylight && (Material->GetShadingModel() != MSM_Unlit));
		return bCacheShaders
			&& (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
			&& TBasePassPixelShaderBaseType<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// For deferred decals, the shader class used is FDeferredDecalPS. the TBasePassPS is only used in the material editor and will read wrong values.
		OutEnvironment.SetDefine(TEXT("SCENE_TEXTURES_DISABLED"), Material->GetMaterialDomain() == MD_DeferredDecal); 

		OutEnvironment.SetDefine(TEXT("ENABLE_SKY_LIGHT"), bEnableSkyLight);
		TBasePassPixelShaderBaseType<LightMapPolicyType>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
	
	/** Initialization constructor. */
	TBasePassPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		TBasePassPixelShaderBaseType<LightMapPolicyType>(Initializer)
	{}

	/** Default constructor. */
	TBasePassPS() {}
};

/**
 * Get shader templates allowing to redirect between compatible shaders.
 */

template <typename LightMapPolicyType>
void GetBasePassShaders(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	LightMapPolicyType LightMapPolicy, 
	bool bNeedsHSDS,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>*& PixelShader
	)
{
	if (bNeedsHSDS)
	{
		DomainShader = Material.GetShader<TBasePassDS<LightMapPolicyType > >(VertexFactoryType);
		
		// Metal requires matching permutations, but no other platform should worry about this complication.
		if (bEnableAtmosphericFog && DomainShader && IsMetalPlatform(EShaderPlatform(DomainShader->GetTarget().Platform)))
		{
			HullShader = Material.GetShader<TBasePassHS<LightMapPolicyType, true > >(VertexFactoryType);
		}
		else
		{
			HullShader = Material.GetShader<TBasePassHS<LightMapPolicyType, false > >(VertexFactoryType);
		}
	}

	if (bEnableAtmosphericFog)
	{
		VertexShader = Material.GetShader<TBasePassVS<LightMapPolicyType, true> >(VertexFactoryType);
	}
	else
	{
		VertexShader = Material.GetShader<TBasePassVS<LightMapPolicyType, false> >(VertexFactoryType);
	}
	if (bEnableSkyLight)
	{
		PixelShader = Material.GetShader<TBasePassPS<LightMapPolicyType, true> >(VertexFactoryType);
	}
	else
	{
		PixelShader = Material.GetShader<TBasePassPS<LightMapPolicyType, false> >(VertexFactoryType);
	}
}

template <>
void GetBasePassShaders<FUniformLightMapPolicy>(
	const FMaterial& Material, 
	FVertexFactoryType* VertexFactoryType, 
	FUniformLightMapPolicy LightMapPolicy, 
	bool bNeedsHSDS,
	bool bEnableAtmosphericFog,
	bool bEnableSkyLight,
	FBaseHS*& HullShader,
	FBaseDS*& DomainShader,
	TBasePassVertexShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& VertexShader,
	TBasePassPixelShaderPolicyParamType<FUniformLightMapPolicyShaderParametersType>*& PixelShader
	);

class FBasePassDrawingPolicy : public FMeshDrawingPolicy
{
public:
	FBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy, 
		const FMaterial& InMaterialResource, 
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode,
		bool bInEnableReceiveDecalOutput,
		bool bInEnableEditorPrimitiveDepthTest) : FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, InDebugViewShaderMode), bEnableReceiveDecalOutput(bInEnableReceiveDecalOutput), bEnableEditorPrimitiveDepthTest(bInEnableEditorPrimitiveDepthTest)
	{}

	void ApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither);

protected:

	/** Whether or not outputing the receive decal boolean */
	uint32 bEnableReceiveDecalOutput : 1;
	/** Whether or not this policy is compositing editor primitives and needs to depth test against the scene geometry in the base pass pixel shader */
	uint32 bEnableEditorPrimitiveDepthTest : 1;
};

/**
 * Draws the emissive color and the light-map of a mesh.
 */
template<typename LightMapPolicyType>
class TBasePassDrawingPolicy : public FBasePassDrawingPolicy
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

// WaveWorks Start
	void ConfigureWaveWorksInputMapping(FMaterialShader* VSShader, FMaterialShader* HSShader, FMaterialShader* DSShader, FMaterialShader* PSShader)
	{
		const TArray<WaveWorksShaderInput>* WaveWorksShaderInput = GDynamicRHI->RHIGetDefaultContext()->RHIGetWaveWorksShaderInput();
		if (nullptr != WaveWorksShaderInput)
		{
			uint32 Count = WaveWorksShaderInput->Num();
			WaveWorksShaderInputMapping.SetNumZeroed(Count);

			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FMaterialShader* Shader = nullptr;
				switch ((*WaveWorksShaderInput)[Index].Frequency)
				{
				case SF_Vertex:
					Shader = VSShader;
					break;
				case SF_Hull:
					Shader = HSShader;
					break;
				case SF_Domain:
					Shader = DSShader;
					break;
				case SF_Pixel:
					Shader = PSShader;
					break;
				default:
					break;
				}

				if (nullptr != Shader)
				{
					FWaveWorksShaderParameters* WaveWorksShaderParams = Shader->GetWaveWorksShaderParameters();
					WaveWorksShaderInputMapping[Index] = WaveWorksShaderParams->ShaderInputMappings[Index];
				}				
			}
		}
	}
// WaveWorks End

	/** Initialization constructor. */
	TBasePassDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		ERHIFeatureLevel::Type InFeatureLevel,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		ESceneRenderTargetsMode::Type InSceneTextureMode,
		bool bInEnableSkyLight,
		bool bInEnableAtmosphericFog,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode = DVSM_None,
		bool bInEnableEditorPrimitiveDepthTest = false,
		bool bInEnableReceiveDecalOutput = false
		):
		FBasePassDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, InDebugViewShaderMode, bInEnableReceiveDecalOutput, bInEnableEditorPrimitiveDepthTest),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode), 
		SceneTextureMode(InSceneTextureMode),
		bEnableSkyLight(bInEnableSkyLight),
		bEnableAtmosphericFog(bInEnableAtmosphericFog)
	{
		HullShader = NULL;
		DomainShader = NULL;
	
		const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();

		const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
								&& InVertexFactory->GetType()->SupportsTessellationShaders() 
								&& MaterialTessellationMode != MTM_NoTessellation;

		GetBasePassShaders<LightMapPolicyType>(
			InMaterialResource, 
			VertexFactory->GetType(), 
			InLightMapPolicy, 
			bNeedsHSDS,
			bEnableAtmosphericFog,
			bEnableSkyLight,
			HullShader,
			DomainShader,
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

	FDrawingPolicyMatchResult Matches(const TBasePassDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(SceneTextureMode == Other.SceneTextureMode) &&
			DRAWING_POLICY_MATCH(bEnableSkyLight == Other.bEnableSkyLight) && 
			DRAWING_POLICY_MATCH(LightMapPolicy == Other.LightMapPolicy) &&
			DRAWING_POLICY_MATCH(bEnableReceiveDecalOutput == Other.bEnableReceiveDecalOutput) &&
			DRAWING_POLICY_MATCH(UseDebugViewPS() == Other.UseDebugViewPS());
		DRAWING_POLICY_MATCH_END
	}

// WaveWorks Start
	void SetSharedWaveWorksState(FRHICommandList& RHICmdList, const FViewInfo* View, FWaveWorksResource* WaveWorksResources)
	{
		// If the current debug view shader modes are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (!View->Family->UseDebugViewVSDSHS())
		{
			VertexShader->SetWaveWorksParameters<FVertexShaderRHIParamRef>(RHICmdList, (FVertexShaderRHIParamRef)VertexShader->GetVertexShader(), *View, WaveWorksResources);

			if (HullShader)
			{
				HullShader->SetWaveWorksParameters<FHullShaderRHIParamRef>(RHICmdList, (FHullShaderRHIParamRef)HullShader->GetHullShader(), *View, WaveWorksResources);
			}

			if (DomainShader)
			{
				DomainShader->SetWaveWorksParameters<FDomainShaderRHIParamRef>(RHICmdList, (FDomainShaderRHIParamRef)DomainShader->GetDomainShader(), *View, WaveWorksResources);
			}
		}

		if (!UseDebugViewPS())
		{
			PixelShader->SetWaveWorksParameters<FPixelShaderRHIParamRef>(RHICmdList, (FPixelShaderRHIParamRef)PixelShader->GetPixelShader(), *View, WaveWorksResources);
		}

		if (nullptr != WaveWorksResources)
		{
			FWaveWorksRHIRef WaveWorksRHI = WaveWorksResources->GetWaveWorksRHI();
			if (WaveWorksRHI)
			{
				if (WaveWorksShaderInputMapping.Num() == 0)
					ConfigureWaveWorksInputMapping(VertexShader, HullShader, DomainShader, PixelShader);

				RHICmdList.SetWaveWorksState(WaveWorksRHI, View->ViewMatrices.GetViewMatrix(), WaveWorksShaderInputMapping);
			}
		}		
	}
// WaveWorks End

	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View) const
	{
		if (UseDebugViewPS())
		{
			if (IsTranslucentBlendMode(BlendMode))
			{
				if (View.Family->EngineShowFlags.ShaderComplexity)
				{	// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
				}
				else if (View.Family->GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
				{	// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
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
				// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
				// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Additive:
				// Add to the existing scene color
				// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
				// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Modulate:
				// Modulate with the existing scene color, preserve destination alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
				break;
			case BLEND_AlphaComposite:
				// Blend with existing scene color. New color is already pre-multiplied by alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			};
		}
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FViewInfo* View, const ContextDataType PolicyContext, bool bUseDownsampledTranslucencyViewUniformBuffer = false) const
	{
		// If the current debug view shader modes are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (View->Family->UseDebugViewVSDSHS())
		{
			FDebugViewMode::SetParametersVSHSDS(RHICmdList, MaterialRenderProxy, MaterialResource, *View, VertexFactory, HullShader && DomainShader);
		}
		else
		{
			// Set the light-map policy.
			LightMapPolicy.Set(RHICmdList, VertexShader, !UseDebugViewPS() ? PixelShader : nullptr, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);

			VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, SceneTextureMode, PolicyContext.bIsInstancedStereo, bUseDownsampledTranslucencyViewUniformBuffer);

			if(HullShader)
			{
				HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
			}

			if (DomainShader)
			{
				DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
			}
		}

		if (UseDebugViewPS())
		{
			FDebugViewMode::GetPSInterface(View->ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetParameters(RHICmdList, VertexShader, PixelShader, MaterialRenderProxy, *MaterialResource, *View);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, BlendMode, bEnableEditorPrimitiveDepthTest, SceneTextureMode, PolicyContext.bIsInstancedStereo, bUseDownsampledTranslucencyViewUniformBuffer);
		}
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
	{
		VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
	}

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		FBoundShaderStateInput BoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader), 
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef()
			);

		if (UseDebugViewPS())
		{
			FDebugViewMode::PatchBoundShaderState(BoundShaderStateInput, MaterialResource, VertexFactory, InFeatureLevel, GetDebugViewShaderMode());
		}
		return BoundShaderStateInput;
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

		// If debug view shader mode are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (View.Family->UseDebugViewVSDSHS())
		{
			FDebugViewMode::SetMeshVSHSDS(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState, MaterialResource, HullShader && DomainShader);
		}
		else
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

			VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy, Mesh,BatchElement,DrawRenderState);
		
			if(HullShader && DomainShader)
			{
				HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
				DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
			}
		}

		if (UseDebugViewPS())
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FDebugViewMode::GetPSInterface(View.ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.VisualizeLODIndex, BatchElement, DrawRenderState);
#endif
		}
		else
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState,BlendMode);
		}
	}

	friend int32 CompareDrawingPolicy(const TBasePassDrawingPolicy& A,const TBasePassDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(SceneTextureMode);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableSkyLight);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableReceiveDecalOutput);

		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

protected:

	// Here we don't store the most derived type of shaders, for instance TBasePassVertexShaderBaseType<LightMapPolicyType>.
	// This is to allow any shader using the same parameters to be used, and is required to allow FUniformLightMapPolicy to use shaders derived from TUniformLightMapPolicy.
	TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>* VertexShader;
	FBaseHS* HullShader; // Does not depend on LightMapPolicyType
	FBaseDS* DomainShader; // Does not depend on LightMapPolicyType
	TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;

	ESceneRenderTargetsMode::Type SceneTextureMode;

	uint32 bEnableSkyLight : 1;

	/** Whether or not this policy enables atmospheric fog */
	uint32 bEnableAtmosphericFog : 1;

// WaveWorks Start
	TArray<uint32> WaveWorksShaderInputMapping;
// WaveWorks End
};

// WaveWorks Start
/**
* Draws a WaveWorks mesh
*/
template<typename LightMapPolicyType>
class TBasePassWaveWorksDrawingPolicy : public FBasePassDrawingPolicy
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
			: LightMapElementData(InLightMapElementData)
		{}
	};

	void ConfigureQuardTreeInputMapping(FMaterialShader* VSShader, FMaterialShader* HSShader, FMaterialShader* DSShader, FMaterialShader* PSShader)
	{
		const TArray<WaveWorksShaderInput>* QuadTreeShaderInput = GDynamicRHI->RHIGetDefaultContext()->RHIGetWaveWorksQuadTreeShaderInput();
		if (nullptr != QuadTreeShaderInput)
		{
			uint32 Count = QuadTreeShaderInput->Num();
			QuadTreeShaderInputMapping.SetNumZeroed(Count);

			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FMaterialShader* Shader = nullptr;
				switch ((*QuadTreeShaderInput)[Index].Frequency)
				{
				case SF_Vertex:
					Shader = VSShader;
					break;
				case SF_Hull:
					Shader = HSShader;
					break;
				case SF_Domain:
					Shader = DSShader;
					break;
				case SF_Pixel:
					Shader = PSShader;
					break;
				default:
					break;
				}

				FWaveWorksShaderParameters* WaveWorksShaderParams = Shader->GetWaveWorksShaderParameters();
				QuadTreeShaderInputMapping[Index] = WaveWorksShaderParams->QuadTreeShaderInputMappings[Index];
			}
		}
	}

	void ConfigureWaveWorksInputMapping(FMaterialShader* VSShader, FMaterialShader* HSShader, FMaterialShader* DSShader, FMaterialShader* PSShader)
	{
		const TArray<WaveWorksShaderInput>* WaveWorksShaderInput = GDynamicRHI->RHIGetDefaultContext()->RHIGetWaveWorksShaderInput();
		if (nullptr != WaveWorksShaderInput)
		{
			uint32 Count = WaveWorksShaderInput->Num();
			WaveWorksShaderInputMapping.SetNumZeroed(Count);

			for (uint32 Index = 0; Index < Count; ++Index)
			{
				FMaterialShader* Shader = nullptr;
				switch ((*WaveWorksShaderInput)[Index].Frequency)
				{
				case SF_Vertex:
					Shader = VSShader;
					break;
				case SF_Hull:
					Shader = HSShader;
					break;
				case SF_Domain:
					Shader = DSShader;
					break;
				case SF_Pixel:
					Shader = PSShader;
					break;
				default:
					break;
				}

				FWaveWorksShaderParameters* WaveWorksShaderParams = Shader->GetWaveWorksShaderParameters();
				WaveWorksShaderInputMapping[Index] = WaveWorksShaderParams->ShaderInputMappings[Index];
			}
		}
	}

	/** Initialization constructor. */
	TBasePassWaveWorksDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		ERHIFeatureLevel::Type InFeatureLevel,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		ESceneRenderTargetsMode::Type InSceneTextureMode,
		FMatrix InViewMatrix,
		FMatrix InProjMatrix,
		bool bInEnableSkyLight,
		bool bInEnableAtmosphericFog,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		EDebugViewShaderMode InDebugViewShaderMode = DVSM_None,
		bool bInEnableEditorPrimitiveDepthTest = false,
		bool bInEnableReceiveDecalOutput = false
	) :
		FBasePassDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings, 
			InDebugViewShaderMode, bInEnableReceiveDecalOutput, bInEnableEditorPrimitiveDepthTest),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode),
		SceneTextureMode(InSceneTextureMode),
		bEnableSkyLight(bInEnableSkyLight),
		bEnableAtmosphericFog(bInEnableAtmosphericFog),
		bEnableReceiveDecalOutput(bInEnableReceiveDecalOutput),
		CurrentViewMatrix(InViewMatrix),
		CurrentProjMatrix(InProjMatrix)
	{
		HullShader = NULL;
		DomainShader = NULL;

		const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetTessellationMode();

		const bool bNeedsHSDS = RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
			&& InVertexFactory->GetType()->SupportsTessellationShaders()
			&& MaterialTessellationMode != MTM_NoTessellation;

		GetBasePassShaders<LightMapPolicyType>(
			InMaterialResource,
			VertexFactory->GetType(),
			InLightMapPolicy,
			bNeedsHSDS,
			bEnableAtmosphericFog,
			bEnableSkyLight,
			HullShader,
			DomainShader,
			VertexShader,
			PixelShader
			);		

		QuadTreeShaderInputMapping.Empty();
		WaveWorksShaderInputMapping.Empty();

		ConfigureQuardTreeInputMapping(VertexShader, HullShader, DomainShader, PixelShader);
		ConfigureWaveWorksInputMapping(VertexShader, HullShader, DomainShader, PixelShader);

#if DO_GUARD_SLOW
		// Somewhat hacky
		if (SceneTextureMode == ESceneRenderTargetsMode::DontSet && !bEnableEditorPrimitiveDepthTest && InMaterialResource.IsUsedWithEditorCompositing())
		{
			SceneTextureMode = ESceneRenderTargetsMode::DontSetIgnoreBoundByEditorCompositing;
		}
#endif
	}

	// FMeshDrawingPolicy interface.

	FDrawingPolicyMatchResult Matches(const TBasePassWaveWorksDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(SceneTextureMode == Other.SceneTextureMode) &&
			DRAWING_POLICY_MATCH(bEnableSkyLight == Other.bEnableSkyLight) &&
			DRAWING_POLICY_MATCH(LightMapPolicy == Other.LightMapPolicy) &&
			DRAWING_POLICY_MATCH(bEnableReceiveDecalOutput == Other.bEnableReceiveDecalOutput);
		DRAWING_POLICY_MATCH_END
	}

	void SetSharedWaveWorksState(FRHICommandList& RHICmdList, const FViewInfo* View, FWaveWorksResource* WaveWorksResources) const
	{
		// If the current debug view shader modes are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (!View->Family->UseDebugViewVSDSHS())
		{
			VertexShader->SetWaveWorksParameters<FVertexShaderRHIParamRef>(RHICmdList, (FVertexShaderRHIParamRef)VertexShader->GetVertexShader(), *View, WaveWorksResources);

			if (HullShader)
			{
				HullShader->SetWaveWorksParameters<FHullShaderRHIParamRef>(RHICmdList, (FHullShaderRHIParamRef)HullShader->GetHullShader(), *View, WaveWorksResources);
			}

			if (DomainShader)
			{
				DomainShader->SetWaveWorksParameters<FDomainShaderRHIParamRef>(RHICmdList, (FDomainShaderRHIParamRef)DomainShader->GetDomainShader(), *View, WaveWorksResources);
			}
		}

		if (!UseDebugViewPS())
		{
			PixelShader->SetWaveWorksParameters<FPixelShaderRHIParamRef>(RHICmdList, (FPixelShaderRHIParamRef)PixelShader->GetPixelShader(), *View, WaveWorksResources);
		}

		FWaveWorksRHIRef WaveWorksRHI = WaveWorksResources->GetWaveWorksRHI();
		if (WaveWorksRHI)
		{
			RHICmdList.SetWaveWorksState(WaveWorksRHI, View->ViewMatrices.GetViewMatrix(), WaveWorksShaderInputMapping);
		}
	}

	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View) const
	{
		if (UseDebugViewPS())
		{
			if (IsTranslucentBlendMode(BlendMode))
			{
				if (View.Family->EngineShowFlags.ShaderComplexity)
				{	// If we are in the translucent pass then override the blend mode, otherwise maintain additive blending.
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_One>::GetRHI());
				}
				else if (View.Family->GetDebugViewShaderMode() != DVSM_OutputMaterialTextureScales)
				{	// Otherwise, force translucent blend mode (shaders will use an hardcoded alpha).
					DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
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
				// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
				// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Additive:
				// Add to the existing scene color
				// Note: alpha channel used by separate translucency, storing how much of the background should be added when doing the final composite
				// The Alpha channel is also used by non-separate translucency when rendering to scene captures, which store the final opacity
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			case BLEND_Modulate:
				// Modulate with the existing scene color, preserve destination alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGB, BO_Add, BF_DestColor, BF_Zero>::GetRHI());
				break;
			case BLEND_AlphaComposite:
				// Blend with existing scene color. New color is already pre-multiplied by alpha.
				DrawRenderState.SetBlendState(TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI());
				break;
			};
		}
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FViewInfo* View, const ContextDataType PolicyContext, bool bUseDownsampledTranslucencyViewUniformBuffer = false) const
	{
		// If the current debug view shader modes are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (View->Family->UseDebugViewVSDSHS())
		{
			FDebugViewMode::SetParametersVSHSDS(RHICmdList, MaterialRenderProxy, MaterialResource, *View, VertexFactory, HullShader && DomainShader);
		}
		else
		{
			// Set the light-map policy.
			LightMapPolicy.Set(RHICmdList, VertexShader, !UseDebugViewPS() ? PixelShader : nullptr, VertexShader, PixelShader, VertexFactory, MaterialRenderProxy, View);

			VertexShader->SetParameters(RHICmdList, MaterialRenderProxy, VertexFactory, *MaterialResource, *View, SceneTextureMode, PolicyContext.bIsInstancedStereo, bUseDownsampledTranslucencyViewUniformBuffer);

			if (HullShader)
			{
				HullShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
			}

			if (DomainShader)
			{
				DomainShader->SetParameters(RHICmdList, MaterialRenderProxy, *View);
			}
		}

		if (UseDebugViewPS())
		{
			FDebugViewMode::GetPSInterface(View->ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetParameters(RHICmdList, VertexShader, PixelShader, MaterialRenderProxy, *MaterialResource, *View);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, MaterialRenderProxy, *MaterialResource, View, BlendMode, bEnableEditorPrimitiveDepthTest, SceneTextureMode, PolicyContext.bIsInstancedStereo, bUseDownsampledTranslucencyViewUniformBuffer);
		}
	}

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const
	{
		VertexShader->SetInstancedEyeIndex(RHICmdList, EyeIndex);
	}

	/**
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel)
	{
		FBoundShaderStateInput BoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(),
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader),
			GETSAFERHISHADER_DOMAIN(DomainShader),
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef()
		);

		if (UseDebugViewPS())
		{
			FDebugViewMode::PatchBoundShaderState(BoundShaderStateInput, MaterialResource, VertexFactory, InFeatureLevel, GetDebugViewShaderMode());
		}
		return BoundShaderStateInput;
	}

	void SetMeshRenderState(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
	) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

		// If debug view shader mode are allowed, different VS/DS/HS must be used (with only SV_POSITION as PS interpolant).
		if (View.Family->UseDebugViewVSDSHS())
		{
			FDebugViewMode::SetMeshVSHSDS(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState, MaterialResource, HullShader && DomainShader);
		}
		else
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

			VertexShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh, BatchElement, DrawRenderState);

			if (HullShader && DomainShader)
			{
				HullShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
				DomainShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState);
			}
		}

		if (UseDebugViewPS())
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			FDebugViewMode::GetPSInterface(View.ShaderMap, MaterialResource, GetDebugViewShaderMode())->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, Mesh.VisualizeLODIndex, BatchElement, DrawRenderState);
#endif
		}
		else
		{
			PixelShader->SetMesh(RHICmdList, VertexFactory, View, PrimitiveSceneProxy, BatchElement, DrawRenderState, BlendMode);
		}
	}

	void DrawMesh(FRHICommandList& RHICmdList, const FMeshBatch& Mesh, int32 BatchElementIndex)
	{
		if (SceneProxy)
		{
			GFSDK_WaveWorks_Quadtree* QuadTreeHandle = SceneProxy->GetQuadTreeHandle();
			FWaveWorksResource* WaveworksResource = SceneProxy->GetWaveWorksResource();
			if (QuadTreeHandle != nullptr && WaveworksResource != nullptr)
			{
				auto WaveWorksRHI = WaveworksResource->GetWaveWorksRHI();
				if (NULL != WaveWorksRHI)
				{
					RHICmdList.DrawQuadTreeWaveWorks(WaveWorksRHI, QuadTreeHandle, CurrentViewMatrix, CurrentProjMatrix, QuadTreeShaderInputMapping);
				}
			}
		}
	}

	friend int32 CompareDrawingPolicy(const TBasePassWaveWorksDrawingPolicy& A, const TBasePassWaveWorksDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		COMPAREDRAWINGPOLICYMEMBERS(SceneTextureMode);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableSkyLight);
		COMPAREDRAWINGPOLICYMEMBERS(bEnableReceiveDecalOutput);

		return CompareDrawingPolicy(A.LightMapPolicy, B.LightMapPolicy);
	}

protected:

	// Here we don't store the most derived type of shaders, for instance TBasePassVertexShaderBaseType<LightMapPolicyType>.
	// This is to allow any shader using the same parameters to be used, and is required to allow FUniformLightMapPolicy to use shaders derived from TUniformLightMapPolicy.
	TBasePassVertexShaderPolicyParamType<typename LightMapPolicyType::VertexParametersType>* VertexShader;
	FBaseHS* HullShader; // Does not depend on LightMapPolicyType
	FBaseDS* DomainShader; // Does not depend on LightMapPolicyType
	TBasePassPixelShaderPolicyParamType<typename LightMapPolicyType::PixelParametersType>* PixelShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;

	ESceneRenderTargetsMode::Type SceneTextureMode;

	uint32 bEnableSkyLight : 1;

	/** Whether or not this policy is compositing editor primitives and needs to depth test against the scene geometry in the base pass pixel shader */
	uint32 bEnableEditorPrimitiveDepthTest : 1;
	/** Whether or not this policy enables atmospheric fog */
	uint32 bEnableAtmosphericFog : 1;

	/** Whether or not outputing the receive decal boolean */
	uint32 bEnableReceiveDecalOutput : 1;

public:
	/** Vertex/Hull Shader Input Mappings */
	TArray<uint32> QuadTreeShaderInputMapping;
	TArray<uint32> WaveWorksShaderInputMapping;

	/** The WaveWorks scene proxy */
	class FWaveWorksSceneProxy* SceneProxy;

	/** The current View matrix */
	FMatrix CurrentViewMatrix;

	/** The current Projection matrix */
	FMatrix CurrentProjMatrix;
};
// WaveWorks End


/**
 * A drawing policy factory for the base pass drawing policy.
 */
class FBasePassOpaqueDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		/** Whether or not to perform depth test in the pixel shader */
		bool bEditorCompositeDepthTest;

		ESceneRenderTargetsMode::Type TextureMode;

		ContextType(bool bInEditorCompositeDepthTest, ESceneRenderTargetsMode::Type InTextureMode)
			: bEditorCompositeDepthTest( bInEditorCompositeDepthTest )
			, TextureMode( InTextureMode )
		{}
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
		FHitProxyId HitProxyId, 
		const bool bIsInstancedStereo = false
		);
};

/** The parameters used to process a base pass mesh. */
class FProcessBasePassMeshParameters
{
public:

	const FMeshBatch& Mesh;
	const uint64 BatchElementMask;
	const FMaterial* Material;
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	EBlendMode BlendMode;
	EMaterialShadingModel ShadingModel;
	const bool bAllowFog;
	/** Whether or not to perform depth test in the pixel shader */
	const bool bEditorCompositeDepthTest;
	ESceneRenderTargetsMode::Type TextureMode;
	ERHIFeatureLevel::Type FeatureLevel;
	const bool bIsInstancedStereo;
	const bool bUseMobileMultiViewMask;

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		bool bInEditorCompositeDepthTest,
		ESceneRenderTargetsMode::Type InTextureMode,
		ERHIFeatureLevel::Type InFeatureLevel,
		const bool InbIsInstancedStereo = false,
		const bool InbUseMobileMultiViewMask = false
		):
		Mesh(InMesh),
		BatchElementMask(Mesh.Elements.Num()==1 ? 1 : (1<<Mesh.Elements.Num())-1), // 1 bit set for each mesh element
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
		TextureMode(InTextureMode),
		FeatureLevel(InFeatureLevel), 
		bIsInstancedStereo(InbIsInstancedStereo), 
		bUseMobileMultiViewMask(InbUseMobileMultiViewMask)
	{
	}

	/** Initialization constructor. */
	FProcessBasePassMeshParameters(
		const FMeshBatch& InMesh,
		const uint64& InBatchElementMask,
		const FMaterial* InMaterial,
		const FPrimitiveSceneProxy* InPrimitiveSceneProxy,
		bool InbAllowFog,
		bool bInEditorCompositeDepthTest,
		ESceneRenderTargetsMode::Type InTextureMode,
		ERHIFeatureLevel::Type InFeatureLevel, 
		bool InbIsInstancedStereo = false, 
		bool InbUseMobileMultiViewMask = false
		) :
		Mesh(InMesh),
		BatchElementMask(InBatchElementMask),
		Material(InMaterial),
		PrimitiveSceneProxy(InPrimitiveSceneProxy),
		BlendMode(InMaterial->GetBlendMode()),
		ShadingModel(InMaterial->GetShadingModel()),
		bAllowFog(InbAllowFog),
		bEditorCompositeDepthTest(bInEditorCompositeDepthTest),
		TextureMode(InTextureMode),
		FeatureLevel(InFeatureLevel),
		bIsInstancedStereo(InbIsInstancedStereo), 
		bUseMobileMultiViewMask(InbUseMobileMultiViewMask)
	{
	}
};

template<typename ProcessActionType>
void ProcessBasePassMeshForSimpleForwardShading(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	ProcessActionType&& Action,
	const FLightMapInteraction& LightMapInteraction,
	bool bIsLitMaterial,
	bool bAllowStaticLighting
	)
{
	if (bAllowStaticLighting && LightMapInteraction.GetType() == LMIT_Texture)
	{
		const FShadowMapInteraction ShadowMapInteraction = (Parameters.Mesh.LCI && bIsLitMaterial) 
			? Parameters.Mesh.LCI->GetShadowMapInteraction() 
			: FShadowMapInteraction();

		if (ShadowMapInteraction.GetType() == SMIT_Texture)
		{
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_PRECOMPUTED_SHADOW_LIGHTING), Parameters.Mesh.LCI);
		}
		else
		{
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_LIGHTMAP_ONLY_LIGHTING), Parameters.Mesh.LCI);
		}
	}
	if (bIsLitMaterial
		&& bAllowStaticLighting
		&& Action.UseVolumetricLightmap()
		&& Parameters.PrimitiveSceneProxy)
	{
		Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_VOLUMETRICLIGHTMAP_SHADOW_LIGHTING), Parameters.Mesh.LCI);
	}
	else if (bIsLitMaterial
		&& IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
		&& Action.AllowIndirectLightingCache()
		&& Parameters.PrimitiveSceneProxy)
	{
		const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
		const bool bPrimitiveIsMovable = Parameters.PrimitiveSceneProxy->IsMovable();
		const bool bPrimitiveUsesILC = Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;								

		// Use the indirect lighting cache shaders if the object has a cache allocation
		// This happens for objects with unbuilt lighting
		if (bPrimitiveUsesILC &&
			((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
			// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
			// And movable objects are sometimes rendered in the static draw lists
			|| bPrimitiveIsMovable))
		{
			// Use a lightmap policy that supports reading indirect lighting from a single SH sample
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_STATIONARY_SINGLESAMPLE_SHADOW_LIGHTING), Parameters.Mesh.LCI);
		}
		else
		{
			Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_NO_LIGHTMAP), Parameters.Mesh.LCI);
		}
	}
	else if (bIsLitMaterial)
	{
		// Always choosing shaders to support dynamic directional even if one is not present
		Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_DIRECTIONAL_LIGHT_LIGHTING), Parameters.Mesh.LCI);
	}
	else
	{
		Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_SIMPLE_NO_LIGHTMAP), Parameters.Mesh.LCI);
	}
}

/** Processes a base pass mesh using an unknown light map policy, and unknown fog density policy. */
template<typename ProcessActionType>
void ProcessBasePassMesh(
	FRHICommandList& RHICmdList,
	const FProcessBasePassMeshParameters& Parameters,
	ProcessActionType&& Action
	)
{
	// Check for a cached light-map.
	const bool bIsLitMaterial = (Parameters.ShadingModel != MSM_Unlit);
	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);
	
	
	const FLightMapInteraction LightMapInteraction = (bAllowStaticLighting && Parameters.Mesh.LCI && bIsLitMaterial) 
		? Parameters.Mesh.LCI->GetLightMapInteraction(Parameters.FeatureLevel) 
		: FLightMapInteraction();

	// force LQ lightmaps based on system settings
	const bool bPlatformAllowsHighQualityLightMaps = AllowHighQualityLightmaps(Parameters.FeatureLevel);
	const bool bAllowHighQualityLightMaps = bPlatformAllowsHighQualityLightMaps && LightMapInteraction.AllowsHighQualityLightmaps();

	if (IsSimpleForwardShadingEnabled(GetFeatureLevelShaderPlatform(Parameters.FeatureLevel)))
	{
		// Only compiling simple lighting shaders for HQ lightmaps to save on permutations
		check(bPlatformAllowsHighQualityLightMaps);
		ProcessBasePassMeshForSimpleForwardShading(RHICmdList, Parameters, Action, LightMapInteraction, bIsLitMaterial, bAllowStaticLighting);
	}
	// Render self-shadowing only for >= SM4 and fallback to non-shadowed for lesser shader models
	else if (bIsLitMaterial && Action.UseTranslucentSelfShadowing() && Parameters.FeatureLevel >= ERHIFeatureLevel::SM4)
	{
		if (bIsLitMaterial
			&& bAllowStaticLighting
			&& Action.UseVolumetricLightmap()
			&& Parameters.PrimitiveSceneProxy)
		{
			Action.template Process<FSelfShadowedVolumetricLightmapPolicy>(RHICmdList, Parameters, FSelfShadowedVolumetricLightmapPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
		}
		else if (IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
				&& Action.AllowIndirectLightingCache()
				&& Parameters.PrimitiveSceneProxy)
		{
			// Apply cached point indirect lighting as well as self shadowing if needed
			Action.template Process<FSelfShadowedCachedPointIndirectLightingPolicy>(RHICmdList, Parameters, FSelfShadowedCachedPointIndirectLightingPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
		}
		else
		{
			Action.template Process<FSelfShadowedTranslucencyPolicy>(RHICmdList, Parameters, FSelfShadowedTranslucencyPolicy(), FSelfShadowedTranslucencyPolicy::ElementDataType(Action.GetTranslucentSelfShadow()));
		}
	}
	else
	{
		static const auto CVarSupportLowQualityLightmap = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
		const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmap) || (CVarSupportLowQualityLightmap->GetValueOnAnyThread() != 0);

		switch (LightMapInteraction.GetType())
		{
		case LMIT_Texture:
			if (bAllowHighQualityLightMaps)
			{
				const FShadowMapInteraction ShadowMapInteraction = (bAllowStaticLighting && Parameters.Mesh.LCI && bIsLitMaterial)
					? Parameters.Mesh.LCI->GetShadowMapInteraction()
					: FShadowMapInteraction();

				if (ShadowMapInteraction.GetType() == SMIT_Texture)
				{
					Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_DISTANCE_FIELD_SHADOWS_AND_HQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
				else
				{
					Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_HQ_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else if (bAllowLowQualityLightMaps)
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_LQ_LIGHTMAP), Parameters.Mesh.LCI);
			}
			else
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
			}
			break;
		default:
			if (bIsLitMaterial
				&& bAllowStaticLighting
				&& Action.UseVolumetricLightmap()
				&& Parameters.PrimitiveSceneProxy
				&& (Parameters.PrimitiveSceneProxy->IsMovable() || Parameters.PrimitiveSceneProxy->NeedsUnbuiltPreviewLighting()))
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_PRECOMPUTED_IRRADIANCE_VOLUME_INDIRECT_LIGHTING), Parameters.Mesh.LCI);
			}
			else if (bIsLitMaterial
				&& IsIndirectLightingCacheAllowed(Parameters.FeatureLevel)
				&& Action.AllowIndirectLightingCache()
				&& Parameters.PrimitiveSceneProxy)
			{
				const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation = Parameters.PrimitiveSceneProxy->GetPrimitiveSceneInfo()->IndirectLightingCacheAllocation;
				const bool bPrimitiveIsMovable = Parameters.PrimitiveSceneProxy->IsMovable();
				const bool bPrimitiveUsesILC = Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() != ILCQ_Off;

				// Use the indirect lighting cache shaders if the object has a cache allocation
				// This happens for objects with unbuilt lighting
				if (bPrimitiveUsesILC &&
					((IndirectLightingCacheAllocation && IndirectLightingCacheAllocation->IsValid())
						// Use the indirect lighting cache shaders if the object is movable, it may not have a cache allocation yet because that is done in InitViews
						// And movable objects are sometimes rendered in the static draw lists
						|| bPrimitiveIsMovable))
				{
					if (CanIndirectLightingCacheUseVolumeTexture(Parameters.FeatureLevel)
						// Translucency forces point sample for pixel performance
						&& Action.AllowIndirectLightingCacheVolumeTexture()
						&& ((IndirectLightingCacheAllocation && !IndirectLightingCacheAllocation->bPointSample)
							|| (bPrimitiveIsMovable && Parameters.PrimitiveSceneProxy->GetIndirectLightingCacheQuality() == ILCQ_Volume)))
					{
						// Use a lightmap policy that supports reading indirect lighting from a volume texture for dynamic objects
						Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_CACHED_VOLUME_INDIRECT_LIGHTING), Parameters.Mesh.LCI);
					}
					else
					{
						// Use a lightmap policy that supports reading indirect lighting from a single SH sample
						Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_CACHED_POINT_INDIRECT_LIGHTING), Parameters.Mesh.LCI);
					}
				}
				else
				{
					Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
				}
			}
			else
			{
				Action.template Process< FUniformLightMapPolicy >(RHICmdList, Parameters, FUniformLightMapPolicy(LMP_NO_LIGHTMAP), Parameters.Mesh.LCI);
			}
			break;
		};
	}	
}
