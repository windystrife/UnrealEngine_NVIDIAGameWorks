// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RendererPrivate.h: Renderer interface private definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"

class FCanvas;
class FHitProxyId;
class FLightCacheInterface;
class FMaterial;
class FPrimitiveSceneInfo;
class FSceneInterface;
class FSceneRenderTargets;
class FSceneView;
class FSceneViewFamily;
class FSceneViewStateInterface;
struct FMeshBatch;
struct FSynthBenchmarkResults;

template<class ResourceType> class TGlobalResource;

DECLARE_LOG_CATEGORY_EXTERN(LogRenderer, Log, All);

/** The renderer module implementation. */
class FRendererModule : public IRendererModule
{
public:
	FRendererModule();
	virtual bool SupportsDynamicReloading() override { return true; }

	virtual void BeginRenderingViewFamily(FCanvas* Canvas,FSceneViewFamily* ViewFamily) override;
	virtual void CreateAndInitSingleView(FRHICommandListImmediate& RHICmdList, class FSceneViewFamily* ViewFamily, const struct FSceneViewInitOptions* ViewInitOptions) override;
	virtual FSceneInterface* AllocateScene(UWorld* World, bool bInRequiresHitProxies, bool bCreateFXSystem, ERHIFeatureLevel::Type InFeatureLevel) override;
	virtual void RemoveScene(FSceneInterface* Scene) override;
	virtual void UpdateStaticDrawListsForMaterials(const TArray<const FMaterial*>& Materials) override;
	virtual FSceneViewStateInterface* AllocateViewState() override;
	virtual uint32 GetNumDynamicLightsAffectingPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FLightCacheInterface* LCI) override;
	virtual void ReallocateSceneRenderTargets() override;
	virtual void SceneRenderTargetsSetBufferSize(uint32 SizeX, uint32 SizeY) override;
	virtual void InitializeSystemTextures(FRHICommandListImmediate& RHICmdList);
	virtual void DrawTileMesh(FRHICommandListImmediate& RHICmdList, FDrawingPolicyRenderState& DrawRenderState, const FSceneView& View, const FMeshBatch& Mesh, bool bIsHitTesting, const FHitProxyId& HitProxyId) override;
	virtual void RenderTargetPoolFindFreeElement(FRHICommandListImmediate& RHICmdList, const FPooledRenderTargetDesc& Desc, TRefCountPtr<IPooledRenderTarget> &Out, const TCHAR* InDebugName) override;
	virtual void TickRenderTargetPool() override;
	virtual void DebugLogOnCrash() override;
	virtual void GPUBenchmark(FSynthBenchmarkResults& InOut, float WorkScale) override;
	virtual void QueryVisualizeTexture(FQueryVisualizeTexureInfo& Out) override;
	virtual void ExecVisualizeTextureCmd(const FString& Cmd) override;
	virtual void UpdateMapNeedsLightingFullyRebuiltState(UWorld* World) override;
	virtual void DrawRectangle(
		FRHICommandList& RHICmdList,
		float X,
		float Y,
		float SizeX,
		float SizeY,
		float U,
		float V,
		float SizeU,
		float SizeV,
		FIntPoint TargetSize,
		FIntPoint TextureSize,
		class FShader* VertexShader,
		EDrawRectangleFlags Flags = EDRF_Default
		) override;
	virtual TGlobalResource<FFilterVertexDeclaration>& GetFilterVertexDeclaration() override;

	virtual const TSet<FSceneInterface*>& GetAllocatedScenes() override
	{
		return AllocatedScenes;
	}

	virtual void RegisterCustomCullingImpl(ICustomCulling* impl) override;
	virtual void UnregisterCustomCullingImpl(ICustomCulling* impl) override;

	virtual void RegisterPostOpaqueComputeDispatcher(FComputeDispatcher *Dispatcher) override;
	virtual void UnRegisterPostOpaqueComputeDispatcher(FComputeDispatcher *Dispatcher) override;
	virtual void DispatchPostOpaqueCompute(FRHICommandList &CmdList) override;

	virtual void RegisterPostOpaqueRenderDelegate(const FPostOpaqueRenderDelegate& PostOpaqueRenderDelegate) override;
	virtual void RegisterOverlayRenderDelegate(const FPostOpaqueRenderDelegate& OverlayRenderDelegate) override;
	virtual void RenderPostOpaqueExtensions(const FSceneView& View, FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext) override;
	virtual void RenderOverlayExtensions(const FSceneView& View, FRHICommandListImmediate& RHICmdList, FSceneRenderTargets& SceneContext) override;

	virtual bool HasPostOpaqueExtentions() const override
	{
		return PostOpaqueRenderDelegate.IsBound();
	}

	virtual FOnResolvedSceneColor& GetResolvedSceneColorCallbacks() override
	{
		return PostResolvedSceneColorCallbacks;
	}

	virtual void RenderPostResolvedSceneColorExtension(FRHICommandListImmediate& RHICmdList, class FSceneRenderTargets& SceneContext) override;

	virtual void PostRenderAllViewports() override;

private:
	TSet<FSceneInterface*> AllocatedScenes;
	ICustomCulling* CustomCullingImpl;
	FPostOpaqueRenderDelegate PostOpaqueRenderDelegate;
	FPostOpaqueRenderDelegate OverlayRenderDelegate;
	FOnResolvedSceneColor PostResolvedSceneColorCallbacks;
	TArray<FComputeDispatcher*>PostOpaqueDispatchers;
};

extern ICustomCulling* GCustomCullingImpl;
