// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneInfo.h: Primitive scene info definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "RenderingThread.h"
#include "SceneTypes.h"
#include "HitProxies.h"
#include "GenericOctreePublic.h"
#include "Engine/Scene.h"
#include "RendererInterface.h"

class FPrimitiveSceneInfo;
class FPrimitiveSceneProxy;
class FReflectionCaptureProxy;
class FScene;
class FViewInfo;
class UPrimitiveComponent;
template<typename ElementType,typename OctreeSemantics> class TOctree;

/** Data used to track a primitive's allocation in the volume texture atlas that stores indirect lighting. */
class FIndirectLightingCacheAllocation
{
public:

	FIndirectLightingCacheAllocation() :
		Add(FVector(0, 0, 0)),
		Scale(FVector(0, 0, 0)),
		MinUV(FVector(0, 0, 0)),
		MaxUV(FVector(0, 0, 0)),
		MinTexel(FIntVector(-1, -1, -1)),
		AllocationTexelSize(0),
		TargetPosition(FVector(0, 0, 0)),
		TargetDirectionalShadowing(1),
		TargetSkyBentNormal(FVector4(0, 0, 1, 1)),
		SingleSamplePosition(FVector(0, 0, 0)),
		CurrentDirectionalShadowing(1),
		CurrentSkyBentNormal(FVector4(0, 0, 1, 1)),
		bHasEverUpdatedSingleSample(false),
		bPointSample(true),
		bIsDirty(false),
		bUnbuiltPreview(false)
	{
		for (int32 VectorIndex = 0; VectorIndex < 3; VectorIndex++) // RGB
		{
			TargetSamplePacked0[VectorIndex] = FVector4(0, 0, 0, 0);
			SingleSamplePacked0[VectorIndex] = FVector4(0, 0, 0, 0);
			TargetSamplePacked1[VectorIndex] = FVector4(0, 0, 0, 0);
			SingleSamplePacked1[VectorIndex] = FVector4(0, 0, 0, 0);
		}
		TargetSamplePacked2 = FVector4(0, 0, 0, 0);
		SingleSamplePacked2 = FVector4(0, 0, 0, 0);
	}

	/** Add factor for calculating UVs from position. */
	FVector Add;

	/** Scale factor for calculating UVs from position. */
	FVector Scale;

	/** Used to clamp lookup UV to a valid range for pixels outside the object's bounding box. */
	FVector MinUV;

	/** Used to clamp lookup UV to a valid range for pixels outside the object's bounding box. */
	FVector MaxUV;

	/** Block index in the volume texture atlas, can represent unallocated. */
	FIntVector MinTexel;

	/** Size in texels of the allocation into the volume texture atlas. */
	int32 AllocationTexelSize;

	/** Position at the new single lighting sample. Used for interpolation over time. */
	FVector TargetPosition;

	/** SH sample at the new single lighting sample position. Used for interpolation over time. */
	FVector4 TargetSamplePacked0[3];	// { { R.C0, R.C1, R.C2, R.C3 }, { G.C0, G.C1, G.C2, G.C3 }, { B.C0, B.C1, B.C2, B.C3 } }
	FVector4 TargetSamplePacked1[3];	// { { R.C4, R.C5, R.C6, R.C7 }, { G.C4, G.C5, G.C6, G.C7 }, { B.C4, B.C5, B.C6, B.C7 } }
	FVector4 TargetSamplePacked2;		// { R.C8, R.C8, R.C8, R.C8 }

	/** Target shadowing of the stationary directional light. */
	float TargetDirectionalShadowing;

	/** Target directional occlusion of the sky. */
	FVector4 TargetSkyBentNormal;

	/** Current position of the single lighting sample.  Used for interpolation over time. */
	FVector SingleSamplePosition;

	/** Current SH sample used when lighting the entire object with one sample. */
	FVector4 SingleSamplePacked0[3];	// { { R.C0, R.C1, R.C2, R.C3 }, { G.C0, G.C1, G.C2, G.C3 }, { B.C0, B.C1, B.C2, B.C3 } }
	FVector4 SingleSamplePacked1[3];	// { { R.C4, R.C5, R.C6, R.C7 }, { G.C4, G.C5, G.C6, G.C7 }, { B.C4, B.C5, B.C6, B.C7 } }
	FVector4 SingleSamplePacked2;		// { R.C8, R.C8, R.C8, R.C8 }

	/** Current shadowing of the stationary directional light. */
	float CurrentDirectionalShadowing;

	/** Current directional occlusion of the sky. */
	FVector4 CurrentSkyBentNormal;

	/** Whether SingleSamplePacked has ever been populated with valid results, used to initialize. */
	bool bHasEverUpdatedSingleSample;

	/** Whether this allocation is a point sample and therefore was not put into the volume texture atlas. */
	bool bPointSample;

	/** Whether the primitive allocation is dirty and should be updated regardless of having moved. */
	bool bIsDirty;

	bool bUnbuiltPreview;

	void SetDirty() 
	{ 
		bIsDirty = true; 
	}

	bool IsValid() const
	{
		return MinTexel.X >= 0 && MinTexel.Y >= 0 && MinTexel.Z >= 0 && AllocationTexelSize > 0;
	}

	void SetParameters(FIntVector InMinTexel, int32 InAllocationTexelSize, FVector InScale, FVector InAdd, FVector InMinUV, FVector InMaxUV, bool bInPointSample, bool bInUnbuiltPreview)
	{
		checkf(InAllocationTexelSize > 1 || bInPointSample, TEXT("%i, %i"), InAllocationTexelSize, bInPointSample ? 1 : 0);
		Add = InAdd;
		Scale = InScale;
		MinUV = InMinUV;
		MaxUV = InMaxUV;
		MinTexel = InMinTexel;
		AllocationTexelSize = InAllocationTexelSize;
		bIsDirty = false;
		bPointSample = bInPointSample;
		bUnbuiltPreview = bInUnbuiltPreview;
	}
};

/** Flags needed for shadow culling.  These are pulled out of the FPrimitiveSceneProxy so we can do rough culling before dereferencing the proxy. */
struct FPrimitiveFlagsCompact
{
	/** True if the primitive casts dynamic shadows. */
	uint32 bCastDynamicShadow : 1;
	/** True if the primitive will cache static lighting. */
	uint32 bStaticLighting : 1;
	/** True if the primitive casts static shadows. */
	uint32 bCastStaticShadow : 1;

	FPrimitiveFlagsCompact(const FPrimitiveSceneProxy* Proxy);
};

/** The information needed to determine whether a primitive is visible. */
class FPrimitiveSceneInfoCompact
{
public:
	FPrimitiveSceneInfo* PrimitiveSceneInfo;
	FPrimitiveSceneProxy* Proxy;
	FBoxSphereBounds Bounds;
	float MinDrawDistance;
	float MaxDrawDistance;
	/** Used for precomputed visibility */
	int32 VisibilityId;
	FPrimitiveFlagsCompact PrimitiveFlagsCompact;

	/** Initialization constructor. */
	FPrimitiveSceneInfoCompact(FPrimitiveSceneInfo* InPrimitiveSceneInfo);
};

/** The type of the octree used by FScene to find primitives. */
typedef TOctree<FPrimitiveSceneInfoCompact,struct FPrimitiveOctreeSemantics> FScenePrimitiveOctree;

/**
 * The renderer's internal state for a single UPrimitiveComponent.  This has a one to one mapping with FPrimitiveSceneProxy, which is in the engine module.
 */
class FPrimitiveSceneInfo : public FDeferredCleanupInterface
{
public:

	/** The render proxy for the primitive. */
	FPrimitiveSceneProxy* Proxy;

	/** 
	 * Id for the component this primitive belongs to.  
	 * This will stay the same for the lifetime of the component, so it can be used to identify the component across re-registers.
	 */
	FPrimitiveComponentId PrimitiveComponentId;

	/** 
	 * Pointer to the primitive's last render time variable, which is written to by the RT and read by the GT.
	 * The value of LastRenderTime will therefore not be deterministic due to race conditions, but the GT uses it in a way that allows this.
	 * Storing a pointer to the UObject member variable only works because UPrimitiveComponent has a mechanism to ensure it does not get deleted before the proxy (DetachFence).
	 * In general feedback from the renderer to the game thread like this should be avoided.
	 */
	float* ComponentLastRenderTime;

	/** Same as ComponentLastRenderTime but only updated if the component is on screen. Used by the texture streamer. */
	float* ComponentLastRenderTimeOnScreen;

	/** 
	 * The root attachment component id for use with lighting, if valid.
	 * If the root id is not valid, this is a parent primitive.
	 */
	FPrimitiveComponentId LightingAttachmentRoot;

	/** 
	 * The component id of the LOD parent if valid.
	 */
	FPrimitiveComponentId LODParentComponentId;

	/** The primitive's static meshes. */
	TIndirectArray<class FStaticMesh> StaticMeshes;

	/** The identifier for the primitive in Scene->PrimitiveOctree. */
	FOctreeElementId OctreeId;

	/** 
	 * Caches the primitive's indirect lighting cache allocation.
	 * Note: This is only valid during the rendering of a frame, not just once the primitive is attached. 
	 */
	const FIndirectLightingCacheAllocation* IndirectLightingCacheAllocation;

	/** 
	 * The uniform buffer holding precomputed lighting parameters for the indirect lighting cache allocation.
	 * WARNING : This can hold buffer valid for a single frame only, don't cache anywhere. 
	 * See FPrimitiveSceneInfo::UpdatePrecomputedLightingBuffer()
	 */
	FUniformBufferRHIRef IndirectLightingCacheUniformBuffer;

	/** 
	 * Planar reflection that was closest to this primitive, used for forward reflections.
	 */
	const class FPlanarReflectionSceneProxy* CachedPlanarReflectionProxy;

	/** 
	 * Reflection capture proxy that was closest to this primitive, used for the forward shading rendering path. 
	 */
	const FReflectionCaptureProxy* CachedReflectionCaptureProxy;

	/** Mapping from instance index in this primitive to index in the global distance field object buffers. */
	TArray<int32, TInlineAllocator<1>> DistanceFieldInstanceIndices;

	/** Whether the primitive is newly registered or moved and CachedReflectionCaptureProxy needs to be updated on the next render. */
	uint32 bNeedsCachedReflectionCaptureUpdate : 1;

	static const uint32 MaxCachedReflectionCaptureProxies = 3;
	const FReflectionCaptureProxy* CachedReflectionCaptureProxies[MaxCachedReflectionCaptureProxies];
	
	/** The hit proxies used by the primitive. */
	TArray<TRefCountPtr<HHitProxy> > HitProxies;

	/** The hit proxy which is used to represent the primitive's dynamic elements. */
	HHitProxy* DefaultDynamicHitProxy;

	/** The ID of the hit proxy which is used to represent the primitive's dynamic elements. */
	FHitProxyId DefaultDynamicHitProxyId;

	/** The list of lights affecting this primitive. */
	class FLightPrimitiveInteraction* LightList;

	/** Last render time in seconds since level started play. */
	float LastRenderTime;

	/** Last time that the primitive became visible in seconds since level started play. */
	float LastVisibilityChangeTime;

	/** The scene the primitive is in. */
	FScene* Scene;

	/** The number of dynamic point lights for ES2 */
	int32 NumES2DynamicPointLights;

	// NVCHANGE_BEGIN: Add VXGI
	int32 VxgiLastVoxelizationPass;
	int32 VoxelizationOnlyMeshStartIdx;
	// NVCHANGE_END: Add VXGI

	/** Initialization constructor. */
	FPrimitiveSceneInfo(UPrimitiveComponent* InPrimitive,FScene* InScene);

	/** Destructor. */
	~FPrimitiveSceneInfo();

	/** Adds the primitive to the scene. */
	void AddToScene(FRHICommandListImmediate& RHICmdList, bool bUpdateStaticDrawLists);

	/** Removes the primitive from the scene. */
	void RemoveFromScene(bool bUpdateStaticDrawLists);

	/** return true if we need to call ConditionalUpdateStaticMeshes */
	FORCEINLINE bool NeedsUpdateStaticMeshes()
	{
		return bNeedsStaticMeshUpdate;
	}

	/** return true if we need to call LazyUpdateForRendering */
	FORCEINLINE bool NeedsUniformBufferUpdate() const
	{
		return bNeedsUniformBufferUpdate;
	}

	/** return true if we need to call LazyUpdateForRendering */
	FORCEINLINE bool NeedsPrecomputedLightingBufferUpdate()
	{
		return bPrecomputedLightingBufferDirty;
	}

	/** return true if we need to call ConditionalLazyUpdateForRendering */
	FORCEINLINE bool NeedsLazyUpdateForRendering()
	{
		return NeedsUniformBufferUpdate() || NeedsUpdateStaticMeshes();
	}

	/** Updates the primitive's static meshes in the scene. */
	void UpdateStaticMeshes(FRHICommandListImmediate& RHICmdList);

	/** Updates the primitive's static meshes in the scene. */
	FORCEINLINE void ConditionalUpdateStaticMeshes(FRHICommandListImmediate& RHICmdList)
	{
		if (NeedsUpdateStaticMeshes())
		{
			UpdateStaticMeshes(RHICmdList);
		}
	}

	/** Updates the primitive's uniform buffer. */
	void UpdateUniformBuffer(FRHICommandListImmediate& RHICmdList);

	/** Updates the primitive's uniform buffer. */
	FORCEINLINE void ConditionalUpdateUniformBuffer(FRHICommandListImmediate& RHICmdList)
	{
		if (NeedsUniformBufferUpdate())
		{
			UpdateUniformBuffer(RHICmdList);
		}
	}

	/** Updates all lazy data for the rendering. */
	FORCEINLINE void ConditionalLazyUpdateForRendering(FRHICommandListImmediate& RHICmdList)
	{
		ConditionalUpdateUniformBuffer(RHICmdList);
		ConditionalUpdateStaticMeshes(RHICmdList);
	}

	/** Sets a flag to update the primitive's static meshes before it is next rendered. */
	void BeginDeferredUpdateStaticMeshes();

	/** Adds the primitive's static meshes to the scene. */
	void AddStaticMeshes(FRHICommandListImmediate& RHICmdList);

	/** Removes the primitive's static meshes from the scene. */
	void RemoveStaticMeshes();

	/** Set LOD Parent primitive information to the scene. */
	void LinkLODParentComponent();

	/** clear LOD parent primitive information from the scene. */
	void UnlinkLODParentComponent();

	/** Adds the primitive to the scene's attachment groups. */
	void LinkAttachmentGroup();

	/** Removes the primitive from the scene's attachment groups. */
	void UnlinkAttachmentGroup();

	/** 
	 * Builds an array of all primitive scene info's in this primitive's attachment group. 
	 * This only works on potential parents (!LightingAttachmentRoot.IsValid()) and will include the current primitive in the output array.
	 */
	void GatherLightingAttachmentGroupPrimitives(TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos);
	void GatherLightingAttachmentGroupPrimitives(TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos) const;

	/** 
	 * Builds a cumulative bounding box of this primitive and all the primitives in the same attachment group. 
	 * This only works on potential parents (!LightingAttachmentRoot.IsValid()).
	 */
	FBoxSphereBounds GetAttachmentGroupBounds() const;

	/** Size this class uses in bytes */
	uint32 GetMemoryFootprint();

	// FDeferredCleanupInterface implementation.
	virtual void FinishCleanup() { delete this; }

	/** 
	 * Retrieves the index of the primitive in the scene's primitives array.
	 * This index is only valid until a primitive is added to or removed from
	 * the scene!
	 */
	inline int32 GetIndex() const { return PackedIndex; }
	/** 
	 * Retrieves the address of the primitives index into in the scene's primitives array.
	 * This address is only for reference purposes
	 */
	inline const int32* GetIndexAddress() const { return &PackedIndex; }

	/* @return true if the object needs to be rendered in the velocity pass (is not moving like the world, needed for motionblur and TemporalAA) */
	bool ShouldRenderVelocity(const FViewInfo& View, bool bCheckVisibility = true) const;
	
	/**
	 * Shifts primitive position and all relevant data by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	void ApplyWorldOffset(FVector InOffset);

	FORCEINLINE void SetNeedsUniformBufferUpdate(bool bInNeedsUniformBufferUpdate)
	{
		bNeedsUniformBufferUpdate = bInNeedsUniformBufferUpdate;
	}

	FORCEINLINE void MarkPrecomputedLightingBufferDirty()
	{
		bPrecomputedLightingBufferDirty = true;
	}

	void UpdatePrecomputedLightingBuffer();
	void ClearPrecomputedLightingBuffer(bool bSingleFrameOnly);

private:

	/** Let FScene have direct access to the Id. */
	friend class FScene;

	/**
	 * The index of the primitive in the scene's packed arrays. This value may
	 * change as primitives are added and removed from the scene.
	 */
	int32 PackedIndex;

	/** 
	 * The UPrimitiveComponent this scene info is for, useful for quickly inspecting properties on the corresponding component while debugging.
	 * This should not be dereferenced on the rendering thread.  The game thread can be modifying UObject members at any time.
	 * Use PrimitiveComponentId instead when a component identifier is needed.
	 */
	const UPrimitiveComponent* ComponentForDebuggingOnly;

	/** If this is TRUE, this primitive's static meshes needs to be updated before it can be rendered. */
	bool bNeedsStaticMeshUpdate;

	/** If this is TRUE, this primitive's uniform buffer needs to be updated before it can be rendered. */
	bool bNeedsUniformBufferUpdate;

	/** If this is TRUE, this primitive's precomputed lighting buffer needs to be updated before it can be rendered. */
	bool bPrecomputedLightingBufferDirty;
};

/** Defines how the primitive is stored in the scene's primitive octree. */
struct FPrimitiveOctreeSemantics
{
	/** Note: this is coupled to shadow gather task granularity, see r.ParallelGatherShadowPrimitives. */
	enum { MaxElementsPerLeaf = 256 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPrimitiveSceneInfoCompact& PrimitiveSceneInfoCompact)
	{
		return PrimitiveSceneInfoCompact.Bounds;
	}

	FORCEINLINE static bool AreElementsEqual(const FPrimitiveSceneInfoCompact& A,const FPrimitiveSceneInfoCompact& B)
	{
		return A.PrimitiveSceneInfo == B.PrimitiveSceneInfo;
	}

	FORCEINLINE static void SetElementId(const FPrimitiveSceneInfoCompact& Element,FOctreeElementId Id)
	{
		Element.PrimitiveSceneInfo->OctreeId = Id;
	}

	FORCEINLINE static void ApplyOffset(FPrimitiveSceneInfoCompact& Element, FVector Offset)
	{
		Element.Bounds.Origin+= Offset;
	}
};

