// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "UObject/ScriptMacros.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "RenderCommandFence.h"
#include "Templates/ScopedPointer.h"
#include "Components.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Engine/MeshMerging.h"
#include "UniquePtr.h"
#include "StaticMeshResources.h"
#include "StaticMesh.generated.h"

class FSpeedTreeWind;
class UAssetUserData;
class UMaterialInterface;
struct FStaticMeshLODResources;

/*-----------------------------------------------------------------------------
	Legacy mesh optimization settings.
-----------------------------------------------------------------------------*/

/** Optimization settings used to simplify mesh LODs. */
UENUM()
enum ENormalMode
{
	NM_PreserveSmoothingGroups,
	NM_RecalculateNormals,
	NM_RecalculateNormalsSmooth,
	NM_RecalculateNormalsHard,
	TEMP_BROKEN,
	ENormalMode_MAX,
};

UENUM()
enum EImportanceLevel
{
	IL_Off,
	IL_Lowest,
	IL_Low,
	IL_Normal,
	IL_High,
	IL_Highest,
	TEMP_BROKEN2,
	EImportanceLevel_MAX,
};

/** Enum specifying the reduction type to use when simplifying static meshes. */
UENUM()
enum EOptimizationType
{
	OT_NumOfTriangles,
	OT_MaxDeviation,
	OT_MAX,
};

/** Old optimization settings. */
USTRUCT()
struct FStaticMeshOptimizationSettings
{
	GENERATED_USTRUCT_BODY()

	/** The method to use when optimizing the skeletal mesh LOD */
	UPROPERTY()
	TEnumAsByte<enum EOptimizationType> ReductionMethod;

	/** If ReductionMethod equals SMOT_NumOfTriangles this value is the ratio of triangles [0-1] to remove from the mesh */
	UPROPERTY()
	float NumOfTrianglesPercentage;

	/**If ReductionMethod equals SMOT_MaxDeviation this value is the maximum deviation from the base mesh as a percentage of the bounding sphere. */
	UPROPERTY()
	float MaxDeviationPercentage;

	/** The welding threshold distance. Vertices under this distance will be welded. */
	UPROPERTY()
	float WeldingThreshold;

	/** Whether Normal smoothing groups should be preserved. If false then NormalsThreshold is used **/
	UPROPERTY()
	bool bRecalcNormals;

	/** If the angle between two triangles are above this value, the normals will not be
	smooth over the edge between those two triangles. Set in degrees. This is only used when PreserveNormals is set to false*/
	UPROPERTY()
	float NormalsThreshold;

	/** How important the shape of the geometry is (EImportanceLevel). */
	UPROPERTY()
	uint8 SilhouetteImportance;

	/** How important texture density is (EImportanceLevel). */
	UPROPERTY()
	uint8 TextureImportance;

	/** How important shading quality is. */
	UPROPERTY()
	uint8 ShadingImportance;


	FStaticMeshOptimizationSettings()
	: ReductionMethod( OT_MaxDeviation )
	, NumOfTrianglesPercentage( 1.0f )
	, MaxDeviationPercentage( 0.0f )
	, WeldingThreshold( 0.1f )
	, bRecalcNormals( true )
	, NormalsThreshold( 60.0f )
	, SilhouetteImportance( IL_Normal )
	, TextureImportance( IL_Normal )
	, ShadingImportance( IL_Normal )
	{
	}

	/** Serialization for FStaticMeshOptimizationSettings. */
	inline friend FArchive& operator<<( FArchive& Ar, FStaticMeshOptimizationSettings& Settings )
	{
		Ar << Settings.ReductionMethod;
		Ar << Settings.MaxDeviationPercentage;
		Ar << Settings.NumOfTrianglesPercentage;
		Ar << Settings.SilhouetteImportance;
		Ar << Settings.TextureImportance;
		Ar << Settings.ShadingImportance;
		Ar << Settings.bRecalcNormals;
		Ar << Settings.NormalsThreshold;
		Ar << Settings.WeldingThreshold;

		return Ar;
	}

};

/*-----------------------------------------------------------------------------
	UStaticMesh
-----------------------------------------------------------------------------*/

/**
 * Source model from which a renderable static mesh is built.
 */
USTRUCT()
struct FStaticMeshSourceModel
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITOR
	/** Imported raw mesh data. Optional for all but the first LOD. */
	class FRawMeshBulkData* RawMeshBulkData;
#endif // #if WITH_EDITOR

	/** Settings applied when building the mesh. */
	UPROPERTY(EditAnywhere, Category=BuildSettings)
	FMeshBuildSettings BuildSettings;

	/** Reduction settings to apply when building render data. */
	UPROPERTY(EditAnywhere, Category=ReductionSettings)
	FMeshReductionSettings ReductionSettings; 

	UPROPERTY()
	float LODDistance_DEPRECATED;

	/** 
	 * ScreenSize to display this LOD.
	 * The screen size is based around the projected diameter of the bounding
	 * sphere of the model. i.e. 0.5 means half the screen's maximum dimension.
	 */
	UPROPERTY(EditAnywhere, Category=ReductionSettings)
	float ScreenSize;

	/** Default constructor. */
	ENGINE_API FStaticMeshSourceModel();

	/** Destructor. */
	ENGINE_API ~FStaticMeshSourceModel();

#if WITH_EDITOR
	/** Serializes bulk data. */
	void SerializeBulkData(FArchive& Ar, UObject* Owner);
#endif
};

/**
 * Per-section settings.
 */
USTRUCT()
struct FMeshSectionInfo
{
	GENERATED_USTRUCT_BODY()

	/** Index in to the Materials array on UStaticMesh. */
	UPROPERTY()
	int32 MaterialIndex;

	/** If true, collision is enabled for this section. */
	UPROPERTY()
	bool bEnableCollision;

	/** If true, this section will cast shadows. */
	UPROPERTY()
	bool bCastShadow;

	/** Default values. */
	FMeshSectionInfo()
		: MaterialIndex(0)
		, bEnableCollision(true)
		, bCastShadow(true)
	{
	}

	/** Default values with an explicit material index. */
	explicit FMeshSectionInfo(int32 InMaterialIndex)
		: MaterialIndex(InMaterialIndex)
		, bEnableCollision(true)
		, bCastShadow(true)
	{
	}
};

/** Comparison for mesh section info. */
bool operator==(const FMeshSectionInfo& A, const FMeshSectionInfo& B);
bool operator!=(const FMeshSectionInfo& A, const FMeshSectionInfo& B);

/**
 * Map containing per-section settings for each section of each LOD.
 */
USTRUCT()
struct FMeshSectionInfoMap
{
	GENERATED_USTRUCT_BODY()

	/** Maps an LOD+Section to the material it should render with. */
	UPROPERTY()
	TMap<uint32,FMeshSectionInfo> Map;

	/** Serialize. */
	void Serialize(FArchive& Ar);

	/** Clears all entries in the map resetting everything to default. */
	ENGINE_API void Clear();

	/** Get the number of section for a LOD. */
	ENGINE_API int32 GetSectionNumber(int32 LODIndex) const;

	/** Return true if the section exist, false otherwise. */
	ENGINE_API bool IsValidSection(int32 LODIndex, int32 SectionIndex) const;

	/** Gets per-section settings for the specified LOD + section. */
	ENGINE_API FMeshSectionInfo Get(int32 LODIndex, int32 SectionIndex) const;

	/** Sets per-section settings for the specified LOD + section. */
	ENGINE_API void Set(int32 LODIndex, int32 SectionIndex, FMeshSectionInfo Info);

	/** Resets per-section settings for the specified LOD + section to defaults. */
	ENGINE_API void Remove(int32 LODIndex, int32 SectionIndex);

	/** Copies per-section settings from the specified section info map. */
	ENGINE_API void CopyFrom(const FMeshSectionInfoMap& Other);

	/** Returns true if any section has collision enabled. */
	bool AnySectionHasCollision() const;
};

USTRUCT()
struct FAssetEditorOrbitCameraPosition
{
	GENERATED_USTRUCT_BODY()

	FAssetEditorOrbitCameraPosition()
		: bIsSet(false)
	{
	}

	FAssetEditorOrbitCameraPosition(const FVector& InCamOrbitPoint, const FVector& InCamOrbitZoom, const FRotator& InCamOrbitRotation)
		: bIsSet(true)
		, CamOrbitPoint(InCamOrbitPoint)
		, CamOrbitZoom(InCamOrbitZoom)
		, CamOrbitRotation(InCamOrbitRotation)
	{
	}

	/** Whether or not this has been set to a valid value */
	UPROPERTY()
	bool bIsSet;

	/** The position to orbit the camera around */
	UPROPERTY()
	FVector	CamOrbitPoint;

	/** The distance of the camera from the orbit point */
	UPROPERTY()
	FVector CamOrbitZoom;

	/** The rotation to apply around the orbit point */
	UPROPERTY()
	FRotator CamOrbitRotation;
};

#if WITH_EDITOR
/** delegate type for pre mesh build events */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreMeshBuild, class UStaticMesh*);
/** delegate type for pre mesh build events */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostMeshBuild, class UStaticMesh*);
#endif

//~ Begin Material Interface for UStaticMesh - contains a material and other stuff
USTRUCT(BlueprintType)
struct FStaticMaterial
{
	GENERATED_USTRUCT_BODY()

		FStaticMaterial()
		: MaterialInterface(NULL)
		, MaterialSlotName(NAME_None)
#if WITH_EDITORONLY_DATA
		, ImportedMaterialSlotName(NAME_None)
#endif //WITH_EDITORONLY_DATA
	{

	}

	FStaticMaterial(class UMaterialInterface* InMaterialInterface
		, FName InMaterialSlotName = NAME_None
#if WITH_EDITORONLY_DATA
		, FName InImportedMaterialSlotName = NAME_None)
#else
		)
#endif
		: MaterialInterface(InMaterialInterface)
		, MaterialSlotName(InMaterialSlotName)
#if WITH_EDITORONLY_DATA
		, ImportedMaterialSlotName(InImportedMaterialSlotName)
#endif //WITH_EDITORONLY_DATA
	{

	}

	friend FArchive& operator<<(FArchive& Ar, FStaticMaterial& Elem);

	ENGINE_API friend bool operator==(const FStaticMaterial& LHS, const FStaticMaterial& RHS);
	ENGINE_API friend bool operator==(const FStaticMaterial& LHS, const UMaterialInterface& RHS);
	ENGINE_API friend bool operator==(const UMaterialInterface& LHS, const FStaticMaterial& RHS);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = StaticMesh)
	class UMaterialInterface* MaterialInterface;

	/*This name should be use by the gameplay to avoid error if the skeletal mesh Materials array topology change*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = StaticMesh)
	FName MaterialSlotName;

#if WITH_EDITORONLY_DATA
	/*This name should be use when we re-import a skeletal mesh so we can order the Materials array like it should be*/
	UPROPERTY(VisibleAnywhere, Category = StaticMesh)
	FName ImportedMaterialSlotName;
#endif //WITH_EDITORONLY_DATA

	/** Data used for texture streaming relative to each UV channels. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = StaticMesh)
	FMeshUVChannelInfo			UVChannelData;
};


enum EImportStaticMeshVersion
{
	// Before any version changes were made
	BeforeImportStaticMeshVersionWasAdded,
	// Remove the material re-order workflow
	RemoveStaticMeshSkinxxWorkflow,
	VersionPlusOne,
	LastVersion = VersionPlusOne - 1
};

USTRUCT()
struct FMaterialRemapIndex
{
	GENERATED_USTRUCT_BODY()

	FMaterialRemapIndex()
	{
		ImportVersionKey = 0;
	}

	FMaterialRemapIndex(uint32 VersionKey, TArray<int32> RemapArray)
	: ImportVersionKey(VersionKey)
	, MaterialRemap(RemapArray)
	{
	}

	UPROPERTY()
	uint32 ImportVersionKey;

	UPROPERTY()
	TArray<int32> MaterialRemap;
};


/**
 * A StaticMesh is a piece of geometry that consists of a static set of polygons.
 * Static Meshes can be translated, rotated, and scaled, but they cannot have their vertices animated in any way. As such, they are more efficient
 * to render than other types of geometry such as USkeletalMesh, and they are often the basic building block of levels created in the engine.
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Content/Types/StaticMeshes/
 * @see AStaticMeshActor, UStaticMeshComponent
 */
UCLASS(hidecategories=Object, customconstructor, MinimalAPI, BlueprintType, config=Engine)
class UStaticMesh : public UObject, public IInterface_CollisionDataProvider, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	/** Notification when bounds changed */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnExtendedBoundsChanged, const FBoxSphereBounds&);
#endif

	/** Pointer to the data used to render this static mesh. */
	TUniquePtr<class FStaticMeshRenderData> RenderData;

#if WITH_EDITORONLY_DATA
	static const float MinimumAutoLODPixelError;

	/** Imported raw mesh bulk data. */
	UPROPERTY()
	TArray<FStaticMeshSourceModel> SourceModels;

	/** Map of LOD+Section index to per-section info. */
	UPROPERTY()
	FMeshSectionInfoMap SectionInfoMap;

	/**
	 * We need the OriginalSectionInfoMap to be able to build mesh in a non destructive way. Reduce has to play with SectionInfoMap in case some sections disappear.
	 * This member will be update in the following situation
	 * 1. After a static mesh import/reimport
	 * 2. Postload, if the OriginalSectionInfoMap is empty, we will fill it with the current SectionInfoMap
	 *
	 * We do not update it when the user shuffle section in the staticmesh editor because the OriginalSectionInfoMap must always be in sync with the saved rawMesh bulk data.
	 */
	UPROPERTY()
	FMeshSectionInfoMap OriginalSectionInfoMap;

	/** The LOD group to which this mesh belongs. */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category=LodSettings)
	FName LODGroup;

	/** If true, the screen sizees at which LODs swap are computed automatically. */
	UPROPERTY()
	uint32 bAutoComputeLODScreenSize:1;

	/* The last import version */
	UPROPERTY()
	int32 ImportVersion;

	UPROPERTY()
	TArray<FMaterialRemapIndex> MaterialRemapIndexPerImportVersion;
	
	/* The lightmap UV generation version used during the last derived data build */
	UPROPERTY()
	int32 LightmapUVVersion;

	/**
	* If true on post load we need to calculate Display Factors from the
	* loaded LOD distances.
	*/
	bool bRequiresLODDistanceConversion : 1;

	/**
	 * If true on post load we need to calculate resolution independent Display Factors from the
	 * loaded LOD screen sizes.
	 */
	bool bRequiresLODScreenSizeConversion : 1;

#endif // #if WITH_EDITORONLY_DATA

	/** Minimum LOD to use for rendering.  This is the default setting for the mesh and can be overridden by component settings. */
	UPROPERTY()
	int32 MinLOD;

	/** Materials used by this static mesh. Individual sections index in to this array. */
	UPROPERTY()
	TArray<UMaterialInterface*> Materials_DEPRECATED;

	UPROPERTY()
	TArray<FStaticMaterial> StaticMaterials;

	UPROPERTY()
	float LightmapUVDensity;

	UPROPERTY(EditAnywhere, Category=StaticMesh, meta=(ClampMax = 4096, ToolTip="The light map resolution", FixedIncrement="4.0"))
	int32 LightMapResolution;

	/** The light map coordinate index */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=StaticMesh, meta=(ToolTip="The light map coordinate index"))
	int32 LightMapCoordinateIndex;

	/** Useful for reducing self shadowing from distance field methods when using world position offset to animate the mesh's vertices. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = StaticMesh)
	float DistanceFieldSelfShadowBias;

	/** 
	 * Whether to generate a distance field for this mesh, which can be used by DistanceField Indirect Shadows.
	 * This is ignored if the project's 'Generate Mesh Distance Fields' setting is enabled.
	 */
	UPROPERTY(EditAnywhere, Category=StaticMesh)
	uint32 bGenerateMeshDistanceField : 1;

	// Physics data.
	UPROPERTY(EditAnywhere, transient, duplicatetransient, Instanced, Category = StaticMesh)
	class UBodySetup* BodySetup;

	/** 
	 *	Specifies which mesh LOD to use for complex (per-poly) collision. 
	 *	Sometimes it can be desirable to use a lower poly representation for collision to reduce memory usage, improve performance and behaviour.
	 *	Collision representation does not change based on distance to camera.
	 */
	UPROPERTY(EditAnywhere, Category = StaticMesh, meta=(DisplayName="LOD For Collision"))
	int32 LODForCollision;

	/** If true, strips unwanted complex collision data aka kDOP tree when cooking for consoles.
		On the Playstation 3 data of this mesh will be stored in video memory. */
	UPROPERTY()
	uint32 bStripComplexCollisionForConsole_DEPRECATED:1;

	/** If true, mesh will have NavCollision property with additional data for navmesh generation and usage.
	    Set to false for distant meshes (always outside navigation bounds) to save memory on collision data. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Navigation)
	uint32 bHasNavigationData:1;

	/**	
		Mesh supports uniformly distributed sampling in constant time.
		Memory cost is 8 bytes per triangle.
		Example usage is uniform spawning of particles.
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = StaticMesh)
	uint32 bSupportUniformlyDistributedSampling : 1;

	/** Bias multiplier for Light Propagation Volume lighting */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=StaticMesh, meta=(UIMin = "0.0", UIMax = "3.0"))
	float LpvBiasMultiplier;

	/** 
	 *	If true, will keep geometry data CPU-accessible in cooked builds, rather than uploading to GPU memory and releasing it from CPU memory.
	 *	This is required if you wish to access StaticMesh geometry data on the CPU at runtime in cooked builds (e.g. to convert StaticMesh to ProceduralMeshComponent)
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = StaticMesh)
	bool bAllowCPUAccess;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence ReleaseResourcesFence;

	/**
	 * For simplified meshes, this is the fully qualified path and name of the static mesh object we were
	 * originally duplicated from.  This is serialized to disk, but is discarded when cooking for consoles.
	 */
	FString HighResSourceMeshName;

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this mesh */
	UPROPERTY(EditAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

	/** Path to the resource used to construct this static mesh */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, AdvancedDisplay, Category=StaticMesh)
	class UThumbnailInfo* ThumbnailInfo;

	/** The stored camera position to use as a default for the static mesh editor */
	UPROPERTY()
	FAssetEditorOrbitCameraPosition EditorCameraPosition;

	/** If the user has modified collision in any way or has custom collision imported. Used for determining if to auto generate collision on import */
	UPROPERTY(EditAnywhere, Category = Collision)
	bool bCustomizedCollision;

#endif // WITH_EDITORONLY_DATA

	/** For simplified meshes, this is the CRC of the high res mesh we were originally duplicated from. */
	uint32 HighResSourceMeshCRC;

	/** Unique ID for tracking/caching this mesh during distributed lighting */
	FGuid LightingGuid;

	/**
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying
	 *	everything explicitly to AttachComponent in the StaticMeshComponent.
	 */
	UPROPERTY()
	TArray<class UStaticMeshSocket*> Sockets;

	/** Data that is only available if this static mesh is an imported SpeedTree */
	TSharedPtr<class FSpeedTreeWind> SpeedTreeWind;

	/** Bound extension values in the positive direction of XYZ, positive value increases bound size */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = StaticMesh)
	FVector PositiveBoundsExtension;
	/** Bound extension values in the negative direction of XYZ, positive value increases bound size */
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = StaticMesh)
	FVector NegativeBoundsExtension;
	/** Original mesh bounds extended with Positive/NegativeBoundsExtension */
	UPROPERTY()
	FBoxSphereBounds ExtendedBounds;

#if WITH_EDITOR
	FOnExtendedBoundsChanged OnExtendedBoundsChanged;
#endif

protected:
	/**
	 * Index of an element to ignore while gathering streaming texture factors.
	 * This is useful to disregard automatically generated vertex data which breaks texture factor heuristics.
	 */
	UPROPERTY()
	int32 ElementToIgnoreForTexFactor;

	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = StaticMesh)
	TArray<UAssetUserData*> AssetUserData;

public:
	/** Pre-build navigation collision */
	UPROPERTY(VisibleAnywhere, transient, duplicatetransient, Instanced, Category = Navigation)
	class UNavCollision* NavCollision;
    
    /** Properties for the associated Flex object */
    UPROPERTY(EditAnywhere, Instanced, Category = Flex)
    class UFlexAsset* FlexAsset;

public:
	/**
	 * Default constructor
	 */
	ENGINE_API UStaticMesh(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~ Begin UObject Interface.
#if WITH_EDITOR
	ENGINE_API virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	ENGINE_API virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
	ENGINE_API void SetLODGroup(FName NewGroup, bool bRebuildImmediately = true);
	ENGINE_API void BroadcastNavCollisionChange();

	FOnExtendedBoundsChanged& GetOnExtendedBoundsChanged() { return OnExtendedBoundsChanged; }
#endif // WITH_EDITOR

	ENGINE_API virtual void Serialize(FArchive& Ar) override;
	ENGINE_API virtual void PostInitProperties() override;
	ENGINE_API virtual void PostLoad() override;
	ENGINE_API virtual void BeginDestroy() override;
	ENGINE_API virtual bool IsReadyForFinishDestroy() override;
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	ENGINE_API virtual FString GetDesc() override;
	ENGINE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	ENGINE_API virtual bool CanBeClusterRoot() const override;
	
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);					   

	//~ End UObject Interface.

	/**
	 * Rebuilds renderable data for this static mesh.
	 * @param bSilent - If true will not popup a progress dialog.
	 */
	ENGINE_API void Build(bool bSilent = false, TArray<FText>* OutErrors = nullptr);

	/**
	 * Initialize the static mesh's render resources.
	 */
	ENGINE_API virtual void InitResources();

	/**
	 * Releases the static mesh's render resources.
	 */
	ENGINE_API virtual void ReleaseResources();

	/**
	 * Update missing material UV channel data used for texture streaming. 
	 *
	 * @param bRebuildAll		If true, rebuild everything and not only missing data.
	 */
	ENGINE_API void UpdateUVChannelData(bool bRebuildAll);

	/**
	 * Returns the material bounding box. Computed from all lod-section using the material index.
	 *
	 * @param MaterialIndex			Material Index to look at
	 * @param TransformMatrix		Matrix to be applied to the position before computing the bounds
	 *
	 * @return false if some parameters are invalid
	 */
	ENGINE_API FBox GetMaterialBox(int32 MaterialIndex, const FTransform& Transform) const;

	/**
	 * Returns the UV channel data for a given material index. Used by the texture streamer.
	 * This data applies to all lod-section using the same material.
	 *
	 * @param MaterialIndex		the material index for which to get the data for.
	 * @return the data, or null if none exists.
	 */
	ENGINE_API const FMeshUVChannelInfo* GetUVChannelData(int32 MaterialIndex) const;

	/**
	 * Returns the number of vertices for the specified LOD.
	 */
	ENGINE_API int32 GetNumVertices(int32 LODIndex) const;

	/**
	 * Returns the number of LODs used by the mesh.
	 */
	UFUNCTION(BlueprintCallable, Category = "StaticMesh")
	ENGINE_API int32 GetNumLODs() const;

	/**
	 * Returns true if the mesh has data that can be rendered.
	 */
	ENGINE_API bool HasValidRenderData() const;

	/**
	 * Returns the number of bounds of the mesh.
	 *
	 * @return	The bounding box represented as box origin with extents and also a sphere that encapsulates that box
	 */
	UFUNCTION( BlueprintPure, Category="StaticMesh" )
	ENGINE_API FBoxSphereBounds GetBounds() const;

	/** Returns the bounding box, in local space including bounds extension(s), of the StaticMesh asset */
	UFUNCTION(BlueprintCallable, Category="StaticMesh")
	ENGINE_API FBox GetBoundingBox() const;

	/** Returns number of Sections that this StaticMesh has, in the supplied LOD (LOD 0 is the highest) */
	UFUNCTION(BlueprintCallable, Category = "StaticMesh")
	ENGINE_API int32 GetNumSections(int32 InLOD) const;

	/**
	 * Gets a Material given a Material Index and an LOD number
	 *
	 * @return Requested material
	 */
	UFUNCTION(BlueprintCallable, Category = "StaticMesh")
	ENGINE_API UMaterialInterface* GetMaterial(int32 MaterialIndex) const;

	/**
	* Gets a Material index given a slot name
	*
	* @return Requested material
	*/
	UFUNCTION(BlueprintCallable, Category = "StaticMesh")
	ENGINE_API int32 GetMaterialIndex(FName MaterialSlotName) const;

	/**
	 * Returns the render data to use for exporting the specified LOD. This method should always
	 * be called when exporting a static mesh.
	 */
	ENGINE_API const FStaticMeshLODResources& GetLODForExport(int32 LODIndex) const;

	/**
	 * Static: Processes the specified static mesh for light map UV problems
	 *
	 * @param	InStaticMesh					Static mesh to process
	 * @param	InOutAssetsWithMissingUVSets	Array of assets that we found with missing UV sets
	 * @param	InOutAssetsWithBadUVSets		Array of assets that we found with bad UV sets
	 * @param	InOutAssetsWithValidUVSets		Array of assets that we found with valid UV sets
	 * @param	bInVerbose						If true, log the items as they are found
	 */
	ENGINE_API static void CheckLightMapUVs( UStaticMesh* InStaticMesh, TArray< FString >& InOutAssetsWithMissingUVSets, TArray< FString >& InOutAssetsWithBadUVSets, TArray< FString >& InOutAssetsWithValidUVSets, bool bInVerbose = true );

	//~ Begin Interface_CollisionDataProvider Interface
	ENGINE_API virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	ENGINE_API virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override
	{
		return true;
	}
	ENGINE_API virtual void GetMeshId(FString& OutMeshId) override;
	//~ End Interface_CollisionDataProvider Interface

	/** Return the number of sections of the StaticMesh with collision enabled */
	int32 GetNumSectionsWithCollision() const;

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface


	/**
	 * Create BodySetup for this staticmesh if it doesn't have one
	 */
	ENGINE_API void CreateBodySetup();

	/**
	 * Calculates navigation collision for caching
	 */
	ENGINE_API void CreateNavCollision(const bool bIsUpdate = false);

	FORCEINLINE const UNavCollision* GetNavCollision() const { return NavCollision; }

	/** Configures this SM as bHasNavigationData = false and clears stored UNavCollision */
	ENGINE_API void MarkAsNotHavingNavigationData();

	const FGuid& GetLightingGuid() const
	{
#if WITH_EDITORONLY_DATA
		return LightingGuid;
#else
		static const FGuid NullGuid( 0, 0, 0, 0 );
		return NullGuid;
#endif // WITH_EDITORONLY_DATA
	}

	void SetLightingGuid()
	{
#if WITH_EDITORONLY_DATA
		LightingGuid = FGuid::NewGuid();
#endif // WITH_EDITORONLY_DATA
	}

	/**
	 *	Find a socket object in this StaticMesh by name.
	 *	Entering NAME_None will return NULL. If there are multiple sockets with the same name, will return the first one.
	 */
	ENGINE_API class UStaticMeshSocket* FindSocket(FName InSocketName);
	/**
	 * Returns vertex color data by position.
	 * For matching to reimported meshes that may have changed or copying vertex paint data from mesh to mesh.
	 *
	 *	@param	VertexColorData		(out)A map of vertex position data and its color. The method fills this map.
	 */
	ENGINE_API void GetVertexColorData(TMap<FVector, FColor>& VertexColorData);

	/**
	 * Sets vertex color data by position.
	 * Map of vertex color data by position is matched to the vertex position in the mesh
	 * and nearest matching vertex color is used.
	 *
	 *	@param	VertexColorData		A map of vertex position data and color.
	 */
	ENGINE_API void SetVertexColorData(const TMap<FVector, FColor>& VertexColorData);

	/** Removes all vertex colors from this mesh and rebuilds it (Editor only */
	ENGINE_API void RemoveVertexColors();

	void EnforceLightmapRestrictions();

	/** Calculates the extended bounds */
	ENGINE_API void CalculateExtendedBounds();

#if WITH_EDITOR

	/**
	 * Returns true if LODs of this static mesh may share texture lightmaps.
	 */
	bool CanLODsShareStaticLighting() const;

	/**
	 * Retrieves the names of all LOD groups.
	 */
	ENGINE_API static void GetLODGroups(TArray<FName>& OutLODGroups);

	/**
	 * Retrieves the localized display names of all LOD groups.
	 */
	ENGINE_API static void GetLODGroupsDisplayNames(TArray<FText>& OutLODGroupsDisplayNames);

	ENGINE_API void GenerateLodsInPackage();

	/** Get multicast delegate broadcast prior to mesh building */
	FOnPreMeshBuild& OnPreMeshBuild() { return PreMeshBuild; }

	/** Get multicast delegate broadcast after mesh building */
	FOnPostMeshBuild& OnPostMeshBuild() { return PostMeshBuild; }

private:
	/**
	 * Converts legacy LODDistance in the source models to Display Factor
	 */
	void ConvertLegacyLODDistance();

	/**
	 * Converts legacy LOD screen area in the source models to resolution-independent screen size
	 */
	void ConvertLegacyLODScreenArea();

	/**
	 * Fixes up static meshes that were imported with sections that had zero triangles.
	 */
	void FixupZeroTriangleSections();

	/**
	 * Caches derived renderable data.
	 */
	void CacheDerivedData();


	FOnPreMeshBuild PreMeshBuild;
	FOnPostMeshBuild PostMeshBuild;

	/**
	 * Fixes up the material when it was converted to the new staticmesh build process
	 */
	bool CleanUpRedondantMaterialPostLoad;

#endif // #if WITH_EDITOR
};
