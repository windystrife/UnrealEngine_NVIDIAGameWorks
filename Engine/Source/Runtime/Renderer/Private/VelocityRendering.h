// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VelocityRendering.h: Velocity rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "HitProxies.h"
#include "RendererInterface.h"
#include "DrawingPolicy.h"
#include "DepthRendering.h"

class FPrimitiveSceneInfo;
class FPrimitiveSceneProxy;
class FScene;
class FStaticMesh;
class FViewInfo;

/**
 * Outputs a 2d velocity vector.
 */
class FVelocityDrawingPolicy : public FMeshDrawingPolicy
{
public:
	FVelocityDrawingPolicy(
		const FVertexFactory* InVertexFactory,
		const FMaterialRenderProxy* InMaterialRenderProxy,
		const FMaterial& InMaterialResource,
		const FMeshDrawingPolicyOverrideSettings& InOverrideSettings,
		ERHIFeatureLevel::Type InFeatureLevel
		);

	// FMeshDrawingPolicy interface.
	FDrawingPolicyMatchResult Matches(const FVelocityDrawingPolicy& Other) const
	{
		DRAWING_POLICY_MATCH_BEGIN
			DRAWING_POLICY_MATCH(FMeshDrawingPolicy::Matches(Other)) &&
			DRAWING_POLICY_MATCH(HullShader == Other.HullShader) &&
			DRAWING_POLICY_MATCH(DomainShader == Other.DomainShader) &&
			DRAWING_POLICY_MATCH(VertexShader == Other.VertexShader) &&
			DRAWING_POLICY_MATCH(PixelShader == Other.PixelShader);
		DRAWING_POLICY_MATCH_END
	}
	void SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* SceneView, const FVelocityDrawingPolicy::ContextDataType PolicyContext) const;

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

	void SetInstancedEyeIndex(FRHICommandList& RHICmdList, const uint32 EyeIndex) const;

	FBoundShaderStateInput GetBoundShaderStateInput(ERHIFeatureLevel::Type InFeatureLevel) const;

	friend int32 Compare(const FVelocityDrawingPolicy& A, const FVelocityDrawingPolicy& B);

	bool SupportsVelocity( ) const;

	/** Determines whether this primitive has motionblur velocity to render */
	static bool HasVelocity(const FViewInfo& View, const FPrimitiveSceneInfo* PrimitiveSceneInfo);
	static bool HasVelocityOnBasePass(const FViewInfo& View,const FPrimitiveSceneProxy* Proxy, const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, bool& bOutHasTransform, FMatrix& OutTransform);

private:
	class FVelocityVS* VertexShader;
	class FVelocityPS* PixelShader;
	class FVelocityHS* HullShader;
	class FVelocityDS* DomainShader;
	class FShaderPipeline* ShaderPipeline;
};

/**
 * A drawing policy factory for rendering motion velocity.
 */
class FVelocityDrawingPolicyFactory : public FDepthDrawingPolicyFactory
{
public:
	static void AddStaticMesh(FScene* Scene, FStaticMesh* StaticMesh);
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

/** Get the cvar clamped state */
int32 GetMotionBlurQualityFromCVar();

/** If this view need motion blur processing */
bool IsMotionBlurEnabled(const FViewInfo& View);

// Group Velocity Rendering accessors, types, etc.
struct FVelocityRendering
{
	static FPooledRenderTargetDesc GetRenderTargetDesc();

	static bool OutputsToGBuffer();
	static bool OutputsOnlyToGBuffer(bool bSupportsStaticLighting);
};
