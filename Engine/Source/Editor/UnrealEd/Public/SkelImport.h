// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*-----------------------------------------------------------------------------
	Data structures only used for importing skeletal meshes and animations.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "ReferenceSkeleton.h"
#include "SkeletalMeshTypes.h"
#include "Engine/SkeletalMesh.h"

class UAssetImportData;
class UMorphTarget;
class UPhysicsAsset;
class USkeletalMeshSocket;
class USkeleton;
class UThumbnailInfo;

// Raw data material.
struct VMaterial
{
	/** The actual material created on import or found among existing materials */
	TWeakObjectPtr<UMaterialInterface> Material;
	/** The material name found by the importer */
	FString MaterialImportName;
};


// Raw data bone.
struct VBone
{
	FString		Name;     //
	//@ todo FBX - Flags unused?
	uint32		Flags;        // reserved / 0x02 = bone where skin is to be attached...	
	int32 		NumChildren;  // children  // only needed in animation ?
	int32       ParentIndex;  // 0/NULL if this is the root bone.  
	VJointPos	BonePos;      // reference position
};


// Raw data bone influence.
struct VRawBoneInfluence // just weight, vertex, and Bone, sorted later....
{
	float Weight;
	int32   VertexIndex;
	int32   BoneIndex;
};

// Vertex with texturing info, akin to Hoppe's 'Wedge' concept - import only.
struct VVertex
{
	uint32	VertexIndex; // Index to a vertex.
	FVector2D UVs[MAX_TEXCOORDS];        // Scaled to BYTES, rather...-> Done in digestion phase, on-disk size doesn't matter here.
	FColor	Color;		 // Vertex colors
	uint8    MatIndex;    // At runtime, this one will be implied by the face that's pointing to us.
	uint8    Reserved;    // Top secret.

	VVertex()
	{
		FMemory::Memzero(this,sizeof(VVertex));
	}

	bool operator==( const VVertex& Other ) const
	{
		bool Equal = true;

		Equal &= (VertexIndex == Other.VertexIndex);
		Equal &= (MatIndex == Other.MatIndex);
		Equal &= (Color == Other.Color);
		Equal &= (Reserved == Other.Reserved);

		bool bUVsEqual = true;
		for( uint32 UVIdx = 0; UVIdx < MAX_TEXCOORDS; ++UVIdx )
		{
			if( UVs[UVIdx] != Other.UVs[UVIdx] )
			{
				bUVsEqual = false;
				break;
			}
		}

		Equal &= bUVsEqual;

		return Equal;
	}

	friend uint32 GetTypeHash( const VVertex& Vertex )
	{
		return FCrc::MemCrc_DEPRECATED( &Vertex, sizeof(VVertex) );
	}
};


// Points: regular FVectors (for now..)
struct VPoint
{	
	FVector	Point; // Change into packed integer later IF necessary, for 3x size reduction...
};

//
// FSavedCustomSortSectionInfo - saves and restores custom triangle order for a single section of the skeletal mesh.
//
struct FSavedCustomSortSectionInfo
{
	FSavedCustomSortSectionInfo(USkeletalMesh* ExistingSkelMesh, int32 LODModelIndex, int32 InSectionIdx);

	void Restore(USkeletalMesh* NewSkelMesh, int32 LODModelIndex, TArray<int32>& UnmatchedSections);

	int32 SavedSectionIdx;
	int32 SavedNumTriangles;
	ETriangleSortOption SavedSortOption;
	ETriangleSortAxis SavedCustomLeftRightAxis;
	FName SavedCustomLeftRightBoneName;
	TArray<FVector> SavedVertices;
	TArray<uint32> SavedIndices;
};


//
// Class to save and restore the custom sorting for all sections not marked TRISORT_None
//
struct FSavedCustomSortInfo
{
public:
	void Save(USkeletalMesh* ExistingSkelMesh, int32 LODModelIndex);
	void Restore(USkeletalMesh* NewSkeletalMesh, int32 LODModelIndex);
private:
	TArray<FSavedCustomSortSectionInfo> SortSectionInfos;
};

struct ExistingMeshLodSectionData
{
	ExistingMeshLodSectionData(FName InImportedMaterialSlotName, bool InbCastShadow, bool InbRecomputeTangents)
	: ImportedMaterialSlotName(InImportedMaterialSlotName)
	, bCastShadow(InbCastShadow)
	, bRecomputeTangents(InbRecomputeTangents)
	{}
	FName ImportedMaterialSlotName;
	bool bCastShadow;
	bool bRecomputeTangents;
};

struct ExistingSkelMeshData
{
	TArray<USkeletalMeshSocket*>			ExistingSockets;
	TIndirectArray<FStaticLODModel>			ExistingLODModels;
	TArray<FSkeletalMeshLODInfo>			ExistingLODInfo;
	TArray<FMultiSizeIndexContainerData>	ExistingIndexBufferData;
	TArray<FMultiSizeIndexContainerData>	ExistingAdjacencyIndexBufferData;
	FReferenceSkeleton						ExistingRefSkeleton;
	TArray<FSkeletalMaterial>				ExistingMaterials;
	bool									bSaveRestoreMaterials;
	TArray<UMorphTarget*>					ExistingMorphTargets;
	TArray<UPhysicsAsset*>					ExistingPhysicsAssets;
	UPhysicsAsset*							ExistingShadowPhysicsAsset;
	USkeleton*								ExistingSkeleton;
	TArray<FTransform>						ExistingRetargetBasePose;

	bool									bExistingUseFullPrecisionUVs;

	TArray<FBoneMirrorExport>				ExistingMirrorTable;
	FSavedCustomSortInfo					ExistingSortInfo;

	TWeakObjectPtr<UAssetImportData>		ExistingAssetImportData;
	TWeakObjectPtr<UThumbnailInfo>			ExistingThumbnailInfo;

	TArray<UClothingAssetBase*>				ExistingClothingAssets;

	bool UseMaterialNameSlotWorkflow;
	//The existing import material data (the state of sections before the reimport)
	TArray<FName> ExistingImportMaterialOriginalNameData;
	TArray<TArray<ExistingMeshLodSectionData>> ExistingImportMeshLodSectionMaterialData;
	//The last import material data (fbx original data before user changes)
	TArray<FName> LastImportMaterialOriginalNameData;
	TArray<TArray<FName>> LastImportMeshLodSectionMaterialData;
};

/**
 * Container and importer for skeletal mesh (FBX file) data
 **/
class UNREALED_API FSkeletalMeshImportData
{
public:
	TArray <VMaterial>			Materials;		// Materials
	TArray <FVector>			Points;			// 3D Points
	TArray <VVertex>			Wedges;			// Wedges
	TArray <VTriangle>			Faces;			// Faces
	TArray <VBone>				RefBonesBinary;	// Reference Skeleton
	TArray <VRawBoneInfluence>	Influences;		// Influences
	TArray <int32>				PointToRawMap;	// Mapping from current point index to the original import point index
	uint32	NumTexCoords;						// The number of texture coordinate sets
	uint32	MaxMaterialIndex;					// The max material index found on a triangle
	bool 	bHasVertexColors; 					// If true there are vertex colors in the imported file
	bool	bHasNormals;						// If true there are normals in the imported file
	bool	bHasTangents;						// If true there are tangents in the imported file
	bool	bUseT0AsRefPose;					// If true, then the pose at time=0 will be used instead of the ref pose
	bool	bDiffPose;							// If true, one of the bones has a different pose at time=0 vs the ref pose

	FSkeletalMeshImportData()
		: NumTexCoords(0)
		, MaxMaterialIndex(0)
		, bHasVertexColors(false)
		, bHasNormals(false)
		, bHasTangents(false)
		, bUseT0AsRefPose(false)
		, bDiffPose(false)
	{

	}


	/**
 	 * Copy mesh data for importing a single LOD
	 *
	 * @param LODPoints - vertex data.
	 * @param LODWedges - wedge information to static LOD level.
	 * @param LODFaces - triangle/ face data to static LOD level.
	 * @param LODInfluences - weights/ influences to static LOD level.
	 */ 
	void CopyLODImportData( 
		TArray<FVector>& LODPoints, 
		TArray<FMeshWedge>& LODWedges,
		TArray<FMeshFace>& LODFaces,	
		TArray<FVertInfluence>& LODInfluences,
		TArray<int32>& LODPointToRawMap) const;

	static FString FixupBoneName( const FString& InBoneName );

	/**
 	 * Removes all import data
	 */
	void Empty()
	{
		Materials.Empty();
		Points.Empty();
		Wedges.Empty();
		Faces.Empty();
		RefBonesBinary.Empty();
		Influences.Empty();
		PointToRawMap.Empty();
	}
};

/** 
 * Optional data passed in when importing a skeletal mesh LDO
 */
class FSkelMeshOptionalImportData
{
public:
	FSkelMeshOptionalImportData() {}

	/** extra data used for importing extra weight/bone influences */
	FSkeletalMeshImportData RawMeshInfluencesData;
	int32 MaxBoneCountPerChunk;
};
