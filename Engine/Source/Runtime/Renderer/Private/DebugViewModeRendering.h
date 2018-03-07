// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeRendering.h: Contains definitions for rendering debug viewmodes.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameterUtils.h"
#include "MeshMaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"

class FPrimitiveSceneProxy;
struct FMeshBatchElement;
struct FMeshDrawingRenderState;

static const int32 NumStreamingAccuracyColors = 5;
static const float UndefinedStreamingAccuracyIntensity = .015f;

/**
 * Vertex shader for quad overdraw. Required because overdraw shaders need to have SV_Position as first PS interpolant.
 */
class FDebugViewModeVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FDebugViewModeVS,MeshMaterial);
protected:

	FDebugViewModeVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer) : FMeshMaterialShader(Initializer)
	{
		IsInstancedStereoParameter.Bind(Initializer.ParameterMap, TEXT("bIsInstancedStereo"));
		InstancedEyeIndexParameter.Bind(Initializer.ParameterMap, TEXT("InstancedEyeIndex"));
	}

	FDebugViewModeVS() {}

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return AllowDebugViewVSDSHS(Platform) && (Material->IsDefaultMaterial() || Material->HasVertexPositionOffsetConnected() || Material->GetTessellationMode() != MTM_NoTessellation);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FMaterial& Material,const FSceneView& View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(),MaterialRenderProxy,Material,View,View.ViewUniformBuffer,ESceneRenderTargetsMode::DontSet);

		if (IsInstancedStereoParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), IsInstancedStereoParameter, false);
		}

		if (InstancedEyeIndexParameter.IsBound())
		{
			SetShaderValue(RHICmdList, GetVertexShader(), InstancedEyeIndexParameter, 0);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	static void SetCommonDefinitions(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		// SM4 has less input interpolants. Also instanced meshes use more interpolants.
		if (Material->IsDefaultMaterial() || (IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5) && !Material->IsUsedWithInstancedStaticMeshes()))
		{	// Force the default material to pass enough texcoords to the pixel shaders (even though not using them).
			// This is required to allow material shaders to have access to the sampled coords.
			OutEnvironment.SetDefine(TEXT("MIN_MATERIAL_TEXCOORDS"), (uint32)4);
		}
		else // Otherwise still pass at minimum amount to have debug shader using a texcoord to work (material might not use any).
		{
			OutEnvironment.SetDefine(TEXT("MIN_MATERIAL_TEXCOORDS"), (uint32)2);
		}

	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		SetCommonDefinitions(Platform, Material, OutEnvironment);
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		const bool Result = FMeshMaterialShader::Serialize(Ar);
		Ar << IsInstancedStereoParameter;
		Ar << InstancedEyeIndexParameter;
		return Result;
	}

private:

	FShaderParameter IsInstancedStereoParameter;
	FShaderParameter InstancedEyeIndexParameter;
};

/**
 * Hull shader for quad overdraw. Required because overdraw shaders need to have SV_Position as first PS interpolant.
 */
class FDebugViewModeHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FDebugViewModeHS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType) && FDebugViewModeVS::ShouldCache(Platform,Material,VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FDebugViewModeVS::SetCommonDefinitions(Platform, Material, OutEnvironment);
		FBaseHS::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}


	FDebugViewModeHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FBaseHS(Initializer) {}
	FDebugViewModeHS() {}
};

/**
 * Domain shader for quad overdraw. Required because overdraw shaders need to have SV_Position as first PS interpolant.
 */
class FDebugViewModeDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FDebugViewModeDS,MeshMaterial);
public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType) && FDebugViewModeVS::ShouldCache(Platform,Material,VertexFactoryType);		
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FDebugViewModeVS::SetCommonDefinitions(Platform, Material, OutEnvironment);
		FBaseDS::ModifyCompilationEnvironment(Platform, OutEnvironment);
	}

	FDebugViewModeDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer): FBaseDS(Initializer) {}
	FDebugViewModeDS() {}
};

// Interface for debug viewmode pixel shaders. Devired classes can be of global shader or material shaders.
class IDebugViewModePSInterface
{
public:

	virtual ~IDebugViewModePSInterface() {}

	virtual void SetParameters(
		FRHICommandList& RHICmdList, 
		const FShader* OriginalVS, 
		const FShader* OriginalPS, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FMaterial& Material,
		const FSceneView& View
		) = 0;

	virtual void SetMesh(
		FRHICommandList& RHICmdList, 
		const FVertexFactory* VertexFactory,
		const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy,
		int32 VisualizeLODIndex,
		const FMeshBatchElement& BatchElement, 
		const FDrawingPolicyRenderState& DrawRenderState
		) = 0;

	// Used for custom rendering like decals.
	virtual void SetMesh(FRHICommandList& RHICmdList, const FSceneView& View) = 0;

	virtual FShader* GetShader() = 0;
};

/**
 * Namespace holding the interface for debugview modes.
 */
struct FDebugViewMode
{
	static IDebugViewModePSInterface* GetPSInterface(TShaderMap<FGlobalShaderType>* ShaderMap, const FMaterial* Material, EDebugViewShaderMode DebugViewShaderMode);

	static void GetMaterialForVSHSDS(const FMaterialRenderProxy** MaterialRenderProxy, const FMaterial** Material, ERHIFeatureLevel::Type FeatureLevel);

	static void SetParametersVSHSDS(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy, 
		const FMaterial* Material, 
		const FSceneView& View,
		const FVertexFactory* VertexFactory,
		bool bHasHullAndDomainShader
		);

	static void SetMeshVSHSDS(
		FRHICommandList& RHICmdList, 
		const FVertexFactory* VertexFactory,
		const FSceneView& View,
		const FPrimitiveSceneProxy* Proxy,
		const FMeshBatchElement& BatchElement, 
		const FDrawingPolicyRenderState& DrawRenderState,
		const FMaterial* Material, 
		bool bHasHullAndDomainShader
		);

	static void PatchBoundShaderState(
			FBoundShaderStateInput& BoundShaderStateInput,
			const FMaterial* Material,
			const FVertexFactory* VertexFactory,
			ERHIFeatureLevel::Type FeatureLevel, 
			EDebugViewShaderMode DebugViewShaderMode
			);
};
