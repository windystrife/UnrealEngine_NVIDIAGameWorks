// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShadowDepthRendering.cpp: Shadow depth rendering implementation
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "RHIDefinitions.h"
#include "HAL/IConsoleManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "RHI.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "RenderResource.h"
#include "RendererInterface.h"
#include "PrimitiveViewRelevance.h"
#include "UniformBuffer.h"
#include "Shader.h"
#include "StaticBoundShaderState.h"
#include "SceneUtils.h"
#include "Materials/Material.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "GlobalShader.h"
#include "MaterialShaderType.h"
#include "MaterialShader.h"
#include "DrawingPolicy.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "ShadowRendering.h"
#include "SceneRendering.h"
#include "LightPropagationVolume.h"
#include "ScenePrivate.h"
#include "PostProcess/SceneFilterRendering.h"
#include "ScreenRendering.h"
#include "ClearQuad.h"
#include "PipelineStateCache.h"
// @third party code - BEGIN HairWorks
#include "HairWorksRenderer.h"
// @third party code - END HairWorks

DECLARE_FLOAT_COUNTER_STAT(TEXT("Shadow Depths"), Stat_GPU_ShadowDepths, STATGROUP_GPU);

/**
 * A vertex shader for rendering the depth of a mesh.
 */
class FShadowDepthVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FShadowDepthVS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return false;
	}

	FShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		ShadowParameters.Bind(Initializer.ParameterMap);
		ShadowViewProjectionMatrices.Bind(Initializer.ParameterMap, TEXT("ShadowViewProjectionMatrices"));
		MeshVisibleToFace.Bind(Initializer.ParameterMap, TEXT("MeshVisibleToFace"));
		InstanceCount.Bind(Initializer.ParameterMap, TEXT("InstanceCount"));
	}

	FShadowDepthVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << ShadowParameters;
		Ar << ShadowViewProjectionMatrices;
		Ar << MeshVisibleToFace;
		Ar << InstanceCount;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,Material,View,View.ViewUniformBuffer,ESceneRenderTargetsMode::DontSet);
		ShadowParameters.SetVertexShader(RHICmdList, this, View, ShadowInfo, MaterialRenderProxy);
		
		if(ShadowViewProjectionMatrices.IsBound())
		{
			const FMatrix Translation = FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation());
			
			FMatrix TranslatedShadowViewProjectionMatrices[6];
			for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				// Have to apply the pre-view translation to the view - projection matrices
				TranslatedShadowViewProjectionMatrices[FaceIndex] = Translation * ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex];
			}
			
			// Set the view projection matrices that will transform positions from world to cube map face space
			SetShaderValueArray<FVertexShaderRHIParamRef, FMatrix>(RHICmdList,
																	 GetVertexShader(),
																	 ShadowViewProjectionMatrices,
																	 TranslatedShadowViewProjectionMatrices,
																	 ARRAY_COUNT(TranslatedShadowViewProjectionMatrices)
																	 );
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState,FProjectedShadowInfo const* ShadowInfo)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
		
		if (MeshVisibleToFace.IsBound())
		{
			const FBoxSphereBounds& PrimitiveBounds = Proxy->GetBounds();
			
			FVector4 MeshVisibleToFaceValue[6];
			for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				MeshVisibleToFaceValue[FaceIndex] = FVector4(ShadowInfo->OnePassShadowFrustums[FaceIndex].IntersectBox(PrimitiveBounds.Origin,PrimitiveBounds.BoxExtent), 0, 0, 0);
			}
			
			// Set the view projection matrices that will transform positions from world to cube map face space
			SetShaderValueArray<FVertexShaderRHIParamRef, FVector4>(
																	RHICmdList,
																	GetVertexShader(),
																	MeshVisibleToFace,
																	MeshVisibleToFaceValue,
																	ARRAY_COUNT(MeshVisibleToFaceValue)
																	);
		}
	}
		
	void SetDrawInstanceCount(FRHICommandList& RHICmdList, uint32 NumInstances)
	{
		if(InstanceCount.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstanceCount, NumInstances);
		}
	}

private:
	FShadowDepthShaderParameters ShadowParameters;
	FShaderParameter ShadowViewProjectionMatrices;
	FShaderParameter MeshVisibleToFace;
	FShaderParameter InstanceCount;
};

enum EShadowDepthVertexShaderMode
{
	VertexShadowDepth_PerspectiveCorrect,
	VertexShadowDepth_OutputDepth,
	VertexShadowDepth_OnePassPointLight
};

static TAutoConsoleVariable<int32> CVarSupportPointLightWholeSceneShadows(
	TEXT("r.SupportPointLightWholeSceneShadows"),
	1,
	TEXT("Enables shadowcasting point lights."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

/**
 * A vertex shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode, bool bRenderReflectiveShadowMap, bool bUsePositionOnlyStream, bool bIsForGeometryShader=false>
class TShadowDepthVS : public FShadowDepthVS
{
	DECLARE_SHADER_TYPE(TShadowDepthVS,MeshMaterial);
public:

	TShadowDepthVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthVS(Initializer)
	{
	}

	TShadowDepthVS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		static const auto CVarSupportAllShaderPermutations = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportAllShaderPermutations"));
		const bool bForceAllPermutations = CVarSupportAllShaderPermutations && CVarSupportAllShaderPermutations->GetValueOnAnyThread() != 0;
		const bool bSupportPointLightWholeSceneShadows = CVarSupportPointLightWholeSceneShadows.GetValueOnAnyThread() != 0 || bForceAllPermutations;
		const bool bRHISupportsShadowCastingPointLights = RHISupportsGeometryShaders(Platform) || RHISupportsVertexShaderLayer(Platform);

		if (bIsForGeometryShader && (!bSupportPointLightWholeSceneShadows || !bRHISupportsShadowCastingPointLights))
		{
			return false;
		}

		//Note: This logic needs to stay in sync with OverrideWithDefaultMaterialForShadowDepth!
		// Compile for special engine materials.
		if(bRenderReflectiveShadowMap)
		{
			// Reflective shadow map shaders must be compiled for every material because they access the material normal
			return !bUsePositionOnlyStream
				// Don't render ShadowDepth for translucent unlit materials, unless we're injecting emissive
				&& (Material->ShouldCastDynamicShadows() || Material->ShouldInjectEmissiveIntoLPV() 
					|| Material->ShouldBlockGI() )
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
		}
		else
		{
			return (Material->IsSpecialEngineMaterial()
						// Masked and WPO materials need their shaders but cannot be used with a position only stream.
					|| ((!Material->WritesEveryPixel(true) || Material->MaterialMayModifyMeshPosition()) && !bUsePositionOnlyStream))
					// Only compile one pass point light shaders for feature levels >= SM4
					&& (ShaderMode != VertexShadowDepth_OnePassPointLight || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
					// Only compile position-only shaders for vertex factories that support it.
					&& (!bUsePositionOnlyStream || VertexFactoryType->SupportsPositionOnly())
					// Don't render ShadowDepth for translucent unlit materials
					&& Material->ShouldCastDynamicShadows()
					// Only compile perspective correct light shaders for feature levels >= SM4
					&& (ShaderMode != VertexShadowDepth_PerspectiveCorrect || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4));
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FShadowDepthVS::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == VertexShadowDepth_PerspectiveCorrect));
		OutEnvironment.SetDefine(TEXT("ONEPASS_POINTLIGHT_SHADOW"), (uint32)(ShaderMode == VertexShadowDepth_OnePassPointLight));
		OutEnvironment.SetDefine(TEXT("REFLECTIVE_SHADOW_MAP"), (uint32)bRenderReflectiveShadowMap);
		OutEnvironment.SetDefine(TEXT("POSITION_ONLY"), (uint32)bUsePositionOnlyStream);

		if( bIsForGeometryShader )
		{
			OutEnvironment.CompilerFlags.Add( CFLAG_VertexToGeometryShader );
		}
	}
};


/**
 * A Hull shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode, bool bRenderReflectiveShadowMap> 
class TShadowDepthHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TShadowDepthHS,MeshMaterial);
public:

	
	TShadowDepthHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	TShadowDepthHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use ShouldCache from vertex shader
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use compilation env from vertex shader

		TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/**
 * A domain shader for rendering the depth of a mesh.
 */
class FShadowDepthDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FShadowDepthDS,MeshMaterial);
public:

	FShadowDepthDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{
		ShadowParameters.Bind(Initializer.ParameterMap);
		ShadowViewProjectionMatrices.Bind(Initializer.ParameterMap, TEXT("ShadowViewProjectionMatrices"));
	}

	FShadowDepthDS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FBaseDS::Serialize(Ar);
		Ar << ShadowParameters;
		Ar << ShadowViewProjectionMatrices;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FBaseDS::SetParameters(RHICmdList, MaterialRenderProxy, View);
		ShadowParameters.SetDomainShader(RHICmdList, this, View, ShadowInfo, MaterialRenderProxy);
		
		if(ShadowViewProjectionMatrices.IsBound())
		{
			const FMatrix Translation = FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation());
			
			FMatrix TranslatedShadowViewProjectionMatrices[6];
			for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				// Have to apply the pre-view translation to the view - projection matrices
				TranslatedShadowViewProjectionMatrices[FaceIndex] = Translation * ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex];
			}
			
			// Set the view projection matrices that will transform positions from world to cube map face space
			SetShaderValueArray<FDomainShaderRHIParamRef, FMatrix>(RHICmdList,
																   GetDomainShader(),
																   ShadowViewProjectionMatrices,
																   TranslatedShadowViewProjectionMatrices,
																   ARRAY_COUNT(TranslatedShadowViewProjectionMatrices)
																   );
		}
	}

private:
	FShadowDepthShaderParameters ShadowParameters;
	FShaderParameter ShadowViewProjectionMatrices;
};

/**
 * A Domain shader for rendering the depth of a mesh.
 */
template <EShadowDepthVertexShaderMode ShaderMode, bool bRenderReflectiveShadowMap> 
class TShadowDepthDS : public FShadowDepthDS
{
	DECLARE_SHADER_TYPE(TShadowDepthDS,MeshMaterial);
public:

	TShadowDepthDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShadowDepthDS(Initializer)
	{}

	TShadowDepthDS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// Re-use ShouldCache from vertex shader
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Re-use compilation env from vertex shader
		TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}
};

/** Geometry shader that allows one pass point light shadows by cloning triangles to all faces of the cube map. */
class FOnePassPointShadowDepthGS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FOnePassPointShadowDepthGS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return RHISupportsGeometryShaders(Platform) && TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, false, true>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, false, true>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	FOnePassPointShadowDepthGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		ShadowViewProjectionMatrices.Bind(Initializer.ParameterMap, TEXT("ShadowViewProjectionMatrices"));
		MeshVisibleToFace.Bind(Initializer.ParameterMap, TEXT("MeshVisibleToFace"));
	}

	FOnePassPointShadowDepthGS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		Ar << ShadowViewProjectionMatrices;
		Ar << MeshVisibleToFace;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		FMaterialShader::SetViewParameters(RHICmdList, GetGeometryShader(),View,View.ViewUniformBuffer);

		const FMatrix Translation = FTranslationMatrix(-View.ViewMatrices.GetPreViewTranslation());

		FMatrix TranslatedShadowViewProjectionMatrices[6];
		for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			// Have to apply the pre-view translation to the view - projection matrices
			TranslatedShadowViewProjectionMatrices[FaceIndex] = Translation * ShadowInfo->OnePassShadowViewProjectionMatrices[FaceIndex];
		}

		// Set the view projection matrices that will transform positions from world to cube map face space
		SetShaderValueArray<FGeometryShaderRHIParamRef, FMatrix>(
			RHICmdList, 
			GetGeometryShader(),
			ShadowViewProjectionMatrices,
			TranslatedShadowViewProjectionMatrices,
			ARRAY_COUNT(TranslatedShadowViewProjectionMatrices)
			);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FProjectedShadowInfo* ShadowInfo, const FSceneView& View)
	{
		if (MeshVisibleToFace.IsBound())
		{
			const FBoxSphereBounds& PrimitiveBounds = PrimitiveSceneProxy->GetBounds();

			FVector4 MeshVisibleToFaceValue[6];
			for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
			{
				MeshVisibleToFaceValue[FaceIndex] = FVector4(ShadowInfo->OnePassShadowFrustums[FaceIndex].IntersectBox(PrimitiveBounds.Origin,PrimitiveBounds.BoxExtent), 0, 0, 0);
			}

			// Set the view projection matrices that will transform positions from world to cube map face space
			SetShaderValueArray<FGeometryShaderRHIParamRef, FVector4>(
				RHICmdList, 
				GetGeometryShader(),
				MeshVisibleToFace,
				MeshVisibleToFaceValue,
				ARRAY_COUNT(MeshVisibleToFaceValue)
				);
		}
	}

private:
	FShaderParameter ShadowViewProjectionMatrices;
	FShaderParameter MeshVisibleToFace;
};

#define IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(ShaderMode,bRenderReflectiveShadowMap) \
	typedef TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false> TShadowDepthVS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVS##ShaderMode##bRenderReflectiveShadowMap,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("Main"),SF_Vertex);	\
	typedef TShadowDepthVS<ShaderMode, bRenderReflectiveShadowMap, false, true> TShadowDepthVSForGS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSForGS##ShaderMode##bRenderReflectiveShadowMap,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("MainForGS"),SF_Vertex);	\
	typedef TShadowDepthHS<ShaderMode, bRenderReflectiveShadowMap> TShadowDepthHS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthHS##ShaderMode##bRenderReflectiveShadowMap,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("MainHull"),SF_Hull);	\
	typedef TShadowDepthDS<ShaderMode, bRenderReflectiveShadowMap> TShadowDepthDS##ShaderMode##bRenderReflectiveShadowMap;	\
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthDS##ShaderMode##bRenderReflectiveShadowMap,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("MainDomain"),SF_Domain);

IMPLEMENT_SHADER_TYPE(,FOnePassPointShadowDepthGS,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("MainOnePassPointLightGS"),SF_Geometry);

IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_PerspectiveCorrect, true); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_PerspectiveCorrect, false); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OutputDepth, true); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OutputDepth, false); 
IMPLEMENT_SHADOW_DEPTH_SHADERMODE_SHADERS(VertexShadowDepth_OnePassPointLight, false);

// Position only vertex shaders.
typedef TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, false, true> TShadowDepthVSVertexShadowDepth_PerspectiveCorrectPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSVertexShadowDepth_PerspectiveCorrectPositionOnly,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("PositionOnlyMain"),SF_Vertex);
typedef TShadowDepthVS<VertexShadowDepth_OutputDepth, false, true> TShadowDepthVSVertexShadowDepth_OutputDepthPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSVertexShadowDepth_OutputDepthPositionOnly,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("PositionOnlyMain"),SF_Vertex);
typedef TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, true> TShadowDepthVSVertexShadowDepth_OnePassPointLightPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSVertexShadowDepth_OnePassPointLightPositionOnly,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("PositionOnlyMain"),SF_Vertex);
typedef TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, true, true> TShadowDepthVSForGSVertexShadowDepth_OnePassPointLightPositionOnly;
IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthVSForGSVertexShadowDepth_OnePassPointLightPositionOnly,TEXT("/Engine/Private/ShadowDepthVertexShader.usf"),TEXT("PositionOnlyMainForGS"),SF_Vertex);

/**
 * A pixel shader for rendering the depth of a mesh.
 */
template <bool bRenderReflectiveShadowMap>
class TShadowDepthBasePS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(TShadowDepthBasePS,MeshMaterial);
public:

	TShadowDepthBasePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FMeshMaterialShader(Initializer)
	{
		ShadowParams.Bind(Initializer.ParameterMap,TEXT("ShadowParams"));
		ReflectiveShadowMapTextureResolution.Bind(Initializer.ParameterMap,TEXT("ReflectiveShadowMapTextureResolution"));
		ProjectionMatrixParameter.Bind(Initializer.ParameterMap,TEXT("ProjectionMatrix"));
		GvListBuffer.Bind(Initializer.ParameterMap,TEXT("RWGvListBuffer"));
		GvListHeadBuffer.Bind(Initializer.ParameterMap,TEXT("RWGvListHeadBuffer"));
		VplListBuffer.Bind(Initializer.ParameterMap,TEXT("RWVplListBuffer"));
		VplListHeadBuffer.Bind(Initializer.ParameterMap,TEXT("RWVplListHeadBuffer"));
	}

	TShadowDepthBasePS() {}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View,
		const FProjectedShadowInfo* ShadowInfo
		)
	{
		const FPixelShaderRHIParamRef ShaderRHI = GetPixelShader();

		FMeshMaterialShader::SetParameters(RHICmdList, ShaderRHI, MaterialRenderProxy, Material, View, View.ViewUniformBuffer, ESceneRenderTargetsMode::DontSet);

		SetShaderValue(RHICmdList, ShaderRHI, ShadowParams, FVector2D(ShadowInfo->GetShaderDepthBias(), ShadowInfo->InvMaxSubjectDepth));

		if(bRenderReflectiveShadowMap)
		{
			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			// LPV also propagates light transmission (for transmissive materials)
			SetShaderValue(RHICmdList, ShaderRHI,ReflectiveShadowMapTextureResolution,
				FVector2D(SceneContext.GetReflectiveShadowMapResolution(),
					SceneContext.GetReflectiveShadowMapResolution()));
			SetShaderValue(
				RHICmdList, 
				ShaderRHI,
				ProjectionMatrixParameter,
				FTranslationMatrix(ShadowInfo->PreShadowTranslation - View.ViewMatrices.GetPreViewTranslation()) * ShadowInfo->SubjectAndReceiverMatrix
				);

			const FSceneViewState* ViewState = (const FSceneViewState*)View.State;
			if(ViewState)
			{
				const FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume(View.GetFeatureLevel());

				if(Lpv)
				{
					SetUniformBufferParameter(RHICmdList, ShaderRHI, GetUniformBufferParameter<FLpvWriteUniformBufferParameters>(), Lpv->GetRsmUniformBuffer());
				}
			}
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);

		Ar << ShadowParams;

		Ar << ReflectiveShadowMapTextureResolution;
		Ar << ProjectionMatrixParameter;
		Ar << GvListBuffer;
		Ar << GvListHeadBuffer;
		Ar << VplListBuffer;
		Ar << VplListHeadBuffer;

		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter ShadowParams;

	FShaderParameter ReflectiveShadowMapTextureResolution;
	FShaderParameter ProjectionMatrixParameter;

	FRWShaderParameter GvListBuffer;
	FRWShaderParameter GvListHeadBuffer;
	FRWShaderParameter VplListBuffer;
	FRWShaderParameter VplListHeadBuffer;
};

enum EShadowDepthPixelShaderMode
{
	PixelShadowDepth_NonPerspectiveCorrect,
	PixelShadowDepth_PerspectiveCorrect,
	PixelShadowDepth_OnePassPointLight
};

template <EShadowDepthPixelShaderMode ShaderMode, bool bRenderReflectiveShadowMap>
class TShadowDepthPS : public TShadowDepthBasePS<bRenderReflectiveShadowMap>
{
	DECLARE_SHADER_TYPE(TShadowDepthPS, MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		if (!IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
		{
			return (Material->IsSpecialEngineMaterial()
				// Only compile for masked or lit translucent materials
				|| !Material->WritesEveryPixel(true))
				&& ShaderMode == PixelShadowDepth_NonPerspectiveCorrect
				// Don't render ShadowDepth for translucent unlit materials
				&& Material->ShouldCastDynamicShadows()
				&& !bRenderReflectiveShadowMap;
		}

		if ( bRenderReflectiveShadowMap )
		{
			//Note: This logic needs to stay in sync with OverrideWithDefaultMaterialForShadowDepth!
			// Reflective shadow map shaders must be compiled for every material because they access the material normal
			return 
				// Only compile one pass point light shaders for feature levels >= SM4
				( Material->ShouldCastDynamicShadows() || Material->ShouldInjectEmissiveIntoLPV() || Material->ShouldBlockGI() )
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
		}
		else
		{
			//Note: This logic needs to stay in sync with OverrideWithDefaultMaterialForShadowDepth!
			return (Material->IsSpecialEngineMaterial()
					// Only compile for masked or lit translucent materials
					|| !Material->WritesEveryPixel(true)
					|| (Material->MaterialMayModifyMeshPosition() && Material->IsUsedWithInstancedStaticMeshes())
					// Perspective correct rendering needs a pixel shader and WPO materials can't be overridden with default material.
					|| (ShaderMode == PixelShadowDepth_PerspectiveCorrect && Material->MaterialMayModifyMeshPosition()))
				// Only compile one pass point light shaders for feature levels >= SM4
				&& (ShaderMode != PixelShadowDepth_OnePassPointLight || IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4))
				// Don't render ShadowDepth for translucent unlit materials
				&& Material->ShouldCastDynamicShadows()
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
		}
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		TShadowDepthBasePS<bRenderReflectiveShadowMap>::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("PERSPECTIVE_CORRECT_DEPTH"), (uint32)(ShaderMode == PixelShadowDepth_PerspectiveCorrect));
		OutEnvironment.SetDefine(TEXT("ONEPASS_POINTLIGHT_SHADOW"), (uint32)(ShaderMode == PixelShadowDepth_OnePassPointLight));
		OutEnvironment.SetDefine(TEXT("REFLECTIVE_SHADOW_MAP"), (uint32)bRenderReflectiveShadowMap);
	}

	TShadowDepthPS()
	{
	}

	TShadowDepthPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: TShadowDepthBasePS<bRenderReflectiveShadowMap>(Initializer)
	{
	}
};

// typedef required to get around macro expansion failure due to commas in template argument list for TShadowDepthPixelShader
#define IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(ShaderMode, bRenderReflectiveShadowMap) \
	typedef TShadowDepthPS<ShaderMode, bRenderReflectiveShadowMap> TShadowDepthPS##ShaderMode##bRenderReflectiveShadowMap; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TShadowDepthPS##ShaderMode##bRenderReflectiveShadowMap,TEXT("/Engine/Private/ShadowDepthPixelShader.usf"),TEXT("Main"),SF_Pixel);

IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_NonPerspectiveCorrect, true);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_NonPerspectiveCorrect, false);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_PerspectiveCorrect, true);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_PerspectiveCorrect, false);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_OnePassPointLight, true);
IMPLEMENT_SHADOWDEPTHPASS_PIXELSHADER_TYPE(PixelShadowDepth_OnePassPointLight, false);

/** 
 * Overrides a material used for shadow depth rendering with the default material when appropriate.
 * Overriding in this manner can reduce state switches and the number of shaders that have to be compiled.
 * This logic needs to stay in sync with shadow depth shader ShouldCache logic.
 */
void OverrideWithDefaultMaterialForShadowDepth(
	const FMaterialRenderProxy*& InOutMaterialRenderProxy, 
	const FMaterial*& InOutMaterialResource,
	bool bReflectiveShadowmap,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// Override with the default material when possible.
	if (InOutMaterialResource->WritesEveryPixel(true) &&						// Don't override masked materials.
		!InOutMaterialResource->MaterialModifiesMeshPosition_RenderThread() &&	// Don't override materials using world position offset.
		!bReflectiveShadowmap)													// Don't override when rendering reflective shadow maps.
	{
		const FMaterialRenderProxy* DefaultProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy(false);
		const FMaterial* DefaultMaterialResource = DefaultProxy->GetMaterial(InFeatureLevel);

		// Override with the default material for opaque materials that don't modify mesh position.
		InOutMaterialRenderProxy = DefaultProxy;
		InOutMaterialResource = DefaultMaterialResource;
	}
}

/*-----------------------------------------------------------------------------
	FShadowDepthDrawingPolicy
-----------------------------------------------------------------------------*/

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::UpdateElementState(FShadowStaticMeshElement& State, ERHIFeatureLevel::Type InFeatureLevel)
{
	FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(*State.Mesh);
	OverrideSettings.MeshOverrideFlags |= State.bIsTwoSided ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;

	// can be optimized
	*this = FShadowDepthDrawingPolicy(
		State.MaterialResource,
		bDirectionalLight,
		bOnePassPointLightShadow,
		bPreShadow,
		OverrideSettings,
		InFeatureLevel,
		State.Mesh->VertexFactory,
		State.RenderProxy,
		State.Mesh->ReverseCulling);
}

template <bool bRenderingReflectiveShadowMaps>
FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::FShadowDepthDrawingPolicy(
	const FMaterial* InMaterialResource,
	bool bInDirectionalLight,
	bool bInOnePassPointLightShadow,
	bool bInPreShadow,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	bool bInReverseCulling
	):
	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,*InMaterialResource,InOverrideSettings,DVSM_None),
	GeometryShader(NULL),
	FeatureLevel(InFeatureLevel),
	bDirectionalLight(bInDirectionalLight),
	bReverseCulling(bInReverseCulling),
	bOnePassPointLightShadow(bInOnePassPointLightShadow),
	bPreShadow(bInPreShadow)
{
	check(!bInOnePassPointLightShadow || !bRenderingReflectiveShadowMaps);

	if(!InVertexFactory)
	{
		// dummy object, needs call to UpdateElementState() to be fully initialized
		return;
	}

	// Use perspective correct shadow depths for shadow types which typically render low poly meshes into the shadow depth buffer.
	// Depth will be interpolated to the pixel shader and written out, which disables HiZ and double speed Z.
	// Directional light shadows use an ortho projection and can use the non-perspective correct path without artifacts.
	// One pass point lights don't output a linear depth, so they are already perspective correct.
	const bool bUsePerspectiveCorrectShadowDepths = !bInDirectionalLight && !bInOnePassPointLightShadow;

	HullShader = NULL;
	DomainShader = NULL;

	FVertexFactoryType* VFType = InVertexFactory->GetType();

	const bool bInitializeTessellationShaders = 
		MaterialResource->GetTessellationMode() != MTM_NoTessellation
		&& RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& VFType->SupportsTessellationShaders();

	bUsePositionOnlyVS = !bRenderingReflectiveShadowMaps
		&& VertexFactory->SupportsPositionOnlyStream()
		&& MaterialResource->WritesEveryPixel(true)
		&& !MaterialResource->MaterialModifiesMeshPosition_RenderThread();

	// Vertex related shaders
	if (bOnePassPointLightShadow)
	{
		if (bUsePositionOnlyVS)
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, true, true> >(VFType);
		}
		else
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OnePassPointLight, false, false, true> >(VFType);
		}
		if(RHISupportsGeometryShaders(GShaderPlatformForFeatureLevel[InFeatureLevel]))
		{
			// Use the geometry shader which will clone output triangles to all faces of the cube map
			GeometryShader = MaterialResource->GetShader<FOnePassPointShadowDepthGS>(VFType);
		}
		if(bInitializeTessellationShaders)
		{
			HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_OnePassPointLight, false> >(VFType);	
			DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_OnePassPointLight, false> >(VFType);	
		}
	}
	else if (bUsePerspectiveCorrectShadowDepths)
	{
		if (bRenderingReflectiveShadowMaps)
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, true, false> >(VFType);	
		}
		else
		{
			if (bUsePositionOnlyVS)
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, false, true> >(VFType);
			}
			else
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_PerspectiveCorrect, false, false> >(VFType);
			}
		}
		if(bInitializeTessellationShaders)
		{
			HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_PerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);	
			DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_PerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);	
		}
	}
	else
	{
		if (bRenderingReflectiveShadowMaps)
		{
			VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OutputDepth, true, false> >(VFType);	
			if(bInitializeTessellationShaders)
			{
				HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_OutputDepth, true> >(VFType);	
				DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_OutputDepth, true> >(VFType);	
			}
		}
		else
		{
			if (bUsePositionOnlyVS)
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OutputDepth, false, true> >(VFType);
			}
			else
			{
				VertexShader = MaterialResource->GetShader<TShadowDepthVS<VertexShadowDepth_OutputDepth, false, false> >(VFType);
			}
			if(bInitializeTessellationShaders)
			{
				HullShader = MaterialResource->GetShader<TShadowDepthHS<VertexShadowDepth_OutputDepth, false> >(VFType);	
				DomainShader = MaterialResource->GetShader<TShadowDepthDS<VertexShadowDepth_OutputDepth, false> >(VFType);	
			}
		}
	}

	// Pixel shaders
	if (MaterialResource->WritesEveryPixel(true) && !bUsePerspectiveCorrectShadowDepths && !bRenderingReflectiveShadowMaps && VertexFactory->SupportsNullPixelShader())
	{
		// No pixel shader necessary.
		PixelShader = NULL;
	}
	else
	{
		if (bUsePerspectiveCorrectShadowDepths)
		{
			PixelShader = (TShadowDepthBasePS<bRenderingReflectiveShadowMaps> *)MaterialResource->GetShader<TShadowDepthPS<PixelShadowDepth_PerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);
		}
		else if (bOnePassPointLightShadow)
		{
			PixelShader = (TShadowDepthBasePS<bRenderingReflectiveShadowMaps> *)MaterialResource->GetShader<TShadowDepthPS<PixelShadowDepth_OnePassPointLight, false> >(VFType);
		}
		else
		{
			PixelShader = (TShadowDepthBasePS<bRenderingReflectiveShadowMaps> *)MaterialResource->GetShader<TShadowDepthPS<PixelShadowDepth_NonPerspectiveCorrect, bRenderingReflectiveShadowMaps> >(VFType);
		}
	}
}

static void SetViewFlagsForShadowPass(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View, ERHIFeatureLevel::Type FeatureLevel, bool isTwoSided, bool isReflectiveShadowmap, bool isOnePassPointLightShadow)
{
	// @TODO: only render directional light shadows as two sided, and only when blocking is enabled (required by geometry volume injection)
	bool bIsTwoSided = isReflectiveShadowmap ? true : isTwoSided; //PolicyContext.ShadowInfo->bReflectiveShadowmap
	// Invert culling order when mobile HDR == false.
	auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
	static auto* MobileHDRCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MobileHDR"));
	check(MobileHDRCvar);
	const bool bPlatformReversesCulling = (RHINeedsToSwitchVerticalAxis(ShaderPlatform) && MobileHDRCvar->GetValueOnAnyThread() == 0);

	EDrawingPolicyOverrideFlags& ViewOverrideFlags = DrawRenderState.ModifyViewOverrideFlags();

	ViewOverrideFlags = (View.bRenderSceneTwoSided || bIsTwoSided) ?
		ViewOverrideFlags | EDrawingPolicyOverrideFlags::TwoSided : 
		ViewOverrideFlags & ~EDrawingPolicyOverrideFlags::TwoSided;
	ViewOverrideFlags = XOR(View.bReverseCulling, XOR(bPlatformReversesCulling, isOnePassPointLightShadow)) ?
		ViewOverrideFlags | EDrawingPolicyOverrideFlags::ReverseCullMode : 
		ViewOverrideFlags & ~EDrawingPolicyOverrideFlags::ReverseCullMode;
}

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType PolicyContext) const
{
	checkSlow(bDirectionalLight == PolicyContext.ShadowInfo->bDirectionalLight && bPreShadow == PolicyContext.ShadowInfo->bPreShadow);

	VertexShader->SetParameters(RHICmdList, MaterialRenderProxy,*MaterialResource,*View,PolicyContext.ShadowInfo);

	if (GeometryShader)
	{
		GeometryShader->SetParameters(RHICmdList, *View,PolicyContext.ShadowInfo);
	}

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View,PolicyContext.ShadowInfo);
	}

	if (PixelShader)
	{
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*MaterialResource,*View,PolicyContext.ShadowInfo);
	}

	// Set the shared mesh resources.
	if (bUsePositionOnlyVS)
	{
		VertexFactory->SetPositionStream(RHICmdList);
	}
	else
	{
		FMeshDrawingPolicy::SetSharedState(RHICmdList, DrawRenderState, View, PolicyContext);
	}
}

/** 
 * Create bound shader state using the vertex decl from the mesh draw policy
 * as well as the shaders needed to draw the mesh
 * @return new bound shader state object
 */
template <bool bRenderingReflectiveShadowMaps>
FBoundShaderStateInput FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
{
	FVertexDeclarationRHIRef VertexDeclaration;
	if (bUsePositionOnlyVS)
	{
		VertexDeclaration = VertexFactory->GetPositionDeclaration();
	}
	else
	{
		VertexDeclaration = FMeshDrawingPolicy::GetVertexDeclaration();
	}

	return FBoundShaderStateInput(
		VertexDeclaration, 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		GETSAFERHISHADER_PIXEL(PixelShader),
		GETSAFERHISHADER_GEOMETRY(GeometryShader));
}

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::SetMeshRenderState(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	const FMeshBatch& Mesh,
	int32 BatchElementIndex,
	const FDrawingPolicyRenderState& DrawRenderState,
	const ElementDataType& ElementData,
	const ContextDataType PolicyContext
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState, PolicyContext.ShadowInfo);

	if( HullShader && DomainShader )
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
	if (GeometryShader)
	{
		GeometryShader->SetMesh(RHICmdList, PrimitiveSceneProxy, PolicyContext.ShadowInfo, View);
	}
	if (PixelShader)
	{
		PixelShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}
	// Not calling FMeshDrawingPolicy::SetMeshRenderState as SetSharedState sets the rasterizer state
}

template <bool bRenderingReflectiveShadowMaps>
int32 CompareDrawingPolicy(const FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>& A,const FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
	COMPAREDRAWINGPOLICYMEMBERS(GeometryShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	COMPAREDRAWINGPOLICYMEMBERS(bDirectionalLight);
	COMPAREDRAWINGPOLICYMEMBERS(MeshPrimitiveType);
	COMPAREDRAWINGPOLICYMEMBERS(bOnePassPointLightShadow);
	COMPAREDRAWINGPOLICYMEMBERS(bUsePositionOnlyVS);
	COMPAREDRAWINGPOLICYMEMBERS(bPreShadow);
	return 0;
}

void FShadowDepthDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh)
{
	if (StaticMesh->CastShadow)
	{
		const auto FeatureLevel = Scene->GetFeatureLevel();
		const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
		const EBlendMode BlendMode = Material->GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material->GetShadingModel();

		const bool bLightPropagationVolume = UseLightPropagationVolumeRT(FeatureLevel);
		const bool bTwoSided  = Material->IsTwoSided() || StaticMesh->PrimitiveSceneInfo->Proxy->CastsShadowAsTwoSided();
		const bool bLitOpaque = !IsTranslucentBlendMode(BlendMode) && ShadingModel != MSM_Unlit;

		FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(*StaticMesh);
		OverrideSettings.MeshOverrideFlags |= bTwoSided ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;

		if (bLightPropagationVolume && ((!IsTranslucentBlendMode(BlendMode) && ShadingModel != MSM_Unlit) || Material->ShouldInjectEmissiveIntoLPV() || Material->ShouldBlockGI()))
		{
			// Add the static mesh to the shadow's subject draw list.
			if ( StaticMesh->PrimitiveSceneInfo->Proxy->AffectsDynamicIndirectLighting() )
			{
				Scene->WholeSceneReflectiveShadowMapDrawList.AddMesh(
					StaticMesh,
					FShadowDepthDrawingPolicy<true>::ElementDataType(),
					FShadowDepthDrawingPolicy<true>(
						Material,
						true,
						false,
						false,
						OverrideSettings,
						FeatureLevel,
						StaticMesh->VertexFactory,
						MaterialRenderProxy,
						StaticMesh->ReverseCulling),
					FeatureLevel
					);
			}
		}
		if ( bLitOpaque )
		{
			OverrideWithDefaultMaterialForShadowDepth(MaterialRenderProxy, Material, false, FeatureLevel); 

			// Add the static mesh to the shadow's subject draw list.
			Scene->WholeSceneShadowDepthDrawList.AddMesh(
				StaticMesh,
				FShadowDepthDrawingPolicy<false>::ElementDataType(),
				FShadowDepthDrawingPolicy<false>(
					Material,
					true,
					false,
					false,
					OverrideSettings,
					FeatureLevel,
					StaticMesh->VertexFactory,
					MaterialRenderProxy,
					StaticMesh->ReverseCulling),
				FeatureLevel
				);
		}
	}
}

bool FShadowDepthDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FSceneView& View,
	ContextType Context,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	bool bDirty = false;

	// Use a per-FMeshBatch check on top of the per-primitive check because dynamic primitives can submit multiple FMeshElements.
	if (Mesh.CastShadow)
	{
		const auto FeatureLevel = View.GetFeatureLevel();
		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
		const EBlendMode BlendMode = Material->GetBlendMode();
		const EMaterialShadingModel ShadingModel = Material->GetShadingModel();

		const bool bLocalOnePassPointLightShadow = Context.ShadowInfo->bOnePassPointLightShadow;
		const bool bReflectiveShadowmap = Context.ShadowInfo->bReflectiveShadowmap && !bLocalOnePassPointLightShadow;

		bool bProcess = !IsTranslucentBlendMode(BlendMode) && ShadingModel != MSM_Unlit && ShouldIncludeDomainInMeshPass(Material->GetMaterialDomain());

		if (bReflectiveShadowmap && Material->ShouldInjectEmissiveIntoLPV())
		{
			bProcess = true;
		}

		if (bProcess)
		{
			const bool bLocalDirectionalLight = Context.ShadowInfo->bDirectionalLight;
			const bool bPreShadow = Context.ShadowInfo->bPreShadow;
			const bool bTwoSided = Material->IsTwoSided() || PrimitiveSceneProxy->CastsShadowAsTwoSided();
			const FShadowDepthDrawingPolicyContext PolicyContext(Context.ShadowInfo);

			FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(Mesh);
			OverrideSettings.MeshOverrideFlags |= bTwoSided ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;

			OverrideWithDefaultMaterialForShadowDepth(MaterialRenderProxy, Material, bReflectiveShadowmap, FeatureLevel);

			if(bReflectiveShadowmap)
			{
				FShadowDepthDrawingPolicy<true> DrawingPolicy(
					MaterialRenderProxy->GetMaterial(FeatureLevel),
					bLocalDirectionalLight,
					bLocalOnePassPointLightShadow,
					bPreShadow,
					OverrideSettings,
					FeatureLevel,
					Mesh.VertexFactory,
					MaterialRenderProxy,
					Mesh.ReverseCulling
					);

				//TODO MaybeRemovable if ShadowDepth never support LOD Transitions
				FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
				DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
				SetViewFlagsForShadowPass(DrawRenderStateLocal, View, View.GetFeatureLevel(), DrawingPolicy.IsTwoSided(), true, bLocalOnePassPointLightShadow);
				DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
				CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
				DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, PolicyContext);

				for (int32 BatchElementIndex = 0, Num = Mesh.Elements.Num(); BatchElementIndex < Num; BatchElementIndex++)
				{
					TDrawEvent<FRHICommandList> MeshEvent;
					BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,DrawRenderStateLocal,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
			}
			else
			{
				FShadowDepthDrawingPolicy<false> DrawingPolicy(
					MaterialRenderProxy->GetMaterial(FeatureLevel),
					bLocalDirectionalLight,
					bLocalOnePassPointLightShadow,
					bPreShadow,
					OverrideSettings,
					FeatureLevel,
					Mesh.VertexFactory,
					MaterialRenderProxy,
					Mesh.ReverseCulling
					);

				//TODO MaybeRemovable if ShadowDepth never support LOD Transitions
				FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
				DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
				SetViewFlagsForShadowPass(DrawRenderStateLocal, View, View.GetFeatureLevel(), DrawingPolicy.IsTwoSided(), false, bLocalOnePassPointLightShadow);
				DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
				CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
				DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, PolicyContext);

				for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					TDrawEvent<FRHICommandList> MeshEvent;
					BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,DrawRenderStateLocal,FMeshDrawingPolicy::ElementDataType(),PolicyContext);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
			}

			
			bDirty = true;
		}
	}
	
	return bDirty;
}

template <bool bRenderingReflectiveShadowMaps>
void FShadowDepthDrawingPolicy<bRenderingReflectiveShadowMaps>::DrawMesh(FRHICommandList& RHICmdList, const FMeshBatch& Mesh, int32 BatchElementIndex, const bool bIsInstancedStereo) const
{
	if(!bOnePassPointLightShadow || RHISupportsGeometryShaders(GShaderPlatformForFeatureLevel[FeatureLevel]))
	{
		FMeshDrawingPolicy::DrawMesh(RHICmdList, Mesh, BatchElementIndex, bIsInstancedStereo);
	}
	else
	{
		INC_DWORD_STAT(STAT_MeshDrawCalls);
		SCOPED_DRAW_EVENT(RHICmdList, OnePassPointLightMeshDraw);
		
		const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];
		
		if (Mesh.UseDynamicData)
		{
			check(Mesh.DynamicVertexData);
			
			// @todo This code path *assumes* that DrawPrimitiveUP & DrawIndexedPrimitiveUP implicitly
			// turn the following into instanced draw calls to route a draw to each face.
			// This avoids adding anything to the public RHI API but is a filthy hack.
			
			VertexShader->SetDrawInstanceCount(RHICmdList, 1);
			if (BatchElement.DynamicIndexData)
			{
				DrawIndexedPrimitiveUP(
									   RHICmdList,
									   Mesh.Type,
									   BatchElement.MinVertexIndex,
									   BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
									   BatchElement.NumPrimitives,
									   BatchElement.DynamicIndexData,
									   BatchElement.DynamicIndexStride,
									   Mesh.DynamicVertexData,
									   Mesh.DynamicVertexStride
									   );
			}
			else
			{
				DrawPrimitiveUP(
								RHICmdList,
								Mesh.Type,
								BatchElement.NumPrimitives,
								Mesh.DynamicVertexData,
								Mesh.DynamicVertexStride
								);
			}
		}
		else
		{
			if(BatchElement.IndexBuffer)
			{
				check(BatchElement.IndexBuffer->IsInitialized());
				if (BatchElement.bIsInstanceRuns)
				{
					checkSlow(BatchElement.bIsInstanceRuns);
					if (bUsePositionOnlyVS)
					{
						for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
						{
							VertexFactory->OffsetPositionInstanceStreams(RHICmdList, BatchElement.InstanceRuns[Run * 2]);
							uint32 Instances = 1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2];
							VertexShader->SetDrawInstanceCount(RHICmdList, Instances);
							RHICmdList.DrawIndexedPrimitive(
															BatchElement.IndexBuffer->IndexBufferRHI,
															Mesh.Type,
															0,
															0,
															BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
															BatchElement.FirstIndex,
															BatchElement.NumPrimitives,
															Instances
															);
						}
					}
					else
					{
						for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
						{
							VertexFactory->OffsetInstanceStreams(RHICmdList, BatchElement.InstanceRuns[Run * 2]);
							uint32 Instances = 1 + BatchElement.InstanceRuns[Run * 2 + 1] - BatchElement.InstanceRuns[Run * 2];
							VertexShader->SetDrawInstanceCount(RHICmdList, Instances);
							RHICmdList.DrawIndexedPrimitive(
															BatchElement.IndexBuffer->IndexBufferRHI,
															Mesh.Type,
															0,
															0,
															BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
															BatchElement.FirstIndex,
															BatchElement.NumPrimitives,
															Instances * 6
															);
						}
					}
				}
				else
				{
					// Point light shadow cube maps shouldn't be rendered in stereo
					check(!bIsInstancedStereo);

					VertexShader->SetDrawInstanceCount(RHICmdList, BatchElement.NumInstances);
					RHICmdList.DrawIndexedPrimitive(
													BatchElement.IndexBuffer->IndexBufferRHI,
													Mesh.Type,
													0,
													0,
													BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
													BatchElement.FirstIndex,
													BatchElement.NumPrimitives,
													BatchElement.NumInstances * 6
													);
				}
			}
			else
			{
				VertexShader->SetDrawInstanceCount(RHICmdList, BatchElement.NumInstances);
				RHICmdList.DrawPrimitive(
										 Mesh.Type,
										 BatchElement.FirstIndex,
										 BatchElement.NumPrimitives,
										 BatchElement.NumInstances * 6
										 );
			}
		}
	}
}

/*-----------------------------------------------------------------------------
	FProjectedShadowInfo
-----------------------------------------------------------------------------*/

static void CheckShadowDepthMaterials(const FMaterialRenderProxy* InRenderProxy, const FMaterial* InMaterial, bool bReflectiveShadowmap, ERHIFeatureLevel::Type InFeatureLevel)
{
	const FMaterialRenderProxy* RenderProxy = InRenderProxy;
	const FMaterial* Material = InMaterial;
	OverrideWithDefaultMaterialForShadowDepth(RenderProxy, Material, bReflectiveShadowmap, InFeatureLevel);
	check(RenderProxy == InRenderProxy);
	check(Material == InMaterial);
}

void FProjectedShadowInfo::ClearDepth(FRHICommandList& RHICmdList, class FSceneRenderer* SceneRenderer, int32 NumColorTextures, FTextureRHIParamRef* ColorTextures, FTextureRHIParamRef DepthTexture, bool bPerformClear)
{
	uint32 ViewportMinX = X;
	uint32 ViewportMinY = Y;
	float ViewportMinZ = 0.0f;
	uint32 ViewportMaxX = X + BorderSize * 2 + ResolutionX;
	uint32 ViewportMaxY = Y + BorderSize * 2 + ResolutionY;
	float ViewportMaxZ = 1.0f;

	int32 NumClearColors;
	bool bClearColor;
	FLinearColor Colors[2];

	// Translucent shadows use draw call clear
	check(!bTranslucentShadow);
		
	if (bReflectiveShadowmap)
	{
		// Clear color and depth targets			
		bClearColor = true;
		Colors[0] = FLinearColor(0, 0, 1, 0);
		Colors[1] = FLinearColor(0, 0, 0, 0);
		
		NumClearColors = FMath::Min(2, NumColorTextures);
	}
	else
	{
		// Clear depth only.
		bClearColor = false;
		Colors[0] = FLinearColor::White;
		NumClearColors = FMath::Min(1, NumColorTextures);
	}

	if (bPerformClear)
	{
		RHICmdList.SetViewport(
			ViewportMinX,
			ViewportMinY,
			ViewportMinZ,
			ViewportMaxX,
			ViewportMaxY,
			ViewportMaxZ
			);
		
		DrawClearQuadMRT(RHICmdList, bClearColor, NumClearColors, Colors, true, 1.0f, false, 0);
	}
	else
	{
		RHICmdList.BindClearMRTValues(bClearColor, true, false);
	}
}

template <bool bReflectiveShadowmap>
void DrawMeshElements(FRHICommandList& RHICmdList, FShadowDepthDrawingPolicy<bReflectiveShadowmap>& SharedDrawingPolicy, const FShadowStaticMeshElement& State, const FViewInfo& View, FShadowDepthDrawingPolicyContext PolicyContext, const FDrawingPolicyRenderState& DrawRenderState, const FStaticMesh* Mesh)
{
#if UE_BUILD_DEBUG
	// During shadow setup we should have already overridden materials with default material where needed.
	// Make sure of it!
	CheckShadowDepthMaterials(State.RenderProxy, State.MaterialResource, bReflectiveShadowmap, View.GetFeatureLevel());
#endif

#if UE_BUILD_DEBUG
	FMeshDrawingPolicyOverrideSettings OverrideSettings = ComputeMeshOverrideSettings(*State.Mesh);
	OverrideSettings.MeshOverrideFlags |= State.bIsTwoSided ? EDrawingPolicyOverrideFlags::TwoSided : EDrawingPolicyOverrideFlags::None;

	FShadowDepthDrawingPolicy<bReflectiveShadowmap> DebugPolicy(
		State.MaterialResource,
		SharedDrawingPolicy.bDirectionalLight,
		SharedDrawingPolicy.bOnePassPointLightShadow,
		SharedDrawingPolicy.bPreShadow,
		OverrideSettings,
		View.GetFeatureLevel(),
		State.Mesh->VertexFactory,
		State.RenderProxy,
		State.Mesh->ReverseCulling);
	// Verify that SharedDrawingPolicy can be used to draw this mesh without artifacts by checking the comparison functions that static draw lists use
	checkSlow(DebugPolicy.Matches(SharedDrawingPolicy).Result());
	checkSlow(CompareDrawingPolicy(DebugPolicy, SharedDrawingPolicy) == 0);
#endif

	//TODO MaybeRemovable if ShadowDepth never support LOD Transitions
	FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
	SharedDrawingPolicy.ApplyDitheredLODTransitionState(DrawRenderStateLocal, View, *Mesh, false);

	// Render only those batch elements that match the current LOD
	uint64 BatchElementMask = Mesh->bRequiresPerElementVisibility ? View.StaticMeshBatchVisibility[Mesh->BatchVisibilityId] : ((1ull << Mesh->Elements.Num()) - 1);
	int32 BatchElementIndex = 0;
	do
	{
		if(BatchElementMask & 1)
		{
			TDrawEvent<FRHICommandList> MeshEvent;
			BeginMeshDrawEvent(RHICmdList, Mesh->PrimitiveSceneInfo->Proxy, *Mesh, MeshEvent);

			SharedDrawingPolicy.SetMeshRenderState(RHICmdList, View, Mesh->PrimitiveSceneInfo->Proxy, *Mesh, BatchElementIndex, DrawRenderStateLocal, FMeshDrawingPolicy::ElementDataType(),PolicyContext);
			SharedDrawingPolicy.DrawMesh(RHICmdList, *Mesh, BatchElementIndex);
			INC_DWORD_STAT(STAT_ShadowDynamicPathDrawCalls);
		}

		BatchElementMask >>= 1;
		BatchElementIndex++;
	} while(BatchElementMask);
}

template <bool bReflectiveShadowmap>
void DrawShadowMeshElements(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const FProjectedShadowInfo& ShadowInfo)
{
	FShadowDepthDrawingPolicyContext PolicyContext(&ShadowInfo);
	const FShadowStaticMeshElement& FirstShadowMesh = ShadowInfo.StaticSubjectMeshElements[0];
	const FMaterial* FirstMaterialResource = FirstShadowMesh.MaterialResource;
	auto FeatureLevel = View.GetFeatureLevel();

	FShadowDepthDrawingPolicy<bReflectiveShadowmap> SharedDrawingPolicy(
		FirstMaterialResource,
		ShadowInfo.bDirectionalLight,
		ShadowInfo.bOnePassPointLightShadow,
		ShadowInfo.bPreShadow,
		ComputeMeshOverrideSettings(*FirstShadowMesh.Mesh),
		FeatureLevel);

	FShadowStaticMeshElement OldState;

	FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);

	uint32 ElementCount = ShadowInfo.StaticSubjectMeshElements.Num();
	for(uint32 ElementIndex = 0; ElementIndex < ElementCount; ++ElementIndex)
	{
		const FShadowStaticMeshElement& ShadowMesh = ShadowInfo.StaticSubjectMeshElements[ElementIndex];

		if(!View.StaticMeshShadowDepthMap[ShadowMesh.Mesh->Id])
		{
			// not visible
			continue;
		}

		FShadowStaticMeshElement CurrentState(ShadowMesh.RenderProxy, ShadowMesh.MaterialResource, ShadowMesh.Mesh, ShadowMesh.bIsTwoSided);

		// Only call draw shared when the vertex factory or material have changed
		if(OldState.DoesDeltaRequireADrawSharedCall(CurrentState))
		{
			OldState = CurrentState;

			SharedDrawingPolicy.UpdateElementState(CurrentState, FeatureLevel);
			DrawRenderStateLocal.SetDitheredLODTransitionAlpha(ShadowMesh.Mesh->DitheredLODTransitionAlpha);
			SetViewFlagsForShadowPass(DrawRenderStateLocal, View, View.GetFeatureLevel(), SharedDrawingPolicy.IsTwoSided(), bReflectiveShadowmap, ShadowInfo.bOnePassPointLightShadow);
			SharedDrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
			CommitGraphicsPipelineState(RHICmdList, SharedDrawingPolicy, DrawRenderStateLocal, SharedDrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
			SharedDrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, PolicyContext);
		}

		DrawMeshElements(RHICmdList, SharedDrawingPolicy, OldState, View, PolicyContext, DrawRenderStateLocal, ShadowMesh.Mesh);
	}
}

void FProjectedShadowInfo::RenderDepthDynamic(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer, const FViewInfo* FoundView, const FDrawingPolicyRenderState& DrawRenderState)
{
	// Draw the subject's dynamic elements.
	SCOPE_CYCLE_COUNTER(STAT_WholeSceneDynamicShadowDepthsTime);

	FShadowDepthDrawingPolicyFactory::ContextType Context(this);

	for (int32 MeshBatchIndex = 0; MeshBatchIndex < DynamicSubjectMeshElements.Num(); MeshBatchIndex++)
	{
		const FMeshBatchAndRelevance& MeshBatchAndRelevance = DynamicSubjectMeshElements[MeshBatchIndex];
		const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
		FShadowDepthDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, *FoundView, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
	}

	// @third party code - BEGIN HairWorks
	// Draw hairs.
	checkSlow(RHICmdList.IsImmediate());
	if(RHICmdList.IsImmediate())
		HairWorksRenderer::RenderShadow(static_cast<FRHICommandListImmediate&>(RHICmdList), *this, DynamicSubjectPrimitives, *FoundView);
	// @third party code - END HairWorks
}

class FDrawShadowMeshElementsThreadTask : public FRenderTask
{
	FProjectedShadowInfo& ThisShadow;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	bool bReflective;

public:

	FDrawShadowMeshElementsThreadTask(
		FProjectedShadowInfo& InThisShadow,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState,
		bool InbReflective
		)
		: ThisShadow(InThisShadow)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
		, bReflective(InbReflective)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FDrawShadowMeshElementsThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticShadowDepthsTime);

		if (bReflective)
		{
			// reflective shadow map
			DrawShadowMeshElements<true>(RHICmdList, View, DrawRenderState, ThisShadow);
		}
		else
		{
			// normal shadow map
			DrawShadowMeshElements<false>(RHICmdList, View, DrawRenderState, ThisShadow);
		}
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

class FRenderDepthDynamicThreadTask : public FRenderTask
{
	FProjectedShadowInfo& ThisShadow;
	FRHICommandList& RHICmdList;
	const FViewInfo& View;
	FDrawingPolicyRenderState DrawRenderState;
	FSceneRenderer* SceneRenderer;

public:

	FRenderDepthDynamicThreadTask(
		FProjectedShadowInfo& InThisShadow,
		FRHICommandList& InRHICmdList,
		const FViewInfo& InView,
		const FDrawingPolicyRenderState& InDrawRenderState,
		FSceneRenderer* InSceneRenderer
		)
		: ThisShadow(InThisShadow)
		, RHICmdList(InRHICmdList)
		, View(InView)
		, DrawRenderState(InDrawRenderState)
		, SceneRenderer(InSceneRenderer)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FRenderDepthDynamicThreadTask, STATGROUP_TaskGraphTasks);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		ThisShadow.RenderDepthDynamic(RHICmdList, SceneRenderer, &View, DrawRenderState);
		RHICmdList.HandleRTThreadTaskCompletion(MyCompletionGraphEvent);
	}
};

void FProjectedShadowInfo::SetStateForDepth(FRHICommandList& RHICmdList, EShadowDepthRenderMode RenderMode, FDrawingPolicyRenderState& DrawRenderState)
{
	check(bAllocated);

	RHICmdList.SetViewport(
		X + BorderSize,
		Y + BorderSize,
		0.0f,
		X + BorderSize + ResolutionX,
		Y + BorderSize + ResolutionY,
		1.0f
		);

	// GIBlockingVolumes render mode only affects the reflective shadow map, using the opacity of the material to multiply against the existing color.
	if (RenderMode == ShadowDepthRenderMode_GIBlockingVolumes)
	{
		DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One,
			CW_RGBA, BO_Add, BF_Zero, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI());
	}
	// The EmissiveOnly render mode shouldn't write into the reflective shadow map, only into the LPV.
	else if (RenderMode == ShadowDepthRenderMode_EmissiveOnly)
	{
		DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE, BO_Add, BF_Zero, BF_One, BO_Add, BF_Zero, BF_One, CW_NONE>::GetRHI());
	}
	else if (bReflectiveShadowmap && !bOnePassPointLightShadow)
	{
		// Enable color writes to the reflective shadow map targets with opaque blending
		DrawRenderState.SetBlendState(TStaticBlendStateWriteMask<CW_RGBA, CW_RGBA>::GetRHI());
	}
	else
	{
		// Disable color writes
		DrawRenderState.SetBlendState(TStaticBlendState<CW_NONE>::GetRHI());
	}

	if (RenderMode == ShadowDepthRenderMode_EmissiveOnly || RenderMode == ShadowDepthRenderMode_GIBlockingVolumes)
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false, CF_LessEqual>::GetRHI());
	}
	else
	{
		DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true, CF_LessEqual>::GetRHI());
	}
}

static TAutoConsoleVariable<int32> CVarParallelShadows(
	TEXT("r.ParallelShadows"),
	1,
	TEXT("Toggles parallel shadow rendering. Parallel rendering must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);
static TAutoConsoleVariable<int32> CVarParallelShadowsNonWholeScene(
	TEXT("r.ParallelShadowsNonWholeScene"),
	0,
	TEXT("Toggles parallel shadow rendering for non whole-scene shadows. r.ParallelShadows must be enabled for this to have an effect."),
	ECVF_RenderThreadSafe
	);


static TAutoConsoleVariable<int32> CVarRHICmdShadowDeferredContexts(
	TEXT("r.RHICmdShadowDeferredContexts"),
	1,
	TEXT("True to use deferred contexts to parallelize shadow command list execution."));

static TAutoConsoleVariable<int32> CVarRHICmdFlushRenderThreadTasksShadowPass(
	TEXT("r.RHICmdFlushRenderThreadTasksShadowPass"),
	0,
	TEXT("Wait for completion of parallel render thread tasks at the end of each shadow pass.  A more granular version of r.RHICmdFlushRenderThreadTasks. If either r.RHICmdFlushRenderThreadTasks or r.RHICmdFlushRenderThreadTasksShadowPass is > 0 we will flush."));

DECLARE_CYCLE_STAT(TEXT("Shadow"), STAT_CLP_Shadow, STATGROUP_ParallelCommandListMarkers);

class FShadowParallelCommandListSet : public FParallelCommandListSet
{
	FProjectedShadowInfo& ProjectedShadowInfo;
	FSetShadowRenderTargetFunction SetShadowRenderTargets;
	EShadowDepthRenderMode RenderMode;

public:
	FShadowParallelCommandListSet(const FViewInfo& InView, FRHICommandListImmediate& InParentCmdList, bool bInParallelExecute, bool bInCreateSceneContext, FProjectedShadowInfo& InProjectedShadowInfo, FSetShadowRenderTargetFunction InSetShadowRenderTargets, EShadowDepthRenderMode RenderModeIn )
		: FParallelCommandListSet(GET_STATID(STAT_CLP_Shadow), InView, InParentCmdList, bInParallelExecute, bInCreateSceneContext)
		, ProjectedShadowInfo(InProjectedShadowInfo)
		, SetShadowRenderTargets(InSetShadowRenderTargets)
		, RenderMode(RenderModeIn)
	{
		SetStateOnCommandList(ParentCmdList);
	}

	virtual ~FShadowParallelCommandListSet()
	{
		Dispatch();
	}

	virtual void SetStateOnCommandList(FRHICommandList& CmdList) override
	{
		FParallelCommandListSet::SetStateOnCommandList(CmdList);
		SetShadowRenderTargets(CmdList, false);
		ProjectedShadowInfo.SetStateForDepth(CmdList,RenderMode, DrawRenderState);
	}
};

class FCopyShadowMapsCubeGS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyShadowMapsCubeGS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return RHISupportsGeometryShaders(Platform) && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	FCopyShadowMapsCubeGS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}
	FCopyShadowMapsCubeGS() {}
};

IMPLEMENT_SHADER_TYPE(,FCopyShadowMapsCubeGS,TEXT("/Engine/Private/CopyShadowMaps.usf"),TEXT("CopyCubeDepthGS"),SF_Geometry);

class FCopyShadowMapsCubePS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyShadowMapsCubePS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FCopyShadowMapsCubePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ShadowDepthTexture.Bind(Initializer.ParameterMap,TEXT("ShadowDepthCubeTexture"));
		ShadowDepthSampler.Bind(Initializer.ParameterMap,TEXT("ShadowDepthSampler"));
	}
	FCopyShadowMapsCubePS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, IPooledRenderTarget* SourceShadowMap)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

		SetTextureParameter(RHICmdList, GetPixelShader(), ShadowDepthTexture, ShadowDepthSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), SourceShadowMap->GetRenderTargetItem().ShaderResourceTexture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowDepthTexture;
		Ar << ShadowDepthSampler;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter ShadowDepthTexture;
	FShaderResourceParameter ShadowDepthSampler;
};

IMPLEMENT_SHADER_TYPE(,FCopyShadowMapsCubePS,TEXT("/Engine/Private/CopyShadowMaps.usf"),TEXT("CopyCubeDepthPS"),SF_Pixel);

/** */
class FCopyShadowMaps2DPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FCopyShadowMaps2DPS,Global);
public:

	static bool ShouldCache(EShaderPlatform Platform)
	{ 
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Platform,OutEnvironment);
	}

	FCopyShadowMaps2DPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		ShadowDepthTexture.Bind(Initializer.ParameterMap,TEXT("ShadowDepthTexture"));
		ShadowDepthSampler.Bind(Initializer.ParameterMap,TEXT("ShadowDepthSampler"));
	}
	FCopyShadowMaps2DPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FSceneView& View, IPooledRenderTarget* SourceShadowMap)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetPixelShader(), View.ViewUniformBuffer);

		SetTextureParameter(RHICmdList, GetPixelShader(), ShadowDepthTexture, ShadowDepthSampler, TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(), SourceShadowMap->GetRenderTargetItem().ShaderResourceTexture);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << ShadowDepthTexture;
		Ar << ShadowDepthSampler;
		return bShaderHasOutdatedParameters;
	}

	FShaderResourceParameter ShadowDepthTexture;
	FShaderResourceParameter ShadowDepthSampler;
};

IMPLEMENT_SHADER_TYPE(,FCopyShadowMaps2DPS,TEXT("/Engine/Private/CopyShadowMaps.usf"),TEXT("Copy2DDepthPS"),SF_Pixel);

void FProjectedShadowInfo::CopyCachedShadowMap(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, FSceneRenderer* SceneRenderer, const FViewInfo& View, FSetShadowRenderTargetFunction SetShadowRenderTargets)
{
	check(CacheMode == SDCM_MovablePrimitivesOnly);
	const FCachedShadowMapData& CachedShadowMapData = SceneRenderer->Scene->CachedShadowMaps.FindChecked(GetLightSceneInfo().Id);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	DrawRenderState.ApplyToPSO(GraphicsPSOInit);
	uint32 StencilRef = DrawRenderState.GetStencilRef();

	if (CachedShadowMapData.bCachedShadowMapHasPrimitives)
	{
		SCOPED_DRAW_EVENT(RHICmdList, CopyCachedShadowMap);

		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
		// No depth tests, so we can replace the clear
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();

		extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

		if (bOnePassPointLightShadow)
		{
			if (RHISupportsGeometryShaders(GShaderPlatformForFeatureLevel[SceneRenderer->FeatureLevel]))
			{
				// Set shaders and texture
				TShaderMapRef<TScreenVSForGS<false>> ScreenVertexShader(View.ShaderMap);
				TShaderMapRef<FCopyShadowMapsCubeGS> GeometryShader(View.ShaderMap);
				TShaderMapRef<FCopyShadowMapsCubePS> PixelShader(View.ShaderMap);

				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
				GraphicsPSOInit.BoundShaderState.GeometryShaderRHI = GETSAFERHISHADER_GEOMETRY(*GeometryShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				RHICmdList.SetStencilRef(StencilRef);

				PixelShader->SetParameters(RHICmdList, View, CachedShadowMapData.ShadowMap.DepthTarget.GetReference());

				DrawRectangle(
					RHICmdList,
					0, 0,
					ResolutionX, ResolutionY,
					BorderSize, BorderSize,
					ResolutionX, ResolutionY,
					FIntPoint(ResolutionX, ResolutionY),
					CachedShadowMapData.ShadowMap.GetSize(),
					*ScreenVertexShader,
					EDRF_Default);
			}
			else
			{
				check(RHISupportsVertexShaderLayer(GShaderPlatformForFeatureLevel[SceneRenderer->FeatureLevel]));
				
				// Set shaders and texture
				TShaderMapRef<TScreenVSForGS<true>> ScreenVertexShader(View.ShaderMap);
				TShaderMapRef<FCopyShadowMapsCubePS> PixelShader(View.ShaderMap);
				
				GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
				GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
				GraphicsPSOInit.PrimitiveType = PT_TriangleList;

				SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
				RHICmdList.SetStencilRef(StencilRef);

				PixelShader->SetParameters(RHICmdList, View, CachedShadowMapData.ShadowMap.DepthTarget.GetReference());
				
				DrawRectangle(
					  RHICmdList,
					  0, 0,
					  ResolutionX, ResolutionY,
					  BorderSize, BorderSize,
					  ResolutionX, ResolutionY,
					  FIntPoint(ResolutionX, ResolutionY),
					  CachedShadowMapData.ShadowMap.GetSize(),
					  *ScreenVertexShader,
					  EDRF_Default,
					  6);
			}
		}
		else
		{
			// Set shaders and texture
			TShaderMapRef<FScreenVS> ScreenVertexShader(View.ShaderMap);
			TShaderMapRef<FCopyShadowMaps2DPS> PixelShader(View.ShaderMap);

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*ScreenVertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);
			RHICmdList.SetStencilRef(StencilRef);

			PixelShader->SetParameters(RHICmdList, View, CachedShadowMapData.ShadowMap.DepthTarget.GetReference());

			DrawRectangle(
				RHICmdList,
				0, 0,
				ResolutionX, ResolutionY,
				BorderSize, BorderSize,
				ResolutionX, ResolutionY,
				FIntPoint(ResolutionX, ResolutionY),
				CachedShadowMapData.ShadowMap.GetSize(),
				*ScreenVertexShader,
				EDRF_Default);
		}
	}
}

void FProjectedShadowInfo::RenderDepthInner(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer, const FViewInfo* FoundView, FSetShadowRenderTargetFunction SetShadowRenderTargets, EShadowDepthRenderMode RenderMode)
{
	FDrawingPolicyRenderState DrawRenderState(*FoundView);
	SetStateForDepth(RHICmdList, RenderMode, DrawRenderState);
	
	if (CacheMode == SDCM_MovablePrimitivesOnly)
	{
		// Copy in depths of static primitives before we render movable primitives
		CopyCachedShadowMap(RHICmdList, DrawRenderState, SceneRenderer, *FoundView, SetShadowRenderTargets);
	}

	FShadowDepthDrawingPolicyContext StackPolicyContext(this);
	FShadowDepthDrawingPolicyContext* PolicyContext(&StackPolicyContext);

	bool bIsWholeSceneDirectionalShadow = IsWholeSceneDirectionalShadow();

	if (RHICmdList.IsImmediate() &&  // translucent shadows are draw on the render thread, using a recursive cmdlist (which is not immediate)
		GRHICommandList.UseParallelAlgorithms() && CVarParallelShadows.GetValueOnRenderThread() &&
		(bIsWholeSceneDirectionalShadow || CVarParallelShadowsNonWholeScene.GetValueOnRenderThread() > 0)
		)
	{
		check(IsInRenderingThread());

		// parallel version
		bool bFlush = CVarRHICmdFlushRenderThreadTasksShadowPass.GetValueOnRenderThread() > 0 || CVarRHICmdFlushRenderThreadTasks.GetValueOnRenderThread() > 0; 
		FScopedCommandListWaitForTasks Flusher(bFlush); 
		if (!bFlush)
		{
			/** CAUTION, this is assumed to be a POD type. We allocate the on the scene allocator and NEVER CALL A DESTRUCTOR.
				If you want to add non-pod data, not a huge problem, we just need to track and destruct them at the end of the scene.
			**/
			check(IsInRenderingThread() && FMemStack::Get().GetNumMarks() == 1); // we do not want this popped before the end of the scene and it better be the scene allocator
			PolicyContext = new (FMemStack::Get()) FShadowDepthDrawingPolicyContext(this);
		}
		{
			check(RHICmdList.IsImmediate());
			FRHICommandListImmediate& Immed = static_cast<FRHICommandListImmediate&>(RHICmdList);
			FShadowParallelCommandListSet ParallelCommandListSet(*FoundView, Immed, CVarRHICmdShadowDeferredContexts.GetValueOnRenderThread() > 0, !bFlush, *this, SetShadowRenderTargets, RenderMode);

			// Draw the subject's static elements using static draw lists
			if (bIsWholeSceneDirectionalShadow && RenderMode != ShadowDepthRenderMode_EmissiveOnly && RenderMode != ShadowDepthRenderMode_GIBlockingVolumes)
			{
				SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticDrawListShadowDepthsTime);

				if (bReflectiveShadowmap)
				{
					SceneRenderer->Scene->WholeSceneReflectiveShadowMapDrawList.DrawVisibleParallel(*PolicyContext, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility, ParallelCommandListSet);
				}
				else
				{
					// Use the scene's shadow depth draw list with this shadow's visibility map
					SceneRenderer->Scene->WholeSceneShadowDepthDrawList.DrawVisibleParallel(*PolicyContext, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility, ParallelCommandListSet);
				}
			}
			// Draw the subject's static elements using manual state filtering
			else if (StaticSubjectMeshElements.Num() > 0)
			{
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FDrawShadowMeshElementsThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, *FoundView, DrawRenderState, bReflectiveShadowmap && !bOnePassPointLightShadow);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent, StaticSubjectMeshElements.Num());
			}
			if (DynamicSubjectMeshElements.Num())
			{
				FRHICommandList* CmdList = ParallelCommandListSet.NewParallelCommandList();

				FGraphEventRef AnyThreadCompletionEvent = TGraphTask<FRenderDepthDynamicThreadTask>::CreateTask(ParallelCommandListSet.GetPrereqs(), ENamedThreads::RenderThread)
					.ConstructAndDispatchWhenReady(*this, *CmdList, *FoundView, DrawRenderState, SceneRenderer);

				ParallelCommandListSet.AddParallelCommandList(CmdList, AnyThreadCompletionEvent, DynamicSubjectMeshElements.Num());
			}
		}
	}
	else
	{
		// single threaded version
		SetStateForDepth(RHICmdList, RenderMode, DrawRenderState);

		// Draw the subject's static elements using static draw lists
		if (bIsWholeSceneDirectionalShadow && RenderMode != ShadowDepthRenderMode_EmissiveOnly && RenderMode != ShadowDepthRenderMode_GIBlockingVolumes)
		{
			SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticDrawListShadowDepthsTime);

			if (bReflectiveShadowmap)
			{
				SceneRenderer->Scene->WholeSceneReflectiveShadowMapDrawList.DrawVisible(RHICmdList, *FoundView, *PolicyContext, DrawRenderState, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility);
			}
			else
			{
				// Use the scene's shadow depth draw list with this shadow's visibility map
				SceneRenderer->Scene->WholeSceneShadowDepthDrawList.DrawVisible(RHICmdList, *FoundView, *PolicyContext, DrawRenderState, StaticMeshWholeSceneShadowDepthMap, StaticMeshWholeSceneShadowBatchVisibility);
			}
		}
		// Draw the subject's static elements using manual state filtering
		else if (StaticSubjectMeshElements.Num() > 0)
		{
			SCOPE_CYCLE_COUNTER(STAT_WholeSceneStaticShadowDepthsTime);

			if (bReflectiveShadowmap && !bOnePassPointLightShadow)
			{
				// reflective shadow map
				DrawShadowMeshElements<true>(RHICmdList, *FoundView, DrawRenderState, *this);
			}
			else
			{
				// normal shadow map
				DrawShadowMeshElements<false>(RHICmdList, *FoundView, DrawRenderState, *this);
			}
		}
		RenderDepthDynamic(RHICmdList, SceneRenderer, FoundView, DrawRenderState);
	}
}

void FProjectedShadowInfo::ModifyViewForShadow(FRHICommandList& RHICmdList, FViewInfo* FoundView) const
{
	FIntRect OriginalViewRect = FoundView->ViewRect;
	FoundView->ViewRect.Min.X = 0;
	FoundView->ViewRect.Min.Y = 0;
	FoundView->ViewRect.Max.X = ResolutionX;
	FoundView->ViewRect.Max.Y =  ResolutionY;

	FoundView->ViewMatrices.HackRemoveTemporalAAProjectionJitter();

	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	FoundView->CachedViewUniformShaderParameters = MakeUnique<FViewUniformShaderParameters>();

	// Override the view matrix so that billboarding primitives will be aligned to the light
	FoundView->ViewMatrices.HackOverrideViewMatrixForShadows(ShadowViewMatrix);
	FBox VolumeBounds[TVC_MAX];
	FoundView->SetupUniformBufferParameters(
		SceneContext,
		VolumeBounds,
		TVC_MAX,
		*FoundView->CachedViewUniformShaderParameters);

	FoundView->ViewUniformBuffer = TUniformBufferRef<FViewUniformShaderParameters>::CreateUniformBufferImmediate(*FoundView->CachedViewUniformShaderParameters, UniformBuffer_SingleFrame);

	// we are going to set this back now because we only want the correct view rect for the uniform buffer. For LOD calculations, we want the rendering viewrect and proj matrix.
	FoundView->ViewRect = OriginalViewRect;

	extern int32 GPreshadowsForceLowestLOD;

	if (bPreShadow && GPreshadowsForceLowestLOD)
	{
		FoundView->DrawDynamicFlags = EDrawDynamicFlags::ForceLowestLOD;
	}
}

FViewInfo* FProjectedShadowInfo::FindViewForShadow(FSceneRenderer* SceneRenderer) const
{
	// Choose an arbitrary view where this shadow's subject is relevant.
	FViewInfo* FoundView = NULL;
	for(int32 ViewIndex = 0;ViewIndex < SceneRenderer->Views.Num();ViewIndex++)
	{
		FViewInfo* CheckView = &SceneRenderer->Views[ViewIndex];
		const FVisibleLightViewInfo& VisibleLightViewInfo = CheckView->VisibleLightInfos[LightSceneInfo->Id];
		FPrimitiveViewRelevance ViewRel = VisibleLightViewInfo.ProjectedShadowViewRelevanceMap[ShadowId];
		if (ViewRel.bShadowRelevance)
		{
			FoundView = CheckView;
			break;
		}
	}
	check(FoundView);
	return FoundView;
}

void FProjectedShadowInfo::RenderDepth(FRHICommandList& RHICmdList, FSceneRenderer* SceneRenderer, FSetShadowRenderTargetFunction SetShadowRenderTargets, EShadowDepthRenderMode RenderMode)
{
	// Select the correct set of arrays for the current render mode
	TArray<FShadowStaticMeshElement,SceneRenderingAllocator>* PtrCurrentMeshElements = nullptr;
	PrimitiveArrayType* PtrCurrentPrimitives = nullptr;

	switch(RenderMode)
	{
	case ShadowDepthRenderMode_Normal:
		PtrCurrentMeshElements = &StaticSubjectMeshElements;
		PtrCurrentPrimitives = &DynamicSubjectPrimitives;
		break;
	case ShadowDepthRenderMode_EmissiveOnly:
		PtrCurrentMeshElements = &EmissiveOnlyMeshElements;
		PtrCurrentPrimitives = &EmissiveOnlyPrimitives;
		break;
	case ShadowDepthRenderMode_GIBlockingVolumes:
		PtrCurrentMeshElements = &GIBlockingMeshElements;
		PtrCurrentPrimitives = &GIBlockingPrimitives;
		break;
	default:
		check(0);
	}
	TArray<FShadowStaticMeshElement,SceneRenderingAllocator>& CurrentMeshElements = *PtrCurrentMeshElements;
	PrimitiveArrayType& CurrentPrimitives = *PtrCurrentPrimitives;

#if WANTS_DRAW_MESH_EVENTS
	FString EventName;
	GetShadowTypeNameForDrawEvent(EventName);

	if (GEmitDrawEvents)
	{
		EventName += FString(TEXT(" ")) + FString::FromInt(ResolutionX) + TEXT("x") + FString::FromInt(ResolutionY);
	}

	SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepthActor, *EventName);
#endif

	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_RenderWholeSceneShadowDepthsTime, bWholeSceneShadow);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_RenderPerObjectShadowDepthsTime, !bWholeSceneShadow);

	// Exit early if there are no meshes or primitives to render in the emissive only render mode.
	if (RenderMode != ShadowDepthRenderMode_Normal && CurrentMeshElements.Num() == 0 && CurrentPrimitives.Num() == 0)
	{
		return;
	}

	RenderDepthInner(RHICmdList, SceneRenderer, ShadowDepthView, SetShadowRenderTargets, RenderMode);
}

void FProjectedShadowInfo::SetupShadowDepthView(FRHICommandListImmediate& RHICmdList, FSceneRenderer* SceneRenderer)
{
	FViewInfo* FoundView = FindViewForShadow(SceneRenderer);
	check(FoundView && IsInRenderingThread());
	FViewInfo* DepthPassView = FoundView->CreateSnapshot();
	ModifyViewForShadow(RHICmdList, DepthPassView);
	ShadowDepthView = DepthPassView;
}

void FProjectedShadowInfo::SortSubjectMeshElements()
{
	// Note: this should match the criteria in FProjectedShadowInfo::RenderDepth for deciding when to call SetSharedState on a static mesh element for best performance
	struct FCompareFShadowStaticMeshElement
	{
		FORCEINLINE bool operator()( const FShadowStaticMeshElement& A, const FShadowStaticMeshElement& B ) const
		{
			if( A.Mesh->VertexFactory != B.Mesh->VertexFactory ) return A.Mesh->VertexFactory < B.Mesh->VertexFactory;
			if( A.RenderProxy != B.RenderProxy ) return A.RenderProxy < B.RenderProxy;
			if( A.bIsTwoSided != B.bIsTwoSided ) return A.bIsTwoSided < B.bIsTwoSided;
			if( A.Mesh->ReverseCulling != B.Mesh->ReverseCulling ) return A.Mesh->ReverseCulling < B.Mesh->ReverseCulling;

			return false;
		}
	};

	StaticSubjectMeshElements.Sort( FCompareFShadowStaticMeshElement() );
}

void FProjectedShadowInfo::GetShadowTypeNameForDrawEvent(FString& TypeName) const
{
	if (GEmitDrawEvents)
	{
		const FName ParentName = ParentSceneInfo ? ParentSceneInfo->Proxy->GetOwnerName() : NAME_None;

		if (bWholeSceneShadow)
		{
			if (CascadeSettings.ShadowSplitIndex >= 0)
			{
				TypeName = FString(TEXT("WholeScene split")) + FString::FromInt(CascadeSettings.ShadowSplitIndex);
			}
			else
			{
				if (CacheMode == SDCM_MovablePrimitivesOnly)
				{
					TypeName = FString(TEXT("WholeScene MovablePrimitives"));
				}
				else if (CacheMode == SDCM_StaticPrimitivesOnly)
				{
					TypeName = FString(TEXT("WholeScene StaticPrimitives"));
				}
				else
				{
					TypeName = FString(TEXT("WholeScene"));
				}
			}
		}
		else if (bPreShadow)
		{
			TypeName = FString(TEXT("PreShadow ")) + ParentName.ToString();
		}
		else
		{
			TypeName = FString(TEXT("PerObject ")) + ParentName.ToString();
		}
	}
}

void FSceneRenderer::RenderShadowDepthMapAtlases(FRHICommandListImmediate& RHICmdList)
{
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	for (int32 AtlasIndex = 0; AtlasIndex < SortedShadowsForShadowDepthPass.ShadowMapAtlases.Num(); AtlasIndex++)
	{
		const FSortedShadowMapAtlas& ShadowMapAtlas = SortedShadowsForShadowDepthPass.ShadowMapAtlases[AtlasIndex];
		FSceneRenderTargetItem& RenderTarget = ShadowMapAtlas.RenderTargets.DepthTarget->GetRenderTargetItem();
		FIntPoint AtlasSize = ShadowMapAtlas.RenderTargets.DepthTarget->GetDesc().Extent;

		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, ShadowMapAtlas.RenderTargets.DepthTarget.GetReference());

		SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepths, TEXT("Atlas%u %ux%u"), AtlasIndex, AtlasSize.X, AtlasSize.Y);

		auto SetShadowRenderTargets = [this, &RenderTarget, &SceneContext](FRHICommandList& InRHICmdList, bool bPerformClear)
		{
			FRHISetRenderTargetsInfo Info(0, nullptr, FRHIDepthRenderTargetView(RenderTarget.TargetableTexture, 
				bPerformClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 
				ERenderTargetStoreAction::EStore, 
				ERenderTargetLoadAction::ELoad, 
				ERenderTargetStoreAction::EStore));

			check(Info.DepthStencilRenderTarget.Texture->GetDepthClearValue() == 1.0f);	
			Info.ColorRenderTarget[0].StoreAction = ERenderTargetStoreAction::ENoAction;

			if (!GSupportsDepthRenderTargetWithoutColorRenderTarget)
			{
				Info.NumColorRenderTargets = 1;
				Info.ColorRenderTarget[0].Texture = SceneContext.GetOptionalShadowDepthColorSurface(InRHICmdList, Info.DepthStencilRenderTarget.Texture->GetTexture2D()->GetSizeX(), Info.DepthStencilRenderTarget.Texture->GetTexture2D()->GetSizeY());
				InRHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Info.ColorRenderTarget[0].Texture);
			}
			InRHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Info.DepthStencilRenderTarget.Texture);
			InRHICmdList.SetRenderTargetsAndClear(Info);

			if (!bPerformClear)
			{
				InRHICmdList.BindClearMRTValues(false, true, false);
			}
		};

		{
			SCOPED_DRAW_EVENT(RHICmdList, Clear);
			SetShadowRenderTargets(RHICmdList, true);	
		}

		FLightSceneProxy* CurrentLightForDrawEvent = NULL;

#if WANTS_DRAW_MESH_EVENTS
		TDrawEvent<FRHICommandList> LightEvent;
#endif

		for (int32 ShadowIndex = 0; ShadowIndex < ShadowMapAtlas.Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ShadowMapAtlas.Shadows[ShadowIndex];

			if (!CurrentLightForDrawEvent || ProjectedShadowInfo->GetLightSceneInfo().Proxy != CurrentLightForDrawEvent)
			{
				if (CurrentLightForDrawEvent)
				{
					STOP_DRAW_EVENT(LightEvent);
				}

				CurrentLightForDrawEvent = ProjectedShadowInfo->GetLightSceneInfo().Proxy;
				FString LightNameWithLevel;
				GetLightNameForDrawEvent(CurrentLightForDrawEvent, LightNameWithLevel);
				
				BEGIN_DRAW_EVENTF(
					RHICmdList, 
					LightNameEvent, 
					LightEvent, 
					*LightNameWithLevel);
			}

			ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets, ShadowDepthRenderMode_Normal);
		}

		if (CurrentLightForDrawEvent)
		{
			STOP_DRAW_EVENT(LightEvent);
			CurrentLightForDrawEvent = NULL;
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTarget.TargetableTexture);
	}
}

void FSceneRenderer::RenderShadowDepthMaps(FRHICommandListImmediate& RHICmdList)
{
	SCOPED_NAMED_EVENT(FSceneRenderer_RenderShadowDepthMaps, FColor::Emerald);
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

	SCOPED_DRAW_EVENT(RHICmdList, ShadowDepths);
	SCOPED_GPU_STAT(RHICmdList, Stat_GPU_ShadowDepths);

	FSceneRenderer::RenderShadowDepthMapAtlases(RHICmdList);

	for (int32 CubemapIndex = 0; CubemapIndex < SortedShadowsForShadowDepthPass.ShadowMapCubemaps.Num(); CubemapIndex++)
	{
		const FSortedShadowMapAtlas& ShadowMap = SortedShadowsForShadowDepthPass.ShadowMapCubemaps[CubemapIndex];
		FSceneRenderTargetItem& RenderTarget = ShadowMap.RenderTargets.DepthTarget->GetRenderTargetItem();
		FIntPoint TargetSize = ShadowMap.RenderTargets.DepthTarget->GetDesc().Extent;

		check(ShadowMap.Shadows.Num() == 1);
		FProjectedShadowInfo* ProjectedShadowInfo = ShadowMap.Shadows[0];

		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, ShadowMap.RenderTargets.DepthTarget.GetReference());

		FString LightNameWithLevel;
		GetLightNameForDrawEvent(ProjectedShadowInfo->GetLightSceneInfo().Proxy, LightNameWithLevel);
		SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepths, TEXT("Cubemap %s %u^2"), *LightNameWithLevel, TargetSize.X, TargetSize.Y);

		auto SetShadowRenderTargets = [this, &RenderTarget, &SceneContext](FRHICommandList& InRHICmdList, bool bPerformClear)
		{
			FRHISetRenderTargetsInfo Info(0, nullptr, FRHIDepthRenderTargetView(RenderTarget.TargetableTexture, 
				bPerformClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 
				ERenderTargetStoreAction::EStore, 
				ERenderTargetLoadAction::ELoad, 
				ERenderTargetStoreAction::EStore));

			check(Info.DepthStencilRenderTarget.Texture->GetDepthClearValue() == 1.0f);	
			Info.ColorRenderTarget[0].StoreAction = ERenderTargetStoreAction::ENoAction;

			if (!GSupportsDepthRenderTargetWithoutColorRenderTarget)
			{
				Info.NumColorRenderTargets = 1;
				Info.ColorRenderTarget[0].Texture = SceneContext.GetOptionalShadowDepthColorSurface(InRHICmdList, Info.DepthStencilRenderTarget.Texture->GetTexture2D()->GetSizeX(), Info.DepthStencilRenderTarget.Texture->GetTexture2D()->GetSizeY());
				InRHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Info.ColorRenderTarget[0].Texture);
			}
			InRHICmdList.TransitionResource(EResourceTransitionAccess::EWritable, Info.DepthStencilRenderTarget.Texture);
			InRHICmdList.SetRenderTargetsAndClear(Info);
		};
			
		{
			bool bDoClear = true;

			if (ProjectedShadowInfo->CacheMode == SDCM_MovablePrimitivesOnly 
				&& Scene->CachedShadowMaps.FindChecked(ProjectedShadowInfo->GetLightSceneInfo().Id).bCachedShadowMapHasPrimitives)
			{
				// Skip the clear when we'll copy from a cached shadowmap
				bDoClear = false;
			}

			SCOPED_CONDITIONAL_DRAW_EVENT(RHICmdList, Clear, bDoClear);
			SetShadowRenderTargets(RHICmdList, bDoClear);	
		}

		ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets, ShadowDepthRenderMode_Normal);

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTarget.TargetableTexture);
	}
			
	if (SortedShadowsForShadowDepthPass.PreshadowCache.Shadows.Num() > 0)
	{
		FSceneRenderTargetItem& RenderTarget = SortedShadowsForShadowDepthPass.PreshadowCache.RenderTargets.DepthTarget->GetRenderTargetItem();

		GRenderTargetPool.VisualizeTexture.SetCheckPoint(RHICmdList, SortedShadowsForShadowDepthPass.PreshadowCache.RenderTargets.DepthTarget.GetReference());

		SCOPED_DRAW_EVENT(RHICmdList, PreshadowCache);

		for (int32 ShadowIndex = 0; ShadowIndex < SortedShadowsForShadowDepthPass.PreshadowCache.Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = SortedShadowsForShadowDepthPass.PreshadowCache.Shadows[ShadowIndex];

			if (!ProjectedShadowInfo->bDepthsCached)
			{
				auto SetShadowRenderTargets = [this, ProjectedShadowInfo](FRHICommandList& InRHICmdList, bool bPerformClear)
				{
					FTextureRHIParamRef PreShadowCacheDepthZ = Scene->PreShadowCacheDepthZ->GetRenderTargetItem().TargetableTexture.GetReference();
					InRHICmdList.TransitionResources(EResourceTransitionAccess::EWritable, &PreShadowCacheDepthZ, 1);

					// Must preserve existing contents as the clear will be scissored
					SetRenderTarget(InRHICmdList, FTextureRHIRef(), PreShadowCacheDepthZ, ESimpleRenderTargetMode::EExistingColorAndDepth);
					ProjectedShadowInfo->ClearDepth(InRHICmdList, this, 0, nullptr, PreShadowCacheDepthZ, bPerformClear);
				};

				SetShadowRenderTargets(RHICmdList, true);

				ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets, ShadowDepthRenderMode_Normal);
				ProjectedShadowInfo->bDepthsCached = true;
			}
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, RenderTarget.TargetableTexture);
	}

	for (int32 AtlasIndex = 0; AtlasIndex < SortedShadowsForShadowDepthPass.TranslucencyShadowMapAtlases.Num(); AtlasIndex++)
	{
		const FSortedShadowMapAtlas& ShadowMapAtlas = SortedShadowsForShadowDepthPass.TranslucencyShadowMapAtlases[AtlasIndex];
		FIntPoint TargetSize = ShadowMapAtlas.RenderTargets.ColorTargets[0]->GetDesc().Extent;

		SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepths, TEXT("TranslucencyAtlas%u %u^2"), AtlasIndex, TargetSize.X, TargetSize.Y);

		FSceneRenderTargetItem ColorTarget0 = ShadowMapAtlas.RenderTargets.ColorTargets[0]->GetRenderTargetItem();
		FSceneRenderTargetItem ColorTarget1 = ShadowMapAtlas.RenderTargets.ColorTargets[1]->GetRenderTargetItem();

		FTextureRHIParamRef RenderTargetArray[2] =
		{
			ColorTarget0.TargetableTexture,
			ColorTarget1.TargetableTexture
		};
		SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargetArray), RenderTargetArray, FTextureRHIParamRef(), 0, NULL, true);

		for (int32 ShadowIndex = 0; ShadowIndex < ShadowMapAtlas.Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ShadowMapAtlas.Shadows[ShadowIndex];
			ProjectedShadowInfo->RenderTranslucencyDepths(RHICmdList, this);
		}

		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, ColorTarget0.TargetableTexture);
		RHICmdList.TransitionResource(EResourceTransitionAccess::EReadable, ColorTarget1.TargetableTexture);
	}

	// Get a copy of LpvWriteUniformBufferParams for parallel RSM draw-call submission
	{
		for (int32 ViewIdx = 0; ViewIdx < Views.Num(); ++ViewIdx)
		{
			FViewInfo& View = Views[ViewIdx];
			FSceneViewState* ViewState = View.ViewState;

			if (ViewState)
			{
				FLightPropagationVolume* Lpv = ViewState->GetLightPropagationVolume(FeatureLevel);

				if (Lpv)
				{
					Lpv->SetRsmUniformBuffer();
				}
			}
		}
	}

	for (int32 AtlasIndex = 0; AtlasIndex < SortedShadowsForShadowDepthPass.RSMAtlases.Num(); AtlasIndex++)
	{
		const FSortedShadowMapAtlas& ShadowMapAtlas = SortedShadowsForShadowDepthPass.RSMAtlases[AtlasIndex];
		FSceneRenderTargetItem ColorTarget0 = ShadowMapAtlas.RenderTargets.ColorTargets[0]->GetRenderTargetItem();
		FSceneRenderTargetItem ColorTarget1 = ShadowMapAtlas.RenderTargets.ColorTargets[1]->GetRenderTargetItem();
		FSceneRenderTargetItem DepthTarget = ShadowMapAtlas.RenderTargets.DepthTarget->GetRenderTargetItem();
		FIntPoint TargetSize = ShadowMapAtlas.RenderTargets.DepthTarget->GetDesc().Extent;

		SCOPED_DRAW_EVENTF(RHICmdList, EventShadowDepths, TEXT("RSM%u %ux%u"), AtlasIndex, TargetSize.X, TargetSize.Y);

		for (int32 ShadowIndex = 0; ShadowIndex < ShadowMapAtlas.Shadows.Num(); ShadowIndex++)
		{
			FProjectedShadowInfo* ProjectedShadowInfo = ShadowMapAtlas.Shadows[ShadowIndex];

			FSceneViewState* ViewState = (FSceneViewState*)ProjectedShadowInfo->DependentView->State;
			FLightPropagationVolume* LightPropagationVolume = ViewState->GetLightPropagationVolume(FeatureLevel);

			auto SetShadowRenderTargets = [this, LightPropagationVolume, ProjectedShadowInfo, &ColorTarget0, &ColorTarget1, &DepthTarget](FRHICommandList& InRHICmdList, bool bPerformClear)
			{
				FTextureRHIParamRef RenderTargets[2];
				RenderTargets[0] = ColorTarget0.TargetableTexture;
				RenderTargets[1] = ColorTarget1.TargetableTexture;

				// Hook up the geometry volume UAVs
				FUnorderedAccessViewRHIParamRef Uavs[4];
				Uavs[0] = LightPropagationVolume->GetGvListBufferUav();
				Uavs[1] = LightPropagationVolume->GetGvListHeadBufferUav();
				Uavs[2] = LightPropagationVolume->GetVplListBufferUav();
				Uavs[3] = LightPropagationVolume->GetVplListHeadBufferUav();
					
				InRHICmdList.TransitionResources(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToGfx, Uavs, ARRAY_COUNT(Uavs));
				SetRenderTargets(InRHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, DepthTarget.TargetableTexture, ARRAY_COUNT(Uavs), Uavs);

				ProjectedShadowInfo->ClearDepth(InRHICmdList, this, ARRAY_COUNT(RenderTargets), RenderTargets, DepthTarget.TargetableTexture, bPerformClear);
			};					

			{
				SCOPED_DRAW_EVENT(RHICmdList, Clear);
				SetShadowRenderTargets(RHICmdList, true);	
			}				

			ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets, ShadowDepthRenderMode_Normal);

			// Render emissive only meshes as they are held in a separate list.
			ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets, ShadowDepthRenderMode_EmissiveOnly);
			// Render gi blocking volume meshes.
			ProjectedShadowInfo->RenderDepth(RHICmdList, this, SetShadowRenderTargets, ShadowDepthRenderMode_GIBlockingVolumes);

			{
				// Resolve the shadow depth z surface.
				RHICmdList.CopyToResolveTarget(DepthTarget.TargetableTexture, DepthTarget.ShaderResourceTexture, false, FResolveParams());
				RHICmdList.CopyToResolveTarget(ColorTarget0.TargetableTexture, ColorTarget0.ShaderResourceTexture, false, FResolveParams());
				RHICmdList.CopyToResolveTarget(ColorTarget1.TargetableTexture, ColorTarget1.ShaderResourceTexture, false, FResolveParams());

				FUnorderedAccessViewRHIParamRef UavsToReadable[2];
				UavsToReadable[0] = LightPropagationVolume->GetGvListBufferUav();
				UavsToReadable[1] = LightPropagationVolume->GetGvListHeadBufferUav();	
				RHICmdList.TransitionResources(EResourceTransitionAccess::EReadable, EResourceTransitionPipeline::EGfxToGfx, UavsToReadable, ARRAY_COUNT(UavsToReadable));

				// Unset render targets
				FTextureRHIParamRef RenderTargets[2] = {NULL};
				FUnorderedAccessViewRHIParamRef Uavs[2] = {NULL};
				SetRenderTargets(RHICmdList, ARRAY_COUNT(RenderTargets), RenderTargets, FTextureRHIParamRef(), ARRAY_COUNT(Uavs), Uavs);
			}
		}
	}
}
