// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneHitProxyRendering.h: Scene hit proxy rendering.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "HitProxies.h"
#include "DrawingPolicy.h"

class FPrimitiveSceneProxy;
class FScene;
class FStaticMesh;

/**
 * Outputs no color, but can be used to write the mesh's depth values to the depth buffer.
 */
class FHitProxyDrawingPolicy : public FMeshDrawingPolicy
{
public:

	typedef FHitProxyId ElementDataType;

	FHitProxyDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		ERHIFeatureLevel::Type InFeatureLevel,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings
		);

	// FMeshDrawingPolicy interface.
	FDrawingPolicyMatchResult Matches(const FHitProxyDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
		DRAWING_POLICY_MATCH_END
	}
	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType PolicyContext) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @return new bound shader state object
	*/
	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;

	void SetMeshRenderState(
		FRHICommandList& RHICmdList, 
		const FSceneView& View,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		const FMeshBatch& Mesh,
		int32 BatchElementIndex,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FHitProxyId HitProxyId,
		const ContextDataType PolicyContext
		) const;

private:
	class FHitProxyVS* VertexShader;
	class FHitProxyPS* PixelShader;
	class FHitProxyHS* HullShader;
	class FHitProxyDS* DomainShader;
};

#if WITH_EDITOR
class FEditorSelectionDrawingPolicy : public FHitProxyDrawingPolicy
{
public:
	FEditorSelectionDrawingPolicy(const FVertexFactory* InVertexFactory, const FMaterialRenderProxy* InMaterialRenderProxy, ERHIFeatureLevel::Type InFeatureLevel, const FMeshDrawingPolicyOverrideSettings& InOverrideSettings)
		: FHitProxyDrawingPolicy(InVertexFactory, InMaterialRenderProxy, InFeatureLevel, InOverrideSettings)
	{}

	// FMeshDrawingPolicy interface.
	void SetMeshRenderState( FRHICommandList& RHICmdList, const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMeshBatch& Mesh, int32 BatchElementIndex, const FDrawingPolicyRenderState& DrawRenderState, const FHitProxyId HitProxyId, const ContextDataType PolicyContext );
	
	void SetupPipelineState(FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View) const;

	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const ContextDataType PolicyContext) const;
	
	/**
	 * Gets the value that should be written into the editor selection stencil buffer for a given primitive
	 * There will be a unique stencil value for each primitive until the max stencil buffer value is written and then the values will repeat
	 *
	 * @param View					The view being rendered
	 * @param PrimitiveSceneProxy	The scene proxy for the primitive being rendered
	 * @return the stencil value that should be written
	 */
	static int32 GetStencilValue(const FSceneView& View, const FPrimitiveSceneProxy* PrimitiveSceneProxy);

	/**
	 * Resets all unique stencil values
	 */
	static void ResetStencilValues();

private:
	/** This map is needed to ensure that individually selected proxies rendered more than once a frame (if they have multiple sections) share a common outline */
	static TMap<const FPrimitiveSceneProxy*, int32> ProxyToStencilIndex;
	/** This map is needed to ensure that proxies rendered more than once a frame (if they have multiple sections) share a common outline */
	static TMap<FName, int32> ActorNameToStencilIndex;
};
#endif

/**
 * A drawing policy factory for the hit proxy drawing policy.
 */
class FHitProxyDrawingPolicyFactory
{
public:

	enum { bAllowSimpleElements = true };
	struct ContextType {};

	static void AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType DrawingContext = ContextType());
	static bool DrawDynamicMesh(
		FRHICommandList& RHICmdList,
		const FSceneView& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
};
