// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapDensityRendering.h: Definitions for rendering lightmap density.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "EngineGlobals.h"
#include "RHI.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "UnrealEngine.h"
#include "MeshMaterialShaderType.h"
#include "DrawingPolicy.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "SceneRendering.h"
#include "Engine/LightMapTexture2D.h"

/**
 * The base shader type for vertex shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityVS : public FMeshMaterialShader, public LightMapPolicyType::VertexParametersType
{
	DECLARE_SHADER_TYPE(TLightMapDensityVS,MeshMaterial);

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
				&& LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType)
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityVS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::VertexParametersType::Bind(Initializer.ParameterMap);
	}
	TLightMapDensityVS() {}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::VertexParametersType::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(		
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}
};

/**
 * The base shader type for hull shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(TLightMapDensityHS,MeshMaterial);

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TLightMapDensityVS<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityHS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseHS(Initializer)
	{}

	TLightMapDensityHS() {}
};

/**
 * The base shader type for domain shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 */
template<typename LightMapPolicyType>
class TLightMapDensityDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(TLightMapDensityDS,MeshMaterial);

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& TLightMapDensityVS<LightMapPolicyType>::ShouldCache(Platform, Material, VertexFactoryType);		
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityDS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FBaseDS(Initializer)
	{}

	TLightMapDensityDS() {}
};

/**
 * The base type for pixel shaders that render the emissive color, and light-mapped/ambient lighting of a mesh.
 * The base type is shared between the versions with and without sky light.
 */
template<typename LightMapPolicyType>
class TLightMapDensityPS : public FMeshMaterialShader, public LightMapPolicyType::PixelParametersType
{
	DECLARE_SHADER_TYPE(TLightMapDensityPS,MeshMaterial);

public:

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() || Material->MaterialMayModifyMeshPosition())
				&& LightMapPolicyType::ShouldCache(Platform,Material,VertexFactoryType)
				&& IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, const FMaterial* Material, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMeshMaterialShader::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
		LightMapPolicyType::ModifyCompilationEnvironment(Platform, Material, OutEnvironment);
	}

	/** Initialization constructor. */
	TLightMapDensityPS(const FMeshMaterialShaderType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialShader(Initializer)
	{
		LightMapPolicyType::PixelParametersType::Bind(Initializer.ParameterMap);
		LightMapDensity.Bind(Initializer.ParameterMap,TEXT("LightMapDensityParameters"));
		BuiltLightingAndSelectedFlags.Bind(Initializer.ParameterMap,TEXT("BuiltLightingAndSelectedFlags"));
		DensitySelectedColor.Bind(Initializer.ParameterMap,TEXT("DensitySelectedColor"));
		LightMapResolutionScale.Bind(Initializer.ParameterMap,TEXT("LightMapResolutionScale"));
		LightMapDensityDisplayOptions.Bind(Initializer.ParameterMap,TEXT("LightMapDensityDisplayOptions"));
		VertexMappedColor.Bind(Initializer.ParameterMap,TEXT("VertexMappedColor"));
		GridTexture.Bind(Initializer.ParameterMap,TEXT("GridTexture"));
		GridTextureSampler.Bind(Initializer.ParameterMap,TEXT("GridTextureSampler"));
	}
	TLightMapDensityPS() {}

	void SetParameters(FRHICommandList& RHICmdList, const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View->GetFeatureLevel()), *View, View->ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);

		if (GridTexture.IsBound())
		{
			SetTextureParameter(
				RHICmdList, 
				GetPixelShader(),
				GridTexture,
				GridTextureSampler,
				TStaticSamplerState<SF_Bilinear,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI(),
				GEngine->LightMapDensityTexture->Resource->TextureRHI);
		}
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FPrimitiveSceneProxy* PrimitiveSceneProxy,const FMeshBatchElement& BatchElement,const FSceneView& View,const FDrawingPolicyRenderState& DrawRenderState,FVector& InBuiltLightingAndSelectedFlags,FVector2D& InLightMapResolutionScale, bool bTextureMapped)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

		if (LightMapDensity.IsBound())
		{
			FVector4 DensityParameters(
				1,
				GEngine->MinLightMapDensity * GEngine->MinLightMapDensity,
				GEngine->IdealLightMapDensity * GEngine->IdealLightMapDensity,
				GEngine->MaxLightMapDensity * GEngine->MaxLightMapDensity );
			SetShaderValue(RHICmdList, GetPixelShader(),LightMapDensity,DensityParameters,0);
		}
		SetShaderValue(RHICmdList, GetPixelShader(),BuiltLightingAndSelectedFlags,InBuiltLightingAndSelectedFlags,0);
		SetShaderValue(RHICmdList, GetPixelShader(),DensitySelectedColor,GEngine->LightMapDensitySelectedColor,0);
		SetShaderValue(RHICmdList, GetPixelShader(),LightMapResolutionScale,InLightMapResolutionScale,0);
		if (LightMapDensityDisplayOptions.IsBound())
		{
			FVector4 OptionsParameter(
				GEngine->bRenderLightMapDensityGrayscale ? GEngine->RenderLightMapDensityGrayscaleScale : 0.0f,
				GEngine->bRenderLightMapDensityGrayscale ? 0.0f : GEngine->RenderLightMapDensityColorScale,
				(bTextureMapped == true) ? 1.0f : 0.0f,
				(bTextureMapped == false) ? 1.0f : 0.0f
				);
			SetShaderValue(RHICmdList, GetPixelShader(),LightMapDensityDisplayOptions,OptionsParameter,0);
		}
		SetShaderValue(RHICmdList, GetPixelShader(),VertexMappedColor,GEngine->LightMapDensityVertexMappedColor,0);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		LightMapPolicyType::PixelParametersType::Serialize(Ar);
		Ar << LightMapDensity;
		Ar << BuiltLightingAndSelectedFlags;
		Ar << DensitySelectedColor;
		Ar << LightMapResolutionScale;
		Ar << LightMapDensityDisplayOptions;
		Ar << VertexMappedColor;
		Ar << GridTexture;
		Ar << GridTextureSampler;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter LightMapDensity;
	FShaderParameter BuiltLightingAndSelectedFlags;
	FShaderParameter DensitySelectedColor;
	FShaderParameter LightMapResolutionScale;
	FShaderParameter LightMapDensityDisplayOptions;
	FShaderParameter VertexMappedColor;
	FShaderResourceParameter GridTexture;
	FShaderResourceParameter GridTextureSampler;
};

/**
 * 
 */
template<typename LightMapPolicyType>
class TLightMapDensityDrawingPolicy : public FMeshDrawingPolicy
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
		ElementDataType(
			const typename LightMapPolicyType::ElementDataType& InLightMapElementData
			):
			LightMapElementData(InLightMapElementData)
		{}
	};

	/** Initialization constructor. */
	TLightMapDensityDrawingPolicy(
		const FViewInfo& View,
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		LightMapPolicyType InLightMapPolicy,
		EBlendMode InBlendMode,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings
		):
		FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, *InMaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), InOverrideSettings),
		LightMapPolicy(InLightMapPolicy),
		BlendMode(InBlendMode)
	{
		HullShader = NULL;
		DomainShader = NULL;

		const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetTessellationMode();
		if(RHISupportsTessellation(View.GetShaderPlatform())
			&& InVertexFactory->GetType()->SupportsTessellationShaders() 
			&& MaterialTessellationMode != MTM_NoTessellation)
		{
			HullShader = MaterialResource->GetShader<TLightMapDensityHS<LightMapPolicyType> >(VertexFactory->GetType());
			DomainShader = MaterialResource->GetShader<TLightMapDensityDS<LightMapPolicyType> >(VertexFactory->GetType());
		}

		VertexShader = MaterialResource->GetShader<TLightMapDensityVS<LightMapPolicyType> >(InVertexFactory->GetType());
		PixelShader = MaterialResource->GetShader<TLightMapDensityPS<LightMapPolicyType> >(InVertexFactory->GetType());
	}

	// FMeshDrawingPolicy interface.
	FDrawingPolicyMatchResult Matches(const TLightMapDensityDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(LightMapPolicy == Other.LightMapPolicy);
		DRAWING_POLICY_MATCH_END
	}

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType) const
	{
		// Set the base pass shader parameters for the material.
		VertexShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,View);

		if(HullShader && DomainShader)
		{
			HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
			DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		}

		// Set the light-map policy.
		LightMapPolicy.Set(RHICmdList, VertexShader,PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,View);
	}

	void SetMeshRenderState(
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

		VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

		if(HullShader && DomainShader)
		{
			HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
			DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		}

		// Set the light-map policy's mesh-specific settings.
		LightMapPolicy.SetMesh(RHICmdList, View,PrimitiveSceneProxy,VertexShader,PixelShader,VertexShader,PixelShader,VertexFactory,MaterialRenderProxy,ElementData.LightMapElementData);

		// BuiltLightingAndSelectedFlags informs the shader is lighting is built or not for this primitive
		FVector BuiltLightingAndSelectedFlags(0.0f,0.0f,0.0f);
		// LMResolutionScale is the physical resolution of the lightmap texture
		FVector2D LMResolutionScale(1.0f,1.0f);

		const auto FeatureLevel = View.GetFeatureLevel();

		bool bHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel);

		bool bTextureMapped = false;
		if (Mesh.LCI &&
			(Mesh.LCI->GetLightMapInteraction(FeatureLevel).GetType() == LMIT_Texture) &&
			Mesh.LCI->GetLightMapInteraction(FeatureLevel).GetTexture(bHighQualityLightMaps))
		{
			LMResolutionScale.X = Mesh.LCI->GetLightMapInteraction(FeatureLevel).GetTexture(bHighQualityLightMaps)->GetSizeX();
			LMResolutionScale.Y = Mesh.LCI->GetLightMapInteraction(FeatureLevel).GetTexture(bHighQualityLightMaps)->GetSizeY();
			bTextureMapped = true;

			BuiltLightingAndSelectedFlags.X = 1.0f;
			BuiltLightingAndSelectedFlags.Y = 0.0f;
		}
		else if (PrimitiveSceneProxy)
 		{
			int32 LightMapResolution = PrimitiveSceneProxy->GetLightMapResolution();
			if (PrimitiveSceneProxy->IsStatic() && LightMapResolution > 0)
			{
				bTextureMapped = true;
				LMResolutionScale = FVector2D(LightMapResolution, LightMapResolution);
				if (bHighQualityLightMaps)
				{	// Compensates the math in GetLightMapCoordinates (used to pack more coefficients per texture)
					LMResolutionScale.Y *= 2.f;
				}
				BuiltLightingAndSelectedFlags.X = 1.0f;
				BuiltLightingAndSelectedFlags.Y = 0.0f;
			}
			else
			{
				LMResolutionScale = FVector2D(0, 0);
				BuiltLightingAndSelectedFlags.X = 0.0f;
				BuiltLightingAndSelectedFlags.Y = 1.0f;
			}
		}

		if (Mesh.MaterialRenderProxy && (Mesh.MaterialRenderProxy->IsSelected() == true))
		{
			BuiltLightingAndSelectedFlags.Z = 1.0f;
		}
		else
		{
			BuiltLightingAndSelectedFlags.Z = 0.0f;
		}

		// Adjust for the grid texture being 2x2 repeating pattern...
		LMResolutionScale *= 0.5f;
		PixelShader->SetMesh(RHICmdList, VertexFactory,PrimitiveSceneProxy, BatchElement, View, DrawRenderState, BuiltLightingAndSelectedFlags, LMResolutionScale, bTextureMapped);
	}

	/** 
	 * Create bound shader state using the vertex decl from the mesh draw policy
	 * as well as the shaders needed to draw the mesh
	 * @return new bound shader state object
	 */
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		return FBoundShaderStateInput(
			FMeshDrawingPolicy::GetVertexDeclaration(), 
			VertexShader->GetVertexShader(),
			GETSAFERHISHADER_HULL(HullShader), 
			GETSAFERHISHADER_DOMAIN(DomainShader),
			PixelShader->GetPixelShader(),
			FGeometryShaderRHIRef());
	}

	friend int32 CompareDrawingPolicy(const TLightMapDensityDrawingPolicy& A,const TLightMapDensityDrawingPolicy& B)
	{
		COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
		COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
		COMPAREDRAWINGPOLICYMEMBERS(HullShader);
		COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
		COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
		COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
		return CompareDrawingPolicy(A.LightMapPolicy,B.LightMapPolicy);
	}

private:
	TLightMapDensityVS<LightMapPolicyType>* VertexShader;
	TLightMapDensityPS<LightMapPolicyType>* PixelShader;
	TLightMapDensityHS<LightMapPolicyType>* HullShader;
	TLightMapDensityDS<LightMapPolicyType>* DomainShader;

	LightMapPolicyType LightMapPolicy;
	EBlendMode BlendMode;
};

/**
 * A drawing policy factory for rendering lightmap density.
 */
class FLightMapDensityDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = false };
	struct ContextType {};

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
};
