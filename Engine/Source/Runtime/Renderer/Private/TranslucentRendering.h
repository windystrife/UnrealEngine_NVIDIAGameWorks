// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TranslucentRendering.h: Translucent rendering definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "VolumeRendering.h"


bool UseNearestDepthNeighborUpsampleForSeparateTranslucency(const FSceneRenderTargets& SceneContext);

/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		const FProjectedShadowInfo* TranslucentSelfShadow;
		ETranslucencyPass::Type TranslucencyPass;
		ESceneRenderTargetsMode::Type TextureMode;
		bool bPostAA;

		ContextType(const FProjectedShadowInfo* InTranslucentSelfShadow, ETranslucencyPass::Type InTranslucencyPass, bool bPostAAIn = false, ESceneRenderTargetsMode::Type InTextureMode = ESceneRenderTargetsMode::SetTextures)
			: TranslucentSelfShadow(InTranslucentSelfShadow)
			, TranslucencyPass(InTranslucencyPass)
			, TextureMode(InTextureMode)
			, bPostAA(bPostAAIn)			
		{}

		/** Whether this material should be processed now */
		bool ShouldDraw(const FMaterial* Material, bool bIsSeparateTranslucency) const;
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
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

	// WaveWorks Start
	static bool DrawDynamicWaveWorksMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
	// WaveWorks End

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
	static bool DrawStaticMesh(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View,
		ContextType DrawingContext,
		const FStaticMesh& StaticMesh,
		const uint64& BatchElementMask,
		bool bPreFog,
		const FDrawingPolicyRenderState& DrawRenderState,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	/**
	* Resolves the scene color target and copies it for use as a source texture.
	*/
	static void CopySceneColor(FRHICommandList& RHICmdList, const FViewInfo& View);
	static void UpsampleTranslucency(FRHICommandList& RHICmdList, const FViewInfo& View, bool bOverwrite);

private:
	/**
	* Render a dynamic or static mesh using a translucent draw policy
	* @return true if the mesh rendered
	*/
	static bool DrawMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		const FDrawingPolicyRenderState& DrawRenderState,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);

	// WaveWorks Start
	static bool DrawWaveWorksMesh(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		const uint64& BatchElementMask,
		const FDrawingPolicyRenderState& DrawRenderState,
		bool bPreFog,
		const FPrimitiveSceneProxy* PrimitiveSceneProxy,
		FHitProxyId HitProxyId
		);
	// WaveWorks End
};


/**
* Translucent draw policy factory.
* Creates the policies needed for rendering a mesh based on its material
*/
class FMobileTranslucencyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = true };
	struct ContextType 
	{
		ESceneRenderTargetsMode::Type TextureMode;
		ETranslucencyPass::Type TranslucencyPass;

		ContextType(ESceneRenderTargetsMode::Type InTextureMode, ETranslucencyPass::Type InTranslucencyPass)
		: TextureMode(InTextureMode)
		, TranslucencyPass(InTranslucencyPass)
		{}
	};

	/**
	* Render a dynamic mesh using a translucent draw policy 
	* @return true if the mesh rendered
	*/
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