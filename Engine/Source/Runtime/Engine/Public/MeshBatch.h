// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UniformBuffer.h"
#include "HitProxies.h"
#include "MaterialShared.h"
#include "Engine/Scene.h"
#include "PrimitiveUniformShaderParameters.h"

class FLightCacheInterface;

/**
 * A batch mesh element definition.
 */
struct FMeshBatchElement
{
	/** Primitive uniform buffer to use for rendering. */
	const TUniformBuffer<FPrimitiveUniformShaderParameters>* PrimitiveUniformBufferResource;

	/** Used for lifetime management of a temporary uniform buffer, can be NULL. */
	TUniformBufferRef<FPrimitiveUniformShaderParameters> PrimitiveUniformBuffer;

	const FIndexBuffer* IndexBuffer;

	union 
	{
		/** If bIsSplineProxy, Instance runs, where number of runs is specified by NumInstances.  Run structure is [StartInstanceIndex, EndInstanceIndex]. */
		uint32* InstanceRuns;
		/** If bIsSplineProxy, a pointer back to the proxy */
		class FSplineMeshSceneProxy* SplineMeshSceneProxy;
	};
	const void* UserData;
	/**
	 *	DynamicIndexData - pointer to user memory containing the index data.
	 *	Used for rendering dynamic data directly.
	 */
	const void* DynamicIndexData;
	uint32 FirstIndex;
	uint32 NumPrimitives;

	/** Number of instances to draw.  If InstanceRuns is valid, this is actually the number of runs in InstanceRuns. */
	uint32 NumInstances;
	uint32 MinVertexIndex;
	uint32 MaxVertexIndex;
	// Meaning depends on the vertex factory, e.g. FGPUSkinPassthroughVertexFactory: element index in FGPUSkinCache::CachedElements
	union
	{
		void* VertexFactoryUserData;
		int32 UserIndex;
	};
	float MinScreenSize;
	float MaxScreenSize;

	uint16 DynamicIndexStride;
	uint8 InstancedLODIndex : 4;
	uint8 InstancedLODRange : 4;
	uint8 bUserDataIsColorVertexBuffer : 1;
	uint8 bIsInstancedMesh : 1;
	uint8 bIsSplineProxy : 1;
	uint8 bIsInstanceRuns : 1;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Conceptual element index used for debug viewmodes. */
	int8 VisualizeElementIndex;
#endif

	FMeshBatchElement()
	:	PrimitiveUniformBufferResource(nullptr)
	,	IndexBuffer(nullptr)
	,	InstanceRuns(nullptr)
	,	UserData(nullptr)
	,	DynamicIndexData(nullptr)
	,	NumInstances(1)
	,	UserIndex(-1)
	,	MinScreenSize(0.0f)
	,	MaxScreenSize(1.0f)
	,	InstancedLODIndex(0)
	,	InstancedLODRange(0)
	,	bUserDataIsColorVertexBuffer(false)
	,   bIsInstancedMesh(false)
	,	bIsSplineProxy(false)
	,	bIsInstanceRuns(false)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	,	VisualizeElementIndex(INDEX_NONE)
#endif

	{
	}
};

/**
 * A batch of mesh elements, all with the same material and vertex buffer
 */
struct FMeshBatch
{
	TArray<FMeshBatchElement,TInlineAllocator<1> > Elements;

	// used with DynamicVertexData
	uint16 DynamicVertexStride;

	/** LOD index of the mesh, used for fading LOD transitions. */
	int8 LODIndex;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Conceptual LOD index used for the LOD Coloration visualization. */
	int8 VisualizeLODIndex;
#endif

	/** Conceptual HLOD index used for the HLOD Coloration visualization. */
	int8 VisualizeHLODIndex;

	uint32 UseDynamicData : 1;
	uint32 ReverseCulling : 1;
	uint32 bDisableBackfaceCulling : 1;
	uint32 CastShadow : 1;				// Wheter it can be used in shadow renderpasses.
	uint32 bUseForMaterial : 1;	// Whether it can be used in renderpasses requiring material outputs.
	uint32 bUseAsOccluder : 1;			// Whether it can be used in renderpasses only depending on the raw geometry (i.e. Depth Prepass).
	uint32 bWireframe : 1;
	// e.g. PT_TriangleList(default), PT_LineList, ..
	uint32 Type : PT_NumBits;
	// e.g. SDPG_World (default), SDPG_Foreground
	uint32 DepthPriorityGroup : SDPG_NumBits;

	/** Whether view mode overrides can be applied to this mesh eg unlit, wireframe. */
	uint32 bCanApplyViewModeOverrides : 1;

	/** 
	 * Whether to treat the batch as selected in special viewmodes like wireframe. 
	 * This is needed instead of just Proxy->IsSelected() because some proxies do weird things with selection highlighting, like FStaticMeshSceneProxy.
	 */
	uint32 bUseWireframeSelectionColoring : 1;

	/** 
	 * Whether the batch should receive the selection outline.  
	 * This is useful for proxies which support selection on a per-mesh batch basis.
	 * They submit multiple mesh batches when selected, some of which have bUseSelectionOutline enabled.
	 */
	uint32 bUseSelectionOutline : 1;

	/** Whether the mesh batch can be selected through editor selection, aka hit proxies. */
	uint32 bSelectable : 1;

	/** Whether the mesh batch needs VertexFactory->GetStaticBatchElementVisibility to be called each frame to determine which elements of the batch are visible. */
	uint32 bRequiresPerElementVisibility : 1;
	
	/** Whether the mesh batch should apply dithered LOD. */
	uint32 bDitheredLODTransition : 1;

	/** If bDitheredLODTransition and this is a dynamic mesh element, then this is the alpha for dither fade (static draw lists need to derive this later as it is changes every frame) */
	float DitheredLODTransitionAlpha;

	// can be NULL
	const FLightCacheInterface* LCI;

	/** Whether the mesh batch should be rendered. */
	uint32 bRenderable : 1;

	/** 
	 *	DynamicVertexData - pointer to user memory containing the vertex data.
	 *	Used for rendering dynamic data directly.
	 *  used with DynamicVertexStride
	 */
	const void* DynamicVertexData;

	/** Vertex factory for rendering, required. */
	const FVertexFactory* VertexFactory;

	/** Material proxy for rendering, required. */
	const FMaterialRenderProxy* MaterialRenderProxy;

	/** The current hit proxy ID being rendered. */
	FHitProxyId BatchHitProxyId;

	FORCEINLINE bool IsTranslucent(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		// Note: blend mode does not depend on the feature level we are actually rendering in.
		return IsTranslucentBlendMode(MaterialRenderProxy->GetMaterial(InFeatureLevel)->GetBlendMode());
	}

	// todo: can be optimized with a single function that returns multiple states (Translucent, Decal, Masked) 
	FORCEINLINE bool IsDecal(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		// Note: does not depend on the feature level we are actually rendering in.
		const FMaterial* Mat = MaterialRenderProxy->GetMaterial(InFeatureLevel);

		return Mat->IsDeferredDecal();
	}

	FORCEINLINE bool IsMasked(ERHIFeatureLevel::Type InFeatureLevel) const
	{
		// Note: blend mode does not depend on the feature level we are actually rendering in.
		return MaterialRenderProxy->GetMaterial(InFeatureLevel)->IsMasked();
	}

	/** Converts from an int32 index into a int8 */
	static int8 QuantizeLODIndex(int32 NewLODIndex)
	{
		checkSlow(NewLODIndex >= SCHAR_MIN && NewLODIndex <= SCHAR_MAX);
		return (int8)NewLODIndex;
	}

	/** 
	* @return vertex stride specified for the mesh. 0 if not dynamic
	*/
	FORCEINLINE uint32 GetDynamicVertexStride(ERHIFeatureLevel::Type /*InFeatureLevel*/) const
	{
		if (UseDynamicData && DynamicVertexData)
		{
			return DynamicVertexStride;
		}
		else
		{
			return 0;
		}
	}

	FORCEINLINE int32 GetNumPrimitives() const
	{
		int32 Count=0;
		for( int32 ElementIdx=0;ElementIdx<Elements.Num();ElementIdx++ )
		{
			if (Elements[ElementIdx].bIsInstanceRuns && Elements[ElementIdx].InstanceRuns)
			{
				for (uint32 Run = 0; Run < Elements[ElementIdx].NumInstances; Run++)
				{
					Count += Elements[ElementIdx].NumPrimitives * (Elements[ElementIdx].InstanceRuns[Run * 2 + 1] - Elements[ElementIdx].InstanceRuns[Run * 2] + 1);
				}
			}
			else
			{
				Count += Elements[ElementIdx].NumPrimitives * Elements[ElementIdx].NumInstances;
			}
		}
		return Count;
	}

#if DO_CHECK
	FORCEINLINE void CheckUniformBuffers() const
	{
		for( int32 ElementIdx=0;ElementIdx<Elements.Num();ElementIdx++ )
		{
			check(IsValidRef(Elements[ElementIdx].PrimitiveUniformBuffer) || Elements[ElementIdx].PrimitiveUniformBufferResource != NULL);
		}
	}
#endif	

	/** Default constructor. */
	FMeshBatch()
	:	DynamicVertexStride(0)
	,	LODIndex(INDEX_NONE)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	,	VisualizeLODIndex(INDEX_NONE)
#endif
	,	VisualizeHLODIndex(INDEX_NONE)
	,	UseDynamicData(false)
	,	ReverseCulling(false)
	,	bDisableBackfaceCulling(false)
	,	CastShadow(true)
	,   bUseForMaterial(true)
	,	bUseAsOccluder(true)
	,	bWireframe(false)
	,	Type(PT_TriangleList)
	,	DepthPriorityGroup(SDPG_World)
	,	bCanApplyViewModeOverrides(false)
	,	bUseWireframeSelectionColoring(false)
	,	bUseSelectionOutline(true)
	,	bSelectable(true)
	,	bRequiresPerElementVisibility(false)
	,	bDitheredLODTransition(false)
	,   DitheredLODTransitionAlpha(0.0f)
	,	LCI(NULL)
	,	DynamicVertexData(NULL)
	,	VertexFactory(NULL)
	,	MaterialRenderProxy(NULL)
	,	bRenderable(true)
	{
		// By default always add the first element.
		new(Elements) FMeshBatchElement;
	}
};
