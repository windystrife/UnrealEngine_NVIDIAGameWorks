// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Contains the shared data that is used by all SkeletalMeshComponents (instances).
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "RenderCommandFence.h"
#include "EngineDefines.h"
#include "Components.h"
#include "ReferenceSkeleton.h"
#include "GPUSkinPublicDefs.h"
#include "SkeletalMeshTypes.h"
#include "Animation/PreviewAssetAttachComponent.h"
#include "BoneContainer.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "EngineTypes.h"

#include "SkeletalMesh.generated.h"

/** The maximum number of skeletal mesh LODs allowed. */
#define MAX_SKELETAL_MESH_LODS 5

class UAnimInstance;
class UAssetUserData;
class UBodySetup;
class UMorphTarget;
class USkeletalMeshSocket;
class USkeleton;
class UClothingAssetBase;
class UBlueprint;
class UNodeMappingContainer;

#if WITH_APEX_CLOTHING

namespace nvidia
{
	namespace apex
	{
		class ClothingAsset;
	}
}
#endif

/** Enum specifying the importance of properties when simplifying skeletal meshes. */
UENUM()
enum SkeletalMeshOptimizationImportance
{	
	SMOI_Off,
	SMOI_Lowest,
	SMOI_Low,
	SMOI_Normal,
	SMOI_High,
	SMOI_Highest,
	SMOI_MAX,
};

/** Enum specifying the reduction type to use when simplifying skeletal meshes. */
UENUM()
enum SkeletalMeshOptimizationType
{
	SMOT_NumOfTriangles,
	SMOT_MaxDeviation,
	SMOT_MAX,
};

USTRUCT()
struct FBoneMirrorInfo
{
	GENERATED_USTRUCT_BODY()

	/** The bone to mirror. */
	UPROPERTY(EditAnywhere, Category=BoneMirrorInfo, meta=(ArrayClamp = "RefSkeleton"))
	int32 SourceIndex;

	/** Axis the bone is mirrored across. */
	UPROPERTY(EditAnywhere, Category=BoneMirrorInfo)
	TEnumAsByte<EAxis::Type> BoneFlipAxis;

	FBoneMirrorInfo()
		: SourceIndex(0)
		, BoneFlipAxis(0)
	{
	}

};

/** Structure to export/import bone mirroring information */
USTRUCT()
struct FBoneMirrorExport
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=BoneMirrorExport)
	FName BoneName;

	UPROPERTY(EditAnywhere, Category=BoneMirrorExport)
	FName SourceBoneName;

	UPROPERTY(EditAnywhere, Category=BoneMirrorExport)
	TEnumAsByte<EAxis::Type> BoneFlipAxis;


	FBoneMirrorExport()
		: BoneFlipAxis(0)
	{
	}

};

/** Struct containing triangle sort settings for a particular section */
USTRUCT()
struct FTriangleSortSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=TriangleSortSettings)
	TEnumAsByte<enum ETriangleSortOption> TriangleSorting;

	UPROPERTY(EditAnywhere, Category=TriangleSortSettings)
	TEnumAsByte<enum ETriangleSortAxis> CustomLeftRightAxis;

	UPROPERTY(EditAnywhere, Category=TriangleSortSettings)
	FName CustomLeftRightBoneName;


	FTriangleSortSettings()
		: TriangleSorting(0)
		, CustomLeftRightAxis(0)
	{
	}

};

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
USTRUCT()
struct ENGINE_API FSkeletalMeshIndexBufferRanges
{
	GENERATED_BODY();

	struct FPerSectionInfo
	{
		TArray<FInt32Range, TInlineAllocator<1>> Regions;

		friend FArchive& operator<<(FArchive& Ar, FPerSectionInfo& S)
		{
			return Ar << S.Regions;
		}
	};

	struct FPerLODInfo
	{
		TArray<FPerSectionInfo> Sections;

		friend FArchive& operator<<(FArchive& Ar, FPerLODInfo& S)
		{
			return Ar << S.Sections;
		}
	};

	TArray<FPerLODInfo, TInlineAllocator<MAX_SKELETAL_MESH_LODS>> LODModels;

	//Using UPROPERTIES here would result in a very large data size since the name is stored for each entry
	bool Serialize(FArchive& Ar)
	{
		Ar << LODModels;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FSkeletalMeshIndexBufferRanges> : public TStructOpsTypeTraitsBase2<FSkeletalMeshIndexBufferRanges>
{
	enum
	{
		WithSerializer = true,
		WithCopy = true,
	};
};
//nv end

/**
* FSkeletalMeshOptimizationSettings - The settings used to optimize a skeletal mesh LOD.
*/
USTRUCT()
struct FSkeletalMeshOptimizationSettings
{
	GENERATED_USTRUCT_BODY()

	/** The method to use when optimizing the skeletal mesh LOD */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationType> ReductionMethod;

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

	/** How important the shape of the geometry is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> SilhouetteImportance;

	/** How important texture density is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> TextureImportance;

	/** How important shading quality is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> ShadingImportance;

	/** How important skinning quality is. */
	UPROPERTY()
	TEnumAsByte<enum SkeletalMeshOptimizationImportance> SkinningImportance;

	/** The ratio of bones that will be removed from the mesh */
	UPROPERTY()
	float BoneReductionRatio;

	/** Maximum number of bones that can be assigned to each vertex. */
	UPROPERTY()
	int32 MaxBonesPerVertex;

	UPROPERTY()
	TArray<FBoneReference> BonesToRemove_DEPRECATED;

	/** Maximum number of bones that can be assigned to each vertex. */
	UPROPERTY()
	int32 BaseLOD;

	UPROPERTY()
	class UAnimSequence* BakePose_DEPRECATED;

	FSkeletalMeshOptimizationSettings()
		: ReductionMethod(SMOT_MaxDeviation)
		, NumOfTrianglesPercentage(1.0f)
		, MaxDeviationPercentage(0)
		, WeldingThreshold(0.1f)
		, bRecalcNormals(true)
		, NormalsThreshold(60.0f)
		, SilhouetteImportance(SMOI_Normal)
		, TextureImportance(SMOI_Normal)
		, ShadingImportance(SMOI_Normal)
		, SkinningImportance(SMOI_Normal)
		, BoneReductionRatio(100.0f)
		, MaxBonesPerVertex(4)
		, BaseLOD(0)
		, BakePose_DEPRECATED(nullptr)
	{
	}

	/** Equality operator. */
	bool operator==(const FSkeletalMeshOptimizationSettings& Other) const
	{
		return ReductionMethod == Other.ReductionMethod
			&& NumOfTrianglesPercentage == Other.NumOfTrianglesPercentage
			&& MaxDeviationPercentage == Other.MaxDeviationPercentage
			&& WeldingThreshold == Other.WeldingThreshold
			&& NormalsThreshold == Other.NormalsThreshold
			&& SilhouetteImportance == Other.SilhouetteImportance
			&& TextureImportance == Other.TextureImportance
			&& ShadingImportance == Other.ShadingImportance
			&& SkinningImportance == Other.SkinningImportance
			&& bRecalcNormals == Other.bRecalcNormals
			&& BoneReductionRatio == Other.BoneReductionRatio
			&& MaxBonesPerVertex == Other.MaxBonesPerVertex
			&& BaseLOD == Other.BaseLOD;
	}

	/** Inequality. */
	bool operator!=(const FSkeletalMeshOptimizationSettings& Other) const
	{
		return !(*this == Other);
	}
};

/** Struct holding parameters needed when creating a new clothing asset or sub asset (LOD) */
USTRUCT()
struct ENGINE_API FSkeletalMeshClothBuildParams
{
	GENERATED_BODY()

	FSkeletalMeshClothBuildParams();

	// Target asset when importing LODs
	UPROPERTY(EditAnywhere, Category = Target)
	TWeakObjectPtr<UClothingAssetBase> TargetAsset;

	// Target LOD to import to when importing LODs
	UPROPERTY(EditAnywhere, Category = Target)
	int32 TargetLod;

	// If reimporting, this will map the old LOD parameters to the new LOD mesh.
	// If adding a new LOD this will map the parameters from the preceeding LOD.
	UPROPERTY(EditAnywhere, Category = Target)
	bool bRemapParameters;

	// Name of the clothing asset 
	UPROPERTY(EditAnywhere, Category = Basic)
	FString AssetName;

	// LOD to extract the section from
	UPROPERTY(EditAnywhere, Category = Basic)
	int32 LodIndex;

	// Section within the specified LOD to extract
	UPROPERTY(EditAnywhere, Category = Basic)
	int32 SourceSection;

	// Whether or not to leave this section behind (if driving a mesh with itself). Enable this if driving a high poly mesh with a low poly
	UPROPERTY(EditAnywhere, Category = Basic)
	bool bRemoveFromMesh;

	// Physics asset to extract collisions from, note this will only extract spheres and Sphyls, as that is what the simulation supports.
	UPROPERTY(EditAnywhere, Category = Collision)
	TSoftObjectPtr<UPhysicsAsset> PhysicsAsset;
};

/** Struct containing information for a particular LOD level, such as materials and info for when to use it. */
USTRUCT()
struct FSkeletalMeshLODInfo
{
	GENERATED_USTRUCT_BODY()

	/** 
	 * ScreenSize to display this LOD.
	 * The screen size is based around the projected diameter of the bounding
	 * sphere of the model. i.e. 0.5 means half the screen's maximum dimension.
	 */
	UPROPERTY(EditAnywhere, Category=SkeletalMeshLODInfo)
	float ScreenSize;

	/**	Used to avoid 'flickering' when on LOD boundary. Only taken into account when moving from complex->simple. */
	UPROPERTY(EditAnywhere, Category=SkeletalMeshLODInfo)
	float LODHysteresis;

	/** Mapping table from this LOD's materials to the USkeletalMesh materials array. */
	UPROPERTY()
	TArray<int32> LODMaterialMap;

	/** Per-section control over whether to enable shadow casting. */
	UPROPERTY()
	TArray<bool> bEnableShadowCasting_DEPRECATED;

	UPROPERTY()
	TArray<struct FTriangleSortSettings> TriangleSortSettings;

	/** Whether to disable morph targets for this LOD. */
	UPROPERTY()
	uint32 bHasBeenSimplified:1;

	/** Reduction settings to apply when building render data. */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	FSkeletalMeshOptimizationSettings ReductionSettings;

	/** This has been removed in editor. We could re-apply this in import time or by mesh reduction utilities*/
	UPROPERTY()
	TArray<FName> RemovedBones_DEPRECATED;

	/** Bones which should be removed from the skeleton for the LOD level */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	TArray<FBoneReference> BonesToRemove;

	/** Pose which should be used to reskin vertex influences for which the bones will be removed in this LOD level, uses ref-pose by default */
	UPROPERTY(EditAnywhere, Category = ReductionSettings)
	class UAnimSequence* BakePose;

	/** The filename of the file tha was used to import this LOD if it was not auto generated. */
	UPROPERTY(VisibleAnywhere, Category= SkeletalMeshLODInfo, AdvancedDisplay)
	FString SourceImportFilename;

	UPROPERTY()
	uint32 bHasPerLODVertexColors : 1;

	FSkeletalMeshLODInfo()
		: ScreenSize(0)
		, LODHysteresis(0)
		, bHasBeenSimplified(false)
		, BakePose(nullptr)
		, bHasPerLODVertexColors(false)
	{
	}

};

/**
 * Legacy object for back-compat loading, no longer used by clothing system
 */
USTRUCT()
struct FClothPhysicsProperties_Legacy
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	float VerticalResistance;
	UPROPERTY()
	float HorizontalResistance;
	UPROPERTY()
	float BendResistance;
	UPROPERTY()
	float ShearResistance;
	UPROPERTY()
	float Friction;
	UPROPERTY()
	float Damping;
	UPROPERTY()
	float TetherStiffness;
	UPROPERTY()
	float TetherLimit;
	UPROPERTY()
	float Drag;
	UPROPERTY()
	float StiffnessFrequency;
	UPROPERTY()
	float GravityScale;
	UPROPERTY()
	float MassScale;
	UPROPERTY()
	float InertiaBlend;
	UPROPERTY()
	float SelfCollisionThickness;
	UPROPERTY()
	float SelfCollisionSquashScale;
	UPROPERTY()
	float SelfCollisionStiffness;
	UPROPERTY()
	float SolverFrequency;
	UPROPERTY()
	float FiberCompression;
	UPROPERTY()
	float FiberExpansion;
	UPROPERTY()
	float FiberResistance;

};


// Legacy struct for handling back compat serialization
USTRUCT()
struct FClothingAssetData_Legacy
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName AssetName;
	UPROPERTY()
	FString	ApexFileName;
	UPROPERTY()
	bool bClothPropertiesChanged;
	UPROPERTY()
	FClothPhysicsProperties_Legacy PhysicsProperties;
#if WITH_APEX_CLOTHING
	nvidia::apex::ClothingAsset* ApexClothingAsset;
	FClothingAssetData_Legacy()
		:ApexClothingAsset(NULL)
	{
	}
#endif// #if WITH_APEX_CLOTHING
	// serialization
	friend FArchive& operator<<(FArchive& Ar, FClothingAssetData_Legacy& A);
	// get resource size
	DEPRECATED(4.14, "GetResourceSize is deprecated. Please use GetResourceSizeEx or GetResourceSizeBytes instead.")
	SIZE_T GetResourceSize() const;
	void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;
	SIZE_T GetResourceSizeBytes() const;
};

//~ Begin Material Interface for USkeletalMesh - contains a material and a shadow casting flag
USTRUCT(BlueprintType)
struct FSkeletalMaterial
{
	GENERATED_USTRUCT_BODY()

	FSkeletalMaterial()
		: MaterialInterface( NULL )
		, bEnableShadowCasting_DEPRECATED( true )
		, bRecomputeTangent_DEPRECATED( false )
		, MaterialSlotName( NAME_None )
#if WITH_EDITORONLY_DATA
		, ImportedMaterialSlotName( NAME_None )
#endif
	{

	}

	FSkeletalMaterial( class UMaterialInterface* InMaterialInterface
						, bool bInEnableShadowCasting = true
						, bool bInRecomputeTangent = false
						, FName InMaterialSlotName = NAME_None
						, FName InImportedMaterialSlotName = NAME_None)
		: MaterialInterface( InMaterialInterface )
		, bEnableShadowCasting_DEPRECATED( bInEnableShadowCasting )
		, bRecomputeTangent_DEPRECATED( bInRecomputeTangent )
		, MaterialSlotName(InMaterialSlotName)
#if WITH_EDITORONLY_DATA
		, ImportedMaterialSlotName(InImportedMaterialSlotName)
#endif //WITH_EDITORONLY_DATA
	{

	}

	friend FArchive& operator<<( FArchive& Ar, FSkeletalMaterial& Elem );

	ENGINE_API friend bool operator==( const FSkeletalMaterial& LHS, const FSkeletalMaterial& RHS );
	ENGINE_API friend bool operator==( const FSkeletalMaterial& LHS, const UMaterialInterface& RHS );
	ENGINE_API friend bool operator==( const UMaterialInterface& LHS, const FSkeletalMaterial& RHS );

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SkeletalMesh)
	class UMaterialInterface *	MaterialInterface;
	UPROPERTY()
	bool						bEnableShadowCasting_DEPRECATED;
	UPROPERTY()
	bool						bRecomputeTangent_DEPRECATED;
	
	/*This name should be use by the gameplay to avoid error if the skeletal mesh Materials array topology change*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SkeletalMesh)
	FName						MaterialSlotName;
#if WITH_EDITORONLY_DATA
	/*This name should be use when we re-import a skeletal mesh so we can order the Materials array like it should be*/
	UPROPERTY(VisibleAnywhere, Category = SkeletalMesh)
	FName						ImportedMaterialSlotName;
#endif //WITH_EDITORONLY_DATA

	/** Data used for texture streaming relative to each UV channels. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = SkeletalMesh)
	FMeshUVChannelInfo			UVChannelData;
};

class FSkeletalMeshResource;

/**
 * SkeletalMesh is geometry bound to a hierarchical skeleton of bones which can be animated for the purpose of deforming the mesh.
 * Skeletal Meshes are built up of two parts; a set of polygons composed to make up the surface of the mesh, and a hierarchical skeleton which can be used to animate the polygons.
 * The 3D models, rigging, and animations are created in an external modeling and animation application (3DSMax, Maya, Softimage, etc).
 *
 * @see https://docs.unrealengine.com/latest/INT/Engine/Content/Types/SkeletalMeshes/
 */
UCLASS(hidecategories=Object, BlueprintType)
class ENGINE_API USkeletalMesh : public UObject, public IInterface_CollisionDataProvider, public IInterface_AssetUserData
{
	GENERATED_UCLASS_BODY()

private:
	/** Rendering resources created at import time. */
	TSharedPtr<FSkeletalMeshResource> ImportedResource;

public:
	/** Get the default resource for this skeletal mesh. */
	FORCEINLINE FSkeletalMeshResource* GetImportedResource() const { return ImportedResource.Get(); }

	/** Get the resource to use for rendering. */
	FORCEINLINE FSkeletalMeshResource* GetResourceForRendering() const { return GetImportedResource(); }

	/** Skeleton of this skeletal mesh **/
	UPROPERTY(Category=Mesh, AssetRegistrySearchable, VisibleAnywhere, BlueprintReadOnly)
	USkeleton* Skeleton;

private:
	/** Original imported mesh bounds */
	UPROPERTY(transient, duplicatetransient)
	FBoxSphereBounds ImportedBounds;

	/** Bounds extended by user values below */
	UPROPERTY(transient, duplicatetransient)
	FBoxSphereBounds ExtendedBounds;

protected:
	// The properties below are protected to force the use of the Set* methods for this data
	// in code so we can keep the extended bounds up to date after changing the data.
	// Property editors will trigger property events to correctly recalculate the extended bounds.

	/** Bound extension values in addition to imported bound in the positive direction of XYZ, 
	 *	positive value increases bound size and negative value decreases bound size. 
	 *	The final bound would be from [Imported Bound - Negative Bound] to [Imported Bound + Positive Bound]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mesh)
	FVector PositiveBoundsExtension;

	/** Bound extension values in addition to imported bound in the negative direction of XYZ, 
	 *	positive value increases bound size and negative value decreases bound size. 
	 *	The final bound would be from [Imported Bound - Negative Bound] to [Imported Bound + Positive Bound]. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mesh)
	FVector NegativeBoundsExtension;

public:

	/** Get the extended bounds of this mesh (imported bounds plus bounds extension) */
	UFUNCTION(BlueprintCallable, Category = Mesh)
	FBoxSphereBounds GetBounds();

	/** Get the original imported bounds of the skel mesh */
	UFUNCTION(BlueprintCallable, Category = Mesh)
	FBoxSphereBounds GetImportedBounds();

	/** Set the original imported bounds of the skel mesh, will recalculate extended bounds */
	void SetImportedBounds(const FBoxSphereBounds& InBounds);

	/** Set bound extension values in the positive direction of XYZ, positive value increases bound size */
	void SetPositiveBoundsExtension(const FVector& InExtension);

	/** Set bound extension values in the negative direction of XYZ, positive value increases bound size */
	void SetNegativeBoundsExtension(const FVector& InExtension);

	/** Calculate the extended bounds based on the imported bounds and the extension values */
	void CalculateExtendedBounds();

	/** Alters the bounds extension values to fit correctly into the current bounds (so negative values never extend the bounds etc.) */
	void ValidateBoundsExtension();

#if WITH_EDITOR
	/** This is a bit hacky. If you are inherriting from SkeletalMesh you can opt out of using the skeletal mesh actor factory. Note that this only works for one level of inherritence and is not a good long term solution */
	virtual bool HasCustomActorFactory() const
	{
		return false;
	}

	/** This is a bit hacky. If you are inherriting from SkeletalMesh you can opt out of using the skeletal mesh actor factory. Note that this only works for one level of inherritence and is not a good long term solution */
	virtual bool HasCustomActorReimportFactory() const
	{
		return false;
	}
#endif

	/** List of materials applied to this mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, transient, duplicatetransient, Category=SkeletalMesh)
	TArray<FSkeletalMaterial> Materials;

	/** List of bones that should be mirrored. */
	UPROPERTY(EditAnywhere, editfixedsize, Category=Mirroring)
	TArray<struct FBoneMirrorInfo> SkelMirrorTable;

	UPROPERTY(EditAnywhere, Category=Mirroring)
	TEnumAsByte<EAxis::Type> SkelMirrorAxis;

	UPROPERTY(EditAnywhere, Category=Mirroring)
	TEnumAsByte<EAxis::Type> SkelMirrorFlipAxis;

	/** Struct containing information for each LOD level, such as materials to use, and when use the LOD. */
	UPROPERTY(EditAnywhere, EditFixedSize, Category=LevelOfDetail)
	TArray<struct FSkeletalMeshLODInfo> LODInfo;

	/** If true, use 32 bit UVs. If false, use 16 bit UVs to save memory */
	UPROPERTY(EditAnywhere, Category=Mesh)
	uint32 bUseFullPrecisionUVs:1;

	/** true if this mesh has ever been simplified with Simplygon. */
	UPROPERTY()
	uint32 bHasBeenSimplified:1;

	/** Whether or not the mesh has vertex colors */
	UPROPERTY()
	uint32 bHasVertexColors:1;

	/** Uses skinned data for collision data. Per poly collision cannot be used for simulation, in most cases you are better off using the physics asset */
	UPROPERTY(EditAnywhere, Category = Physics)
	uint32 bEnablePerPolyCollision : 1;

	// Physics data for the per poly collision case. In 99% of cases you will not need this and are better off using simple ragdoll collision (physics asset)
	UPROPERTY(transient)
	class UBodySetup* BodySetup;

	/**
	 *	Physics and collision information used for this USkeletalMesh, set up in Physics Asset Editor.
	 *	This is used for per-bone hit detection, accurate bounding box calculation and ragdoll physics for example.
	 */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, BlueprintReadOnly, Category=Physics)
	class UPhysicsAsset* PhysicsAsset;

	/**
	 * Physics asset whose shapes will be used for shadowing when components have bCastCharacterCapsuleDirectShadow or bCastCharacterCapsuleIndirectShadow enabled.
	 * Only spheres and sphyl shapes in the physics asset can be supported.  The more shapes used, the higher the cost of the capsule shadows will be.
	 */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, BlueprintReadOnly, Category=Lighting)
	class UPhysicsAsset* ShadowPhysicsAsset;

	/** Mapping data that is saved */
	UPROPERTY(EditAnywhere, editfixedsize, BlueprintReadOnly, Category=Animation)
	TArray<class UNodeMappingContainer*> NodeMappingData;

	UFUNCTION(BlueprintCallable, Category = "Animation")
	class UNodeMappingContainer* GetNodeMappingContainer(class UBlueprint* SourceAsset) const;

#if WITH_EDITORONLY_DATA

	/** Importing data and options used for this mesh */
	UPROPERTY(EditAnywhere, Instanced, Category=ImportSettings)
	class UAssetImportData* AssetImportData;

	/** Path to the resource used to construct this skeletal mesh */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, AdvancedDisplay, Category = Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

	/** Should we use a custom camera transform when viewing this mesh in the tools */
	UPROPERTY()
	bool bHasCustomDefaultEditorCamera;
	/** Default camera location */
	UPROPERTY()
	FVector DefaultEditorCameraLocation;
	/** Default camera rotation */
	UPROPERTY()
	FRotator DefaultEditorCameraRotation;
	/** Default camera look at */
	UPROPERTY()
	FVector DefaultEditorCameraLookAt;
	/** Default camera ortho zoom */
	UPROPERTY()
	float DefaultEditorCameraOrthoZoom;


	/** Optimization settings used to simplify LODs of this mesh. */
	UPROPERTY()
	TArray<struct FSkeletalMeshOptimizationSettings> OptimizationSettings;

	/* Attached assets component for this mesh */
	UPROPERTY()
	FPreviewAssetAttachContainer PreviewAttachedAssetContainer;

	/**
	 * If true on post load we need to calculate resolution independent Display Factors from the
	 * loaded LOD screen sizes.
	 */
	uint32 bRequiresLODScreenSizeConversion : 1;

	/**
	 * If true on post load we need to calculate resolution independent LOD hysteresis from the
	 * loaded LOD hysteresis.
	 */
	uint32 bRequiresLODHysteresisConversion : 1;

#endif // WITH_EDITORONLY_DATA

	UPROPERTY(Category=Mesh, BlueprintReadWrite)
	TArray<UMorphTarget*> MorphTargets;

	/** A fence which is used to keep track of the rendering thread releasing the static mesh resources. */
	FRenderCommandFence ReleaseResourcesFence;

	/** New Reference skeleton type **/
	FReferenceSkeleton RefSkeleton;

	/** Map of morph target name to index into USkeletalMesh::MorphTargets**/
	TMap<FName, int32> MorphTargetIndexMap;

	/** Reference skeleton precomputed bases. */
	TArray<FMatrix> RefBasesInvMatrix;    

#if WITH_EDITORONLY_DATA
	/** The section currently selected in the Editor. Used for highlighting */
	UPROPERTY(transient)
	int32 SelectedEditorSection;

	/** The Material currently selected. need to remember this index for reimporting cloth */
	UPROPERTY(transient)
	int32 SelectedEditorMaterial;

	/** The section currently selected for clothing. need to remember this index for reimporting cloth */
	UPROPERTY(transient)
	int32 SelectedClothingSection;

	/** Height offset for the floor mesh in the editor */
	UPROPERTY()
	float FloorOffset;

	/** This is buffer that saves pose that is used by retargeting*/
	UPROPERTY()
	TArray<FTransform> RetargetBasePose;

#endif

	/** Legacy clothing asset data, will be converted to new assets after loading */
	UPROPERTY()
	TArray<FClothingAssetData_Legacy>		ClothingAssets_DEPRECATED;

	/** Animation Blueprint class to run as a post process for this mesh.
	 *  This blueprint will be ran before physics, but after the main
	 *  anim instance for any skeletal mesh component using this mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = SkeletalMesh)
	TSubclassOf<UAnimInstance> PostProcessAnimBlueprint;

	/** If the given section of the specified LOD has a clothing asset, unbind it's data and remove it from the asset array */
	void RemoveClothingAsset(int32 InLodIndex, int32 InSectionIndex);

	/**
	 * Given an LOD and section index, retrieve a clothing asset bound to that section.
	 * If no clothing asset is in use, returns nullptr
	 */
	UClothingAssetBase* GetSectionClothingAsset(int32 InLodIndex, int32 InSectionIndex);
	const UClothingAssetBase* GetSectionClothingAsset(int32 InLodIndex, int32 InSectionIndex) const;

	/** 
	 * Clothing assets imported to this mesh. May or may not be in use currently on the mesh.
	 * Ordering not guaranteed, use the provided getters to access elements in this array
	 * whenever possible
	 */
	UPROPERTY(EditAnywhere, editfixedsize, BlueprintReadOnly, Category = Clothing)
	TArray<UClothingAssetBase*> MeshClothingAssets;

	/** Get a clothing asset from its associated GUID (returns nullptr if no match is found) */
	UClothingAssetBase* GetClothingAsset(const FGuid& InAssetGuid) const;

	/* Get the index in the clothing asset array for a given asset (INDEX_NONE if InAsset isn't in the array) */
	int32 GetClothingAssetIndex(UClothingAssetBase* InAsset) const;

	/* Get the index in the clothing asset array for a given asset GUID (INDEX_NONE if there is no match) */
	int32 GetClothingAssetIndex(const FGuid& InAssetGuid) const;

	/* Get whether or not any bound clothing assets exist for this mesh **/
	bool HasActiveClothingAssets() const;

	/** Populates OutClothingAssets with all clothing assets that are mapped to sections in the mesh. */
	void GetClothingAssetsInUse(TArray<UClothingAssetBase*>& OutClothingAssets) const;

	/** Adds an asset to this mesh with validation and event broadcast */
	void AddClothingAsset(UClothingAssetBase* InNewAsset);

protected:

	/** Array of user data stored with the asset */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category=SkeletalMesh)
	TArray<UAssetUserData*> AssetUserData;

private:
	/** Skeletal mesh source data */
	class FSkeletalMeshSourceData* SourceData;

	/** 
	 *	Array of named socket locations, set up in editor and used as a shortcut instead of specifying 
	 *	everything explicitly to AttachComponent in the SkeletalMeshComponent. 
	 */
	UPROPERTY()
	TArray<class USkeletalMeshSocket*> Sockets;

	/** Cached matrices from GetComposedRefPoseMatrix */
	TArray<FMatrix> CachedComposedRefPoseMatrices;

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
	/** Cached index buffer ranges */
	UPROPERTY()
	TArray<FSkeletalMeshIndexBufferRanges> IndexBufferRanges;
//nv end

public:
	/**
	* Initialize the mesh's render resources.
	*/
	void InitResources();

	/**
	* Releases the mesh's render resources.
	*/
	void ReleaseResources();


	/** Release CPU access version of buffer */
	void ReleaseCPUResources();

	/** 
	 * Update the material UV channel data used by the texture streamer. 
	 *
	 * @param bResetOverrides		True if overridden values should be reset.
	 */
	void UpdateUVChannelData(bool bResetOverrides);

	/**
	 * Returns the UV channel data for a given material index. Used by the texture streamer.
	 * This data applies to all lod-section using the same material.
	 *
	 * @param MaterialIndex		the material index for which to get the data for.
	 * @return the data, or null if none exists.
	 */
	const FMeshUVChannelInfo* GetUVChannelData(int32 MaterialIndex) const;

	/**
	 * Gets the center point from which triangles should be sorted, if any.
	 */
	bool GetSortCenterPoint(FVector& OutSortCenter) const;

	/**
	 * Computes flags for building vertex buffers.
	 */
	uint32 GetVertexBufferFlags() const;

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostEditUndo() override;
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual FString GetDesc() override;
	virtual FString GetDetailedInfoInternal() const override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.

	/** Setup-only routines - not concerned with the instance. */

	void CalculateInvRefMatrices();

	/** Calculate the required bones for a Skeletal Mesh LOD, including possible extra influences */
	static void CalculateRequiredBones(class FStaticLODModel& LODModel, const struct FReferenceSkeleton& RefSkeleton, const TMap<FBoneIndexType, FBoneIndexType> * BonesToRemove);

	/** 
	 *	Find a socket object in this SkeletalMesh by name. 
	 *	Entering NAME_None will return NULL. If there are multiple sockets with the same name, will return the first one.
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	USkeletalMeshSocket* FindSocket(FName InSocketName) const;

	/**
	*	Find a socket object in this SkeletalMesh by name.
	*	Entering NAME_None will return NULL. If there are multiple sockets with the same name, will return the first one.
	*   Also returns the index for the socket allowing for future fast access via GetSocketByIndex()
	*/
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshSocket* FindSocketAndIndex(FName InSocketName, int32& OutIndex) const;

	/** Returns the number of sockets available. Both on this mesh and it's skeleton. */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	int32 NumSockets() const;

	/** Returns a socket by index. Max index is NumSockets(). The meshes sockets are accessed first, then the skeletons.  */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshSocket* GetSocketByIndex(int32 Index) const;

	// @todo document
	FMatrix GetRefPoseMatrix( int32 BoneIndex ) const;

	/** 
	 *	Get the component orientation of a bone or socket. Transforms by parent bones.
	 */
	FMatrix GetComposedRefPoseMatrix( FName InBoneName ) const;
	FMatrix GetComposedRefPoseMatrix( int32 InBoneIndex ) const;

	/** Allocate and initialise bone mirroring table for this skeletal mesh. Default is source = destination for each bone. */
	void InitBoneMirrorInfo();

	/** Utility for copying and converting a mirroring table from another USkeletalMesh. */
	void CopyMirrorTableFrom(USkeletalMesh* SrcMesh);
	void ExportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo);
	void ImportMirrorTable(TArray<FBoneMirrorExport> &MirrorExportInfo);

	/** 
	 *	Utility for checking that the bone mirroring table of this mesh is good.
	 *	Return true if mirror table is OK, false if there are problems.
	 *	@param	ProblemBones	Output string containing information on bones that are currently bad.
	 */
	bool MirrorTableIsGood(FString& ProblemBones);

	/**
	 * Returns the mesh only socket list - this ignores any sockets in the skeleton
	 * Return value is a non-const reference so the socket list can be changed
	 */
	TArray<USkeletalMeshSocket*>& GetMeshOnlySocketList();

	/**
	 * Const version
	 * Returns the mesh only socket list - this ignores any sockets in the skeleton
	 * Return value is a non-const reference so the socket list can be changed
	 */
	const TArray<USkeletalMeshSocket*>& GetMeshOnlySocketList() const;

	/**
	* Returns the "active" socket list - all sockets from this mesh plus all non-duplicates from the skeleton
	* Const ref return value as this cannot be modified externally
	*/
	TArray<USkeletalMeshSocket*> GetActiveSocketList() const;

#if WITH_EDITOR
	/** Retrieves the source model for this skeletal mesh. */
	FStaticLODModel& GetSourceModel();

	/**
	 * Copies off the source model for this skeletal mesh if necessary and returns it. This function should always be called before
	 * making destructive changes to the mesh's geometry, e.g. simplification.
	 */
	FStaticLODModel& PreModifyMesh();

	/**
	* Makes sure all attached objects are valid and removes any that aren't.
	*
	* @return		NumberOfBrokenAssets
	*/
	int32 ValidatePreviewAttachedObjects();

	/**
	 * Removes a specified section from the skeletal mesh, this is a destructive action
	 *
	 * @param InLodIndex Lod index to remove section from
	 * @param InSectionIndex Section index to remove
	 */
	void RemoveMeshSection(int32 InLodIndex, int32 InSectionIndex);

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
	/**
	 * Ensure that IndexBufferRanges is updated
	 */
	void RebuildIndexBufferRanges();
//nv end

#endif // #if WITH_EDITOR

//#nv begin #Blast Ability to hide bones using a dynamic index buffer
	/**
	 * Const accessor to IndexBufferRanges
	 *
	 * @return const reference to IndexBufferRanges
	 */
	const TArray<FSkeletalMeshIndexBufferRanges>& GetIndexBufferRanges() const
	{
		return IndexBufferRanges;
	}
//nv end

	/**
	* Verify SkeletalMeshLOD is set up correctly	
	*/
	void DebugVerifySkeletalMeshLOD();

	/**
	 * Find a named MorphTarget from the MorphSets array in the SkinnedMeshComponent.
	 * This searches the array in the same way as FindAnimSequence
	 *
	 * @param MorphTargetName Name of MorphTarget to look for.
	 *
	 * @return Pointer to found MorphTarget. Returns NULL if could not find target with that name.
	 */
	UMorphTarget* FindMorphTarget(FName MorphTargetName) const;
	UMorphTarget* FindMorphTargetAndIndex(FName MorphTargetName, int32& OutIndex) const;

	/** if name conflicts, it will overwrite the reference */
	void RegisterMorphTarget(UMorphTarget* MorphTarget);

	void UnregisterMorphTarget(UMorphTarget* MorphTarget);

	/** Initialize MorphSets look up table : MorphTargetIndexMap */
	void InitMorphTargets();

	/** 
	 * Checks whether the provided section is using APEX cloth. if bCheckCorrespondingSections is true
	 * disabled sections will defer to correspond sections to see if they use cloth (non-cloth sections
	 * are disabled and another section added when cloth is enabled, using this flag allows for a check
	 * on the original section to succeed)
	 * @param InSectionIndex Index to check
	 * @param bCheckCorrespondingSections Whether to check corresponding sections for disabled sections
	 */
	UFUNCTION(BlueprintCallable, Category="Cloth")
	bool IsSectionUsingCloth(int32 InSectionIndex, bool bCheckCorrespondingSections = true) const;

	void CreateBodySetup();
	UBodySetup* GetBodySetup();

#if WITH_EDITOR
	/** Trigger a physics build to ensure per poly collision is created */
	void BuildPhysicsData();
	void AddBoneToReductionSetting(int32 LODIndex, const TArray<FName>& BoneNames);
	void AddBoneToReductionSetting(int32 LODIndex, FName BoneName);
#endif
	
#if WITH_EDITORONLY_DATA
	/** Convert legacy screen size (based on fixed resolution) into screen size (diameter in screen units) */
	void ConvertLegacyLODScreenSize();
#endif
	

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override
	{
		return true;
	}
	//~ End Interface_CollisionDataProvider Interface

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData(UAssetUserData* InUserData) override;
	virtual void RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual UAssetUserData* GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

private:

	/** Utility function to help with building the combined socket list */
	bool IsSocketOnMesh( const FName& InSocketName ) const;

	/**
	* Flush current render state
	*/
	void FlushRenderState();
	/**
	* Restart render state. 
	*/
	void RestartRenderState();

	/**
	* In older data, the bEnableShadowCasting flag was stored in LODInfo
	* so it needs moving over to materials
	*/
	void MoveDeprecatedShadowFlagToMaterials();

	/**
	* Test whether all the flags in an array are identical (could be moved to Array.h?)
	*/
	bool AreAllFlagsIdentical( const TArray<bool>& BoolArray ) const;

	/*
	* Ask the reference skeleton to rebuild the NameToIndexMap array. This is use to load old package before this array was created.
	*/
	void RebuildRefSkeletonNameToIndexMap();

	/*
	* In version prior to FEditorObjectVersion::RefactorMeshEditorMaterials
	* The material slot is containing the "Cast Shadow" and the "Recompute Tangent" flag
	* We move those flag to sections to allow artist to control those flag at section level
	* since its a section flag.
	*/
	void MoveMaterialFlagsToSections();

#if WITH_EDITOR
	public:
		/** Delegates for asset editor events */

		FDelegateHandle RegisterOnClothingChange(const FSimpleMulticastDelegate::FDelegate& InDelegate);
		void UnregisterOnClothingChange(const FDelegateHandle& InHandle);

	private:

		/** Called to notify a change to the clothing object array */
		FSimpleMulticastDelegate OnClothingChange;

#endif // WITH_EDITOR
};


/**
 * Refresh Physics Asset Change
 * 
 * Physics Asset has been changed, so it will need to recreate physics state to reflect it
 * Utilities functions to propagate new Physics Asset for InSkeletalMesh
 *
 * @param	InSkeletalMesh	SkeletalMesh that physics asset has been changed for
 */
ENGINE_API void RefreshSkelMeshOnPhysicsAssetChange(const USkeletalMesh* InSkeletalMesh);
