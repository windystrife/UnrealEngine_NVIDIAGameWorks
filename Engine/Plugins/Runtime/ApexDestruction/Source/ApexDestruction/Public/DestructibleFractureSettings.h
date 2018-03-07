// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "EngineDefines.h"
#include "DestructibleFractureSettings.generated.h"

class UMaterialInterface;

#if WITH_APEX

// Forward declares
namespace nvidia
{
	namespace apex
	{
		struct FractureMaterialDesc;
		class DestructibleChunkDesc;
		class DestructibleGeometryDesc;
		class DestructibleAsset;
		class DestructibleAssetAuthoring;
		struct ExplicitRenderTriangle;
		struct ExplicitSubmeshData;
		class DestructibleAssetCookingDesc;
	};
};
#endif // WITH_APEX


/** 
 * Options for APEX asset import.
 **/
namespace EDestructibleImportOptions
{
	enum Type
	{
		// Just imports the APEX asset
		None				= 0,
		// Preserves settings in DestructibleMesh 
		PreserveSettings	= 1<<0,
	};
};


/** Parameters to describe the application of U,V coordinates on a particular slice within a destructible. */
USTRUCT()
struct FFractureMaterial
{
	GENERATED_USTRUCT_BODY()

	/**
	 * The UV scale (geometric distance/unit texture distance) for interior materials.
	 * Default = (100.0f,100.0f).
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	FVector2D	UVScale;

	/**
	 * A UV origin offset for interior materials.
	 * Default = (0.0f,0.0f).
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	FVector2D	UVOffset;

	/**
	 * Object-space vector specifying surface tangent direction.  If this vector is (0.0f,0.0f,0.0f), then an arbitrary direction will be chosen.
	 * Default = (0.0f,0.0f,0.0f).
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	FVector		Tangent;

	/**
	 * Angle from tangent direction for the u coordinate axis.
	 * Default = 0.0f.
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	float		UAngle;

	/**
	 * The element index to use for the newly-created triangles.
	 * If a negative index is given, a new element will be created for interior triangles.
	 * Default = -1
	 */
	UPROPERTY(EditAnywhere, Category=FractureMaterial)
	int32		InteriorElementIndex;

	FFractureMaterial()
		: UVScale(100.0f, 100.0f)
		, UVOffset(0.0f, 0.0f)
		, Tangent(0.0f, 0.0f, 0.0f)
		, UAngle(0.0f)
		, InteriorElementIndex(-1)
	{ }

	//~ Begin FFractureMaterial Interface
#if WITH_APEX
	void	FillNxFractureMaterialDesc(nvidia::apex::FractureMaterialDesc& PFractureMaterialDesc);
#endif // WITH_APEX
	//~ End FFractureMaterial Interface
};


/** Per-chunk authoring data. */
USTRUCT()
struct FDestructibleChunkParameters
{
	GENERATED_USTRUCT_BODY()

	/**
		Defines the chunk to be environmentally supported, if the appropriate NxDestructibleParametersFlag flags
		are set in NxDestructibleParameters.
		Default = false.
	*/
	UPROPERTY(EditAnywhere, Category=DestructibleChunkParameters)
	bool bIsSupportChunk;

	/**
		Defines the chunk to be unfractureable.  If this is true, then none of its children will be fractureable.
		Default = false.
	*/
	UPROPERTY(EditAnywhere, Category=DestructibleChunkParameters)
	bool bDoNotFracture;

	/**
		Defines the chunk to be undamageable.  This means this chunk will not fracture, but its children might.
		Default = false.
	*/
	UPROPERTY(EditAnywhere, Category=DestructibleChunkParameters)
	bool bDoNotDamage;

	/**
		Defines the chunk to be uncrumbleable.  This means this chunk will not be broken down into fluid mesh particles
		no matter how much damage it takes.  Note: this only applies to chunks with no children.  For a chunk with
		children, then:
		1) The chunk may be broken down into its children, and then its children may be crumbled, if the doNotCrumble flag
		is not set on them.
		2) If the Destructible module's chunk depth offset LOD may be set such that this chunk effectively has no children.
		In this case, the doNotCrumble flag will apply to it.
		Default = false.
	*/
	UPROPERTY(EditAnywhere, Category=DestructibleChunkParameters)
	bool bDoNotCrumble;

	FDestructibleChunkParameters()
		: bIsSupportChunk(false)
		, bDoNotFracture(false)
		, bDoNotDamage(false)
		, bDoNotCrumble(false)
	{ }
};


/** Information to create an NxDestructibleAsset */
UCLASS(MinimalAPI)
class UDestructibleFractureSettings
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UDestructibleFractureSettings(FVTableHelper& Helper);

	/** The number of voronoi cell sites. */
	UPROPERTY(EditAnywhere, Category = Voronoi, meta = (ClampMin = "1", UIMin = "1"))
	int32									CellSiteCount;

	/** Stored interior material data.  Just need one as we only support Voronoi splitting. */
	UPROPERTY(EditAnywhere, transient, Category=General)
	FFractureMaterial						FractureMaterialDesc;

	/** Random seed for reproducibility */
	UPROPERTY(EditAnywhere, Category=General)
	int32									RandomSeed;

	/** Stored Voronoi sites */
	UPROPERTY()
	TArray<FVector>							VoronoiSites;

	/** The mesh's original number of submeshes.  APEX needs to store this in the authoring. */
	UPROPERTY()
	int32									OriginalSubmeshCount;

	/** APEX references materials by name, but we'll bypass that mechanism and use of UE materials instead. */
	UPROPERTY()
	TArray<UMaterialInterface*>				Materials;

	/** Per-chunk authoring parameters, which should be made writable when a chunk selection GUI is in place. */
	UPROPERTY()
	TArray<FDestructibleChunkParameters>	ChunkParameters;
	
#if WITH_APEX
	/** ApexDestructibleAsset is a pointer to the Apex asset interface for this destructible asset */
	nvidia::apex::DestructibleAssetAuthoring*	ApexDestructibleAssetAuthoring;

	/** Per-chunk information used to build a destructible asset.  */
	TArray<nvidia::apex::DestructibleChunkDesc>	ChunkDescs;

	/** Per-part (part = geometry = graphics + collision) information used to build a destructible asset.  This is not necessarily 1-1 with chunks, as chunks may instance parts. */
	TArray<nvidia::apex::DestructibleGeometryDesc>	GeometryDescs;
#endif // WITH_APEX

	//~ Begin UObject Interface.
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual void Serialize( FArchive& Ar ) override;
	//~ End  UObject Interface

	//~ Begin UDestructibleFractureSettings Interface
#if WITH_APEX && WITH_EDITOR
	/**
	 * Creates an NxDestructibleAssetCookingDesc suitable for the authoring mesh, which can be passed to CreateApexDestructibleAsset().
	 * The user has the chance to modify the chunk properties before doing so.
	 *
	 * @param NxDestructibleAssetCookingDesc - The NxDestructibleAssetCookingDesc to be filled in
	 */
	APEXDESTRUCTION_API	void					BuildDestructibleAssetCookingDesc(nvidia::apex::DestructibleAssetCookingDesc& DestructibleAssetCookingDesc);

	/**
	 * Sets authoring mesh for fracturing
	 *
	 * @param MeshTriangles - Array of authoring triangles (NxExplicitRenderTriangle) used by APEX fracturing tools
	 * @param Materials - Array of UE materials, parallel to the SubmeshData array.  We are bypassing the material name of the NxExplicitSubmeshData
	 * @param SubmeshData - Array of authoring submesh (analog of Elements in UE) descriptors used by APEX fracturing tools
	 * @param MeshPartition - The MeshTriangles array is partitioned in to parts.  If MeshPartition is zero-length, there is assumed to be only one partition.
	 *	Otherwise, each element of the array gives the end index in MeshTriangles of each partition.  If the number of parts is greater than one, these become
	 *	the depth-1 chunks, while the union (all of MeshTriangles) becomes the depth-0 chunk.  If there is only one partition, only the depth-0 chunk is described by MeshPartition.
	 * @param bFirstPartitionIsDepthZero - If this is true, the first Partition is treated as level0 root chunk, while all others build the level1 destruction chunks. If false, the level0 chunk
	 *  is generated from the union of all partitions.
	 *
	 * @returns true if successful
	 */
	APEXDESTRUCTION_API	bool					SetRootMesh(const TArray<nvidia::apex::ExplicitRenderTriangle>& MeshTriangles, const TArray<UMaterialInterface*>& Materials, const TArray<nvidia::apex::ExplicitSubmeshData>& SubmeshData, const TArray<uint32>& MeshPartition = TArray<uint32>(),
													bool bFirstPartitionIsDepthZero = false);
	/**
	 * Creates a root mesh from an NxDestructibleAsset.  This is useful when importing an NxDestructibleAsset.
	 *
	 * @param ApexDestructibleAsset - The NxDestructibleAsset from which to build the root mesh
	 * @param bTransformVertices - If true y = -y and v = -v transformation is applied to the vertices
	 *
	 * @returns true if successful
	 */
	APEXDESTRUCTION_API	bool					BuildRootMeshFromApexDestructibleAsset(nvidia::apex::DestructibleAsset& ApexDestructibleAsset, EDestructibleImportOptions::Type Options = EDestructibleImportOptions::None);

	/**
	 * Build set of Voronoi sites internal to the root mesh
	 */
	APEXDESTRUCTION_API	void					CreateVoronoiSitesInRootMesh();

	/**
	 * Applies voronoi slicing to the mesh.  Use the FractureMaterialDesc and CreateVoronoiSitesInRootMesh to prepare for this operation.
	 *
	 * @returns true if successful
	 */
	APEXDESTRUCTION_API	bool					VoronoiSplitMesh();

	/**
	 * Creates an NxDestructibleAsset from the internal authoring object.  This will be an APEX version of the mesh set in SetRootMesh, possibly fractured using the fracturing API in this class.
	 *
	 * @param NxDestructibleAssetCookingDesc - The NxDestructibleAssetCookingDesc to used to create the NxDestructibleAsset
	 */
	APEXDESTRUCTION_API	nvidia::apex::DestructibleAsset*	CreateApexDestructibleAsset(const nvidia::apex::DestructibleAssetCookingDesc& DestructibleAssetCookingDesc);
#endif // WITH_APEX
	//~ End UDestructibleFractureSettings Interface
};
