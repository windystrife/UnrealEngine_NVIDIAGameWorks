// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMesh.h: Static mesh class definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Misc/Guid.h"
#include "Engine/EngineTypes.h"
#include "UObject/UObjectIterator.h"
#include "Templates/ScopedPointer.h"
#include "Materials/MaterialInterface.h"
#include "RenderResource.h"
#include "PackedNormal.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "RawIndexBuffer.h"
#include "Components.h"
#include "LocalVertexFactory.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Engine/MeshMerging.h"
#include "UObject/UObjectHash.h"
#include "MeshBatch.h"
#include "SceneManagement.h"
#include "Components/StaticMeshComponent.h"
// WaveWorks Start
#include "Components/WaveWorksStaticMeshComponent.h"
// WaveWorks End
#include "PhysicsEngine/BodySetupEnums.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/ColorVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexDataInterface.h"
#include "UniquePtr.h"
#include "WeightedRandomSampler.h"

class FDistanceFieldVolumeData;
class UBodySetup;

/** The maximum number of static mesh LODs allowed. */
#define MAX_STATIC_MESH_LODS 8

struct FStaticMaterial;

/**
 * The LOD settings to use for a group of static meshes.
 */
class FStaticMeshLODGroup
{
public:
	/** Default values. */
	FStaticMeshLODGroup()
		: DefaultNumLODs(1)
		, DefaultLightMapResolution(64)
		, BasePercentTrianglesMult(1.0f)
		, DisplayName( NSLOCTEXT( "UnrealEd", "None", "None" ) )
	{
		FMemory::Memzero(SettingsBias);
		SettingsBias.PercentTriangles = 1.0f;
	}

	/** Returns the default number of LODs to build. */
	int32 GetDefaultNumLODs() const
	{
		return DefaultNumLODs;
	}

	/** Returns the default lightmap resolution. */
	int32 GetDefaultLightMapResolution() const
	{
		return DefaultLightMapResolution;
	}

	/** Returns default reduction settings for the specified LOD. */
	FMeshReductionSettings GetDefaultSettings(int32 LODIndex) const
	{
		check(LODIndex >= 0 && LODIndex < MAX_STATIC_MESH_LODS);
		return DefaultSettings[LODIndex];
	}

	/** Applies global settings tweaks for the specified LOD. */
	ENGINE_API FMeshReductionSettings GetSettings(const FMeshReductionSettings& InSettings, int32 LODIndex) const;

private:
	/** FStaticMeshLODSettings initializes group entries. */
	friend class FStaticMeshLODSettings;
	/** The default number of LODs to build. */
	int32 DefaultNumLODs;
	/** Default lightmap resolution. */
	int32 DefaultLightMapResolution;
	/** An additional reduction of base meshes in this group. */
	float BasePercentTrianglesMult;
	/** Display name. */
	FText DisplayName;
	/** Default reduction settings for meshes in this group. */
	FMeshReductionSettings DefaultSettings[MAX_STATIC_MESH_LODS];
	/** Biases applied to reduction settings. */
	FMeshReductionSettings SettingsBias;
};

/**
 * Per-group LOD settings for static meshes.
 */
class FStaticMeshLODSettings
{
public:

	/**
	 * Initializes LOD settings by reading them from the passed in config file section.
	 * @param IniFile Preloaded ini file object to load from
	 */
	ENGINE_API void Initialize(const FConfigFile& IniFile);

	/** Retrieve the settings for the specified LOD group. */
	const FStaticMeshLODGroup& GetLODGroup(FName LODGroup) const
	{
		const FStaticMeshLODGroup* Group = Groups.Find(LODGroup);
		if (Group == NULL)
		{
			Group = Groups.Find(NAME_None);
		}
		check(Group);
		return *Group;
	}

	/** Retrieve the names of all defined LOD groups. */
	void GetLODGroupNames(TArray<FName>& OutNames) const;

	/** Retrieves the localized display names of all LOD groups. */
	void GetLODGroupDisplayNames(TArray<FText>& OutDisplayNames) const;

private:
	/** Reads an entry from the INI to initialize settings for an LOD group. */
	void ReadEntry(FStaticMeshLODGroup& Group, FString Entry);
	/** Per-group settings. */
	TMap<FName,FStaticMeshLODGroup> Groups;
};


/**
 * A set of static mesh triangles which are rendered with the same material.
 */
struct FStaticMeshSection
{
	/** The index of the material with which to render this section. */
	int32 MaterialIndex;

	/** Range of vertices and indices used when rendering this section. */
	uint32 FirstIndex;
	uint32 NumTriangles;
	uint32 MinVertexIndex;
	uint32 MaxVertexIndex;

	/** If true, collision is enabled for this section. */
	bool bEnableCollision;
	/** If true, this section will cast a shadow. */
	bool bCastShadow;

#if WITH_EDITORONLY_DATA
	/** The UV channel density in LocalSpaceUnit / UV Unit. */
	float UVDensities[MAX_STATIC_TEXCOORDS];
	/** The weigths to apply to the UV density, based on the area. */
	float Weights[MAX_STATIC_TEXCOORDS];
#endif

	/** Constructor. */
	FStaticMeshSection()
		: MaterialIndex(0)
		, FirstIndex(0)
		, NumTriangles(0)
		, MinVertexIndex(0)
		, MaxVertexIndex(0)
		, bEnableCollision(false)
		, bCastShadow(true)
	{
#if WITH_EDITORONLY_DATA
		FMemory::Memzero(UVDensities);
		FMemory::Memzero(Weights);
#endif
	}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FStaticMeshSection& Section);
};


struct FStaticMeshLODResources;

/** Creates distribution for uniformly sampling a mesh section. */
struct ENGINE_API FStaticMeshSectionAreaWeightedTriangleSampler : FWeightedRandomSampler
{
	FStaticMeshSectionAreaWeightedTriangleSampler();
	void Init(FStaticMeshLODResources* InOwner, int32 InSectionIdx);
	virtual float GetWeights(TArray<float>& OutWeights) override;

protected:

	FStaticMeshLODResources* Owner;
	int32 SectionIdx;
};

struct ENGINE_API FStaticMeshAreaWeightedSectionSampler : FWeightedRandomSampler
{
	FStaticMeshAreaWeightedSectionSampler();
	void Init(FStaticMeshLODResources* InOwner);
	virtual float GetWeights(TArray<float>& OutWeights)override;

protected:
	FStaticMeshLODResources* Owner;
};

/** Rendering resources needed to render an individual static mesh LOD. */
struct FStaticMeshLODResources
{
	/** The buffer containing vertex data. */
	FStaticMeshVertexBuffer VertexBuffer;
	/** The buffer containing the position vertex data. */
	FPositionVertexBuffer PositionVertexBuffer;
	/** The buffer containing the vertex color data. */
	FColorVertexBuffer ColorVertexBuffer;

	/** Index buffer resource for rendering. */
	FRawStaticIndexBuffer IndexBuffer;
	/** Reversed index buffer, used to prevent changing culling state between drawcalls. */
	FRawStaticIndexBuffer ReversedIndexBuffer;
	/** Index buffer resource for rendering in depth only passes. */
	FRawStaticIndexBuffer DepthOnlyIndexBuffer;
	/** Reversed depth only index buffer, used to prevent changing culling state between drawcalls. */
	FRawStaticIndexBuffer ReversedDepthOnlyIndexBuffer;
	/** Index buffer resource for rendering wireframe mode. */
	FRawStaticIndexBuffer WireframeIndexBuffer;
	/** Index buffer containing adjacency information required by tessellation. */
	FRawStaticIndexBuffer AdjacencyIndexBuffer;

	/** The vertex factory used when rendering this mesh. */
	FLocalVertexFactory VertexFactory;

	/** The vertex factory used when rendering this mesh with vertex colors. This is lazy init.*/
	FLocalVertexFactory VertexFactoryOverrideColorVertexBuffer;

	/** Sections for this LOD. */
	TArray<FStaticMeshSection> Sections;

	/** Distance field data associated with this mesh, null if not present.  */
	class FDistanceFieldVolumeData* DistanceFieldData; 

	/** The maximum distance by which this LOD deviates from the base from which it was generated. */
	float MaxDeviation;

	/** True if the adjacency index buffer contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasAdjacencyInfo : 1;

	/** True if the depth only index buffers contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasDepthOnlyIndices : 1;

	/** True if the reversed index buffers contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasReversedIndices : 1;

	/** True if the reversed index buffers contained data at init. Needed as it will not be available to the CPU afterwards. */
	uint32 bHasReversedDepthOnlyIndices: 1;
	
	/**	Allows uniform random selection of mesh sections based on their area. */
	FStaticMeshAreaWeightedSectionSampler AreaWeightedSampler;
	/**	Allows uniform random selection of triangles on each mesh section based on triangle area. */
	TArray<FStaticMeshSectionAreaWeightedTriangleSampler> AreaWeightedSectionSamplers;

	uint32 DepthOnlyNumTriangles;

	struct FSplineMeshVertexFactory* SplineVertexFactory;
	struct FSplineMeshVertexFactory* SplineVertexFactoryOverrideColorVertexBuffer;

#if STATS
	uint32 StaticMeshIndexMemory;
#endif
	
	/** Default constructor. */
	FStaticMeshLODResources();

	~FStaticMeshLODResources();

	/** Initializes all rendering resources. */
	void InitResources(UStaticMesh* Parent);

	/** Releases all rendering resources. */
	void ReleaseResources();

	/** Serialize. */
	void Serialize(FArchive& Ar, UObject* Owner, int32 Idx);

	/** Return the triangle count of this LOD. */
	ENGINE_API int32 GetNumTriangles() const;

	/** Return the number of vertices in this LOD. */
	ENGINE_API int32 GetNumVertices() const;

	ENGINE_API int32 GetNumTexCoords() const;

	/**
	 * Initializes a vertex factory for rendering this static mesh
	 *
	 * @param	InOutVertexFactory				The vertex factory to configure
	 * @param	InParentMesh					Parent static mesh
	 * @param	bInOverrideColorVertexBuffer	If true, make a vertex factory ready for per-instance colors
	 */
	void InitVertexFactory(FLocalVertexFactory& InOutVertexFactory, UStaticMesh* InParentMesh, bool bInOverrideColorVertexBuffer);
};

/**
 * FStaticMeshRenderData - All data needed to render a static mesh.
 */
class FStaticMeshRenderData
{
public:
	/** Default constructor. */
	FStaticMeshRenderData();

	/** Per-LOD resources. */
	TIndirectArray<FStaticMeshLODResources> LODResources;

	/** Screen size to switch LODs */
	float ScreenSize[MAX_STATIC_MESH_LODS];

	/** Bounds of the renderable mesh. */
	FBoxSphereBounds Bounds;

	/** True if LODs share static lighting data. */
	bool bLODsShareStaticLighting;

#if WITH_EDITORONLY_DATA
	/** The derived data key associated with this render data. */
	FString DerivedDataKey;

	/** Map of wedge index to vertex index. */
	TArray<int32> WedgeMap;

	/** Map of material index -> original material index at import time. */
	TArray<int32> MaterialIndexToImportIndex;

	/** UV data used for streaming accuracy debug view modes. In sync for rendering thread */
	TArray<FMeshUVChannelInfo> UVChannelDataPerMaterial;

	void SyncUVChannelData(const TArray<FStaticMaterial>& ObjectData);

	/** The next cached derived data in the list. */
	TUniquePtr<class FStaticMeshRenderData> NextCachedRenderData;

	/**
	 * Cache derived renderable data for the static mesh with the provided
	 * level of detail settings.
	 */
	void Cache(UStaticMesh* Owner, const FStaticMeshLODSettings& LODSettings);
#endif // #if WITH_EDITORONLY_DATA

	/** Serialization. */
	void Serialize(FArchive& Ar, UStaticMesh* Owner, bool bCooked);

	/** Initialize the render resources. */
	void InitResources(UStaticMesh* Owner);

	/** Releases the render resources. */
	ENGINE_API void ReleaseResources();

	/** Compute the size of this resource. */
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize() const;
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	SIZE_T GetResourceSizeBytes() const;

	/** Allocate LOD resources. */
	ENGINE_API void AllocateLODResources(int32 NumLODs);

	/** Update LOD-SECTION uv densities. */
	void ComputeUVDensities();

	void BuildAreaWeighedSamplingData();

private:
#if WITH_EDITORONLY_DATA
	/** Allow the editor to explicitly update section information. */
	friend class FLevelOfDetailSettingsLayout;
#endif // #if WITH_EDITORONLY_DATA
#if WITH_EDITOR
	/** Resolve all per-section settings. */
	ENGINE_API void ResolveSectionInfo(UStaticMesh* Owner);
#endif // #if WITH_EDITORONLY_DATA
};

/**
 * FStaticMeshComponentRecreateRenderStateContext - Destroys render state for all StaticMeshComponents using a given StaticMesh and 
 * recreates them when it goes out of scope. Used to ensure stale rendering data isn't kept around in the components when importing
 * over or rebuilding an existing static mesh.
 */
class FStaticMeshComponentRecreateRenderStateContext
{
public:

	/** Initialization constructor. */
	FStaticMeshComponentRecreateRenderStateContext( UStaticMesh* InStaticMesh, bool InUnbuildLighting = true, bool InRefreshBounds = false )
		: bUnbuildLighting( InUnbuildLighting ),
		  bRefreshBounds( InRefreshBounds )
	{
		for ( TObjectIterator<UStaticMeshComponent> It;It;++It )
		{
			if ( It->GetStaticMesh() == InStaticMesh )
			{
				checkf( !It->IsUnreachable(), TEXT("%s"), *It->GetFullName() );

				if ( It->bRenderStateCreated )
				{
					check( It->IsRegistered() );
					It->DestroyRenderState_Concurrent();
					StaticMeshComponents.Add(*It);
				}
			}
		}

		// Flush the rendering commands generated by the detachments.
		// The static mesh scene proxies reference the UStaticMesh, and this ensures that they are cleaned up before the UStaticMesh changes.
		FlushRenderingCommands();
	}

	/** Destructor: recreates render state for all components that had their render states destroyed in the constructor. */
	~FStaticMeshComponentRecreateRenderStateContext()
	{
		const int32 ComponentCount = StaticMeshComponents.Num();
		for( int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex )
		{
			UStaticMeshComponent* Component = StaticMeshComponents[ ComponentIndex ];
			if ( bUnbuildLighting )
			{
				// Invalidate the component's static lighting.
				// This unregisters and reregisters so must not be in the constructor
				Component->InvalidateLightingCache();
			}

			if( bRefreshBounds )
			{
				Component->UpdateBounds();
			}

			if ( Component->IsRegistered() && !Component->bRenderStateCreated )
			{
				Component->CreateRenderState_Concurrent();
			}
		}
	}

private:

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	bool bUnbuildLighting;
	bool bRefreshBounds;
};

/**
 * A static mesh component scene proxy.
 */
class ENGINE_API FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	FStaticMeshSceneProxy(UStaticMeshComponent* Component, bool bForceLODsShareStaticLighting);

	virtual ~FStaticMeshSceneProxy() {}

	/** Gets the number of mesh batches required to represent the proxy, aside from section needs. */
	virtual int32 GetNumMeshBatches() const
	{
		return 1;
	}

	/** Sets up a shadow FMeshBatch for a specific LOD. */
	virtual bool GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const;

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual bool GetMeshElement(
		int32 LODIndex, 
		int32 BatchIndex, 
		int32 ElementIndex, 
		uint8 InDepthPriorityGroup, 
		bool bUseSelectedMaterial, 
		bool bUseHoveredMaterial, 
		bool bAllowPreCulledIndices,
		FMeshBatch& OutMeshBatch) const;

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual bool GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const;

protected:
	/**
	 * Sets IndexBuffer, FirstIndex and NumPrimitives of OutMeshElement.
	 */
	virtual void SetIndexSource(int32 LODIndex, int32 ElementIndex, FMeshBatch& OutMeshElement, bool bWireframe, bool bRequiresAdjacencyInformation, bool bUseInversedIndices, bool bAllowPreCulledIndices) const;
	bool IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const;

public:
	// FPrimitiveSceneProxy interface.
#if WITH_EDITOR
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies) override;
#endif
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override;
	virtual void OnTransformChanged() override;
	virtual int32 GetLOD(const FSceneView* View) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
	virtual bool CanBeOccluded() const override;
	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override;
	virtual void GetDistancefieldAtlasData(FBox& LocalVolumeBounds, FVector2D& OutDistanceMinMax, FIntVector& OutBlockMin, FIntVector& OutBlockSize, bool& bOutBuiltAsIfTwoSided, bool& bMeshWasPlane, float& SelfShadowBias, TArray<FMatrix>& ObjectLocalToWorldTransforms) const override;
	virtual void GetDistanceFieldInstanceInfo(int32& NumInstances, float& BoundsSurfaceArea) const override;
	virtual bool HasDistanceFieldRepresentation() const override;
	virtual bool HasDynamicIndirectShadowCasterRepresentation() const override;
	virtual uint32 GetMemoryFootprint( void ) const override { return( sizeof( *this ) + GetAllocatedSize() ); }
	uint32 GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() + LODs.GetAllocatedSize() ); }

	virtual void GetMeshDescription(int32 LODIndex, TArray<FMeshBatch>& OutMeshElements) const override;

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual void GetLCIs(FLCIArray& LCIs) override;

#if WITH_EDITORONLY_DATA
	virtual bool GetPrimitiveDistance(int32 LODIndex, int32 SectionIndex, const FVector& ViewOrigin, float& PrimitiveDistance) const override;
	virtual bool GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const override;
	virtual bool GetMaterialTextureScales(int32 LODIndex, int32 SectionIndex, const FMaterialRenderProxy* MaterialRenderProxy, FVector4* OneOverScales, FIntVector4* UVChannelIndices) const override;
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	virtual int32 GetLightMapResolution() const override { return LightMapResolution; }
#endif

protected:

	/** Information used by the proxy about a single LOD of the mesh. */
	class FLODInfo : public FLightCacheInterface
	{
	public:

		/** Information about an element of a LOD. */
		struct FSectionInfo
		{
			/** Default constructor. */
			FSectionInfo()
				: Material(NULL)
				, bSelected(false)
#if WITH_EDITOR
				, HitProxy(NULL)
#endif
				, FirstPreCulledIndex(0)
				, NumPreCulledTriangles(-1)
			{}

			/** The material with which to render this section. */
			UMaterialInterface* Material;

			/** True if this section should be rendered as selected (editor only). */
			bool bSelected;

#if WITH_EDITOR
			/** The editor needs to be able to individual sub-mesh hit detection, so we store a hit proxy on each mesh. */
			HHitProxy* HitProxy;
#endif

#if WITH_EDITORONLY_DATA
			// The material index from the component. Used by the texture streaming accuracy viewmodes.
			int32 MaterialIndex;
#endif

			int32 FirstPreCulledIndex;
			int32 NumPreCulledTriangles;
		};

		/** Per-section information. */
		TArray<FSectionInfo> Sections;

		/** Vertex color data for this LOD (or NULL when not overridden), FStaticMeshComponentLODInfo handle the release of the memory */
		FColorVertexBuffer* OverrideColorVertexBuffer;

		const FRawStaticIndexBuffer* PreCulledIndexBuffer;

		/** Initialization constructor. */
		FLODInfo(const UStaticMeshComponent* InComponent, int32 InLODIndex, bool bLODsShareStaticLighting);

		bool UsesMeshModifyingMaterials() const { return bUsesMeshModifyingMaterials; }

		// FLightCacheInterface.
		virtual FLightInteraction GetInteraction(const FLightSceneProxy* LightSceneProxy) const override;

	private:

		TArray<FGuid> IrrelevantLights;

		/** True if any elements in this LOD use mesh-modifying materials **/
		bool bUsesMeshModifyingMaterials;
	};

	AActor* Owner;
	const UStaticMesh* StaticMesh;
	UBodySetup* BodySetup;
	FStaticMeshRenderData* RenderData;

	TIndirectArray<FLODInfo> LODs;

	const FDistanceFieldVolumeData* DistanceFieldData;	

	/** Hierarchical LOD Index used for rendering */
	uint8 HierarchicalLODIndex;

	/**
	 * The forcedLOD set in the static mesh editor, copied from the mesh component
	 */
	int32 ForcedLodModel;

	/** Minimum LOD index to use.  Clamped to valid range [0, NumLODs - 1]. */
	int32 ClampedMinLOD;

	FVector TotalScale3D;

	uint32 bCastShadow : 1;
	ECollisionTraceFlag		CollisionTraceFlag;

	/** The view relevance for all the static mesh's materials. */
	FMaterialRelevance MaterialRelevance;

	/** Collision Response of this component**/
	FCollisionResponseContainer CollisionResponse;

#if WITH_EDITORONLY_DATA
	/** The component streaming distance multiplier */
	float StreamingDistanceMultiplier;
	/** The cacheed GetTextureStreamingTransformScale */
	float StreamingTransformScale;
	/** Material bounds used for texture streaming. */
	TArray<uint32> MaterialStreamingRelativeBoxes;

	/** Index of the section to preview. If set to INDEX_NONE, all section will be rendered */
	int32 SectionIndexPreview;
	/** Index of the material to preview. If set to INDEX_NONE, all section will be rendered */
	int32 MaterialIndexPreview;
#endif

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** LightMap resolution used for VMI_LightmapDensity */
	int32 LightMapResolution;
#endif

#if !(UE_BUILD_SHIPPING)
	/** LOD used for collision */
	int32 LODForCollision;
	/** Draw mesh collision if used for complex collision */
	uint32 bDrawMeshCollisionIfComplex : 1;
	/** Draw mesh collision if used for simple collision */
	uint32 bDrawMeshCollisionIfSimple : 1;
#endif

	/**
	 * Returns the display factor for the given LOD level
	 *
	 * @Param LODIndex - The LOD to get the display factor for
	 */
	float GetScreenSize(int32 LODIndex) const;

	/**
	 * Returns the LOD mask for a view, this is like the ordinary LOD but can return two values for dither fading
	 */
	FLODMask GetLODMask(const FSceneView* View) const;
};

// WaveWorks Start
/**
* A waveworks static mesh component scene proxy.
*/
class ENGINE_API FWaveWorksStaticMeshSceneProxy : public FStaticMeshSceneProxy
{
public:

	/** Initialization constructor. */
	FWaveWorksStaticMeshSceneProxy(UWaveWorksStaticMeshComponent* Component, bool bForceLODsShareStaticLighting);
	virtual ~FWaveWorksStaticMeshSceneProxy();

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;	

	/** Get WaveWorks Component */
	FORCEINLINE class UWaveWorksStaticMeshComponent* GetWaveWorksStaticMeshComponent() const { return WaveWorksStaticMeshComponent; }

	/** Sample Displacement with XY Plane's Sample Position */
	void SampleDisplacements_GameThread(TArray<FVector> InSamplePoints, FWaveWorksSampleDisplacementsDelegate VectorArrayDelegate);

	/** Get intersect point with ray */
	void GetIntersectPointWithRay_GameThread(FVector InOriginPoint, FVector InDirection, float SeaLevel, FWaveWorksRaycastResultDelegate OnRecieveIntersectPointDelegate);

private:

	/** The WaveWorksComponent */
	class UWaveWorksStaticMeshComponent* WaveWorksStaticMeshComponent;
};
// WaveWorks End

/*-----------------------------------------------------------------------------
	FStaticMeshInstanceData
-----------------------------------------------------------------------------*/

template <class FloatType>
struct FInstanceStream
{
	FVector4 InstanceOrigin;  // per-instance random in w 
	FloatType InstanceTransform1[4];  // hitproxy.r + 256 * selected in .w
	FloatType InstanceTransform2[4]; // hitproxy.g in .w
	FloatType InstanceTransform3[4]; // hitproxy.b in .w
	int16 InstanceLightmapAndShadowMapUVBias[4]; 
	bool IsUsed;

	FORCEINLINE void SetInstance(const FMatrix& Transform, float RandomInstanceID)
	{
		const FMatrix* RESTRICT rTransform = (const FMatrix* RESTRICT)&Transform;
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;

		Me->InstanceOrigin.X = rTransform->M[3][0];
		Me->InstanceOrigin.Y = rTransform->M[3][1];
		Me->InstanceOrigin.Z = rTransform->M[3][2];
		Me->InstanceOrigin.W = RandomInstanceID;

		Me->InstanceTransform1[0] = Transform.M[0][0];
		Me->InstanceTransform1[1] = Transform.M[0][1];
		Me->InstanceTransform1[2] = Transform.M[0][2];
		Me->InstanceTransform1[3] = FloatType();

		Me->InstanceTransform2[0] = Transform.M[1][0];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[1][2];
		Me->InstanceTransform2[3] = FloatType();

		Me->InstanceTransform3[0] = Transform.M[2][0];
		Me->InstanceTransform3[1] = Transform.M[2][1];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = FloatType();

		Me->InstanceLightmapAndShadowMapUVBias[0] = 0;
		Me->InstanceLightmapAndShadowMapUVBias[1] = 0;
		Me->InstanceLightmapAndShadowMapUVBias[2] = 0;
		Me->InstanceLightmapAndShadowMapUVBias[3] = 0;

		Me->IsUsed = true;
	}

	FORCEINLINE void GetInstanceTransform(FMatrix& Transform) const
	{
		Transform.M[3][0] = InstanceOrigin.X;
		Transform.M[3][1] = InstanceOrigin.Y;
		Transform.M[3][2] = InstanceOrigin.Z;

		Transform.M[0][0] = InstanceTransform1[0];
		Transform.M[0][1] = InstanceTransform1[1];
		Transform.M[0][2] = InstanceTransform1[2];

		Transform.M[1][0] = InstanceTransform2[0];
		Transform.M[1][1] = InstanceTransform2[1];
		Transform.M[1][2] = InstanceTransform2[2];

		Transform.M[2][0] = InstanceTransform3[0];
		Transform.M[2][1] = InstanceTransform3[1];
		Transform.M[2][2] = InstanceTransform3[2];

		Transform.M[0][3] = 0.f;
		Transform.M[1][3] = 0.f;
		Transform.M[2][3] = 0.f;
		Transform.M[3][3] = 0.f;
	}

	FORCEINLINE void GetInstanceShaderValues(FVector4 OutInstanceTransform[3], FVector4& OutInstanceLightmapAndShadowMapUVBias, FVector4& OutInstanceOrigin) const
	{
		OutInstanceLightmapAndShadowMapUVBias = FVector4(
			float(InstanceLightmapAndShadowMapUVBias[0]),
			float(InstanceLightmapAndShadowMapUVBias[1]),
			float(InstanceLightmapAndShadowMapUVBias[2]),
			float(InstanceLightmapAndShadowMapUVBias[3]));
			
		OutInstanceTransform[0] = FVector4(
			(float)InstanceTransform1[0],
			(float)InstanceTransform1[1],
			(float)InstanceTransform1[2],
			(float)InstanceTransform1[3]);

		OutInstanceTransform[1] = FVector4(
			(float)InstanceTransform2[0],
			(float)InstanceTransform2[1],
			(float)InstanceTransform2[2],
			(float)InstanceTransform2[3]);
		
		OutInstanceTransform[2] = FVector4(
			(float)InstanceTransform3[0],
			(float)InstanceTransform3[1],
			(float)InstanceTransform3[2],
			(float)InstanceTransform3[3]);

		OutInstanceOrigin = InstanceOrigin;
	}

	FORCEINLINE void SetInstance(const FMatrix& Transform, float RandomInstanceID, const FVector2D& LightmapUVBias, const FVector2D& ShadowmapUVBias, FColor HitProxyColor, bool bSelected)
	{
		const FMatrix* RESTRICT rTransform = (const FMatrix* RESTRICT)&Transform;
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		const FVector2D* RESTRICT rLightmapUVBias = (const FVector2D* RESTRICT)&LightmapUVBias;
		const FVector2D* RESTRICT rShadowmapUVBias = (const FVector2D* RESTRICT)&ShadowmapUVBias;

		Me->InstanceOrigin.X = rTransform->M[3][0];
		Me->InstanceOrigin.Y = rTransform->M[3][1];
		Me->InstanceOrigin.Z = rTransform->M[3][2];
		Me->InstanceOrigin.W = RandomInstanceID;

		Me->InstanceTransform1[0] = Transform.M[0][0];
		Me->InstanceTransform1[1] = Transform.M[0][1];
		Me->InstanceTransform1[2] = Transform.M[0][2];
		Me->InstanceTransform1[3] = ((float)HitProxyColor.R) + (bSelected ? 256.f : 0.0f);

		Me->InstanceTransform2[0] = Transform.M[1][0];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[1][2];
		Me->InstanceTransform2[3] = (float)HitProxyColor.G;

		Me->InstanceTransform3[0] = Transform.M[2][0];
		Me->InstanceTransform3[1] = Transform.M[2][1];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = (float)HitProxyColor.B;

		Me->InstanceLightmapAndShadowMapUVBias[0] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->X  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[1] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->Y  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[2] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->X * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[3] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->Y * 32767.0f), MIN_int16, MAX_int16);

		Me->IsUsed = true;
	}

	FORCEINLINE void SetInstance(const FMatrix& Transform, float RandomInstanceID, const FVector2D& LightmapUVBias, const FVector2D& ShadowmapUVBias)
	{
		const FMatrix* RESTRICT rTransform = (const FMatrix* RESTRICT)&Transform;
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		const FVector2D* RESTRICT rLightmapUVBias = (const FVector2D* RESTRICT)&LightmapUVBias;
		const FVector2D* RESTRICT rShadowmapUVBias = (const FVector2D* RESTRICT)&ShadowmapUVBias;

		Me->InstanceOrigin.X = rTransform->M[3][0];
		Me->InstanceOrigin.Y = rTransform->M[3][1];
		Me->InstanceOrigin.Z = rTransform->M[3][2];
		Me->InstanceOrigin.W = RandomInstanceID;

		Me->InstanceTransform1[0] = Transform.M[0][0];
		Me->InstanceTransform1[1] = Transform.M[0][1];
		Me->InstanceTransform1[2] = Transform.M[0][2];
		Me->InstanceTransform1[3] = FloatType();

		Me->InstanceTransform2[0] = Transform.M[1][0];
		Me->InstanceTransform2[1] = Transform.M[1][1];
		Me->InstanceTransform2[2] = Transform.M[1][2];
		Me->InstanceTransform2[3] = FloatType();

		Me->InstanceTransform3[0] = Transform.M[2][0];
		Me->InstanceTransform3[1] = Transform.M[2][1];
		Me->InstanceTransform3[2] = Transform.M[2][2];
		Me->InstanceTransform3[3] = FloatType();

		Me->InstanceLightmapAndShadowMapUVBias[0] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->X  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[1] = FMath::Clamp<int32>(FMath::TruncToInt(rLightmapUVBias->Y  * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[2] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->X * 32767.0f), MIN_int16, MAX_int16);
		Me->InstanceLightmapAndShadowMapUVBias[3] = FMath::Clamp<int32>(FMath::TruncToInt(rShadowmapUVBias->Y * 32767.0f), MIN_int16, MAX_int16);

		Me->IsUsed = true;
	}

	FORCEINLINE void NullifyInstance()
	{
		// Nullify Instance & Editor data
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		Me->InstanceTransform1[0] = FloatType();
		Me->InstanceTransform1[1] = FloatType();
		Me->InstanceTransform1[2] = FloatType();
		Me->InstanceTransform1[3] = FloatType();

		Me->InstanceTransform2[0] = FloatType();
		Me->InstanceTransform2[1] = FloatType();
		Me->InstanceTransform2[2] = FloatType();
		Me->InstanceTransform2[3] = FloatType();

		Me->InstanceTransform3[0] = FloatType();
		Me->InstanceTransform3[1] = FloatType();
		Me->InstanceTransform3[2] = FloatType();
		Me->InstanceTransform3[3] = FloatType();

		Me->IsUsed = false;
	}

	FORCEINLINE void SetInstanceEditorData(FColor HitProxyColor, bool bSelected)
	{
		FInstanceStream* RESTRICT Me = (FInstanceStream* RESTRICT)this;
		
		Me->InstanceTransform1[3] = ((float)HitProxyColor.R) + (bSelected ? 256.f : 0.0f);
		Me->InstanceTransform2[3] = (float)HitProxyColor.G;
		Me->InstanceTransform3[3] = (float)HitProxyColor.B;

		Me->IsUsed = true;
	}
	
	friend FArchive& operator<<( FArchive& Ar, FInstanceStream& V )
	{
		return Ar 
			<< V.InstanceOrigin.X 
			<< V.InstanceOrigin.Y
			<< V.InstanceOrigin.Z
			<< V.InstanceOrigin.W

			<< V.InstanceTransform1[0]
			<< V.InstanceTransform1[1]
			<< V.InstanceTransform1[2]
			<< V.InstanceTransform1[3]

			<< V.InstanceTransform2[0]
			<< V.InstanceTransform2[1]
			<< V.InstanceTransform2[2]
			<< V.InstanceTransform2[3]

			<< V.InstanceTransform3[0]
			<< V.InstanceTransform3[1]
			<< V.InstanceTransform3[2]
			<< V.InstanceTransform3[3]

			<< V.InstanceLightmapAndShadowMapUVBias[0]
			<< V.InstanceLightmapAndShadowMapUVBias[1]
			<< V.InstanceLightmapAndShadowMapUVBias[2]
			<< V.InstanceLightmapAndShadowMapUVBias[3];
	}
};

typedef FInstanceStream<FFloat16>	FInstanceStream16;
typedef FInstanceStream<float>		FInstanceStream32;

/** The implementation of the static mesh instance data storage type. */
class FStaticMeshInstanceData :
	public FStaticMeshVertexDataInterface
{
public:
	FStaticMeshInstanceData()
		: InstanceStream16(false)
		, InstanceStream32(false)
		, bUseHalfFloat(PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || GVertexElementTypeSupport.IsSupported(VET_Half2))
	{
	}
	/**
	 * Constructor
	 * @param InNeedsCPUAccess - true if resource array data should be CPU accessible
	 * @param bSupportsVertexHalfFloat - true if device has support for half float in vertex arrays
	 */
	FStaticMeshInstanceData(bool InNeedsCPUAccess, bool bInUseHalfFloat)
		: InstanceStream16(InNeedsCPUAccess)
		, InstanceStream32(InNeedsCPUAccess)
		, bUseHalfFloat(bInUseHalfFloat)
	{
	}

	SIZE_T GetResourceSize() const
	{
		return SIZE_T(NumInstances()) * SIZE_T(GetStride());
	}

	static SIZE_T GetResourceSize(int32 InNumInstances, bool bInUseHalfFloat)
	{
		return SIZE_T(InNumInstances) * SIZE_T(bInUseHalfFloat ? sizeof(FInstanceStream16) : sizeof(FInstanceStream32));
	}

	SIZE_T GetResourceSize(int32 InNumInstances)
	{
		return SIZE_T(InNumInstances) * SIZE_T(bUseHalfFloat ? sizeof(FInstanceStream16) : sizeof(FInstanceStream32));
	}

	/**
	 * Resizes the vertex data buffer, discarding any data which no longer fits.
	 * @param NumVertices - The number of vertices to allocate the buffer for.
	 */
	virtual void ResizeBuffer(uint32 NumInstances) override
	{
		checkf(0, TEXT("ArrayType::Add is not supported on all platforms"));
	}

	virtual uint32 GetStride() const override
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return sizeof(FInstanceStream16);
		}
		else
		{
			return sizeof(FInstanceStream32);
		}
	}
	virtual uint8* GetDataPointer() override
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return (uint8*)&(InstanceStream16)[0];
		}
		else
		{
			return (uint8*)&(InstanceStream32)[0];
		}
	}
	virtual FResourceArrayInterface* GetResourceArray() override
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return &InstanceStream16;
		}
		else
		{
			return &InstanceStream32;
		}
	}
	virtual void Serialize(FArchive& Ar) override
	{
		InstanceStream16.BulkSerialize(Ar);
		InstanceStream32.BulkSerialize(Ar);
	}

	void AllocateInstances(int32 NumInstances, bool DestroyExistingInstances)
	{
		// We cannot write directly to the data on all platforms,
		// so we make a TArray of the right type, then assign it
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			if (DestroyExistingInstances)
			{
				InstanceStream16.Empty(NumInstances);
			}

			int32 DeltaToAdd = NumInstances - InstanceStream16.Num();

			if (DeltaToAdd > 0)
			{
				InstanceStream16.AddUninitialized(DeltaToAdd);
			}
		}
		else
		{
			if (DestroyExistingInstances)
			{
				InstanceStream32.Empty(NumInstances);
			}

			int32 DeltaToAdd = NumInstances - InstanceStream32.Num();

			if (DeltaToAdd > 0)
			{
				InstanceStream32.AddUninitialized(DeltaToAdd);
			}
		}
	}

	FORCEINLINE void GetInstanceTransform(int32 InstanceIndex, FMatrix& Transform) const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].GetInstanceTransform(Transform);
		}
		else
		{
			InstanceStream32[InstanceIndex].GetInstanceTransform(Transform);
		}
	}

	FORCEINLINE void GetInstanceShaderValues(int32 InstanceIndex, FVector4 InstanceTransform[3], FVector4& InstanceLightmapAndShadowMapUVBias, FVector4& InstanceOrigin) const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].GetInstanceShaderValues(InstanceTransform, InstanceLightmapAndShadowMapUVBias, InstanceOrigin);
		}
		else
		{
			InstanceStream32[InstanceIndex].GetInstanceShaderValues(InstanceTransform, InstanceLightmapAndShadowMapUVBias, InstanceOrigin);
		}
	}

	FORCEINLINE int32 GetNextAvailableInstanceIndex() const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			for (int32 i = 0; i < InstanceStream16.Num(); ++i)
			{
				if (!InstanceStream16[i].IsUsed)
				{
					return i;
				}
			}
		}
		else
		{
			for (int32 i = 0; i < InstanceStream32.Num(); ++i)
			{
				if (!InstanceStream32[i].IsUsed)
				{
					return i;
				}
			}
		}

		return INDEX_NONE;
	}

	FORCEINLINE void SetInstance(int32 InstanceIndex, const FMatrix& Transform, float RandomInstanceID)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].SetInstance(Transform, RandomInstanceID);
		}
		else
		{
			InstanceStream32[InstanceIndex].SetInstance(Transform, RandomInstanceID);
		}
	}
	
	FORCEINLINE void SetInstance(int32 InstanceIndex, const FMatrix& Transform, float RandomInstanceID, const FVector2D& LightmapUVBias, const FVector2D& ShadowmapUVBias)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].SetInstance(Transform, RandomInstanceID, LightmapUVBias, ShadowmapUVBias);
		}
		else
		{
			InstanceStream32[InstanceIndex].SetInstance(Transform, RandomInstanceID, LightmapUVBias, ShadowmapUVBias);
		}
	}
	
	FORCEINLINE void NullifyInstance(int32 InstanceIndex)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].NullifyInstance();
		}
		else
		{
			InstanceStream32[InstanceIndex].NullifyInstance();
		}
	}

	FORCEINLINE void SetInstanceEditorData(int32 InstanceIndex, FColor HitProxyColor, bool bSelected)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			InstanceStream16[InstanceIndex].SetInstanceEditorData(HitProxyColor, bSelected);
		}
		else
		{
			InstanceStream32[InstanceIndex].SetInstanceEditorData(HitProxyColor, bSelected);
		}
	}

	FORCEINLINE uint8* GetInstanceWriteAddress(int32 InstanceIndex)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return (uint8*)(InstanceStream16.GetData() + InstanceIndex);
		}
		else
		{
			return (uint8*)(InstanceStream32.GetData() + InstanceIndex);
		}
	}

	FORCEINLINE int32 IsValidIndex(int32 Index) const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.IsValidIndex(Index);
		}
		else
		{
			return InstanceStream32.IsValidIndex(Index);
		}
	}

	FORCEINLINE int32 NumInstances() const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.Num();
		}
		else
		{
			return InstanceStream32.Num();
		}
	}

	FORCEINLINE bool GetAllowCPUAccess() const
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.GetAllowCPUAccess();
		}
		else
		{
			return InstanceStream32.GetAllowCPUAccess();
		}
	}

	FORCEINLINE void SetAllowCPUAccess(bool bInNeedsCPUAccess)
	{
		if (PLATFORM_BUILTIN_VERTEX_HALF_FLOAT || bUseHalfFloat)
		{
			return InstanceStream16.SetAllowCPUAccess(bInNeedsCPUAccess);
		}
		else
		{
			return InstanceStream32.SetAllowCPUAccess(bInNeedsCPUAccess);
		}
	}

private:
	TResourceArray<FInstanceStream16, VERTEXBUFFER_ALIGNMENT> InstanceStream16;
	TResourceArray<FInstanceStream32, VERTEXBUFFER_ALIGNMENT> InstanceStream32;
	const bool bUseHalfFloat;
};
	
#if WITH_EDITOR
/**
 * Remaps painted vertex colors when the renderable mesh has changed.
 * @param InPaintedVertices - The original position and normal for each painted vertex.
 * @param InOverrideColors - The painted vertex colors.
 * @param NewPositions - Positions of the new renderable mesh on which colors are to be mapped.
 * @param OptionalVertexBuffer - [optional] Vertex buffer containing vertex normals for the new mesh.
 * @param OutOverrideColors - Will contain vertex colors for the new mesh.
 */
ENGINE_API void RemapPaintedVertexColors(
	const TArray<FPaintedVertex>& InPaintedVertices,
	const FColorVertexBuffer& InOverrideColors,
	const FPositionVertexBuffer& OldPositions,
	const FStaticMeshVertexBuffer& OldVertexBuffer,
	const FPositionVertexBuffer& NewPositions,	
	const FStaticMeshVertexBuffer* OptionalVertexBuffer,
	TArray<FColor>& OutOverrideColors
	);
#endif // #if WITH_EDITOR
