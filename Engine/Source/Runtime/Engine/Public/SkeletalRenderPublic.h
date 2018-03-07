// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderPublic.h: Definitions and inline code for rendering SkeletalMeshComponent
=============================================================================*/
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "ProfilingDebugging/ResourceSize.h"
#include "PackedNormal.h"
#include "RenderingThread.h"
#include "SkeletalMeshTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkinnedMeshComponent.h"

class FPrimitiveDrawInterface;
class FVertexFactory;
class UMorphTarget;

//#include "../Private/SkeletalRenderCPUSkin.h"
struct FSkelMeshSection;

/** data for a single skinned skeletal mesh vertex */
struct FFinalSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX;
	FPackedNormal	TangentZ;
	float			U;
	float			V;
};

/** Which set of indices to select for TRISORT_CustomLeftRight sections. */
enum ECustomSortAlternateIndexMode
{
	CSAIM_Auto = 0,
	CSAIM_Left = 1,
	CSAIM_Right = 2,
};

/**
* Interface for mesh rendering data
*/
class FSkeletalMeshObject : public FDeferredCleanupInterface
{
public:
	FSkeletalMeshObject(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshResource* InSkeletalMeshResource, ERHIFeatureLevel::Type FeatureLevel);
	virtual ~FSkeletalMeshObject();

	/** 
	 * Initialize rendering resources for each LOD 
	 */
	virtual void InitResources(USkinnedMeshComponent* InMeshComponent) = 0;

	/** 
	 * Release rendering resources for each LOD 
	 */
	virtual void ReleaseResources() = 0;

	/**
	 * Called by the game thread for any dynamic data updates for this skel mesh object
	 * @param	LODIndex - lod level to update
	 * @param	InSkeletalMeshComponen - parent prim component doing the updating
	 * @param	ActiveMorphs - morph targets to blend with during skinning
	 */
	virtual void Update(int32 LODIndex,USkinnedMeshComponent* InMeshComponent,const TArray<FActiveMorphTarget>& ActiveMorphTargets, const TArray<float>& MorphTargetWeights) = 0;

	/**
	* Called by the game thread for any update on RecomputeTangent
	* @param	MaterialIndex : Material Index for the update
	* @param	bRecomputeTangent : the new flag
	*/
	virtual void UpdateRecomputeTangent(int32 MaterialIndex, int32 LODIndex, bool bRecomputeTangent) = 0;

	/**
	 * Called by FSkeletalMeshObject prior to GDME. This allows the GPU skin version to update bones etc now that we know we are going to render
	 * @param FrameNumber from GFrameNumber
	 */
	virtual void PreGDMECallback(class FGPUSkinCache* GPUSkinCache, uint32 FrameNumber)
	{
	}

	/**
	 * @param	View - View, must not be 0, allows to cull depending on showflags (normally not needed/used in SHIPPING)
	 * @param	LODIndex - each LOD has its own vertex data
	 * @param	ChunkIdx - not used
	 * @return	vertex factory for rendering the LOD, 0 to suppress rendering
	 */
	virtual const FVertexFactory* GetSkinVertexFactory(const FSceneView* View, int32 LODIndex,int32 ChunkIdx) const = 0;

	/**
	 * Re-skin cached vertices for an LOD and update the vertex buffer. Note that this
	 * function is called from the render thread!
	 * @param	LODIndex - index to LODs
	 * @param	bForce - force update even if LOD index hasn't changed
	 */
	virtual void CacheVertices(int32 LODIndex, bool bForce) const = 0;

	/**
	 * Returns true if this mesh performs skinning on the CPU.
	 */
	virtual bool IsCPUSkinned() const = 0;

	/** 
	 *	Get the array of component-space bone transforms. 
	 *	Not safe to hold this point between frames, because it exists in dynamic data passed from main thread.
	 */
	virtual TArray<FTransform>* GetComponentSpaceTransforms() const = 0;

	/** 
	 *	Get the array of refpose->local matrices
	 *	Not safe to hold this reference between frames, because it exists in dynamic data passed from main thread.
	 */
	virtual const TArray<FMatrix>& GetReferenceToLocalMatrices() const = 0;

	/** Get the LOD to render this mesh at. */
	virtual int32 GetLOD() const = 0;
	/** 
	 * Enable blend weight rendering in the editor
	 * @param bEnabled - turn on or off the rendering mode
	 * optional parameters will decide which one to draw
	 * @param (optional) BonesOfInterest - array of bone indices to capture weights for
	 * @param (optional) MorphTargetsOfInterest - array of morphtargets to render for
	 */
	virtual void EnableOverlayRendering(bool bEnabled, const TArray<int32>* InBonesOfInterest, const TArray<UMorphTarget*>* MorphTargetOfInterest) {}

	/** 
	* Draw Normals/Tangents based on skinned vertex data 
	* @param PDI			- Draw Interface
	* @param ToWorldSpace	- Transform from component space to world space
	* @param bDrawNormals	- Should draw vertex normals
	* @param bDrawTangents	- Should draw vertex tangents
	* @param bDrawBinormals	- Should draw vertex binormals
	*/
	virtual void DrawVertexElements(FPrimitiveDrawInterface* PDI, const FMatrix& ToWorldSpace, bool bDrawNormals, bool bDrawTangents, bool bDrawBinormals) const {}

	/** 
	 *	Given a set of views, update the MinDesiredLODLevel member to indicate the minimum (ie best) LOD we would like to use to render this mesh. 
	 *	This is called from the rendering thread (PreRender) so be very careful what you read/write to.
	 * @param FrameNumber from ViewFamily.FrameNumber
	 */
	void UpdateMinDesiredLODLevel(const FSceneView* View, const FBoxSphereBounds& Bounds, int32 FrameNumber);

	/**
	 * Get the origin and direction vectors for TRISORT_CustomLeftRight sections
	 */
	virtual const FTwoVectors& GetCustomLeftRightVectors(int32 SectionIndex) const = 0;

	/**
	 *	Return true if this does have valid dynamic data to render
	 */
	virtual bool HaveValidDynamicData() = 0;

	// allow access to mesh component
	friend class FDynamicSkelMeshObjectDataCPUSkin;
	friend class FDynamicSkelMeshObjectDataGPUSkin;
	friend class FSkeletalMeshSceneProxy;
	friend class FSkeletalMeshSectionIter;

	/** @return if per-bone motion blur is enabled for this object. This includes is the system overwrites the skeletal mesh setting. */
	bool ShouldUsePerBoneMotionBlur() const { return bUsePerBoneMotionBlur; }

	// FDeferredCleanupInterface
	virtual void FinishCleanup()
	{
		delete this;
	}
	
	/**
	* Returns the size of memory allocated by render data
	*/
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize()
	{
		return GetResourceSizeBytes();
	}

	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) = 0;

	SIZE_T GetResourceSizeBytes()
	{
		FResourceSizeEx ResSize;
		GetResourceSizeEx(ResSize);
		return ResSize.GetTotalMemoryBytes();
	}

	/**
	 * List of sections to be rendered based on instance weight usage. Full swap of weights will render with its own sections.
	 * @return Sections to iterate over for rendering
	 */
	const TArray<FSkelMeshSection>& GetRenderSections(int32 InLODIndex) const;

	/**
	 * Update the hidden material section flags for an LOD entry
	 *
	 * @param InLODIndex - LOD entry to update hidden material flags for
	 * @param HiddenMaterials - array of hidden material sections
	 */
	void SetHiddenMaterials(int32 InLODIndex,const TArray<bool>& HiddenMaterials);
	
	/**
	 * Determine if the material section entry for an LOD is hidden or not
	 *
	 * @param InLODIndex - LOD entry to get hidden material flags for
	 * @param MaterialIdx - index of the material section to check
	 */
	bool IsMaterialHidden(int32 InLODIndex,int32 MaterialIdx) const;
	
	/**
	 * Initialize the array of LODInfo based on the settings of the current skel mesh component
	 */
	void InitLODInfos(const USkinnedMeshComponent* InMeshComponent);

	FORCEINLINE TStatId GetStatId() const 
	{ 
		return StatId; 
	}

	/** Get the skeletal mesh resource for which this mesh object was created. */
	FORCEINLINE FSkeletalMeshResource& GetSkeletalMeshResource() const { return *SkeletalMeshResource; }

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
	/** Get index buffer dynamic override. */
	FORCEINLINE FSkeletalMeshDynamicOverride* GetSkeletalMeshDynamicOverride() const { return SkeletalMeshDynamicOverride; }

	/** Set index buffer dynamic override. */
	void SetSkeletalMeshDynamicOverride(FSkeletalMeshDynamicOverride* NewSkeletalMeshDynamicOverride) {
		SkeletalMeshDynamicOverride = NewSkeletalMeshDynamicOverride;
	}
//nv end

	/** Setup for rendering a specific LOD entry of the component */
	struct FSkelMeshObjectLODInfo
	{
		/** Hidden Material Section Flags for rendering - That is Material Index, not Section Index  */
		TArray<bool>	HiddenMaterials;
	};	

	TArray<FSkelMeshObjectLODInfo> LODInfo;

	TArray<FCapsuleShape> ShadowCapsuleShapes;

	/** 
	 *	Lowest (best) LOD that was desired for rendering this SkeletalMesh last frame. 
	 *	This should only ever be WRITTEN by the RENDER thread (in FSkeletalMeshProxy::PreRenderView) and READ by the GAME thread (in USkeletalMeshComponent::UpdateSkelPose).
	 */
	int32 MinDesiredLODLevel;

	/** 
	 *	High (best) DistanceFactor that was desired for rendering this SkeletalMesh last frame. Represents how big this mesh was in screen space  
	 *	This should only ever be WRITTEN by the RENDER thread (in FSkeletalMeshProxy::PreRenderView) and READ by the GAME thread (in USkeletalMeshComponent::UpdateSkelPose).
	 */
	float MaxDistanceFactor;

	/** This frames min desired LOD level. This is copied (flipped) to MinDesiredLODLevel at the beginning of the next frame. */
	int32 WorkingMinDesiredLODLevel;

	/** This frames max distance factor. This is copied (flipped) to MaxDistanceFactor at the beginning of the next frame. */
	float WorkingMaxDistanceFactor;

	/** This is set to true when we have sent our Mesh data to the rendering thread at least once as it needs to have have a datastructure created there for each MeshObject **/
	bool bHasBeenUpdatedAtLeastOnce;

#if WITH_EDITORONLY_DATA
	/** Index of the section to preview... If set to -1, all section will be rendered */
	int32 SectionIndexPreview;
	int32 MaterialIndexPreview;
#endif

	/** returns the feature level this FSkeletalMeshObject was created with */
	ERHIFeatureLevel::Type GetFeatureLevel() const
	{
		return FeatureLevel;
	}

protected:
	/** The skeletal mesh resource with which to render. */
	FSkeletalMeshResource* SkeletalMeshResource;

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
	/** Dynamic index buffer data. */
	FSkeletalMeshDynamicOverride* SkeletalMeshDynamicOverride;
//nv end

	/** Per-LOD info. */
	TArray<FSkeletalMeshLODInfo> SkeletalMeshLODInfo;

	class FGPUSkinCacheEntry* SkinCacheEntry;

	/** Used to keep track of the first call to UpdateMinDesiredLODLevel each frame. from ViewFamily.FrameNumber */
	uint32 LastFrameNumber;

#if WITH_EDITORONLY_DATA
	/** Editor only. Used for visualizing drawing order in Animset Viewer. If < 1.0,
	 * only the specified fraction of triangles will be rendered
	 */
	float ProgressiveDrawingFraction;
#endif

	/** Use the 2nd copy of indices for separate left/right sort order (when TRISORT_CustomLeftRight) 
	 * Set manually by the AnimSetViewer when editing sort order, or based on viewing angle otherwise.
	 */
	ECustomSortAlternateIndexMode CustomSortAlternateIndexMode;
	/** 
	 *	If true, per-bone motion blur is enabled for this object. This includes is the system overwrites the skeletal mesh setting.
	 */
	bool bUsePerBoneMotionBlur;

	/** Used for dynamic stats */
	TStatId StatId;

	/** Feature level to render for. */
	ERHIFeatureLevel::Type FeatureLevel;
};
