// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Engine/TextureStreamingTypes.h"
#include "Components/PrimitiveComponent.h"

#include "LandscapeComponent.generated.h"

class ALandscape;
class ALandscapeProxy;
class FLightingBuildOptions;
class FMaterialUpdateContext;
class FMeshMapBuildData;
class FPrimitiveSceneProxy;
class ITargetPlatform;
class ULandscapeComponent;
class ULandscapeGrassType;
class ULandscapeHeightfieldCollisionComponent;
class ULandscapeInfo;
class ULandscapeLayerInfoObject;
class ULightComponent;
class UMaterialInstanceConstant;
class UMaterialInterface;
class UTexture2D;
struct FConvexVolume;
struct FEngineShowFlags;
struct FLandscapeEditDataInterface;
struct FLandscapeTextureDataInfo;
struct FStaticLightingPrimitiveInfo;

struct FLandscapeEditDataInterface;
struct FLandscapeMobileRenderData;

//
// FLandscapeEditToolRenderData
//
USTRUCT()
struct FLandscapeEditToolRenderData
{
public:
	GENERATED_USTRUCT_BODY()

	enum SelectionType
	{
		ST_NONE = 0,
		ST_COMPONENT = 1,
		ST_REGION = 2,
		// = 4...
	};

	FLandscapeEditToolRenderData()
		: ToolMaterial(NULL),
		GizmoMaterial(NULL),
		SelectedType(ST_NONE),
		DebugChannelR(INDEX_NONE),
		DebugChannelG(INDEX_NONE),
		DebugChannelB(INDEX_NONE),
		DataTexture(NULL)
	{}

	// Material used to render the tool.
	UPROPERTY(NonTransactional)
	UMaterialInterface* ToolMaterial;

	// Material used to render the gizmo selection region...
	UPROPERTY(NonTransactional)
	UMaterialInterface* GizmoMaterial;

	// Component is selected
	UPROPERTY(NonTransactional)
	int32 SelectedType;

	UPROPERTY(NonTransactional)
	int32 DebugChannelR;

	UPROPERTY(NonTransactional)
	int32 DebugChannelG;

	UPROPERTY(NonTransactional)
	int32 DebugChannelB;

	UPROPERTY(NonTransactional)
	UTexture2D* DataTexture; // Data texture other than height/weight

#if WITH_EDITOR
	void UpdateDebugColorMaterial(const ULandscapeComponent* const Component);
	void UpdateSelectionMaterial(int32 InSelectedType, const ULandscapeComponent* const Component);
#endif
};

class FLandscapeComponentDerivedData
{
	/** The compressed Landscape component data for mobile rendering. Serialized to disk. 
	    On device, freed once it has been decompressed. */
	TArray<uint8> CompressedLandscapeData;
	
	/** Cached render data. Only valid on device. */
	TSharedPtr<FLandscapeMobileRenderData, ESPMode::ThreadSafe > CachedRenderData;

public:
	/** Returns true if there is any valid platform data */
	bool HasValidPlatformData() const
	{
		return CompressedLandscapeData.Num() != 0;
	}

	/** Returns true if there is any valid platform data */
	bool HasValidRuntimeData() const
	{
		return CompressedLandscapeData.Num() != 0 || CachedRenderData.IsValid();
	}

	/** Returns the size of the platform data if there is any. */
	int32 GetPlatformDataSize() const
	{
		return CompressedLandscapeData.Num();
	}

	/** Initializes the compressed data from an uncompressed source. */
	void InitializeFromUncompressedData(const TArray<uint8>& UncompressedData);

	/** Decompresses data if necessary and returns the render data object. 
     *  On device, this frees the compressed data and keeps a reference to the render data. */
	TSharedPtr<FLandscapeMobileRenderData, ESPMode::ThreadSafe> GetRenderData();

	/** Constructs a key string for the DDC that uniquely identifies a the Landscape component's derived data. */
	static FString GetDDCKeyString(const FGuid& StateId);

	/** Loads the platform data from DDC */
	bool LoadFromDDC(const FGuid& StateId);

	/** Saves the compressed platform data to the DDC */
	void SaveToDDC(const FGuid& StateId);

	/* Serializer */
	friend FArchive& operator<<(FArchive& Ar, FLandscapeComponentDerivedData& Data);
};

/* Used to uniquely reference a landscape vertex in a component, and generate a key suitable for a TMap. */
struct FLandscapeVertexRef
{
	FLandscapeVertexRef(int16 InX, int16 InY, int8 InSubX, int8 InSubY)
	: X(InX)
	, Y(InY)
	, SubX(InSubX)
	, SubY(InSubY)
	{}
	int16 X;
	int16 Y;
	int8 SubX;
	int8 SubY;

	uint64 MakeKey() const
	{
		return (uint64)X << 32 | (uint64)Y << 16 | (uint64)SubX << 8 | (uint64)SubY;
	}
};

/** Stores information about which weightmap texture and channel each layer is stored */
USTRUCT()
struct FWeightmapLayerAllocationInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	ULandscapeLayerInfoObject* LayerInfo;

	UPROPERTY()
	uint8 WeightmapTextureIndex;

	UPROPERTY()
	uint8 WeightmapTextureChannel;

	FWeightmapLayerAllocationInfo()
		: WeightmapTextureIndex(0)
		, WeightmapTextureChannel(0)
	{
	}


	FWeightmapLayerAllocationInfo(ULandscapeLayerInfoObject* InLayerInfo)
		:	LayerInfo(InLayerInfo)
		,	WeightmapTextureIndex(255)	// Indicates an invalid allocation
		,	WeightmapTextureChannel(255)
	{
	}
	
	FName GetLayerName() const;
};

struct FLandscapeComponentGrassData
{
#if WITH_EDITORONLY_DATA
	// Variables used to detect when grass data needs to be regenerated:

	// Guid per material instance in the hierarchy between the assigned landscape material (instance) and the root UMaterial
	// used to detect changes to material instance parameters or the root material that could affect the grass maps
	TArray<FGuid, TInlineAllocator<2>> MaterialStateIds;
	// cached component rotation when material world-position-offset is used,
	// as this will affect the direction of world-position-offset deformation (included in the HeightData below)
	FQuat RotationForWPO;
#endif

	TArray<uint16> HeightData;
#if WITH_EDITORONLY_DATA
	// Height data for LODs 1+, keyed on LOD index
	TMap<int32, TArray<uint16>> HeightMipData;
#endif
	TMap<ULandscapeGrassType*, TArray<uint8>> WeightData;

	FLandscapeComponentGrassData() {}

#if WITH_EDITOR
	FLandscapeComponentGrassData(ULandscapeComponent* Component);
#endif

	bool HasData()
	{
		return HeightData.Num() > 0 ||
#if WITH_EDITORONLY_DATA
			HeightMipData.Num() > 0 ||
#endif
			WeightData.Num() > 0;
	}

	SIZE_T GetAllocatedSize() const;

	friend FArchive& operator<<(FArchive& Ar, FLandscapeComponentGrassData& Data);
};

UCLASS(hidecategories=(Display, Attachment, Physics, Debug, Collision, Movement, Rendering, PrimitiveComponent, Object, Transform), showcategories=("Rendering|Material"), MinimalAPI, Within=LandscapeProxy)
class ULandscapeComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
	
	/** X offset from global components grid origin (in quads) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 SectionBaseX;

	/** Y offset from global components grid origin (in quads) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 SectionBaseY;

	/** Total number of quads for this component, has to be >0 */
	UPROPERTY()
	int32 ComponentSizeQuads;

	/** Number of quads for a subsection of the component. SubsectionSizeQuads+1 must be a power of two. */
	UPROPERTY()
	int32 SubsectionSizeQuads;

	/** Number of subsections in X or Y axis */
	UPROPERTY()
	int32 NumSubsections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LandscapeComponent)
	UMaterialInterface* OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LandscapeComponent, AdvancedDisplay)
	UMaterialInterface* OverrideHoleMaterial;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	UMaterialInstanceConstant* MaterialInstance_DEPRECATED;
#endif

	UPROPERTY(TextExportTransient)
	TArray<UMaterialInstanceConstant*> MaterialInstances;

	/** List of layers, and the weightmap and channel they are stored */
	UPROPERTY()
	TArray<FWeightmapLayerAllocationInfo> WeightmapLayerAllocations;

	/** Weightmap texture reference */
	UPROPERTY(TextExportTransient)
	TArray<UTexture2D*> WeightmapTextures;

	/** XYOffsetmap texture reference */
	UPROPERTY(TextExportTransient)
	UTexture2D* XYOffsetmapTexture;

	/** UV offset to component's weightmap data from component local coordinates*/
	UPROPERTY()
	FVector4 WeightmapScaleBias;

	/** U or V offset into the weightmap for the first subsection, in texture UV space */
	UPROPERTY()
	float WeightmapSubsectionOffset;

	/** UV offset to Heightmap data from component local coordinates */
	UPROPERTY()
	FVector4 HeightmapScaleBias;

	/** Heightmap texture reference */
	UPROPERTY(TextExportTransient)
	UTexture2D* HeightmapTexture;

	/** Cached local-space bounding box, created at heightmap update time */
	UPROPERTY()
	FBox CachedLocalBox;

	/** Reference to associated collision component */
	UPROPERTY()
	TLazyObjectPtr<ULandscapeHeightfieldCollisionComponent> CollisionComponent;

private:
#if WITH_EDITORONLY_DATA
	/** Unique ID for this component, used for caching during distributed lighting */
	UPROPERTY()
	FGuid LightingGuid;

#endif // WITH_EDITORONLY_DATA
public:

	/** Uniquely identifies this component's built map data. */
	UPROPERTY()
	FGuid MapBuildDataId;

	/**	Legacy irrelevant lights */
	UPROPERTY()
	TArray<FGuid> IrrelevantLights_DEPRECATED;

	/** Heightfield mipmap used to generate collision */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	int32 CollisionMipLevel;

	/** Heightfield mipmap used to generate simple collision */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	int32 SimpleCollisionMipLevel;

	/** Allows overriding the landscape bounds. This is useful if you distort the landscape with world-position-offset, for example
	 *  Extension value in the negative Z axis, positive value increases bound size */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent, meta=(EditCondition="bOverrideBounds"))
	float NegativeZBoundsExtension;

	/** Allows overriding the landscape bounds. This is useful if you distort the landscape with world-position-offset, for example
	 *  Extension value in the positive Z axis, positive value increases bound size */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent, meta=(EditCondition="bOverrideBounds"))
	float PositiveZBoundsExtension;

	/** StaticLightingResolution overriding per component, default value 0 means no overriding */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent, meta=(ClampMax = 4096))
	float StaticLightingResolution;

	/** Forced LOD level to use when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 ForcedLOD;

	/** LOD level Bias to use when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=LandscapeComponent)
	int32 LODBias;

	UPROPERTY()
	FGuid StateId;

	/** The Material Guid that used when baking, to detect material recompilations */
	UPROPERTY()
	FGuid BakedTextureMaterialGuid;

	/** Pre-baked Base Color texture for use by distance field GI */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = BakedTextures)
	UTexture2D* GIBakedBaseColorTexture;

#if WITH_EDITORONLY_DATA
	/** LOD level Bias to use when lighting buidling via lightmass, -1 Means automatic LOD calculation based on ForcedLOD + LODBias */
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	int32 LightingLODBias;

	// List of layers allowed to be painted on this component
	UPROPERTY(EditAnywhere, Category=LandscapeComponent)
	TArray<ULandscapeLayerInfoObject*> LayerWhitelist;

	/** Pointer to data shared with the render thread, used by the editor tools */
	UPROPERTY(Transient, DuplicateTransient, NonTransactional)
	FLandscapeEditToolRenderData EditToolRenderData;

	/** Hash of source for ES2 generated data. Used for mobile preview and cook-in-editor
	 * to determine if we need to re-generate ES2 pixel data. */
	UPROPERTY(Transient, DuplicateTransient)
	FGuid MobileDataSourceHash;
#endif

	/** For ES2 */
	UPROPERTY()
	uint8 MobileBlendableLayerMask;

	/** Material interface used for ES2. Serialized only when cooking or loading cooked builds. */
	UPROPERTY(NonPIEDuplicateTransient)
	UMaterialInterface* MobileMaterialInterface;

	/** Generated weight/normal map texture used for ES2. Serialized only when cooking or loading cooked builds. */
	UPROPERTY(NonPIEDuplicateTransient)
	UTexture2D* MobileWeightNormalmapTexture;

public:
	/** Platform Data where don't support texture sampling in vertex buffer */
	FLandscapeComponentDerivedData PlatformData;

	/** Grass data for generation **/
	TSharedRef<FLandscapeComponentGrassData, ESPMode::ThreadSafe> GrassData;

	//~ Begin UObject Interface.	
	virtual void PostInitProperties() override;	
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void BeginDestroy() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#if WITH_EDITOR
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform* TargetPlatform) override;
	virtual void PostLoad() override;
	virtual void PostEditUndo() override;
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface

	LANDSCAPE_API void UpdateEditToolRenderData();

	/** Fix up component layers, weightmaps
	 */
	LANDSCAPE_API void FixupWeightmaps();

	// Update layer whitelist to include the currently painted layers
	LANDSCAPE_API void UpdateLayerWhitelistFromPaintedLayers();
	
	//~ Begin UPrimitiveComponent Interface.
	virtual bool GetLightMapResolution( int32& Width, int32& Height ) const override;
	virtual int32 GetStaticLightMapResolution() const override;
	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const override;
	virtual void GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options) override;
#endif
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual ELightMapInteractionType GetStaticLightingType() const override { return LMIT_Texture;	}
	virtual void GetStreamingTextureInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const override;
	virtual bool IsPrecomputedLightingValid() const override;

#if WITH_EDITOR
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual void PreFeatureLevelChange(ERHIFeatureLevel::Type PendingFeatureLevel) override;
#endif
	//~ End UPrimitiveComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
#if WITH_EDITOR
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) override;
#endif
	virtual void PropagateLightingScenarioChange() override;
	//~ End UActorComponent Interface.


#if WITH_EDITOR
	/** Gets the landscape info object for this landscape */
	LANDSCAPE_API ULandscapeInfo* GetLandscapeInfo() const;

	/** Deletes a layer from this component, removing all its data */
	LANDSCAPE_API void DeleteLayer(ULandscapeLayerInfoObject* LayerInfo, FLandscapeEditDataInterface& LandscapeEdit);

	/** Fills a layer to 100% on this component, adding it if needed and removing other layers that get painted away */
	LANDSCAPE_API void FillLayer(ULandscapeLayerInfoObject* LayerInfo, FLandscapeEditDataInterface& LandscapeEdit);

	/** Replaces one layerinfo on this component with another */
	LANDSCAPE_API void ReplaceLayer(ULandscapeLayerInfoObject* FromLayerInfo, ULandscapeLayerInfoObject* ToLayerInfo, FLandscapeEditDataInterface& LandscapeEdit);

	// true if the component's landscape material supports grass
	bool MaterialHasGrass() const;

	/** Creates and destroys cooked grass data stored in the map */
	void RenderGrassMap();
	void RemoveGrassMap();

	/* Could a grassmap currently be generated, disregarding whether our textures are streamed in? */
	bool CanRenderGrassMap() const;

	/* Are the textures we need to render a grassmap currently streamed in? */
	bool AreTexturesStreamedForGrassMapRender() const;

	/* Is the grassmap data outdated, eg by a material */
	bool IsGrassMapOutdated() const;

	/** Renders the heightmap of this component (including material world-position-offset) at the specified LOD */
	TArray<uint16> RenderWPOHeightmap(int32 LOD);

	/* Serialize all hashes/guids that record the current state of this component */
	void SerializeStateHashes(FArchive& Ar);

	// Generates mobile platform data for this component
	void GeneratePlatformVertexData();
	void GeneratePlatformPixelData();

	/** Generate mobile data if it's missing or outdated */
	void CheckGenerateLandscapePlatformData(bool bIsCooking);
#endif

	/** Get the landscape actor associated with this component. */
	ALandscape* GetLandscapeActor() const;

	/** Get the level in which the owning actor resides */
	ULevel* GetLevel() const;

#if WITH_EDITOR
	/** Returns all generated textures and material instances used by this component. */
	LANDSCAPE_API void GetGeneratedTexturesAndMaterialInstances(TArray<UObject*>& OutTexturesAndMaterials) const;
#endif

	/** Gets the landscape proxy actor which owns this component */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxy() const;

	/** @return Component section base as FIntPoint */
	LANDSCAPE_API FIntPoint GetSectionBase() const; 

	/** @param InSectionBase new section base for a component */
	LANDSCAPE_API void SetSectionBase(FIntPoint InSectionBase);

	/** @todo document */
	const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid;
#endif // WITH_EDITORONLY_DATA
	}

	/** @todo document */
	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#endif // WITH_EDITORONLY_DATA
	}

	FGuid GetMapBuildDataId() const
	{
		return MapBuildDataId;
	}

	LANDSCAPE_API const FMeshMapBuildData* GetMeshMapBuildData() const;

#if WITH_EDITOR
	/** Initialize the landscape component */
	LANDSCAPE_API void Init(int32 InBaseX,int32 InBaseY,int32 InComponentSizeQuads, int32 InNumSubsections,int32 InSubsectionSizeQuads);

	/**
	 * Recalculate cached bounds using height values.
	 */
	LANDSCAPE_API void UpdateCachedBounds();

	/**
	 * Update the MaterialInstance parameters to match the layer and weightmaps for this component
	 * Creates the MaterialInstance if it doesn't exist.
	 */
	LANDSCAPE_API void UpdateMaterialInstances();

	// Internal implementation of UpdateMaterialInstances, not safe to call directly
	void UpdateMaterialInstances_Internal(FMaterialUpdateContext& Context);

	/** Helper function for UpdateMaterialInstance to get Material without set parameters */
	UMaterialInstanceConstant* GetCombinationMaterial(bool bMobile = false);
	/**
	 * Generate mipmaps for height and tangent data.
	 * @param HeightmapTextureMipData - array of pointers to the locked mip data.
	 *           This should only include the mips that are generated directly from this component's data
	 *           ie where each subsection has at least 2 vertices.
	* @param ComponentX1 - region of texture to update in component space, MAX_int32 meant end of X component in ALandscape::Import()
	* @param ComponentY1 - region of texture to update in component space, MAX_int32 meant end of Y component in ALandscape::Import()
	* @param ComponentX2 (optional) - region of texture to update in component space
	* @param ComponentY2 (optional) - region of texture to update in component space
	* @param TextureDataInfo - FLandscapeTextureDataInfo pointer, to notify of the mip data region updated.
	 */
	void GenerateHeightmapMips(TArray<FColor*>& HeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, FLandscapeTextureDataInfo* TextureDataInfo=nullptr);

	/**
	 * Generate empty mipmaps for weightmap
	 */
	LANDSCAPE_API static void CreateEmptyTextureMips(UTexture2D* Texture, bool bClear = false);

	/**
	 * Generate mipmaps for weightmap
	 * Assumes all weightmaps are unique to this component.
	 * @param WeightmapTextureBaseMipData: array of pointers to each of the weightmaps' base locked mip data.
	 */
	template<typename DataType>

	/** @todo document */
	static void GenerateMipsTempl(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, DataType* BaseMipData);

	/** @todo document */
	static void GenerateWeightmapMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, FColor* BaseMipData);

	/**
	 * Update mipmaps for existing weightmap texture
	 */
	template<typename DataType>

	/** @todo document */
	static void UpdateMipsTempl(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<DataType*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, FLandscapeTextureDataInfo* TextureDataInfo=nullptr);

	/** @todo document */
	LANDSCAPE_API static void UpdateWeightmapMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* WeightmapTexture, TArray<FColor*>& WeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, FLandscapeTextureDataInfo* TextureDataInfo=nullptr);

	/** @todo document */
	static void UpdateDataMips(int32 InNumSubsections, int32 InSubsectionSizeQuads, UTexture2D* Texture, TArray<uint8*>& TextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, FLandscapeTextureDataInfo* TextureDataInfo=nullptr);

	/**
	 * Create or updates collision component height data
	 * @param HeightmapTextureMipData: heightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param bUpdateBounds: Whether to update bounds from render component.
	 * @param XYOffsetTextureMipData: xy-offset map data
	 */
	void UpdateCollisionHeightData(const FColor* HeightmapTextureMipData, const FColor* SimpleCollisionHeightmapTextureData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32, bool bUpdateBounds=false, const FColor* XYOffsetTextureMipData=nullptr);

	/** Updates collision component height data for the entire component, locking and unlocking heightmap textures
	 * @param: bRebuild: If true, recreates the collision component */
	void UpdateCollisionData(bool bRebuild);

	/**
	 * Update collision component dominant layer data
	 * @param WeightmapTextureMipData: weightmap data
	 * @param ComponentX1, ComponentY1, ComponentX2, ComponentY2: region to update
	 * @param Whether to update bounds from render component.
	 */
	void UpdateCollisionLayerData(const FColor* const* WeightmapTextureMipData, const FColor* const* const SimpleCollisionWeightmapTextureMipData, int32 ComponentX1=0, int32 ComponentY1=0, int32 ComponentX2=MAX_int32, int32 ComponentY2=MAX_int32);

	/**
	 * Update collision component dominant layer data for the whole component, locking and unlocking the weightmap textures.
	 */
	LANDSCAPE_API void UpdateCollisionLayerData();

	/**
	 * Create weightmaps for this component for the layers specified in the WeightmapLayerAllocations array
	 */
	void ReallocateWeightmaps(FLandscapeEditDataInterface* DataInterface=NULL);

	/** Returns the actor's LandscapeMaterial, or the Component's OverrideLandscapeMaterial if set */
	LANDSCAPE_API UMaterialInterface* GetLandscapeMaterial() const;

	/** Returns the actor's LandscapeHoleMaterial, or the Component's OverrideLandscapeHoleMaterial if set */
	LANDSCAPE_API UMaterialInterface* GetLandscapeHoleMaterial() const;

	/** Returns true if this component has visibility painted */
	LANDSCAPE_API bool ComponentHasVisibilityPainted() const;

	/**
	 * Generate a key for this component's layer allocations to use with MaterialInstanceConstantMap.
	 */
	FString GetLayerAllocationKey(UMaterialInterface* LandscapeMaterial, bool bMobile = false) const;

	/** @todo document */
	void GetLayerDebugColorKey(int32& R, int32& G, int32& B) const;

	/** @todo document */
	void RemoveInvalidWeightmaps();

	/** @todo document */
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;

	/** @todo document */
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;

	/** @todo document */
	LANDSCAPE_API void InitHeightmapData(TArray<FColor>& Heights, bool bUpdateCollision);

	/** @todo document */
	LANDSCAPE_API void InitWeightmapData(TArray<ULandscapeLayerInfoObject*>& LayerInfos, TArray<TArray<uint8> >& Weights);

	/** @todo document */
	LANDSCAPE_API float GetLayerWeightAtLocation( const FVector& InLocation, ULandscapeLayerInfoObject* LayerInfo, TArray<uint8>* LayerCache=NULL );

	/** Extends passed region with this component section size */
	LANDSCAPE_API void GetComponentExtent(int32& MinX, int32& MinY, int32& MaxX, int32& MaxY) const;

	/** Updates navigation properties to match landscape's master switch */
	void UpdateNavigationRelevance();
	
	/** Updates the values of component-level properties exposed by the Landscape Actor */
	LANDSCAPE_API void UpdatedSharedPropertiesFromActor();
#endif

	friend class FLandscapeComponentSceneProxy;
	friend struct FLandscapeComponentDataInterface;

	void SetLOD(bool bForced, int32 InLODValue);

protected:

	/** Whether the component type supports static lighting. */
	virtual bool SupportsStaticLighting() const override
	{
		return true;
	}
};

