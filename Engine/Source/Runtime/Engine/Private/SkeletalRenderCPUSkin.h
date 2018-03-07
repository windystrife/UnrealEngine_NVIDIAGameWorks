// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderCPUSkin.h: CPU skinned mesh object and resource definitions
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "RenderResource.h"
#include "LocalVertexFactory.h"
#include "Components/SkinnedMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "ClothingSystemRuntimeTypes.h"

class FPrimitiveDrawInterface;
class UMorphTarget;

/**
 * Skeletal mesh vertices which have been skinned to their final positions 
 */
class FFinalSkinVertexBuffer : public FVertexBuffer
{
public:

	/** 
	 * Constructor
	 * @param	InSkelMesh - parent mesh containing the static model data for each LOD
	 * @param	InLODIdx - index of LOD model to use from the parent mesh
	 */
	FFinalSkinVertexBuffer(FSkeletalMeshResource* InSkelMeshResource, int32 InLODIdx)
	:	LODIdx(InLODIdx)
	,	SkeletalMeshResource(InSkelMeshResource)
	{
		check(SkeletalMeshResource);
		check(SkeletalMeshResource->LODModels.IsValidIndex(LODIdx));
	}
	/** 
	 * Initialize the dynamic RHI for this rendering resource 
	 */
	virtual void InitDynamicRHI();

	/** 
	 * Release the dynamic RHI for this rendering resource 
	 */
	virtual void ReleaseDynamicRHI();

	/** 
	 * Cpu skinned vertex name 
	 */
	virtual FString GetFriendlyName() const { return TEXT("CPU skinned mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	virtual SIZE_T GetResourceSize()
	{
		// all the vertex data for a single LOD of the skel mesh
		FStaticLODModel& LodModel = SkeletalMeshResource->LODModels[LODIdx];

		return LodModel.NumVertices * sizeof(FFinalSkinVertex);
	}

private:
	/** index to the SkeletalMeshResource.LODModels */
	int32	LODIdx;
	/** parent mesh containing the source data */
	FSkeletalMeshResource* SkeletalMeshResource;

	void InitVertexData(FStaticLODModel& LodModel);
};


/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class ENGINE_API FDynamicSkelMeshObjectDataCPUSkin
{
public:

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveMorphTargets - Active Morph Targets to blend with during skinning
	* @param	InMorphTargetWeights - All Morph Target weights to blend with during skinning
	*/
	FDynamicSkelMeshObjectDataCPUSkin(
		USkinnedMeshComponent* InMeshComponent,
		FSkeletalMeshResource* InSkeletalMeshResource,
		int32 InLODIndex,
		const TArray<FActiveMorphTarget>& InActiveMorphTargets,
		const TArray<float>& InMorphTargetWeights
		);

	virtual ~FDynamicSkelMeshObjectDataCPUSkin()
	{
	}

	/** Local to world transform, used for cloth as sim data is in world space */
	FMatrix WorldToLocal;

	/** ref pose to local space transforms */
	TArray<FMatrix> ReferenceToLocal;

	/** origin and direction vectors for TRISORT_CustomLeftRight sections */
	TArray<FTwoVectors> CustomLeftRightVectors;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	/** component space bone transforms*/
	TArray<FTransform> MeshComponentSpaceTransforms;
#endif
	/** currently LOD for bones being updated */
	int32 LODIndex;
	/** Morphs to blend when skinning verts */
	TArray<FActiveMorphTarget> ActiveMorphTargets;
	/** Morph Weights to blend when skinning verts */
	TArray<float> MorphTargetWeights;

	/** data for updating cloth section */
	TMap<int32, FClothSimulData> ClothSimulUpdateData;

	/** a weight factor to blend between simulated positions and skinned positions */
	float ClothBlendWeight;

	/**
	* Returns the size of memory allocated by render data
	*/
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize()
	{
		return GetResourceSizeBytes();
	}

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
 	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));
 		
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ReferenceToLocal.GetAllocatedSize());
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(ActiveMorphTargets.GetAllocatedSize());
 	}

	SIZE_T GetResourceSizeBytes()
	{
		FResourceSizeEx ResSize;
		GetResourceSizeEx(ResSize);
		return ResSize.GetTotalMemoryBytes();
	}

	/** Update Simulated Positions & Normals from Clothing actor */
	bool UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent);
};

/**
 * Render data for a CPU skinned mesh
 */
class ENGINE_API FSkeletalMeshObjectCPUSkin : public FSkeletalMeshObject
{
public:

	/** @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render */
	FSkeletalMeshObjectCPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FSkeletalMeshObjectCPUSkin();

	//~ Begin FSkeletalMeshObject Interface
	virtual void InitResources(USkinnedMeshComponent* InMeshComponent) override;
	virtual void ReleaseResources() override;
	virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetsWeights) override;
	void UpdateDynamicData_RenderThread(FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectDataCPUSkin* InDynamicData, uint32 FrameNumberToPrepare);
	virtual void UpdateRecomputeTangent(int32 MaterialIndex, int32 LODIndex, bool bRecomputeTangent) override;
	virtual void EnableOverlayRendering(bool bEnabled, const TArray<int32>* InBonesOfInterest, const TArray<UMorphTarget*>* InMorphTargetOfInterest) override;
	virtual void CacheVertices(int32 LODIndex, bool bForce) const override;
	virtual bool IsCPUSkinned() const override { return true; }
	virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx) const override;
	virtual TArray<FTransform>* GetComponentSpaceTransforms() const override;
	virtual const TArray<FMatrix>& GetReferenceToLocalMatrices() const override;
	virtual int32 GetLOD() const override
	{
		if(DynamicData)
		{
			return DynamicData->LODIndex;
		}
		else
		{
			return 0;
		}
	}
	virtual const FTwoVectors& GetCustomLeftRightVectors(int32 SectionIndex) const override;

	virtual bool HaveValidDynamicData() override
	{ 
		return ( DynamicData!=NULL ); 
	}

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override
	{
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this));

		if(DynamicData)
		{
			DynamicData->GetResourceSizeEx(CumulativeResourceSize);
		}

		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(LODs.GetAllocatedSize()); 

		// include extra data from LOD
		for (int32 I=0; I<LODs.Num(); ++I)
		{
			LODs[I].GetResourceSizeEx(CumulativeResourceSize);
		}

		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(CachedFinalVertices.GetAllocatedSize());
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(BonesOfInterest.GetAllocatedSize());
	}

	virtual void DrawVertexElements(FPrimitiveDrawInterface* PDI, const FMatrix& ToWorldSpace, bool bDrawNormals, bool bDrawTangents, bool bDrawBinormals) const override;
	//~ End FSkeletalMeshObject Interface

	/** Access cached final vertices */
	const TArray<FFinalSkinVertex>& GetCachedFinalVertices() const { return CachedFinalVertices; }

private:
	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FSkeletalMeshResource* SkelMeshResource;
		// index into FSkeletalMeshResource::LODModels[]
		int32 LODIndex;

		FLocalVertexFactory				VertexFactory;
		mutable FFinalSkinVertexBuffer	VertexBuffer;

		/** Skin weight buffer to use, could be from asset or component override */
		FSkinWeightVertexBuffer* MeshObjectWeightBuffer;

		/** true if resources for this LOD have already been initialized. */
		bool						bResourcesInitialized;

		FSkeletalMeshObjectLOD(FSkeletalMeshResource* InSkelMeshResource, int32 InLOD)
		:	SkelMeshResource(InSkelMeshResource)
		,	LODIndex(InLOD)
		,	VertexBuffer(InSkelMeshResource,InLOD)
		,	MeshObjectWeightBuffer(nullptr)
		,	bResourcesInitialized( false )
		{
		}
		/** 
		 * Init rendering resources for this LOD 
		 */
		void InitResources(FSkelMeshComponentLODInfo* CompLODInfo);
		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();
		/** 
		 * Update the contents of the vertex buffer with new data
		 * @param	NewVertices - array of new vertex data
		 * @param	Size - size of new vertex data aray 
		 */
		void UpdateFinalSkinVertexBuffer(void* NewVertices, uint32 Size) const;

		/**
	 	 * Get Resource Size : return the size of Resource this allocates
	 	 */
		DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
		SIZE_T GetResourceSize()
		{
			return GetResourceSizeBytes();
		}

		void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(VertexBuffer.GetResourceSize());
		}

		SIZE_T GetResourceSizeBytes()
		{
			FResourceSizeEx ResSize;
			GetResourceSizeEx(ResSize);
			return ResSize.GetTotalMemoryBytes();
		}
	};

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

protected:
	/** Data that is updated dynamically and is needed for rendering */
	class FDynamicSkelMeshObjectDataCPUSkin* DynamicData;

private:
 	/** Index of LOD level's vertices that are currently stored in CachedFinalVertices */
 	mutable int32	CachedVertexLOD;

 	/** Cached skinned vertices. Only updated/accessed by the rendering thread and exporters */
 	mutable TArray<FFinalSkinVertex> CachedFinalVertices;

	/** Array of bone's to render bone weights for */
	TArray<int32> BonesOfInterest;
	TArray<UMorphTarget*> MorphTargetOfInterest;

	/** Bone weight viewing in editor */
	bool bRenderOverlayMaterial;
};

