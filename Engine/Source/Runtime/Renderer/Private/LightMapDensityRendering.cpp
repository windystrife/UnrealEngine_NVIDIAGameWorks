// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightMapDensityRendering.cpp: Implementation for rendering lightmap density.
=============================================================================*/

#include "LightMapDensityRendering.h"
#include "DeferredShadingRenderer.h"
#include "LightMapRendering.h"
#include "ScenePrivate.h"

//-----------------------------------------------------------------------------

#if !UE_BUILD_DOCS
// Typedef is necessary because the C preprocessor thinks the comma in the template parameter list is a comma in the macro parameter list.
#define IMPLEMENT_DENSITY_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TLightMapDensityVS< LightMapPolicyType > TLightMapDensityVS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityVS##LightMapPolicyName,TEXT("/Engine/Private/LightMapDensityShader.usf"),TEXT("MainVertexShader"),SF_Vertex); \
	typedef TLightMapDensityHS< LightMapPolicyType > TLightMapDensityHS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityHS##LightMapPolicyName,TEXT("/Engine/Private/LightMapDensityShader.usf"),TEXT("MainHull"),SF_Hull); \
	typedef TLightMapDensityDS< LightMapPolicyType > TLightMapDensityDS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityDS##LightMapPolicyName,TEXT("/Engine/Private/LightMapDensityShader.usf"),TEXT("MainDomain"),SF_Domain); 

#define IMPLEMENT_DENSITY_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	typedef TLightMapDensityPS< LightMapPolicyType > TLightMapDensityPS##LightMapPolicyName; \
	IMPLEMENT_MATERIAL_SHADER_TYPE(template<>,TLightMapDensityPS##LightMapPolicyName,TEXT("/Engine/Private/LightMapDensityShader.usf"),TEXT("MainPixelShader"),SF_Pixel);

// Implement a pixel shader type for skylights and one without, and one vertex shader that will be shared between them
#define IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_DENSITY_VERTEXSHADER_TYPE(LightMapPolicyType,LightMapPolicyName) \
	IMPLEMENT_DENSITY_PIXELSHADER_TYPE(LightMapPolicyType,LightMapPolicyName);

IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_NO_LIGHTMAP>, FNoLightMapPolicy );
IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_DUMMY>, FDummyLightMapPolicy);
IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>, TLightMapPolicyLQ );
IMPLEMENT_DENSITY_LIGHTMAPPED_SHADER_TYPE( TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>, TLightMapPolicyHQ );

#endif

bool FDeferredShadingSceneRenderer::RenderLightMapDensities(FRHICommandListImmediate& RHICmdList)
{
	bool bDirty=0;
	if (Scene->GetFeatureLevel() >= ERHIFeatureLevel::SM4)
	{
		SCOPED_DRAW_EVENT(RHICmdList, LightMapDensity);

		// Draw the scene's emissive and light-map color.
		for(int32 ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
		{
			SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, EventView, Views.Num() > 1, TEXT("View%d"), ViewIndex);

			FViewInfo& View = Views[ViewIndex];

			FDrawingPolicyRenderState DrawRenderState(View);
			// Opaque blending, depth tests and writes.
			DrawRenderState.SetBlendState(TStaticBlendState<>::GetRHI());
			DrawRenderState.SetDepthStencilState(TStaticDepthStencilState<true,CF_DepthNearOrEqual>::GetRHI());
			RHICmdList.SetViewport(View.ViewRect.Min.X, View.ViewRect.Min.Y, 0, View.ViewRect.Max.X, View.ViewRect.Max.Y, 1);

			{
				SCOPED_DRAW_EVENT(RHICmdList, Dynamic);

				FLightMapDensityDrawingPolicyFactory::ContextType Context;

				for (int32 MeshBatchIndex = 0; MeshBatchIndex < View.DynamicMeshElements.Num(); MeshBatchIndex++)
				{
					const FMeshBatchAndRelevance& MeshBatchAndRelevance = View.DynamicMeshElements[MeshBatchIndex];

					if (MeshBatchAndRelevance.GetHasOpaqueOrMaskedMaterial() || ViewFamily.EngineShowFlags.Wireframe)
					{
						const FMeshBatch& MeshBatch = *MeshBatchAndRelevance.Mesh;
						FLightMapDensityDrawingPolicyFactory::DrawDynamicMesh(RHICmdList, View, Context, MeshBatch, true, DrawRenderState, MeshBatchAndRelevance.PrimitiveSceneProxy, MeshBatch.BatchHitProxyId);
					}
				}
			}
		}
	}

	return bDirty;
}

bool FLightMapDensityDrawingPolicyFactory::DrawDynamicMesh(
	FRHICommandList& RHICmdList, 
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	bool bPreFog,
	const FDrawingPolicyRenderState& DrawRenderState,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	FHitProxyId HitProxyId
	)
{
	bool bDirty = false;

	const auto FeatureLevel = View.GetFeatureLevel();
	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial(FeatureLevel);
	const EBlendMode BlendMode = Material->GetBlendMode();
	FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);
	DrawRenderStateLocal.SetDitheredLODTransitionAlpha(Mesh.DitheredLODTransitionAlpha);
	DrawRenderStateLocal.SetBlendState(TStaticBlendState<>::GetRHI());

	const bool bMaterialMasked = Material->IsMasked();
	const bool bMaterialModifiesMesh = Material->MaterialModifiesMeshPosition_RenderThread();
	if (!bMaterialMasked && !bMaterialModifiesMesh)
	{
		// Override with the default material for opaque materials that are not two sided
		MaterialRenderProxy = GEngine->LevelColorationLitMaterial->GetRenderProxy(false);
	}

	bool bIsLitMaterial = (Material->GetShadingModel() != MSM_Unlit);
	/*const */FLightMapInteraction LightMapInteraction = (Mesh.LCI && bIsLitMaterial) ? Mesh.LCI->GetLightMapInteraction(FeatureLevel) : FLightMapInteraction();
	// force simple lightmaps based on system settings
	bool bAllowHighQualityLightMaps = AllowHighQualityLightmaps(FeatureLevel) && LightMapInteraction.AllowsHighQualityLightmaps();

	static const auto CVarSupportLowQualityLightmaps = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SupportLowQualityLightmaps"));
	const bool bAllowLowQualityLightMaps = (!CVarSupportLowQualityLightmaps) || (CVarSupportLowQualityLightmaps->GetValueOnAnyThread() != 0);

	if (bIsLitMaterial && PrimitiveSceneProxy && (LightMapInteraction.GetType() == LMIT_Texture || (PrimitiveSceneProxy->IsStatic() && PrimitiveSceneProxy->GetLightMapResolution() > 0)))
	{
		// Should this object be texture lightmapped? Ie, is lighting not built for it??
		bool bUseDummyLightMapPolicy = Mesh.LCI == NULL || Mesh.LCI->GetLightMapInteraction(FeatureLevel).GetType() != LMIT_Texture;

		//use dummy if we don't support either lightmap quality.
		bUseDummyLightMapPolicy |= (!bAllowHighQualityLightMaps && !bAllowLowQualityLightMaps);
		if (!bUseDummyLightMapPolicy)
		{
			if (bAllowHighQualityLightMaps)
			{
				TLightMapDensityDrawingPolicy< TUniformLightMapPolicy<LMP_HQ_LIGHTMAP> > DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, TUniformLightMapPolicy<LMP_HQ_LIGHTMAP>(), BlendMode, ComputeMeshOverrideSettings(Mesh));
				DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
				CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
				DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, TLightMapDensityDrawingPolicy< FUniformLightMapPolicy >::ContextDataType());
				for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,DrawRenderStateLocal,
						Mesh.LCI,
						TLightMapDensityDrawingPolicy<FUniformLightMapPolicy>::ContextDataType()
						);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
				bDirty = true;
			}
			else
			{
				TLightMapDensityDrawingPolicy< TUniformLightMapPolicy<LMP_LQ_LIGHTMAP> > DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, TUniformLightMapPolicy<LMP_LQ_LIGHTMAP>(), BlendMode, ComputeMeshOverrideSettings(Mesh));
				DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
				CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
				DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, TLightMapDensityDrawingPolicy< FUniformLightMapPolicy >::ContextDataType());
				for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
				{
					DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,DrawRenderStateLocal,
						Mesh.LCI,
						TLightMapDensityDrawingPolicy<FUniformLightMapPolicy>::ContextDataType()
						);
					DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
				}
				bDirty = true;
			}
		}
		else
		{
			TLightMapDensityDrawingPolicy<TUniformLightMapPolicy<LMP_DUMMY> > DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, TUniformLightMapPolicy<LMP_DUMMY>(), BlendMode, ComputeMeshOverrideSettings(Mesh));
			DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
			CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
			DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, TLightMapDensityDrawingPolicy<FUniformLightMapPolicy >::ContextDataType());
			for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
			{
				DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,DrawRenderStateLocal,
					Mesh.LCI,
					TLightMapDensityDrawingPolicy<FUniformLightMapPolicy>::ContextDataType()
					);
				DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
			}
			bDirty = true;
		}
	}
	else
	{
		TLightMapDensityDrawingPolicy<TUniformLightMapPolicy<LMP_NO_LIGHTMAP> > DrawingPolicy(View, Mesh.VertexFactory, MaterialRenderProxy, TUniformLightMapPolicy<LMP_NO_LIGHTMAP>(), BlendMode, ComputeMeshOverrideSettings(Mesh));
		DrawingPolicy.SetupPipelineState(DrawRenderStateLocal, View);
		CommitGraphicsPipelineState(RHICmdList, DrawingPolicy, DrawRenderStateLocal, DrawingPolicy.GetBoundShaderStateInput(View.GetFeatureLevel()));
		DrawingPolicy.SetSharedState(RHICmdList, DrawRenderStateLocal, &View, TLightMapDensityDrawingPolicy<TUniformLightMapPolicy<LMP_NO_LIGHTMAP> >::ContextDataType());
		for (int32 BatchElementIndex = 0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++)
		{
			DrawingPolicy.SetMeshRenderState(RHICmdList, View,PrimitiveSceneProxy,Mesh,BatchElementIndex,DrawRenderStateLocal,
				Mesh.LCI,
				TLightMapDensityDrawingPolicy<FUniformLightMapPolicy>::ContextDataType()
				);
			DrawingPolicy.DrawMesh(RHICmdList, Mesh,BatchElementIndex);
		}
		bDirty = true;
	}

	return bDirty;
}
