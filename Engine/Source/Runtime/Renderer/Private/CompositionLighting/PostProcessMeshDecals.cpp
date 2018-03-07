// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDeferredMeshDecals.cpp: Deferred Decals implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "HitProxies.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "RHIStaticStates.h"
#include "PostProcess/SceneRenderTargets.h"
#include "MaterialShaderType.h"
#include "DrawingPolicy.h"
#include "MeshMaterialShader.h"
#include "ShaderBaseClasses.h"
#include "DepthRendering.h"
#include "DecalRenderingCommon.h"
#include "CompositionLighting/PostProcessDeferredDecals.h"
#include "SceneRendering.h"


/**
* Policy for drawing mesh decals
*/
class FMeshDecalAccumulatePolicy
{	
public:
	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material && Material->IsDeferredDecal() && IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM4);
	}
};

/**
* A vertex shader for rendering mesh decals
*/
class FMeshDecalVS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FMeshDecalVS, MeshMaterial);

protected:

	FMeshDecalVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	FMeshDecalVS()
	{
	}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FMeshDecalAccumulatePolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

public:
	
	void SetParameters(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView* View)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetVertexShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View->GetFeatureLevel()), *View, View->ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetVertexShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}
};


/**
 * A hull shader for rendering mesh decals
 */
class FMeshDecalHS : public FBaseHS
{
	DECLARE_SHADER_TYPE(FMeshDecalHS, MeshMaterial);

protected:

	FMeshDecalHS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseHS(Initializer)
	{}

	FMeshDecalHS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseHS::ShouldCache(Platform, Material, VertexFactoryType)
			&& FMeshDecalAccumulatePolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

/**
 * A domain shader for rendering mesh decals
 */
class FMeshDecalDS : public FBaseDS
{
	DECLARE_SHADER_TYPE(FMeshDecalDS, MeshMaterial);

protected:

	FMeshDecalDS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FBaseDS(Initializer)
	{}

	FMeshDecalDS() {}

	static bool ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return FBaseDS::ShouldCache(Platform, Material, VertexFactoryType)
			&& FMeshDecalAccumulatePolicy::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalVS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainVS"),SF_Vertex); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalHS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainHull"),SF_Hull); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalDS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainDomain"),SF_Domain);


/**
* A pixel shader to render mesh decals
*/
class FMeshDecalsPS : public FMeshMaterialShader
{
	DECLARE_SHADER_TYPE(FMeshDecalsPS, MeshMaterial);

public:
	static bool ShouldCache(EShaderPlatform Platform, const FMaterial* Material, const FVertexFactoryType* VertexFactoryType)
	{
		return FMeshDecalAccumulatePolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	FMeshDecalsPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialShader(Initializer)
	{
	}

	FMeshDecalsPS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList, 
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View
		)
	{
		FMeshMaterialShader::SetParameters(RHICmdList, GetPixelShader(), MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(View.GetFeatureLevel()), View, View.ViewUniformBuffer, ESceneRenderTargetsMode::SetTextures);
	}

	void SetMesh(FRHICommandList& RHICmdList, const FVertexFactory* VertexFactory,const FSceneView& View,const FPrimitiveSceneProxy* Proxy,const FMeshBatchElement& BatchElement,const FDrawingPolicyRenderState& DrawRenderState)
	{
		FMeshMaterialShader::SetMesh(RHICmdList, GetPixelShader(),VertexFactory,View,Proxy,BatchElement,DrawRenderState);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FMeshMaterialShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FMeshDecalsPS,TEXT("/Engine/Private/MeshDecals.usf"),TEXT("MainPS"),SF_Pixel);

/*-----------------------------------------------------------------------------
FMeshDecalsDrawingPolicy
-----------------------------------------------------------------------------*/

/**
* Mesh decals drawing policy
*/
class FMeshDecalsDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	*/
	FMeshDecalsDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& MaterialResouce,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings
		);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return true if the draw policies are a match
	*/
	FDrawingPolicyMatchResult Matches(const FMeshDecalsDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType PolicyContext) const;
	
	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneProxy - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const ElementDataType& ElementData,
		const ContextDataType PolicyContext
		) const;

private:
	FMeshDecalVS* VertexShader;
	FMeshDecalHS* HullShader;
	FMeshDecalDS* DomainShader;
	FMeshDecalsPS* PixelShader;
};

FMeshDecalsDrawingPolicy::FMeshDecalsDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	ERHIFeatureLevel::Type InFeatureLevel,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings)
	: FMeshDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InMaterialResource, InOverrideSettings)
{
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = MaterialResource->GetTessellationMode();
	if (RHISupportsTessellation(GShaderPlatformForFeatureLevel[InFeatureLevel])
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		HullShader = InMaterialResource.GetShader<FMeshDecalHS>(VertexFactory->GetType());
		DomainShader = InMaterialResource.GetShader<FMeshDecalDS>(VertexFactory->GetType());
	}

	VertexShader = InMaterialResource.GetShader<FMeshDecalVS>(InVertexFactory->GetType());

	PixelShader = InMaterialResource.GetShader<FMeshDecalsPS>(InVertexFactory->GetType());
}

FDrawingPolicyMatchResult FMeshDecalsDrawingPolicy::Matches(
	const FMeshDecalsDrawingPolicy& Other
	) const
{
	DRAWING_POLICY_MATCH_BEGIN
		DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
		DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
		DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
		DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
		DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
	DRAWING_POLICY_MATCH_END
}

void FMeshDecalsDrawingPolicy::SetSharedState(
	FRHICommandList& RHICmdList, 
	const FDrawingPolicyRenderState& DrawRenderState,
	const FSceneView* View,
	const ContextDataType PolicyContext
	) const
{
	// Set shared mesh resources
	FMeshDrawingPolicy::SetSharedState(RHICmdList, DrawRenderState, View, PolicyContext);

	// Set the translucent shader parameters for the material instance
	VertexShader->SetParameters(RHICmdList, VertexFactory,MaterialRenderProxy,View);

	if(HullShader && DomainShader)
	{
		HullShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
		DomainShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
	}

	PixelShader->SetParameters(RHICmdList, MaterialRenderProxy,*View);
}

FBoundShaderStateInput FMeshDecalsDrawingPolicy::GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const
{
	FPixelShaderRHIParamRef PixelShaderRHIRef = NULL;

	PixelShaderRHIRef = PixelShader->GetPixelShader();

	return FBoundShaderStateInput(
		FMeshDrawingPolicy::GetVertexDeclaration(), 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		PixelShaderRHIRef,
		FGeometryShaderRHIRef());
}

void FMeshDecalsDrawingPolicy::SetMeshRenderState(
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

	// Set transforms
	VertexShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);

	if(HullShader && DomainShader)
	{
		HullShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
		DomainShader->SetMesh(RHICmdList, VertexFactory,View,PrimitiveSceneProxy,BatchElement,DrawRenderState);
	}	
}


/**
 * A drawing policy factory for the decal drawing policy.
 */
class FDecalDrawingPolicyFactory
{
public:
	struct ContextType
	{
		// context for this pass
		FRenderingCompositePassContext& Context; 
		const EDecalRenderStage CurrentDecalStage;
		
		FDecalRenderingCommon::ERenderTargetMode LastRenderTargetMode;
		EDecalBlendMode LastDecalBlendMode;

		FDecalRenderTargetManager RenderTargetManager;

		ContextType(FRenderingCompositePassContext& InContext, EDecalRenderStage InCurrentDecalStage)
			: Context(InContext)
			, CurrentDecalStage(InCurrentDecalStage)
			, LastRenderTargetMode(FDecalRenderingCommon::RTM_Unknown)
			, LastDecalBlendMode(EDecalBlendMode::DBM_MAX)
			, RenderTargetManager(Context.RHICmdList, Context.GetShaderPlatform(), CurrentDecalStage)
		{
		}

		void SetState(const FMaterial* Material, FDrawingPolicyRenderState& DrawRenderState)
		{
			FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
			const FViewInfo& View = Context.View;

			bool bHasNormal = Material->HasNormalConnected();

			EDecalBlendMode DecalBlendMode = FDecalRenderingCommon::ComputeFinalDecalBlendMode(Context.GetShaderPlatform(), (EDecalBlendMode)Material->GetDecalBlendMode(), bHasNormal);

			FDecalRenderingCommon::ERenderTargetMode RenderTargetMode = FDecalRenderingCommon::ComputeRenderTargetMode(View.GetShaderPlatform(), DecalBlendMode, RenderTargetManager.bGufferADirty);

			if(LastRenderTargetMode != RenderTargetMode)
			{
				LastRenderTargetMode = RenderTargetMode;
				RenderTargetManager.SetRenderTargetMode(RenderTargetMode, bHasNormal);

				DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<false,CF_DepthNearOrEqual>::GetRHI());

				Context.SetViewportAndCallRHI(View.ViewRect);
			}

			if(LastDecalBlendMode != DecalBlendMode)
			{
				LastDecalBlendMode = DecalBlendMode;
	
				const ERHIFeatureLevel::Type FeatureLevel = View.GetFeatureLevel();

				DrawRenderState.SetBlendState(GetDecalBlendState(FeatureLevel, CurrentDecalStage, DecalBlendMode, bHasNormal));
			}
		}
	};

	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId, 
		const bool bIsInstancedStereo = false, 
		const bool bNeedsInstancedStereoBias = false)
	{
		return DrawMesh(
			RHICmdList, 
			View,
			DrawingContext,
			Mesh,
			Mesh.Elements.Num()==1 ? 1 : (1<<Mesh.Elements.Num())-1,	// 1 bit set for each mesh element
			bPreFog,
			DrawRenderState,
			PrimitiveSceneProxy,
			HitProxyId, 
			bIsInstancedStereo, 
			bNeedsInstancedStereoBias);
	}

	
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId, 
		const bool bNeedsInstancedStereoBias = false
		)
	{
		return DrawMesh(
			RHICmdList, 
			View,
			DrawingContext,
			StaticMesh,
			BatchElementMask,
			bPreFog,
			DrawRenderState,
			PrimitiveSceneProxy,
			HitProxyId, 
			false, 
			bNeedsInstancedStereoBias);
	}

private:
	/**
	* Render a dynamic or static mesh using a decal draw policy
	* @return true if the mesh rendered
	*/
	static bool DrawMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId, 
		const bool bIsInstancedStereo = false, 
		const bool bNeedsInstancedStereoBias = false
		)
	{
		bool bDirty = false;
		const auto FeatureLevel = View.GetFeatureLevel();

		const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
		const FMaterial* Material = MaterialRenderProxy->GetMaterialNoFallback(FeatureLevel);

		if (Material && Material->IsDeferredDecal())
		{
			// We have no special engine material for decals since we don't want to eat the compilation & memory cost, so just skip if it failed to compile
			if (Material->GetRenderingThreadShaderMap())
			{
				EDecalRenderStage LocalDecalRenderStage = FDecalRenderingCommon::ComputeRenderStage(View.GetShaderPlatform(), (EDecalBlendMode)Material->GetDecalBlendMode());

				// can be optimized (ranges for different decal stages or separate lists)
				if (DrawingContext.CurrentDecalStage == LocalDecalRenderStage)
				{
					FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
					DrawingContext.SetState(Material, DrawRenderStateLocal);

					FMeshDecalsDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *Material, View.GetFeatureLevel(), ComputeMeshOverrideSettings(Mesh));
					DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
					CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
					DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, FDepthDrawingPolicy::ContextDataType(bIsInstancedStereo, bNeedsInstancedStereoBias));

					int32 BatchElementIndex = 0;
					uint64 Mask = BatchElementMask;
					do
					{
						if (Mask & 1)
						{
							TDrawEvent<FRHICommandList> MeshEvent;
							BeginMeshDrawEvent(RHICmdList, PrimitiveSceneProxy, Mesh, MeshEvent);

							DrawingPolicy.SetMeshRenderState(RHICmdList, View, PrimitiveSceneProxy, Mesh, BatchElementIndex, DrawRenderStateLocal, FMeshDrawingPolicy::ElementDataType(), FDepthDrawingPolicy::ContextDataType());
							DrawingPolicy.DrawMesh(RHICmdList, Mesh, BatchElementIndex, bIsInstancedStereo);
						}
						Mask >>= 1;
						BatchElementIndex++;
					}
					while (Mask);

					bDirty = true;
				}
			}
		}

		return bDirty;
	}
};


static void RenderPrimitive(FRenderingCompositePassContext& Context, FDecalDrawingPolicyFactory::ContextType& DrawContext, const FDrawingPolicyRenderState& DrawRenderState, FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Context.View;

	uint32 PrimitiveId = PrimitiveSceneInfo->GetIndex();
	const FPrimitiveViewRelevance& ViewRelevance = View.PrimitiveViewRelevanceMap[PrimitiveId];

	if (ViewRelevance.bDrawRelevance)
	{
		// Render dynamic scene prim
		{	
			// range in View.DynamicMeshElements[]
			FInt32Range range = View.GetDynamicMeshElementRange(PrimitiveSceneInfo->GetIndex());

			for (int32 MeshBatchIndex = range.GetLowerBoundValue(); MeshBatchIndex < range.GetUpperBoundValue(); MeshBatchIndex++)
			{
				const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

				checkSlow(MeshBatchAndRelevance.PrimitiveSceneProxy == PrimitiveSceneInfo->Proxy);

				const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;

				FDecalDrawingPolicyFactory::DrawDynamicMesh(
					RHICmdList,
					View,
					DrawContext,
					MeshBatch,
					false,
					DrawRenderState,
					MeshBatchAndRelevance.PrimitiveSceneProxy,
					MeshBatch.BatchHitProxyId);
			}
		}

		// Render static scene prim
		if (ViewRelevance.bStaticRelevance)
		{
			// Render static meshes from static scene prim
			for (int32 StaticMeshIdx = 0, Count = PrimitiveSceneInfo->StaticMeshes.Num(); StaticMeshIdx < Count; StaticMeshIdx++)
			{
				FStaticMesh& StaticMesh = PrimitiveSceneInfo->StaticMeshes[StaticMeshIdx];

				// Only render static mesh elements using decal materials
				if (View.StaticMeshVisibilityMap[StaticMesh.Id]
					&& StaticMesh.IsDecal(View.FeatureLevel))
				{
					FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
					FMeshDrawingPolicy::OnlyApplyDitheredLODTransitionState(DrawRenderStateLocal, View, StaticMesh, false);

					FDecalDrawingPolicyFactory::DrawStaticMesh(
						RHICmdList,
						View,
						DrawContext,
						StaticMesh,
						StaticMesh.bRequiresPerElementVisibility ? View.StaticMeshBatchVisibility[StaticMesh.BatchVisibilityId] : ((1ull << StaticMesh.Elements.Num()) - 1),
						false,
						DrawRenderStateLocal,
						PrimitiveSceneInfo->Proxy,
						StaticMesh.BatchHitProxyId);
				}
			}
		}
	}
}

void RenderMeshDecals(FRenderingCompositePassContext& Context, EDecalRenderStage CurrentDecalStage)
{
	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);
	const FViewInfo& View = Context.View;

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSceneRenderer_RenderMeshDecals);
	SCOPED_DRAW_EVENT(RHICmdList, MeshDecals);
	
	FDecalDrawingPolicyFactory::ContextType DrawContext(Context, CurrentDecalStage);

	FDrawingPolicyRenderState DrawRenderState(Context.View);
	for(int32 PrimIdx = 0, Count = View.MeshDecalPrimSet.NumPrims(); PrimIdx < Count; PrimIdx++ )
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = View.MeshDecalPrimSet.Prims[PrimIdx].PrimitiveSceneInfo;

		RenderPrimitive(Context,  DrawContext, DrawRenderState, PrimitiveSceneInfo);
	}
}
