// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.h: GPU skinned mesh object and resource definitions
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "SkeletalMeshTypes.h"
#include "Components/SkinnedMeshComponent.h"
#include "GlobalShader.h"
#include "GPUSkinVertexFactory.h"
#include "SkeletalRenderPublic.h"
#include "ClothingSystemRuntimeTypes.h"

class FGPUSkinCache;

/** 
* Stores the updated matrices needed to skin the verts.
* Created by the game thread and sent to the rendering thread as an update 
*/
class FDynamicSkelMeshObjectDataGPUSkin
{
	/**
	* Constructor, these are recycled, so you never use a constructor
	*/
	FDynamicSkelMeshObjectDataGPUSkin()
	{
		Clear();
	}

	virtual ~FDynamicSkelMeshObjectDataGPUSkin()
	{
	}

	void Clear();

public:

	static FDynamicSkelMeshObjectDataGPUSkin* AllocDynamicSkelMeshObjectDataGPUSkin();
	static void FreeDynamicSkelMeshObjectDataGPUSkin(FDynamicSkelMeshObjectDataGPUSkin* Who);

	/**
	* Constructor
	* Updates the ReferenceToLocal matrices using the new dynamic data.
	* @param	InSkelMeshComponent - parent skel mesh component
	* @param	InLODIndex - each lod has its own bone map 
	* @param	InActiveMorphTargets - morph targets active for the mesh
	* @param	InMorphTargetWeights - All morph target weights for the mesh
	*/
	void InitDynamicSkelMeshObjectDataGPUSkin(
		USkinnedMeshComponent* InMeshComponent,
		FSkeletalMeshResource* InSkeletalMeshResource,
		int32 InLODIndex,
		const TArray<FActiveMorphTarget>& InActiveMorphTargets,
		const TArray<float>& InMorphTargetWeights
		);

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
	/** current morph targets active on this mesh */
	TArray<FActiveMorphTarget> ActiveMorphTargets;
	/** All morph target weights on this mesh */
	TArray<float> MorphTargetWeights;
	/** number of active morph targets with weights > 0 */
	int32 NumWeightedActiveMorphTargets;

	/** data for updating cloth section */
	TMap<int32, FClothSimulData> ClothingSimData;

	/** a weight factor to blend between simulated positions and skinned positions */	
	float ClothBlendWeight;

	/**
	* Compare the given set of active morph targets with the current list to check if different
	* @param CompareActiveMorphTargets - array of morphs to compare
	* @param MorphTargetWeights - array of morphs weights to compare
	* @return true if both sets of active morphs are equal
	*/
	bool ActiveMorphTargetsEqual(const TArray<FActiveMorphTarget>& CompareActiveMorphTargets, const TArray<float>& CompareMorphTargetWeights);
	
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
		
		CumulativeResourceSize.AddUnknownMemoryBytes(ReferenceToLocal.GetAllocatedSize());
		CumulativeResourceSize.AddUnknownMemoryBytes(ActiveMorphTargets.GetAllocatedSize());
	}

	SIZE_T GetResourceSizeBytes()
	{
		FResourceSizeEx ResSize;
		GetResourceSizeEx(ResSize);
		return ResSize.GetTotalMemoryBytes();
	}

	/** Update Simulated Positions & Normals from APEX Clothing actor */
	bool UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent);
};

/** morph target mesh data for a single vertex delta */
struct FMorphGPUSkinVertex
{
	// Changes to this struct must be reflected in MorphTargets.usf!
	FVector			DeltaPosition;
	FVector			DeltaTangentZ;

	FMorphGPUSkinVertex() {};
	
	/** Construct for special case **/
	FMorphGPUSkinVertex(const FVector& InDeltaPosition, const FVector& InDeltaTangentZ)
	{
		DeltaPosition = InDeltaPosition;
		DeltaTangentZ = InDeltaTangentZ;
	}
};

/**
* MorphTarget vertices which have been combined into single position/tangentZ deltas
*/
class FMorphVertexBuffer : public FVertexBuffer
{
public:

	/** 
	* Constructor
	* @param	InSkelMesh - parent mesh containing the static model data for each LOD
	* @param	InLODIdx - index of LOD model to use from the parent mesh
	*/
	FMorphVertexBuffer(FSkeletalMeshResource* InSkelMeshResource, int32 InLODIdx)
		:	bHasBeenUpdated(false)	
		,	bNeedsInitialClear(true)
		,	LODIdx(InLODIdx)
		,	SkelMeshResource(InSkelMeshResource)
	{
		check(SkelMeshResource);
		check(SkelMeshResource->LODModels.IsValidIndex(LODIdx));
		bUsesComputeShader = false;
	}
	/** 
	* Initialize the dynamic RHI for this rendering resource 
	*/
	virtual void InitDynamicRHI();

	/** 
	* Release the dynamic RHI for this rendering resource 
	*/
	virtual void ReleaseDynamicRHI();

	inline void RecreateResourcesIfRequired(bool bInUsesComputeShader)
	{
		if (bUsesComputeShader != bInUsesComputeShader)
		{
			UpdateRHI();
		}
	}

	/** 
	* Morph target vertex name 
	*/
	virtual FString GetFriendlyName() const { return TEXT("Morph target mesh vertices"); }

	/**
	 * Get Resource Size : mostly copied from InitDynamicRHI - how much they allocate when initialize
	 */
	SIZE_T GetResourceSize()
	{
		SIZE_T ResourceSize = sizeof(*this);

		if (VertexBufferRHI)
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIdx];

			// Create the buffer rendering resource
			ResourceSize += LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
		}

		return ResourceSize;
	}

	/**
	* Get Resource Size : only get the size of the GPU resource
	*/
	SIZE_T GetUAVSize()
	{
		SIZE_T ResourceSize = 0;

		if (VertexBufferRHI)
		{
			// LOD of the skel mesh is used to find number of vertices in buffer
			FStaticLODModel& LodModel = SkelMeshResource->LODModels[LODIdx];

			// Create the buffer rendering resource
			ResourceSize += LodModel.NumVertices * sizeof(FMorphGPUSkinVertex);
		}

		return ResourceSize;
	}

	/** Has been updated or not by UpdateMorphVertexBuffer**/
	bool bHasBeenUpdated;

	/** DX12 cannot clear the buffer in InitDynamicRHI with UAV flag enables, we should really have a Zero initzialized flag instead**/
	bool bNeedsInitialClear;

	// @param guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIParamRef GetSRV() const
	{
		return SRVValue;
	}

	// @param guaranteed only to be valid if the vertex buffer is valid
	FUnorderedAccessViewRHIRef GetUAV() const
	{
		return UAVValue;
	}

	FStaticLODModel* GetStaticLODModel() const { return &SkelMeshResource->LODModels[LODIdx]; }

	// @third party code - BEGIN HairWorks
	void RequireSRV();
	// @third party code - END HairWorks

protected:
	// guaranteed only to be valid if the vertex buffer is valid
	FShaderResourceViewRHIRef SRVValue;

	// guaranteed only to be valid if the vertex buffer is valid
	FUnorderedAccessViewRHIRef UAVValue;

	bool bUsesComputeShader;

private:
	/** index to the SkelMeshResource.LODModels */
	int32	LODIdx;
	// parent mesh containing the source data, never 0
	FSkeletalMeshResource* SkelMeshResource;
};

/**
 * Render data for a GPU skinned mesh
 */
class FSkeletalMeshObjectGPUSkin : public FSkeletalMeshObject
{
public:
	/** @param	InSkeletalMeshComponent - skeletal mesh primitive we want to render */
	FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type InFeatureLevel);
	virtual ~FSkeletalMeshObjectGPUSkin();

	//~ Begin FSkeletalMeshObject Interface
	virtual void InitResources(USkinnedMeshComponent* InMeshComponent) override;
	virtual void ReleaseResources() override;
	virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights) override;
	void UpdateDynamicData_RenderThread(FGPUSkinCache* GPUSkinCache, FRHICommandListImmediate& RHICmdList, FDynamicSkelMeshObjectDataGPUSkin* InDynamicData, FSceneInterface* Scene, uint32 FrameNumberToPrepare);
	virtual void UpdateRecomputeTangent(int32 MaterialIndex, int32 LODIndex, bool bRecomputeTangent) override;
	virtual void PreGDMECallback(FGPUSkinCache* GPUSkinCache, uint32 FrameNumber) override;
	virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex,int32 ChunkIdx) const override;
	virtual void CacheVertices(int32 LODIndex, bool bForce) const override {}
	virtual bool IsCPUSkinned() const override { return false; }
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
		
		CumulativeResourceSize.AddUnknownMemoryBytes(LODs.GetAllocatedSize()); 

		// include extra data from LOD
		for (int32 I=0; I<LODs.Num(); ++I)
		{
			LODs[I].GetResourceSizeEx(CumulativeResourceSize);
		}
	}
	//~ End FSkeletalMeshObject Interface

	FSkinWeightVertexBuffer* GetSkinWeightVertexBuffer(int32 LODIndex) const;

	// @third party code - BEGIN HairWorks
	FMorphVertexBuffer& GetMorphVertexBuffer();
	// @third party code - END HairWorks

	/** 
	 * Vertex buffers that can be used for GPU skinning factories 
	 */
	class FVertexFactoryBuffers
	{
	public:
		FVertexFactoryBuffers()
			: VertexBufferGPUSkin(NULL)
			, SkinWeightVertexBuffer(NULL)
			, ColorVertexBuffer(NULL)
			, MorphVertexBuffer(NULL)
			, APEXClothVertexBuffer(NULL)
		{}

		FSkeletalMeshVertexBuffer* VertexBufferGPUSkin;
		FSkinWeightVertexBuffer* SkinWeightVertexBuffer;
		FColorVertexBuffer*	ColorVertexBuffer;
		FMorphVertexBuffer* MorphVertexBuffer;
		FSkeletalMeshVertexClothBuffer*	APEXClothVertexBuffer;
	};

private:

	/**
	 * Vertex factories and their matrix arrays
	 */
	class FVertexFactoryData
	{
	public:
		FVertexFactoryData() {}

		/** one vertex factory for each chunk */
		TArray<TUniquePtr<FGPUBaseSkinVertexFactory>> VertexFactories;

		/** one passthrough vertex factory for each chunk */
		TArray<TUniquePtr<FGPUSkinPassthroughVertexFactory>> PassthroughVertexFactories;

		/** Vertex factory defining both the base mesh as well as the morph delta vertex decals */
		TArray<TUniquePtr<FGPUBaseSkinVertexFactory>> MorphVertexFactories;

		/** Vertex factory defining both the base mesh as well as the APEX cloth vertex data */
		TArray<TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>> ClothVertexFactories;

		/** 
		 * Init default vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Sections - relevant section information (either original or from swapped influence)
		 */
		void InitVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshSection>& Sections, ERHIFeatureLevel::Type FeatureLevel);
		/** 
		 * Release default vertex factory resources for this LOD 
		 */
		void ReleaseVertexFactories();
		/** 
		 * Init morph vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Sections - relevant section information (either original or from swapped influence)
		 */
		void InitMorphVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshSection>& Sections, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type InFeatureLevel);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseMorphVertexFactories();
		/** 
		 * Init APEX cloth vertex factory resources for this LOD 
		 *
		 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
		 * @param Sections - relevant section information (either original or from swapped influence)
		 */
		void InitAPEXClothVertexFactories(const FVertexFactoryBuffers& VertexBuffers, const TArray<FSkelMeshSection>& Sections, ERHIFeatureLevel::Type InFeatureLevel);
		/** 
		 * Release morph vertex factory resources for this LOD 
		 */
		void ReleaseAPEXClothVertexFactories();

		/**
		 * Clear factory arrays
		 */
		void ClearFactories()
		{
			VertexFactories.Empty();
			MorphVertexFactories.Empty();
			ClothVertexFactories.Empty();
		}

		/**
		 * @return memory in bytes of size of the vertex factories and their matrices
		 */
		SIZE_T GetResourceSize()
		{
			SIZE_T Size = 0;
			Size += VertexFactories.GetAllocatedSize();

			Size += MorphVertexFactories.GetAllocatedSize();

			Size += ClothVertexFactories.GetAllocatedSize();

			return Size;
		}	

		private:
			FVertexFactoryData(const FVertexFactoryData&);
			FVertexFactoryData& operator=(const FVertexFactoryData&);
	};

	/** vertex data for rendering a single LOD */
	struct FSkeletalMeshObjectLOD
	{
		FSkeletalMeshObjectLOD(FSkeletalMeshResource* InSkelMeshResource,int32 InLOD)
		:	SkelMeshResource(InSkelMeshResource)
		,	LODIndex(InLOD)
		,	MorphVertexBuffer(InSkelMeshResource,LODIndex)
		,	MeshObjectWeightBuffer(nullptr)
		,	MeshObjectColorBuffer(nullptr)
		{
		}

		/** 
		 * Init rendering resources for this LOD 
		 * @param MeshLODInfo - information about the state of the bone influence swapping
		 * @param CompLODInfo - information about this LOD from the skeletal component 
		 */
		void InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo, FSkelMeshComponentLODInfo* CompLODInfo, ERHIFeatureLevel::Type FeatureLevel);

		/** 
		 * Release rendering resources for this LOD 
		 */
		void ReleaseResources();

		/** 
		 * Init rendering resources for the morph stream of this LOD
		 * @param MeshLODInfo - information about the state of the bone influence swapping
		 * @param Chunks - relevant chunk information (either original or from swapped influence)
		 */
		void InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type FeatureLevel);

		/** 
		 * Release rendering resources for the morph stream of this LOD
		 */
		void ReleaseMorphResources();

		/**
		 * @return memory in bytes of size of the resources for this LOD
		 */
		DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
		SIZE_T GetResourceSize()
		{
			return GetResourceSizeBytes();
		}

		void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
		{
			CumulativeResourceSize.AddUnknownMemoryBytes(MorphVertexBuffer.GetResourceSize());
			CumulativeResourceSize.AddUnknownMemoryBytes(GPUSkinVertexFactories.GetResourceSize());
		}

		SIZE_T GetResourceSizeBytes()
		{
			FResourceSizeEx ResSize;
			GetResourceSizeEx(ResSize);
			return ResSize.GetTotalMemoryBytes();
		}

		FSkeletalMeshResource* SkelMeshResource;
		// index into FSkeletalMeshResource::LODModels[]
		int32 LODIndex;

		/** Vertex buffer that stores the morph target vertex deltas. Updated on the CPU */
		FMorphVertexBuffer MorphVertexBuffer;

		/** Default GPU skinning vertex factories and matrices */
		FVertexFactoryData GPUSkinVertexFactories;

		/** Skin weight buffer to use, could be from asset or component override */
		FSkinWeightVertexBuffer* MeshObjectWeightBuffer;

		/** Color buffer to user, could be from asset or component override */
		FColorVertexBuffer* MeshObjectColorBuffer;

		/**
		 * Update the contents of the morphtarget vertex buffer by accumulating all 
		 * delta positions and delta normals from the set of active morph targets
		 * @param ActiveMorphTargets - Morph to accumulate. assumed to be weighted and have valid targets
		 * @param MorphTargetWeights - All Morph weights
		 */
		void UpdateMorphVertexBufferCPU(const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights);
		void UpdateMorphVertexBufferGPU(FRHICommandListImmediate& RHICmdList, const TArray<float>& MorphTargetWeights, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers);

		/**
		 * Determine the current vertex buffers valid for this LOD
		 *
		 * @param OutVertexBuffers output vertex buffers
		 */
		void GetVertexBuffers(FVertexFactoryBuffers& OutVertexBuffers,FStaticLODModel& LODModel);

		// Temporary arrays used on UpdateMorphVertexBuffer(); these grow to the max and are not thread safe.
		static TArray<float> MorphAccumulatedWeightArray;
	};

	/** 
	* Initialize morph rendering resources for each LOD 
	*/
	void InitMorphResources(bool bInUsePerBoneMotionBlur, const TArray<float>& MorphTargetWeights);

	/** 
	* Release morph rendering resources for each LOD. 
	*/
	void ReleaseMorphResources();

	void ProcessUpdatedDynamicData(FGPUSkinCache* GPUSkinCache, FRHICommandListImmediate& RHICmdList, uint32 FrameNumberToPrepare, bool bMorphNeedsUpdate);

	void WaitForRHIThreadFenceForDynamicData();

	/** Render data for each LOD */
	TArray<struct FSkeletalMeshObjectLOD> LODs;

	/** Data that is updated dynamically and is needed for rendering */
	FDynamicSkelMeshObjectDataGPUSkin* DynamicData;

	/** Fence for dynamic Data */
	FGraphEventRef RHIThreadFenceForDynamicData;

	/** True if we are doing a deferred update later in GDME. */
	bool bNeedsUpdateDeferred;

	/** If true and we are doing a deferred update, then also update the morphs */
	bool bMorphNeedsUpdateDeferred;

	/** true if the morph resources have been initialized */
	bool bMorphResourcesInitialized;
};


class FGPUMorphUpdateCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FGPUMorphUpdateCS, Global);

	FGPUMorphUpdateCS() {}

	FGPUMorphUpdateCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MorphVertexBufferParameter.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));

		MorphTargetWeightParameter.Bind(Initializer.ParameterMap, TEXT("MorphTargetWeight"));
		OffsetAndSizeParameter.Bind(Initializer.ParameterMap, TEXT("OffsetAndSize"));
		PositionScaleParameter.Bind(Initializer.ParameterMap, TEXT("PositionScale"));

		VertexIndicesParameter.Bind(Initializer.ParameterMap, TEXT("VertexIndicies"));
		MorphDeltasParameter.Bind(Initializer.ParameterMap, TEXT("MorphDeltas"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MorphVertexBufferParameter;

		Ar << MorphTargetWeightParameter;
		Ar << OffsetAndSizeParameter;
		Ar << PositionScaleParameter;

		Ar << VertexIndicesParameter;
		Ar << MorphDeltasParameter;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(FRHICommandList& RHICmdList, const FVector4& LocalScale, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers, FMorphVertexBuffer& MorphVertexBuffer);
	void SetOffsetAndSize(FRHICommandList& RHICmdList, uint32 Offset, uint32 Size, float Weight);

	void Dispatch(FRHICommandList& RHICmdList, uint32 Size);
	void EndAllDispatches(FRHICommandList& RHICmdList);

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

protected:
	FShaderResourceParameter MorphVertexBufferParameter;

	FShaderParameter MorphTargetWeightParameter;
	FShaderParameter OffsetAndSizeParameter;
	FShaderParameter PositionScaleParameter;

	FShaderResourceParameter VertexIndicesParameter;
	FShaderResourceParameter MorphDeltasParameter;
};

class FGPUMorphNormalizeCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FGPUMorphNormalizeCS, Global);

	FGPUMorphNormalizeCS() {}

	FGPUMorphNormalizeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		MorphVertexBufferParameter.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));

		MorphTargetWeightParameter.Bind(Initializer.ParameterMap, TEXT("MorphTargetWeight"));
		MorphWorkItemsParameter.Bind(Initializer.ParameterMap, TEXT("MorphWorkItems"));
		PositionScaleParameter.Bind(Initializer.ParameterMap, TEXT("PositionScale"));
	}

	// FShader interface.
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << MorphVertexBufferParameter;

		Ar << MorphTargetWeightParameter;
		Ar << MorphWorkItemsParameter;
		Ar << PositionScaleParameter;

		return bShaderHasOutdatedParameters;
	}

	static bool ShouldCache(EShaderPlatform Platform)
	{
		return IsFeatureLevelSupported(Platform, ERHIFeatureLevel::SM5);
	}

	void SetParameters(FRHICommandList& RHICmdList, uint32 NumVerticies, const FVector4& LocalScale, const float AccumulatedWeight, FMorphVertexBuffer& MorphVertexBuffer);

	void Dispatch(FRHICommandList& RHICmdList, uint32 NumVerticies, const FVector4& LocalScale, const float AccumulatedWeight, FMorphVertexBuffer& MorphVertexBuffer);

protected:
	FShaderResourceParameter MorphVertexBufferParameter;

	FShaderParameter MorphTargetWeightParameter;
	FShaderParameter MorphWorkItemsParameter;
	FShaderParameter PositionScaleParameter;
};
