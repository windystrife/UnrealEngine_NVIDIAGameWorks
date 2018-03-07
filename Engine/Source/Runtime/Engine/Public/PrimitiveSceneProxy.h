// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneProxy.h: Primitive scene proxy definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/MemStack.h"
#include "PrimitiveViewRelevance.h"
#include "SceneTypes.h"
#include "Engine/Scene.h"
#include "UniformBuffer.h"
#include "SceneView.h"
#include "PrimitiveUniformShaderParameters.h"

// NvFlow begin
#include "GameWorks/PrimitiveSceneProxyNvFlow.h"
// NvFlow end

class FLightSceneInfo;
class FLightSceneProxy;
class FPrimitiveDrawInterface;
class FPrimitiveSceneInfo;
class FStaticPrimitiveDrawInterface;
class UPrimitiveComponent;
class UTexture2D;
struct FMeshBatch;

/** Data for a simple dynamic light. */
class FSimpleLightEntry
{
public:
	FVector Color;
	float Radius;
	float Exponent;
	float VolumetricScatteringIntensity;
	bool bAffectTranslucency;
};

/** Data for a simple dynamic light which could change per-view. */
class FSimpleLightPerViewEntry
{
public:
	FVector Position;
};

/**
* Index into the Per-view data for each instance.
* Most uses wont need to add > 1 per view data.
* This array will be the same size as InstanceData for uses that require per view data. Otherwise it will be empty.
*/
class FSimpleLightInstacePerViewIndexData
{
public:
	uint32 PerViewIndex : 31;
	uint32 bHasPerViewData;
};

/** Data pertaining to a set of simple dynamic lights */
class FSimpleLightArray
{
public:
	/** Data per simple dynamic light instance, independent of view */
	TArray<FSimpleLightEntry, TMemStackAllocator<>> InstanceData;
	/** Per-view data for each light */
	TArray<FSimpleLightPerViewEntry, TMemStackAllocator<>> PerViewData;
	/** Indices into the per-view data for each light. */
	TArray<FSimpleLightInstacePerViewIndexData, TMemStackAllocator<>> InstancePerViewDataIndices;

public:

	/**
	* Returns the per-view data for a simple light entry.
	* @param LightIndex - The index of the simple light
	* @param ViewIndex - The index of the view
	* @param NumViews - The number of views in the ViewFamily.
	*/
	inline const FSimpleLightPerViewEntry& GetViewDependentData(int32 LightIndex, int32 ViewIndex, int32 NumViews) const
	{
		// If InstanceData has an equal number of elements to PerViewData then all views share the same PerViewData.
		if (InstanceData.Num() == PerViewData.Num())
		{
			check(InstancePerViewDataIndices.Num() == 0);
			return PerViewData[LightIndex];
		}
		else 
		{
			check(InstancePerViewDataIndices.Num() == InstanceData.Num());

			// Calculate per-view index.
			FSimpleLightInstacePerViewIndexData PerViewIndex = InstancePerViewDataIndices[LightIndex];
			const int32 PerViewDataIndex = PerViewIndex.PerViewIndex + ( PerViewIndex.bHasPerViewData ? ViewIndex : 0 );
			return PerViewData[PerViewDataIndex];
		}
	}
};

/** Information about a heightfield gathered by the renderer for heightfield lighting. */
class FHeightfieldComponentDescription
{
public:
	FVector4 HeightfieldScaleBias;
	FVector4 MinMaxUV;
	FMatrix LocalToWorld;
	FVector2D LightingAtlasLocation;
	FIntRect HeightfieldRect;

	int32 NumSubsections;
	FVector4 SubsectionScaleAndBias;

	FHeightfieldComponentDescription(const FMatrix& InLocalToWorld) :
		LocalToWorld(InLocalToWorld)
	{}
};

extern bool CacheShadowDepthsFromPrimitivesUsingWPO();

/**
 * Encapsulates the data which is mirrored to render a UPrimitiveComponent parallel to the game thread.
 * This is intended to be subclassed to support different primitive types.  
 */
class FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	ENGINE_API FPrimitiveSceneProxy(const UPrimitiveComponent* InComponent, FName ResourceName = NAME_None);

	/** Virtual destructor. */
	ENGINE_API virtual ~FPrimitiveSceneProxy();

	/**
	 * Updates selection for the primitive proxy. This simply sends a message to the rendering thread to call SetSelection_RenderThread.
	 * This is called in the game thread as selection is toggled.
	 * @param bInParentSelected - true if the parent actor is selected in the editor
 	 * @param bInIndividuallySelected - true if the component is selected in the editor directly
	 */
	void SetSelection_GameThread(const bool bInParentSelected, const bool bInIndividuallySelected=false);

	/**
	 * Updates hover state for the primitive proxy. This simply sends a message to the rendering thread to call SetHovered_RenderThread.
	 * This is called in the game thread as hover state changes
	 * @param bInHovered - true if the parent actor is hovered
	 */
	void SetHovered_GameThread(const bool bInHovered);

	/**
	 * Updates the hidden editor view visibility map on the game thread which just enqueues a command on the render thread
	 */
	void SetHiddenEdViews_GameThread( uint64 InHiddenEditorViews );

	/** @return True if the primitive is visible in the given View. */
	ENGINE_API bool IsShown(const FSceneView* View) const;

	/** @return True if the primitive is casting a shadow. */
	ENGINE_API bool IsShadowCast(const FSceneView* View) const;

	/** Helper for components that want to render bounds. */
	ENGINE_API void RenderBounds(FPrimitiveDrawInterface* PDI, const FEngineShowFlags& EngineShowFlags, const FBoxSphereBounds& Bounds, bool bRenderInEditor) const;

	/** Verifies that a material used for rendering was present in the component's GetUsedMaterials list. */
	ENGINE_API void VerifyUsedMaterial(const class FMaterialRenderProxy* MaterialRenderProxy) const;

	/** Returns the LOD that the primitive will render at for this view. */
	virtual int32 GetLOD(const FSceneView* View) const { return INDEX_NONE; }
	
	/**
	 * Creates the hit proxies are used when DrawDynamicElements is called.
	 * Called in the game thread.
	 * @param OutHitProxies - Hit proxes which are created should be added to this array.
	 * @return The hit proxy to use by default for elements drawn by DrawDynamicElements.
	 */
	ENGINE_API virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component,TArray<TRefCountPtr<HHitProxy> >& OutHitProxies);

	/**
	 * Draws the primitive's static elements.  This is called from the rendering thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * @param PDI - The interface which receives the primitive elements.
	 */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) {}

	/** Gathers a description of the mesh elements to be rendered for the given LOD index, without consideration for views. */
	virtual void GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const {}

	/** Gathers shadow shapes from this proxy. */
	virtual void GetShadowShapes(TArray<FCapsuleShape>& CapsuleShapes) const {}

	/** 
	 * Gathers the primitive's dynamic mesh elements.  This will only be called if GetViewRelevance declares dynamic relevance.
	 * This is called from the rendering thread for each set of views that might be rendered.  
	 * Game thread state like UObjects must have their properties mirrored on the proxy to avoid race conditions.  The rendering thread must not dereference UObjects.
	 * The gathered mesh elements will be used multiple times, any memory referenced must last as long as the Collector (eg no stack memory should be referenced).
	 * This function should not modify the proxy but simply collect a description of things to render.  Updates to the proxy need to be pushed from game thread or external events.
	 *
	 * @param Views - the array of views to consider.  These may not exist in the ViewFamily.
	 * @param ViewFamily - the view family, for convenience
	 * @param VisibilityMap - a bit representing this proxy's visibility in the Views array
	 * @param Collector - gathers the mesh elements to be rendered and provides mechanisms for temporary allocations
	 */
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, class FMeshElementCollector& Collector) const {}

	/** 
	 * Gets the boxes for sub occlusion queries
	 * @param View - the view the occlusion results are for
	 * @return pointer to the boxes, must remain valid until the queries are built
	 */
	virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const 
	{
		return nullptr;
	}

	/** 
	 * Gives the primitive the results of sub-occlusion-queries
	 * @param View - the view the occlusion results are for
	 * @param Results - visibility results, allocated from the scene allocator, so valid until the end of the frame
	 * @param NumResults - number of visibility bools
	 */
	virtual void AcceptOcclusionResults(const FSceneView* View, TArray<bool>* Results, int32 ResultsStart, int32 NumResults) {}

	/**
	 * Determines the relevance of this primitive's elements to the given view.
	 * Called in the rendering thread.
	 * @param View - The view to determine relevance for.
	 * @return The relevance of the primitive's elements to the view.
	 */
	ENGINE_API virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const;

	/** Callback from the renderer to gather simple lights that this proxy wants renderered. */
	virtual void GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const {}

	/**
	 *	Determines the relevance of this primitive's elements to the given light.
	 *	@param	LightSceneProxy			The light to determine relevance for
	 *	@param	bDynamic (output)		The light is dynamic for this primitive
	 *	@param	bRelevant (output)		The light is relevant for this primitive
	 *	@param	bLightMapped (output)	The light is light mapped for this primitive
	 */
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const
	{
		// Determine the lights relevance to the primitive.
		bDynamic = true;
		bRelevant = true;
		bLightMapped = false;
		bShadowMapped = false;
	}

	virtual void GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FVector2D& OutDistanceMinMax, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, float& SelfShadowBias, TArray<FMatrix>& ObjectLocalToWorldTransforms) const 
	{
		LocalVolumeBounds = FBox(ForceInit);
		OutDistanceMinMax = FVector2D(0, 0);
		OutBlockMin = FIntVector(-1, -1, -1);
		OutBlockSize = FIntVector(0, 0, 0);
		bOutBuiltAsIfTwoSided = false;
		bMeshWasPlane = false;
		SelfShadowBias = 0;
	}

	virtual void GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const
	{
		NumInstances = 0;
		BoundsSurfaceArea = 0;
	}

	virtual bool HeightfieldHasPendingStreaming() const { return false; }

	virtual void GetHeightfieldRepresentation(UTexture2D*& OutHeightmapTexture, UTexture2D*& OutDiffuseColorTexture, FHeightfieldComponentDescription& OutDescription)
	{
		OutHeightmapTexture = NULL;
	}

	/**
	 *	Called when the rendering thread adds the proxy to the scene.
	 *	This function allows for generating renderer-side resources.
	 *	Called in the rendering thread.
	 */
	virtual void CreateRenderThreadResources() {}

	/**
	 * Called by the rendering thread to notify the proxy when a light is no longer
	 * associated with the proxy, so that it can clean up any cached resources.
	 * @param Light - The light to be removed.
	 */
	virtual void OnDetachLight(const FLightSceneInfo* Light)
	{
	}

	/**
	 * Called to notify the proxy when its transform has been updated.
	 * Called in the thread that owns the proxy; game or rendering.
	 */
	virtual void OnTransformChanged()
	{
	}

	/**
	 * Called to notify the proxy that the level has been fully added to
	 * the world and the primitive will now be rendered.
	 * Only called if bNeedsLevelAddedToWorldNotification is set to true.
	 */
	virtual void OnLevelAddedToWorld() {}

	/**
	* @return true if the proxy can be culled when occluded by other primitives
	*/
	virtual bool CanBeOccluded() const
	{
		return true;
	}

	/**
	* @return true if the proxy has custom occlusion queries
	*/
	virtual bool HasSubprimitiveOcclusionQueries() const
	{
		return false;
	}

	virtual bool ShowInBSPSplitViewmode() const
	{
		return false;
	}

	/**
	 * Determines the DPG to render the primitive in regardless of view.
	 * Should only be called if HasViewDependentDPG()==true.
	 */
	virtual uint8 GetStaticDepthPriorityGroup() const
	{
		check(!HasViewDependentDPG());
		return StaticDepthPriorityGroup;
	}

	/**
	 * Determines the DPG to render the primitive in for the given view.
	 * May be called regardless of the result of HasViewDependentDPG.
	 * @param View - The view to determine the primitive's DPG for.
	 * @return The DPG the primitive should be rendered in for the given view.
	 */
	uint8 GetDepthPriorityGroup(const FSceneView* View) const
	{
		return (bUseViewOwnerDepthPriorityGroup && IsOwnedBy(View->ViewActor)) ?
			ViewOwnerDepthPriorityGroup :
			StaticDepthPriorityGroup;
	}

	/** Every derived class should override these functions */
	virtual uint32 GetMemoryFootprint( void ) const = 0;
	uint32 GetAllocatedSize( void ) const { return( Owners.GetAllocatedSize() ); }

	/**
	 * Set the collision flag on the scene proxy to enable/disable collision drawing
	 *
	 * @param const bool bNewEnabled new state for collision drawing
	 */
	void SetCollisionEnabled_GameThread(const bool bNewEnabled);

	/**
	 * Set the collision flag on the scene proxy to enable/disable collision drawing (RENDER THREAD)
	 *
	 * @param const bool bNewEnabled new state for collision drawing
	 */
	void SetCollisionEnabled_RenderThread(const bool bNewEnabled);

	/**
	* Set the custom depth enabled flag
	*
	* @param the new value
	*/
	void SetCustomDepthEnabled_GameThread(const bool bInRenderCustomDepth);

	/**
	* Set the custom depth enabled flag (RENDER THREAD)
	*
	* @param the new value
	*/
	void SetCustomDepthEnabled_RenderThread(const bool bInRenderCustomDepth);

	/**
	* Set the custom depth stencil value
	*
	* @param the new value
	*/
	void SetCustomDepthStencilValue_GameThread(const int32 InCustomDepthStencilValue );

	/**
	* Set the custom depth stencil value (RENDER THREAD)
	*
	* @param the new value
	*/
	void SetCustomDepthStencilValue_RenderThread(const int32 InCustomDepthStencilValue);

	// Accessors.
	inline FSceneInterface& GetScene() const { return *Scene; }
	inline FPrimitiveComponentId GetPrimitiveComponentId() const { return PrimitiveComponentId; }
	inline FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const { return PrimitiveSceneInfo; }
	inline const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
	inline bool IsLocalToWorldDeterminantNegative() const { return bIsLocalToWorldDeterminantNegative; }
	inline const FBoxSphereBounds& GetBounds() const { return Bounds; }
	inline const FBoxSphereBounds& GetLocalBounds() const { return LocalBounds; }
	inline FName GetOwnerName() const { return OwnerName; }
	inline FName GetResourceName() const { return ResourceName; }
	inline FName GetLevelName() const { return LevelName; }
	FORCEINLINE TStatId GetStatId() const 
	{ 
		return StatId; 
	}	
	inline float GetMinDrawDistance() const { return MinDrawDistance; }
	inline float GetMaxDrawDistance() const { return MaxDrawDistance; }
	inline int32 GetVisibilityId() const { return VisibilityId; }
	inline int16 GetTranslucencySortPriority() const { return TranslucencySortPriority; }
	inline bool HasMotionBlurVelocityMeshes() const { return bHasMotionBlurVelocityMeshes; }

	inline bool IsMovable() const 
	{ 
		// Note: primitives with EComponentMobility::Stationary can still move (as opposed to lights with EComponentMobility::Stationary)
		return Mobility == EComponentMobility::Movable || Mobility == EComponentMobility::Stationary; 
	}

	inline bool IsOftenMoving() const { return Mobility == EComponentMobility::Movable; }

	inline bool IsMeshShapeOftenMoving() const 
	{ 
		return Mobility == EComponentMobility::Movable || !bGoodCandidateForCachedShadowmap; 
	}

	inline bool IsStatic() const { return Mobility == EComponentMobility::Static; }
	inline bool IsSelectable() const { return bSelectable; }
	inline bool IsParentSelected() const { return bParentSelected; }
	inline bool IsIndividuallySelected() const { return bIndividuallySelected; }
	inline bool IsSelected() const { return IsParentSelected() || IsIndividuallySelected(); }
	inline bool WantsSelectionOutline() const { return bWantsSelectionOutline; }
	inline bool ShouldRenderCustomDepth() const { return bRenderCustomDepth; }
	inline uint8 GetCustomDepthStencilValue() const { return CustomDepthStencilValue; }
	inline EStencilMask GetStencilWriteMask() const { return CustomDepthStencilWriteMask; }
	inline uint8 GetLightingChannelMask() const { return LightingChannelMask; }
	inline uint8 GetLightingChannelStencilValue() const 
	{ 
		// Flip the default channel bit so that the default value is 0, to align with the default stencil clear value and GBlackTexture value
		return (LightingChannelMask & 0x6) | (~LightingChannelMask & 0x1); 
	}
	inline bool IsVisibleInReflectionCaptures() const { return bVisibleInReflectionCaptures; }
	inline bool ShouldRenderInMainPass() const { return bRenderInMainPass; }
	inline bool IsCollisionEnabled() const { return bCollisionEnabled; }
	inline bool IsHovered() const { return bHovered; }
	inline bool IsOwnedBy(const AActor* Actor) const { return Owners.Find(Actor) != INDEX_NONE; }
	inline bool HasViewDependentDPG() const { return bUseViewOwnerDepthPriorityGroup; }
	inline bool HasStaticLighting() const { return bStaticLighting; }
	inline bool NeedsUnbuiltPreviewLighting() const { return bNeedsUnbuiltPreviewLighting; }
	inline bool CastsStaticShadow() const { return bCastStaticShadow; }
	inline bool CastsDynamicShadow() const { return bCastDynamicShadow; }
	inline bool AffectsDynamicIndirectLighting() const { return bAffectDynamicIndirectLighting; }
	inline bool AffectsDistanceFieldLighting() const { return bAffectDistanceFieldLighting; }
	inline float GetLpvBiasMultiplier() const { return LpvBiasMultiplier; }
	inline EIndirectLightingCacheQuality GetIndirectLightingCacheQuality() const { return IndirectLightingCacheQuality; }
	inline bool CastsVolumetricTranslucentShadow() const { return bCastVolumetricTranslucentShadow; }
	inline bool CastsCapsuleDirectShadow() const { return bCastCapsuleDirectShadow; }
	inline bool CastsDynamicIndirectShadow() const { return bCastsDynamicIndirectShadow; }
	inline float GetDynamicIndirectShadowMinVisibility() const { return DynamicIndirectShadowMinVisibility; }
	inline bool CastsHiddenShadow() const { return bCastHiddenShadow; }
	inline bool CastsShadowAsTwoSided() const { return bCastShadowAsTwoSided; }
	inline bool CastsSelfShadowOnly() const { return bSelfShadowOnly; }
	inline bool CastsInsetShadow() const { return bCastInsetShadow; }
	inline bool CastsCinematicShadow() const { return bCastCinematicShadow; }
	inline bool CastsFarShadow() const { return bCastFarShadow; }
	inline bool LightAsIfStatic() const { return bLightAsIfStatic; }
	inline bool LightAttachmentsAsGroup() const { return bLightAttachmentsAsGroup; }
	ENGINE_API bool UseSingleSampleShadowFromStationaryLights() const;
	inline bool StaticElementsAlwaysUseProxyPrimitiveUniformBuffer() const { return bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer; }
	inline bool ShouldUseAsOccluder() const { return bUseAsOccluder; }
	inline bool AllowApproximateOcclusion() const { return bAllowApproximateOcclusion; }
	inline const TUniformBuffer<FPrimitiveUniformShaderParameters>& GetUniformBuffer() const 
	{
		return UniformBuffer; 
	}
	inline bool HasPerInstanceHitProxies () const { return bHasPerInstanceHitProxies; }
	inline bool UseEditorCompositing(const FSceneView* View) const { return GIsEditor && bUseEditorCompositing && !View->bIsGameView; }
	inline const FVector& GetActorPosition() const { return ActorPosition; }
	inline const bool ReceivesDecals() const { return bReceivesDecals; }
	inline const bool RenderInMono() const { return bRenderInMono; }
	inline bool WillEverBeLit() const { return bWillEverBeLit; }
	inline bool HasValidSettingsForStaticLighting() const { return bHasValidSettingsForStaticLighting; }
	inline bool AlwaysHasVelocity() const { return bAlwaysHasVelocity; }
	inline bool UseEditorDepthTest() const { return bUseEditorDepthTest; }
	inline bool SupportsDistanceFieldRepresentation() const { return bSupportsDistanceFieldRepresentation; }
	inline bool SupportsHeightfieldRepresentation() const { return bSupportsHeightfieldRepresentation; }
	inline bool IsFlexFluidSurface() const { return bFlexFluidSurface; }	
	// WaveWorks Start
	inline bool IsQuadTreeWaveWorks() const { return bQuadTreeWaveWorks; }
	inline class FWaveWorksResource* GetWaveWorksResource() const { return WaveWorksResource; }
	// WaveWorks End	
	inline bool TreatAsBackgroundForOcclusion() const { return bTreatAsBackgroundForOcclusion; }
	inline bool NeedsLevelAddedToWorldNotification() const { return bNeedsLevelAddedToWorldNotification; }
	inline bool IsComponentLevelVisible() const { return bIsComponentLevelVisible; }
	inline bool IsStaticPathAvailable() const { return !bDisableStaticPath; }
	inline bool ShouldReceiveCombinedCSMAndStaticShadowsFromStationaryLights() const { return bReceiveCombinedCSMAndStaticShadowsFromStationaryLights; }

#if WITH_EDITOR
	inline int32 GetNumUncachedStaticLightingInteractions() { return NumUncachedStaticLightingInteractions; }

	/** This function exist only to perform an update of the UsedMaterialsForVerification on the render thread*/
	ENGINE_API void SetUsedMaterialForVerification(const TArray<UMaterialInterface*>& InUsedMaterialsForVerification);
#endif

	inline FLinearColor GetWireframeColor() const { return WireframeColor; }
	inline FLinearColor GetLevelColor() const { return LevelColor; }
	inline FLinearColor GetPropertyColor() const { return PropertyColor; }

	/**
	* Returns whether this proxy should be considered a "detail mesh".
	* Detail meshes are distance culled even if distance culling is normally disabled for the view. (e.g. in editor)
	*/
	virtual bool IsDetailMesh() const
	{
		return false;
	}

	/**
	 *	Returns whether the proxy utilizes custom occlusion bounds or not
	 *
	 *	@return	bool		true if custom occlusion bounds are used, false if not;
	 */
	virtual bool HasCustomOcclusionBounds() const
	{
		return false;
	}

	/**
	 *	Return the custom occlusion bounds for this scene proxy.
	 *	
	 *	@return	FBoxSphereBounds		The custom occlusion bounds.
	 */
	virtual FBoxSphereBounds GetCustomOcclusionBounds() const
	{
		checkf(false, TEXT("GetCustomOcclusionBounds> Should not be called on base scene proxy!"));
		return GetBounds();
	}

	virtual bool HasDistanceFieldRepresentation() const
	{
		return false;
	}

	virtual bool HasDynamicIndirectShadowCasterRepresentation() const
	{
		return false;
	}

	/** 
	 * Drawing helper. Draws nice bouncy line.
	 */
	static ENGINE_API void DrawArc(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color
		, uint8 DepthPriorityGroup,	const float Thickness = 0.0f, const bool bScreenSpace = false);
	
	static ENGINE_API void DrawArrowHead(FPrimitiveDrawInterface* PDI, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color
		, uint8 DepthPriorityGroup,	const float Thickness = 0.0f, const bool bScreenSpace = false);


	/**
	 * Shifts primitive position and all relevant data by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	ENGINE_API virtual void ApplyWorldOffset(FVector InOffset);

	/**
	 * Applies a "late in the frame" adjustment to the proxy's existing transform
	 * @param LateUpdateTransform - The post-transform to be applied to the LocalToWorld matrix
	 */
	ENGINE_API virtual void ApplyLateUpdateTransform(const FMatrix& LateUpdateTransform);

	/**
	 * Updates the primitive proxy's uniform buffer.
	 */
	ENGINE_API void UpdateUniformBuffer();
	/**
	 * Updates the primitive proxy's uniform buffer.
	 */
	ENGINE_API bool NeedsUniformBufferUpdate() const;

#if !UE_BUILD_SHIPPING

	struct ENGINE_API FDebugMassData
	{
		//Local here just means local to ElemTM which can be different depending on how the component uses the mass data
		FQuat LocalTensorOrientation;
		FVector LocalCenterOfMass;
		FVector MassSpaceInertiaTensor;
		int32 BoneIndex;

		void DrawDebugMass(class FPrimitiveDrawInterface* PDI, const FTransform& ElemTM) const;
	};

	TArray<FDebugMassData> DebugMassData;

	/** Sets the primitive proxy's mass space to component space. Useful for debugging physics center of mass and inertia tensor*/
	ENGINE_API virtual void SetDebugMassData(const TArray<FDebugMassData>& InDebugMassData);
#endif

	/**
	 * Get the list of LCIs. Used to set the precomputed lighting uniform buffers, which can only be created by the RENDERER_API.
	 */
	typedef TArray<class FLightCacheInterface*, TInlineAllocator<8> > FLCIArray;
	ENGINE_API virtual void GetLCIs(FLCIArray& LCIs) {}

#if WITH_EDITORONLY_DATA
	/**
	 * Get primitive distance to view origin for a given LOD-section.
	 * @param LODIndex					LOD index (INDEX_NONE for all) 
	 * @param ElementIndex				Element index (INDEX_NONE for all)
	 * @param PrimitiveDistance (OUT)	LOD-section distance to view
	 * @return							Whether distance was computed or not
	 */
	ENGINE_API virtual bool GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const;

	/**
	 * Get mesh UV density for a LOD-section.
	 * @param LODIndex					LOD index (INDEX_NONE for all) 
	 * @param ElementIndex				Element index (INDEX_NONE for all)
	 * @param WorldUVDensities (OUT)	UV density in world units for each UV channel
	 * @return							Whether the densities were computed or not.
	 */
	ENGINE_API virtual bool GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const;

	/**
	 * Get mesh UV density for a LOD-section.
	 * @param LODIndex					LOD index (INDEX_NONE for all) 
	 * @param ElementIndex				Element index (INDEX_NONE for all)
	 * @param MaterialRenderProxy		Material bound to that LOD-section
	 * @param OneOverScales (OUT)		One over the texture scales (array size = TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL / 4)
	 * @param UVChannelIndices (OUT)	The related index for each (array size = TEXSTREAM_MAX_NUM_TEXTURES_PER_MATERIAL / 4)
	 * @return							Whether scales were computed or not.
	 */
	ENGINE_API virtual bool GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const class FMaterialRenderProxy* MaterialRenderProxy, FVector4* OneOverScales, struct FIntVector4* UVChannelIndices) const;
#endif

	/**
	* Get the lightmap resolution for this primitive. Used in VMI_LightmapDensity.
	*/
	virtual int32 GetLightMapResolution() const { return 0; }

	// NvFlow begin
	FPrimitiveSceneProxyNvFlow FlowData;
	// NvFlow end

protected:

	/** Allow subclasses to override the primitive name. Used primarily by BSP. */
	void OverrideOwnerName(FName InOwnerName)
	{
		OwnerName = InOwnerName;
	}

	FLinearColor WireframeColor;
	FLinearColor LevelColor;
	FLinearColor PropertyColor;

private:
	friend class FScene;

	EComponentMobility::Type Mobility;

	uint32 bIsLocalToWorldDeterminantNegative : 1;
	uint32 DrawInGame : 1;
	uint32 DrawInEditor : 1;
	uint32 bRenderInMono : 1;
	uint32 bReceivesDecals : 1;
	uint32 bOnlyOwnerSee : 1;
	uint32 bOwnerNoSee : 1;
	uint32 bOftenMoving : 1;
	/** Parent Actor is selected */
	uint32 bParentSelected : 1;
	/** Component is selected directly */
	uint32 bIndividuallySelected : 1;
	
	/** true if the mouse is currently hovered over this primitive in a level viewport */
	uint32 bHovered : 1;

	/** true if ViewOwnerDepthPriorityGroup should be used. */
	uint32 bUseViewOwnerDepthPriorityGroup : 1;

	/** true if the primitive has motion blur velocity meshes */
	uint32 bHasMotionBlurVelocityMeshes : 1;

	/** DPG this prim belongs to. */
	uint32 StaticDepthPriorityGroup : SDPG_NumBits;

	/** DPG this primitive is rendered in when viewed by its owner. */
	uint32 ViewOwnerDepthPriorityGroup : SDPG_NumBits;

	/** True if the primitive will cache static lighting. */
	uint32 bStaticLighting : 1;

	/** True if the primitive should be visible in reflection captures. */
	uint32 bVisibleInReflectionCaptures : 1;

	/** If true this primitive Renders in the mainPass */
	uint32 bRenderInMainPass : 1;

	/** If true this primitive will render only after owning level becomes visible */
	uint32 bRequiresVisibleLevelToRender : 1;

	/** Whether component level is currently visible */
	uint32 bIsComponentLevelVisible : 1;
	
	/** Whether this component has any collision enabled */
	uint32 bCollisionEnabled : 1;

	/** Whether the primitive should be treated as part of the background for occlusion purposes. */
	uint32 bTreatAsBackgroundForOcclusion : 1;

	friend class FLightPrimitiveInteraction;
	/** Whether the renderer needs us to temporarily use only the dynamic drawing path */
	uint32 bDisableStaticPath : 1;

protected:

	/** Whether this proxy's mesh is unlikely to be constantly changing. */
	uint32 bGoodCandidateForCachedShadowmap : 1;

	/** Whether the primitive should be statically lit but has unbuilt lighting, and a preview should be used. */
	uint32 bNeedsUnbuiltPreviewLighting : 1;

	/** True if the primitive wants to use static lighting, but has invalid content settings to do so. */
	uint32 bHasValidSettingsForStaticLighting : 1;

	/** Can be set to false to skip some work only needed on lit primitives. */
	uint32 bWillEverBeLit : 1;

	/** True if the primitive casts dynamic shadows. */
	uint32 bCastDynamicShadow : 1;

	/** True if the primitive casts Reflective Shadow Map shadows (meaning it affects Light Propagation Volumes). */
	uint32 bAffectDynamicIndirectLighting : 1;

	uint32 bAffectDistanceFieldLighting : 1;

	/** True if the primitive casts static shadows. */
	uint32 bCastStaticShadow : 1;

	/** 
	 * Whether the object should cast a volumetric translucent shadow.
	 * Volumetric translucent shadows are useful for primitives with smoothly changing opacity like particles representing a volume, 
	 * But have artifacts when used on highly opaque surfaces.
	 */
	uint32 bCastVolumetricTranslucentShadow : 1;

	/** Whether the primitive should use capsules for direct shadowing, if present.  Forces inset shadows. */
	uint32 bCastCapsuleDirectShadow : 1;

	/** Whether the primitive should use an inset indirect shadow from capsules or mesh distance fields. */
	uint32 bCastsDynamicIndirectShadow : 1;

	/** True if the primitive casts shadows even when hidden. */
	uint32 bCastHiddenShadow : 1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	uint32 bCastShadowAsTwoSided : 1;

	/** When enabled, the component will only cast a shadow on itself and not other components in the world.  This is especially useful for first person weapons, and forces bCastInsetShadow to be enabled. */
	uint32 bSelfShadowOnly : 1;

	/** Whether this component should create a per-object shadow that gives higher effective shadow resolution. true if bSelfShadowOnly is true. */
	uint32 bCastInsetShadow : 1;

	/** 
	 * Whether this component should create a per-object shadow that gives higher effective shadow resolution. 
	 * Useful for cinematic character shadowing. Assumed to be enabled if bSelfShadowOnly is enabled.
	 */
	uint32 bCastCinematicShadow : 1;

	/* When enabled, the component will be rendering into the distant shadow cascades (only for directional lights). */
	uint32 bCastFarShadow : 1;

	/** 
	 * This has to be known by the rendering thread to avoid marking lighting dirty when new interactions are created,
	 * Which happens when a movable mesh with bLightAsIfStatic moves into the influence of a light it was not baked against.
	 */
	uint32 bLightAsIfStatic : 1;

	/** 
	 * Whether to light this component and any attachments as a group.  This only has effect on the root component of an attachment tree.
	 * When enabled, attached component shadowing settings like bCastInsetShadow, bCastVolumetricTranslucentShadow, etc, will be ignored.
	 * This is useful for improving performance when multiple movable components are attached together.
	 */
	uint32 bLightAttachmentsAsGroup : 1;

	/** 
	 * Whether the whole component should be shadowed as one from stationary lights, which makes shadow receiving much cheaper.
	 * When enabled shadowing data comes from the volume lighting samples precomputed by Lightmass, which are very sparse.
	 * This is currently only used on stationary directional lights.  
	 */
	uint32 bSingleSampleShadowFromStationaryLights : 1;

	/** 
	 * Whether this proxy always uses UniformBuffer and no other uniform buffers.  
	 * When true, a fast path for updating can be used that does not update static draw lists.
	 */
	uint32 bStaticElementsAlwaysUseProxyPrimitiveUniformBuffer : 1;

	/** Whether the primitive should always be considered to have velocities, even if it hasn't moved. */
	uint32 bAlwaysHasVelocity : 1;

	/** Whether editor compositing depth testing should be used for this primitive.  Only matters for primitives with bUseEditorCompositing. */
	uint32 bUseEditorDepthTest : 1;

	/** Whether the primitive type supports a distance field representation.  Does not mean the primitive has a valid representation. */
	uint32 bSupportsDistanceFieldRepresentation : 1;

	/** Whether the primitive implements GetHeightfieldRepresentation() */
	uint32 bSupportsHeightfieldRepresentation : 1;

	/** Whether this primitive requires notification when its level is added to the world and made visible for the first time. */
	uint32 bNeedsLevelAddedToWorldNotification : 1;

	/** true by default, if set to false will make given proxy never drawn with selection outline */
	uint32 bWantsSelectionOutline : 1;

	uint32 bVerifyUsedMaterials : 1;

	/** Whether the primitive has Flex fluid surface functionality */
	uint32 bFlexFluidSurface : 1;	// WaveWorks Start
	/** Whether the primitive is a WaveWorks primitive */
	uint32 bQuadTreeWaveWorks : 1;
	class FWaveWorksResource* WaveWorksResource;
	// WaveWorks Endprivate:

	/** If this is True, this primitive will be used to occlusion cull other primitives. */
	uint32 bUseAsOccluder:1;

	/** If this is True, this primitive doesn't need exact occlusion info. */
	uint32 bAllowApproximateOcclusion : 1;

	/** If this is True, this primitive can be selected in the editor. */
	uint32 bSelectable : 1;

	/** If this primitive has per-instance hit proxies. */
	uint32 bHasPerInstanceHitProxies : 1;

	/** Whether this primitive should be composited onto the scene after post processing (editor only) */
	uint32 bUseEditorCompositing : 1;

	/** Should this primitive receive dynamic-only CSM shadows */
	uint32 bReceiveCombinedCSMAndStaticShadowsFromStationaryLights : 1;
		
	/** This primitive has bRenderCustomDepth enabled */
	uint32 bRenderCustomDepth : 1;

	/** Optionally write this stencil value during the CustomDepth pass */
	uint8 CustomDepthStencilValue;

	/** When writing custom depth stencil, use this write mask */
	EStencilMask CustomDepthStencilWriteMask;

	uint8 LightingChannelMask;

protected:
	/** The bias applied to LPV injection */
	float LpvBiasMultiplier;

	/** Quality of interpolated indirect lighting for Movable components. */
	EIndirectLightingCacheQuality IndirectLightingCacheQuality;

	/** Min visibility for capsule shadows. */
	float DynamicIndirectShadowMinVisibility;

	float DistanceFieldSelfShadowBias;

private:
	/** The primitive's local to world transform. */
	FMatrix LocalToWorld;

	/** The primitive's bounds. */
	FBoxSphereBounds Bounds;

	/** The primitive's local space bounds. */
	FBoxSphereBounds LocalBounds;

	/** The component's actor's position. */
	FVector ActorPosition;

	/** The hierarchy of owners of this primitive.  These must not be dereferenced on the rendering thread, but the pointer values can be used for identification.  */
	TArray<const AActor*> Owners;

	/** The scene the primitive is in. */
	FSceneInterface* Scene;

	/** 
	 * Id for the component this proxy belongs to.  
	 * This will stay the same for the lifetime of the component, so it can be used to identify the component across re-registers.
	 */
	FPrimitiveComponentId PrimitiveComponentId;

	/** Pointer back to the PrimitiveSceneInfo that owns this Proxy. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The name of the actor this component is attached to. */
	FName OwnerName;

	/** The name of the resource used by the component. */
	FName ResourceName;

	/** The name of the level the primitive is in. */
	FName LevelName;

#if WITH_EDITOR
	/** A copy of the actor's group membership for handling per-view group hiding */
	uint64 HiddenEditorViews;
#endif

	/** The translucency sort priority */
	int16 TranslucencySortPriority;

	/** Used for precomputed visibility */
	int32 VisibilityId;

	/** Used for dynamic stats */
	TStatId StatId;

	/** The primitive's cull distance. */
	float MaxDrawDistance;

	/** The primitive's minimum cull distance. */
	float MinDrawDistance;

	/** The primitive's uniform buffer. */
	TUniformBuffer<FPrimitiveUniformShaderParameters> UniformBuffer;

	/** 
	 * The UPrimitiveComponent this proxy is for, useful for quickly inspecting properties on the corresponding component while debugging.
	 * This should not be dereferenced on the rendering thread.  The game thread can be modifying UObject members at any time.
	 * Use PrimitiveComponentId instead when a component identifier is needed.
	 */
	const UPrimitiveComponent* ComponentForDebuggingOnly;

#if WITH_EDITOR
	/**
	*	How many invalid lights for this primitive, just refer for scene outliner
	*/
	int32 NumUncachedStaticLightingInteractions;

	TArray<UMaterialInterface*> UsedMaterialsForVerification;
#endif

	/**
	 * Updates the primitive proxy's cached transforms, and calls OnUpdateTransform to notify it of the change.
	 * Called in the thread that owns the proxy; game or rendering.
	 * @param InLocalToWorld - The new local to world transform of the primitive.
	 * @param InBounds - The new bounds of the primitive.
	 * @param InLocalBounds - The local space bounds of the primitive.
	 */
	ENGINE_API void SetTransform(const FMatrix& InLocalToWorld, const FBoxSphereBounds& InBounds, const FBoxSphereBounds& InLocalBounds, FVector InActorPosition);

	/**
	 * Either updates the uniform buffer or defers it until it becomes visible depending on a cvar
	 */
	ENGINE_API void UpdateUniformBufferMaybeLazy();

	/** Updates the hidden editor view visibility map on the render thread */
	void SetHiddenEdViews_RenderThread( uint64 InHiddenEditorViews );

protected:
	/** Updates selection for the primitive proxy. This is called in the rendering thread by SetSelection_GameThread. */
	void SetSelection_RenderThread(const bool bInParentSelected, const bool bInIndividuallySelected);

	/** Updates hover state for the primitive proxy. This is called in the rendering thread by SetHovered_GameThread. */
	void SetHovered_RenderThread(const bool bInHovered);
};
