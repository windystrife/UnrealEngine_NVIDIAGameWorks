// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshDrawList.h: Static mesh draw list definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "EngineDefines.h"
#include "RendererInterface.h"
#include "ScenePrivateBase.h"
#include "SceneCore.h"
#include "SceneRendering.h"

extern ENGINE_API bool GDrawListsLocked;
/** View state for instanced stereo rendering. */
struct StereoPair
{
	StereoPair() :
		LeftView(nullptr), 
		RightView(nullptr),
		LeftViewVisibilityMap(nullptr),
		RightViewVisibilityMap(nullptr),
		LeftViewBatchVisibilityArray(nullptr),
		RightViewBatchVisibilityArray(nullptr)
	{}

	StereoPair(
		const FViewInfo& InLeftView,
		const FViewInfo& InRightView,
		const TBitArray<SceneRenderingBitArrayAllocator>& InLeftViewVisibilityMap,
		const TBitArray<SceneRenderingBitArrayAllocator>& InRightViewVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>& InLeftViewBatchVisibilityArray,
		const TArray<uint64, SceneRenderingAllocator>& InRightViewBatchVisibilityArray
	) :
		LeftView(&InLeftView), 
		RightView(&InRightView),
		LeftViewVisibilityMap(&InLeftViewVisibilityMap),
		RightViewVisibilityMap(&InRightViewVisibilityMap),
		LeftViewBatchVisibilityArray(&InLeftViewBatchVisibilityArray),
		RightViewBatchVisibilityArray(&InRightViewBatchVisibilityArray)
	{}

	const FViewInfo* LeftView;
	const FViewInfo* RightView;
	const TBitArray<SceneRenderingBitArrayAllocator>* LeftViewVisibilityMap;
	const TBitArray<SceneRenderingBitArrayAllocator>* RightViewVisibilityMap;
	const TArray<uint64, SceneRenderingAllocator>* LeftViewBatchVisibilityArray;
	const TArray<uint64, SceneRenderingAllocator>* RightViewBatchVisibilityArray;
};

/** Used to statically branch between regular rendering and instanced stereo rendering. */
enum class InstancedStereoPolicy {
	Enabled, 
	MobileMultiView,
	Disabled
};

/** Base class of the static draw list, used when comparing draw lists and the drawing policy type is not necessary. */
class FStaticMeshDrawListBase
{
public:

	static SIZE_T TotalBytesUsed;
};

/**
 * Statistics for a static mesh draw list.
 */
struct FDrawListStats
{
	int32 NumMeshes;
	int32 NumDrawingPolicies;
	int32 MedianMeshesPerDrawingPolicy;
	int32 MaxMeshesPerDrawingPolicy;
	int32 NumSingleMeshDrawingPolicies;
	TMap<FString, int32> SingleMeshPolicyMatchFailedReasons;
	TMap<FName, int32> SingleMeshPolicyVertexFactoryFrequency;
};

/** Fields in the key used to sort mesh elements in a draw list. */
struct FDrawListSortKeyFields
{
	uint64 MeshElementIndex : 16;
	uint64 DepthBits : 16;
	uint64 DrawingPolicyIndex : 16;
	uint64 DrawingPolicyDepthBits : 15;
	uint64 bBackground : 1;
};

/** Key for sorting mesh elements. */
union FDrawListSortKey
{
	FDrawListSortKeyFields Fields;
	uint64 PackedInt;
};

FORCEINLINE bool operator<(FDrawListSortKey A, FDrawListSortKey B)
{
	return A.PackedInt < B.PackedInt;
}

/** Builds a sort key. */
inline FDrawListSortKey GetSortKey(bool bBackground, float BoundsRadius, float DrawingPolicyDistance, int32 DrawingPolicyIndex, float Distance, int32 MeshElementIndex)
{
	union FFloatToInt { float F; uint32 I; };
	FFloatToInt F2I;

	FDrawListSortKey Key;
	Key.Fields.bBackground = bBackground || BoundsRadius > HALF_WORLD_MAX/4.0f;
	F2I.F = Distance;
	Key.Fields.DrawingPolicyDepthBits = ((-int32(F2I.I >> 31) | 0x80000000) ^ F2I.I) >> 17;
	Key.Fields.DrawingPolicyIndex = DrawingPolicyIndex;
	F2I.F = Distance;
	Key.Fields.DepthBits = ((-int32(F2I.I >> 31) | 0x80000000) ^ F2I.I) >> 16;
	Key.Fields.MeshElementIndex = MeshElementIndex;
	return Key;
}

/**
 * A set of static meshs, each associated with a mesh drawing policy of a particular type.
 * @param DrawingPolicyType - The drawing policy type used to draw mesh in this draw list.
 * @param HashSize - The number of buckets to use in the drawing policy hash.
 */
template<typename DrawingPolicyType>
class FDrawVisibleAnyThreadTask;

template<typename DrawingPolicyType>
class TStaticMeshDrawList : public FStaticMeshDrawListBase, public FRenderResource
{
public:
	friend class FDrawVisibleAnyThreadTask<DrawingPolicyType>;

	typedef typename DrawingPolicyType::ElementDataType ElementPolicyDataType;

private:

	/** A handle to an element in the draw list.  Used by FStaticMesh to keep track of draw lists containing the mesh. */
	class FElementHandle : public FStaticMesh::FDrawListElementLink
	{
	public:

		/** Initialization constructor. */
		FElementHandle(TStaticMeshDrawList* InStaticMeshDrawList, FSetElementId InSetId, int32 InElementIndex) :
			StaticMeshDrawList(InStaticMeshDrawList)
			, SetId(InSetId)
			, ElementIndex(InElementIndex)
		{
		}

		virtual bool IsInDrawList(const FStaticMeshDrawListBase* DrawList) const
		{
			return DrawList == StaticMeshDrawList;
		}
		// FAbstractDrawListElementLink interface.
		virtual void Remove(const bool bUnlinkMesh = true );

	private:
		TStaticMeshDrawList* StaticMeshDrawList;
		FSetElementId SetId;
		int32 ElementIndex;
	};

	/**
	 * This structure stores the info needed for visibility culling a static mesh element.
	 * Stored separately to avoid bringing the other info about non-visible meshes into the cache.
	 */
	struct FElementCompact
	{
		int32 MeshId;
		FElementCompact() {}
		FElementCompact(int32 InMeshId)
			: MeshId(InMeshId)
		{}
	};

	struct FElement
	{
		ElementPolicyDataType PolicyData;
		FStaticMesh* Mesh;
		FBoxSphereBounds Bounds;
		bool bBackground;
		TRefCountPtr<FElementHandle> Handle;

		/** Default constructor. */
		FElement() :
			Mesh(NULL)
		{}

		/** Minimal initialization constructor. */
		FElement(
			FStaticMesh* InMesh,
			const ElementPolicyDataType& InPolicyData,
			TStaticMeshDrawList* StaticMeshDrawList,
			FSetElementId SetId,
			int32 ElementIndex
		) :
			PolicyData(InPolicyData),
			Mesh(InMesh),
			Handle(new FElementHandle(StaticMeshDrawList, SetId, ElementIndex))
		{
			// Cache bounds so we can use them for sorting quickly, without having to dereference the proxy
			Bounds = Mesh->PrimitiveSceneInfo->Proxy->GetBounds();
			bBackground = Mesh->PrimitiveSceneInfo->Proxy->TreatAsBackgroundForOcclusion();
		}

		/** Destructor. */
		~FElement()
		{
			if (Mesh)
			{
				Mesh->UnlinkDrawList(Handle);
			}
		}
	};

	/** A set of draw list elements with the same drawing policy. */
	struct FDrawingPolicyLink
	{
		/** The elements array and the compact elements array are always synchronized */
		TArray<FElementCompact>		 CompactElements;
		TArray<FElement>			 Elements;
		DrawingPolicyType		 	 DrawingPolicy;
		FBoundShaderStateInput		 BoundShaderStateInput;
		ERHIFeatureLevel::Type		 FeatureLevel;

		/** Used when sorting policy links */
		FSphere						 CachedBoundingSphere;

		/** The id of this link in the draw list's set of drawing policy links. */
		FSetElementId SetId;

		TStaticMeshDrawList* DrawList;

		uint32 VisibleCount;

		/** Initialization constructor. */
		FDrawingPolicyLink(TStaticMeshDrawList* InDrawList, const DrawingPolicyType& InDrawingPolicy, ERHIFeatureLevel::Type InFeatureLevel) :
			DrawingPolicy(InDrawingPolicy),
			FeatureLevel(InFeatureLevel),
			DrawList(InDrawList),
			VisibleCount(0)
		{
			check(IsInRenderingThread());
			BoundShaderStateInput = DrawingPolicy.GetBoundShaderStateInput(FeatureLevel);
		}

		SIZE_T GetSizeBytes() const
		{
			return sizeof(*this) + CompactElements.GetAllocatedSize() + Elements.GetAllocatedSize();
		}
	};

	/** Functions to extract the drawing policy from FDrawingPolicyLink as a key for TSet. */
	struct FDrawingPolicyKeyFuncs : BaseKeyFuncs<FDrawingPolicyLink, DrawingPolicyType>
	{
		static const DrawingPolicyType& GetSetKey(const FDrawingPolicyLink& Link)
		{
			return Link.DrawingPolicy;
		}

		static bool Matches(const DrawingPolicyType& A, const DrawingPolicyType& B)
		{
			return A.Matches(B).Result();
		}

		static uint32 GetKeyHash(const DrawingPolicyType& DrawingPolicy)
		{
			return DrawingPolicy.GetTypeHash();
		}
	};

	/**
	* Draws a single FElement
	* @param View - The view of the meshes to render.
	* @param Element - The mesh element
	* @param BatchElementMask - Visibility bitmask for element's batch elements.
	* @param DrawingPolicyLink - the drawing policy link
	* @param bDrawnShared - determines whether to draw shared
	*/
	template<InstancedStereoPolicy InstancedStereo>
	int32 DrawElement(FRHICommandList& RHICmdList, const FViewInfo& View, const typename DrawingPolicyType::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState, const FElement& Element, uint64 BatchElementMask, FDrawingPolicyLink* DrawingPolicyLink, bool &bDrawnShared);

public:

	/**
	 * Adds a mesh to the draw list.
	 * @param Mesh - The mesh to add.
	 * @param PolicyData - The drawing policy data for the mesh.
	 * @param InDrawingPolicy - The drawing policy to use to draw the mesh.
	 * @param InFeatureLevel - The feature level of the scene we're rendering
	 */
	void AddMesh(
		FStaticMesh* Mesh,
		const ElementPolicyDataType& PolicyData,
		const DrawingPolicyType& InDrawingPolicy,
		ERHIFeatureLevel::Type InFeatureLevel
	);

	/**
	* Draws only the static meshes which are in the visibility map, limited to a range of policies
	* Both StaticMeshVisibilityMap and BatchVisibilityArray should be non-null for regular rendering or StereoView should be non-null if rendering with instanced stereo
	* @param View - The view of the meshes to render (use the left view of a stereo pair when rendering with instanced stereo)
	* @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	* @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	* @param StereoView - Stereo pair to render
	* @param FirstPolicy - First policy to render
	* @param LastPolicy - Last policy to render
	* @return True if any static meshes were drawn.
	*/
	template<InstancedStereoPolicy InstancedStereo>
	bool DrawVisibleInner(FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		FDrawingPolicyRenderState& DrawRenderState,
		const TBitArray<SceneRenderingBitArrayAllocator>* const StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>* const BatchVisibilityArray,
		const StereoPair* const StereoView,
		int32 FirstPolicy,
		int32 LastPolicy,
		bool bUpdateCounts);

	/**
	* Draws only the static meshes which are in the visibility map of the stereo pair
	* @param StereoView - The stereo pair to render.
	* @return True if any static meshes were drawn.
	*/
	inline bool DrawVisibleInstancedStereo(
		FRHICommandList& RHICmdList,
		const StereoPair& StereoView,
		const FDrawingPolicyRenderState& DrawRenderState)
	{
		//moved out of the inner loop and only modified if bDrawnShared
		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);

		return DrawVisibleInner<InstancedStereoPolicy::Enabled>(RHICmdList, *StereoView.LeftView, typename DrawingPolicyType::ContextDataType(true), DrawRenderStateLocal, nullptr, nullptr, &StereoView, 0, OrderedDrawingPolicies.Num() - 1, false);
	}

	/**
	* Draws only the static meshes which are in the visibility map of the stereo pair
	* Stereo instancing is not enabled, the driver handles this for mobile multi-view
	* @param StereoView - The stereo pair to render.
	* @return True if any static meshes were drawn.
	*/
	inline bool DrawVisibleMobileMultiView(
		FRHICommandList& RHICmdList,
		const StereoPair& StereoView,
		const FDrawingPolicyRenderState& DrawRenderState)
	{
		//moved out of the inner loop and only modified if bDrawnShared
		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);

		return DrawVisibleInner<InstancedStereoPolicy::MobileMultiView>(RHICmdList, *StereoView.LeftView, typename DrawingPolicyType::ContextDataType(false), DrawRenderStateLocal, nullptr, nullptr, &StereoView, 0, OrderedDrawingPolicies.Num() - 1, false);
	}

	/**
	 * Draws only the static meshes which are in the visibility map.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	 * @return True if any static meshes were drawn.
	 */
	bool DrawVisible(FRHICommandList& RHICmdList, const FViewInfo& View, const typename DrawingPolicyType::ContextDataType PolicyContext, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray);

private:

	void DrawVisibleParallelInternal(
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		const TBitArray<SceneRenderingBitArrayAllocator>* StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>* BatchVisibilityArray,
		const StereoPair* const StereoView,
		FParallelCommandListSet& ParallelCommandListSet);

public:

	/**
	* Draws only the static meshes which are in the visibility map.
	* @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	* @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	* @param ParallelCommandListSet - holds information on how to get a fresh command list and deal with submits, etc
	*/
	inline void DrawVisibleParallel(
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray,
		FParallelCommandListSet& ParallelCommandListSet)
	{
		DrawVisibleParallelInternal(PolicyContext, &StaticMeshVisibilityMap, &BatchVisibilityArray, nullptr, ParallelCommandListSet);
	}

	/**
	* Draws only the static meshes which are in the visibility map of either the left or right view using instanced stereo
	* @param StereoView - Instanced stereo view state
	* @param ParallelCommandListSet - holds information on how to get a fresh command list and deal with submits, etc
	*/
	inline void DrawVisibleParallelInstancedStereo(const StereoPair& StereoView, FParallelCommandListSet& ParallelCommandListSet)
	{
		DrawVisibleParallelInternal(typename DrawingPolicyType::ContextDataType(true), nullptr, nullptr, &StereoView, ParallelCommandListSet);
	}

	/**
	* Draws only the static meshes which are in the visibility map, sorted front-to-back.
	* Both StaticMeshVisibilityMap and BatchVisibilityArray should be non-null for regular rendering or StereoView should be non-null if rendering with mobile multi-view
	* @param View - The view of the meshes to render (use the left view of a stereo pair when rendering with mobile multi-view)
	* @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	* @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	* @param StereoView - Stereo pair to render
	* @param MaxToDraw - The maximum number of meshes to be drawn.
	 * @return The number of static meshes drawn.
	*/
	template<InstancedStereoPolicy InstancedStereo>
	int32 DrawVisibleFrontToBackInner(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		FDrawingPolicyRenderState& DrawRenderState,
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		const TBitArray<SceneRenderingBitArrayAllocator>* const StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>* const BatchVisibilityArray,
		const StereoPair* const StereoView,
		int32 MaxToDraw);

	/**
	 * Draws only the static meshes which are in the visibility map, sorted front-to-back.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	 * @param MaxToDraw - The maximum number of meshes to be drawn.
	 * @return The number of static meshes drawn.
	 */
	inline int32 DrawVisibleFrontToBack(
		FRHICommandList& RHICmdList, 
		const FViewInfo& View, 
		const FDrawingPolicyRenderState& DrawRenderState,
		const typename DrawingPolicyType::ContextDataType PolicyContext, 
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, 
		const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray, 
		int32 MaxToDraw)
	{
		//moved out of the inner loop and only modified if bDrawnShared
		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);

		return DrawVisibleFrontToBackInner<InstancedStereoPolicy::Disabled>(RHICmdList, View, DrawRenderStateLocal, PolicyContext, &StaticMeshVisibilityMap, &BatchVisibilityArray, nullptr, MaxToDraw);
	}

	/**
	* Draws only the static meshes which are in the visibility map of the stereo pair
	* Stereo instancing is not enabled, the driver handles this for mobile multi-view
	* @param StereoView - The stereo pair to render.
	* @param MaxToDraw - The maximum number of meshes to be drawn.
	* @return The number of static meshes drawn.
	*/
	inline int32 DrawVisibleFrontToBackMobileMultiView(FRHICommandList& RHICmdList, const StereoPair &StereoView, const FDrawingPolicyRenderState& DrawRenderState, const int32 MaxToDraw)
	{
		//moved out of the inner loop and only modified if bDrawnShared
		FDrawingPolicyRenderState DrawRenderStateLocal(DrawRenderState);

		return DrawVisibleFrontToBackInner<InstancedStereoPolicy::MobileMultiView>(RHICmdList, *StereoView.LeftView, DrawRenderStateLocal, typename DrawingPolicyType::ContextDataType(false), nullptr, nullptr, &StereoView, MaxToDraw);
	}

	/**
	 * Helper functions when policy context is not needed.
	 */
	inline bool DrawVisible(const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap)
	{
		return DrawVisible(View, typename DrawingPolicyType::ContextDataType(), DrawRenderState, StaticMeshVisibilityMap);
	}

	inline bool DrawVisible(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray)
	{
		return DrawVisible(RHICmdList, View, typename DrawingPolicyType::ContextDataType(), DrawRenderState, StaticMeshVisibilityMap, BatchVisibilityArray);
	}

	inline void DrawVisibleParallel(const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray, FParallelCommandListSet& ParallelCommandListSet)
	{
		DrawVisibleParallel(typename DrawingPolicyType::ContextDataType(), StaticMeshVisibilityMap, BatchVisibilityArray, ParallelCommandListSet);
	}

	inline int32 DrawVisibleFrontToBack(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray, int32 MaxToDraw)
	{
		return DrawVisibleFrontToBack(RHICmdList, View, DrawRenderState, typename DrawingPolicyType::ContextDataType(), StaticMeshVisibilityMap, BatchVisibilityArray, MaxToDraw);
	}

	/** Sorts OrderedDrawingPolicies front to back. */
	void SortFrontToBack(FVector ViewPosition);

	/** Builds a list of primitives that use the given materials in this static draw list. */
	void GetUsedPrimitivesBasedOnMaterials(ERHIFeatureLevel::Type InFeatureLevel, const TArray<const FMaterial*>& Materials, TArray<FPrimitiveSceneInfo*>& PrimitivesToUpdate);

	/**
	 * Shifts all meshes bounds by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	void ApplyWorldOffset(FVector InOffset);

	/**
	 * @return total number of meshes in all draw policies
	 */
	int32 NumMeshes() const;

	TStaticMeshDrawList();
	~TStaticMeshDrawList();

	// FRenderResource interface.
	virtual void ReleaseRHI();

	typedef TSet<FDrawingPolicyLink, FDrawingPolicyKeyFuncs> TDrawingPolicySet;
	/** Sorts OrderedDrawingPolicies front to back.  Relies on static variables SortDrawingPolicySet and SortViewPosition being set. */
	static int32 Compare(FSetElementId A, FSetElementId B, const TDrawingPolicySet * const InSortDrawingPolicySet, const FVector InSortViewPosition);

	/** Computes statistics for this draw list. */
	FDrawListStats GetStats() const;

	// NVCHANGE_BEGIN: Add VXGI
	void IterateOverMeshes(TFunctionRef<void(FStaticMesh*)> ProcessMesh);
	// NVCHANGE_END: Add VXGI

private:
	void CollectClosestMatchingPolicies(
		typename TArray<FSetElementId>::TConstIterator DrawingPolicyIter,
		TMap<FString, int32>& MatchFailedReasons
		) const;

	/** All drawing policies in the draw list, in rendering order. */
	TArray<FSetElementId> OrderedDrawingPolicies;
	
	/** All drawing policy element sets in the draw list, hashed by drawing policy. */
	TDrawingPolicySet DrawingPolicySet;

	uint32 FrameNumberForVisibleCount;
	uint32 ViewStateUniqueId;
};

/** Helper struct for sorting */
template<typename DrawingPolicyType>
struct TCompareStaticMeshDrawList
{
private:
	const typename TStaticMeshDrawList<DrawingPolicyType>::TDrawingPolicySet * const SortDrawingPolicySet;
	const FVector SortViewPosition;

public:
	TCompareStaticMeshDrawList(const typename TStaticMeshDrawList<DrawingPolicyType>::TDrawingPolicySet * const InSortDrawingPolicySet, const FVector InSortViewPosition)
		: SortDrawingPolicySet(InSortDrawingPolicySet)
		, SortViewPosition(InSortViewPosition)
	{
	}

	FORCEINLINE bool operator()(const FSetElementId& A, const FSetElementId& B) const
	{
		// Use static Compare from TStaticMeshDrawList
		return TStaticMeshDrawList<DrawingPolicyType>::Compare(A, B, SortDrawingPolicySet, SortViewPosition) < 0;
	}
};

#include "StaticMeshDrawList.inl"

