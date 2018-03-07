// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCore.h: Core scene definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "HitProxies.h"
#include "MeshBatch.h"

class FLightSceneInfo;
class FPrimitiveSceneInfo;
class FScene;
class FStaticMeshDrawListBase;
class UExponentialHeightFogComponent;

/**
 * An interaction between a light and a primitive.
 */
class FLightPrimitiveInteraction
{
public:

	/** Creates an interaction for a light-primitive pair. */
	static void InitializeMemoryPool();
	static void Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo);
	static void Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction);

	/** Returns current size of memory pool */
	static uint32 GetMemoryPoolSize();

	// Accessors.
	bool HasShadow() const { return bCastShadow; }
	bool IsLightMapped() const { return bLightMapped; }
	bool IsDynamic() const { return bIsDynamic; }
	bool IsShadowMapped() const { return bIsShadowMapped; }
	bool IsUncachedStaticLighting() const { return bUncachedStaticLighting; }
	bool HasTranslucentObjectShadow() const { return bHasTranslucentObjectShadow; }
	bool HasInsetObjectShadow() const { return bHasInsetObjectShadow; }
	bool CastsSelfShadowOnly() const { return bSelfShadowOnly; }
	bool IsES2DynamicPointLight() const { return bES2DynamicPointLight; }
	FLightSceneInfo* GetLight() const { return LightSceneInfo; }
	int32 GetLightId() const { return LightId; }
	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const { return PrimitiveSceneInfo; }
	FLightPrimitiveInteraction* GetNextPrimitive() const { return NextPrimitive; }
	FLightPrimitiveInteraction* GetNextLight() const { return NextLight; }

	/** Hash function required for TMap support */
	friend uint32 GetTypeHash( const FLightPrimitiveInteraction* Interaction )
	{
		return (uint32)Interaction->LightId;
	}

	/** Clears cached shadow maps, if possible */
	void FlushCachedShadowMapData();

	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

private:
	/** The light which affects the primitive. */
	FLightSceneInfo* LightSceneInfo;

	/** The primitive which is affected by the light. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** A pointer to the NextPrimitive member of the previous interaction in the light's interaction list. */
	FLightPrimitiveInteraction** PrevPrimitiveLink;

	/** The next interaction in the light's interaction list. */
	FLightPrimitiveInteraction* NextPrimitive;

	/** A pointer to the NextLight member of the previous interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction** PrevLightLink;

	/** The next interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction* NextLight;

	/** The index into Scene->Lights of the light which affects the primitive. */
	int32 LightId;

	/** True if the primitive casts a shadow from the light. */
	uint32 bCastShadow : 1;

	/** True if the primitive has a light-map containing the light. */
	uint32 bLightMapped : 1;

	/** True if the interaction is dynamic. */
	uint32 bIsDynamic : 1;

	/** Whether the light's shadowing is contained in the primitive's static shadow map. */
	uint32 bIsShadowMapped : 1;

	/** True if the interaction is an uncached static lighting interaction. */
	uint32 bUncachedStaticLighting : 1;

	/** True if the interaction has a translucent per-object shadow. */
	uint32 bHasTranslucentObjectShadow : 1;

	/** True if the interaction has an inset per-object shadow. */
	uint32 bHasInsetObjectShadow : 1;

	/** True if the primitive only shadows itself. */
	uint32 bSelfShadowOnly : 1;

	/** True this is an ES2 dynamic point light interaction. */
	uint32 bES2DynamicPointLight : 1;	

	/** Initialization constructor. */
	FLightPrimitiveInteraction(FLightSceneInfo* InLightSceneInfo,FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		bool bIsDynamic,bool bInLightMapped,bool bInIsShadowMapped, bool bInHasTranslucentObjectShadow, bool bInHasInsetObjectShadow);

	/** Hide dtor */
	~FLightPrimitiveInteraction();

};

/**
 * A mesh which is defined by a primitive at scene segment construction time and never changed.
 * Lights are attached and detached as the segment containing the mesh is added or removed from a scene.
 */
class FStaticMesh : public FMeshBatch
{
public:

	/**
	 * An interface to a draw list's reference to this static mesh.
	 * used to remove the static mesh from the draw list without knowing the draw list type.
	 */
	class FDrawListElementLink : public FRefCountedObject
	{
	public:
		virtual bool IsInDrawList(const class FStaticMeshDrawListBase* DrawList) const = 0;
		virtual void Remove(const bool bUnlinkMesh) = 0;
	};

	/** The screen space size to draw this primitive at */
	float ScreenSize;

	/** The render info for the primitive which created this mesh. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The index of the mesh in the scene's static meshes array. */
	int32 Id;

	/** Index of the mesh into the scene's StaticMeshBatchVisibility array. */
	int32 BatchVisibilityId;

	// Constructor/destructor.
	FStaticMesh(
		FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		const FMeshBatch& InMesh,
		float InScreenSize,
		FHitProxyId InHitProxyId
		):
		FMeshBatch(InMesh),
		ScreenSize(InScreenSize),
		PrimitiveSceneInfo(InPrimitiveSceneInfo),
		Id(INDEX_NONE),
		BatchVisibilityId(INDEX_NONE)
	{
		BatchHitProxyId = InHitProxyId;
	}

	~FStaticMesh();

	/** Adds a link from the mesh to its entry in a draw list. */
	void LinkDrawList(FDrawListElementLink* Link);

	/** Removes a link from the mesh to its entry in a draw list. */
	void UnlinkDrawList(FDrawListElementLink* Link);

	/** Adds the static mesh to the appropriate draw lists in a scene. */
	void AddToDrawLists(FRHICommandListImmediate& RHICmdList, FScene* Scene);

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	void AddToVXGIDrawLists(FRHICommandListImmediate& RHICmdList, FScene* Scene);
#endif
	// NVCHANGE_END: Add VXGI

	/** Removes the static mesh from all draw lists. */
	void RemoveFromDrawLists();

	/** Returns true if the mesh is linked to the given draw list. */
	bool IsLinkedToDrawList(const FStaticMeshDrawListBase* DrawList) const;

private:
	/** Links to the draw lists this mesh is an element of. */
	TArray<TRefCountPtr<FDrawListElementLink> > DrawListLinks;

	/** Private copy constructor. */
	FStaticMesh(const FStaticMesh& InStaticMesh):
		FMeshBatch(InStaticMesh),
		ScreenSize(InStaticMesh.ScreenSize),
		PrimitiveSceneInfo(InStaticMesh.PrimitiveSceneInfo),
		Id(InStaticMesh.Id),
		BatchVisibilityId(InStaticMesh.BatchVisibilityId)
	{}
};

/** The properties of a exponential height fog layer which are used for rendering. */
class FExponentialHeightFogSceneInfo
{
public:

	/** The fog component the scene info is for. */
	const UExponentialHeightFogComponent* Component;
	float FogHeight;
	float FogDensity;
	float FogHeightFalloff;
	float FogMaxOpacity;
	float StartDistance;
	float FogCutoffDistance;
	float LightTerminatorAngle;
	FLinearColor FogColor;
	float DirectionalInscatteringExponent;
	float DirectionalInscatteringStartDistance;
	FLinearColor DirectionalInscatteringColor;
	UTextureCube* InscatteringColorCubemap;
	float InscatteringColorCubemapAngle;
	float FullyDirectionalInscatteringColorDistance;
	float NonDirectionalInscatteringColorDistance;

	bool bEnableVolumetricFog;
	float VolumetricFogScatteringDistribution;
	FLinearColor VolumetricFogAlbedo;
	FLinearColor VolumetricFogEmissive;
	float VolumetricFogExtinctionScale;
	float VolumetricFogDistance;
	float VolumetricFogStaticLightingScatteringIntensity;
	bool bOverrideLightColorsWithFogInscatteringColors;

	/** Initialization constructor. */
	FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent);
};

/** Returns true if the indirect lighting cache can be used at all. */
extern bool IsIndirectLightingCacheAllowed(ERHIFeatureLevel::Type InFeatureLevel);

/** Returns true if the indirect lighting cache can use the volume texture atlas on this feature level. */
extern bool CanIndirectLightingCacheUseVolumeTexture(ERHIFeatureLevel::Type InFeatureLevel);
