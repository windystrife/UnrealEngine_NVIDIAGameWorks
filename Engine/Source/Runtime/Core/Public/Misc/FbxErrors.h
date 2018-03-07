// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "Templates/SharedPointer.h"
#include "Logging/TokenizedMessage.h"


/**
 * This file contains known map errors that can be referenced by name.
 * Documentation for these errors is assumed to lie in UDN at: Engine\Documentation\Source\Shared\Editor\MapErrors
 */
struct CORE_API FFbxErrors
{
	/** Generic */
	static FName Generic_ImportingNewObjectFailed;

	static FName Generic_ReimportingObjectFailed;

	static FName Generic_LoadingSceneFailed;

	static FName Generic_InvalidCharacterInName;

	static FName Generic_SameNameAssetExists;

	static FName Generic_SameNameAssetOverriding;

	static FName Generic_CannotDeleteReferenced;

	static FName Generic_FBXFileParseFailed;

	static FName Generic_MeshNotFound;

	static FName Generic_CannotDetectImportType;

	/** Mesh Generic **/

	static FName Generic_Mesh_NoGeometry;

	static FName Generic_Mesh_SmallGeometry;

	static FName Generic_Mesh_TriangulationFailed;

	static FName Generic_Mesh_ConvertSmoothingGroupFailed;

	static FName Generic_Mesh_UnsupportingSmoothingGroup;

	static FName Generic_Mesh_MaterialIndexInconsistency;

	static FName Generic_Mesh_MeshNotFound;

	static FName Generic_Mesh_NoSmoothingGroup;

	static FName Generic_Mesh_LOD_InvalidIndex;

	static FName Generic_Mesh_LOD_NoFileSelected;

	static FName Generic_Mesh_LOD_MultipleFilesSelected;

	static FName Generic_Mesh_SkinxxNameError;

	static FName Generic_Mesh_TooManyLODs;

	/** Static Mesh **/
	static FName StaticMesh_TooManyMaterials;

	static FName StaticMesh_UVSetLayoutProblem;

	static FName StaticMesh_NoTriangles;

	static FName StaticMesh_BuildError;

	static FName StaticMesh_AllTrianglesDegenerate;

	/** SkeletalMesh **/
	static FName SkeletalMesh_DifferentRoots;

	static FName SkeletalMesh_DuplicateBones;

	static FName SkeletalMesh_NoInfluences;

	static FName SkeletalMesh_TooManyInfluences;

	static FName SkeletalMesh_RestoreSortingMismatchedStrips;

	static FName SkeletalMesh_RestoreSortingNoSectionMatch;

	static FName SkeletalMesh_RestoreSortingForSectionNumber;

	static FName SkeletalMesh_NoMeshFoundOnRoot;
	
	static FName SkeletalMesh_InvalidRoot;

	static FName SkeletalMesh_InvalidBone;

	static FName SkeletalMesh_InvalidNode;

	static FName SkeletalMesh_NoWeightsOnDeformer;

	static FName SkeletalMesh_NoBindPoseInScene;

	static FName SkeletalMesh_NoAssociatedCluster;

	static FName SkeletalMesh_NoBoneFound;

	static FName SkeletalMesh_InvalidBindPose;

	static FName SkeletalMesh_MultipleRoots;

	static FName SkeletalMesh_BonesAreMissingFromBindPose;

	static FName SkeletalMesh_VertMissingInfluences;

	static FName SkeletalMesh_SectionWithNoTriangle;

	static FName SkeletalMesh_TooManyVertices;

	static FName SkeletalMesh_FailedToCreatePhyscisAsset;

	static FName SkeletalMesh_SkeletonRecreateError;

	static FName SkeletalMesh_ExceedsMaxBoneCount;

	static FName SkeletalMesh_NoUVSet;

	static FName SkeletalMesh_LOD_MissingBone;

	static FName SkeletalMesh_LOD_FailedToImport;

	static FName SkeletalMesh_LOD_RootNameIncorrect;

	static FName SkeletalMesh_LOD_BonesDoNotMatch;

	static FName SkeletalMesh_LOD_IncorrectParent;

	static FName SkeletalMesh_LOD_HasSoftVerts;

	static FName SkeletalMesh_LOD_MissingSocketBone;

	static FName SkeletalMesh_LOD_MissingMorphTarget;

	static FName SkeletalMesh_FillImportDataFailed;

	static FName SkeletalMesh_InvalidPosition;

	/** Animation **/
	static FName Animation_CouldNotFindRootTrack;

	static FName Animation_CouldNotBuildSkeleton;

	static FName Animation_CouldNotFindTrack;

	static FName Animation_ZeroLength;

	static FName Animation_RootTrackMismatch;

	static FName Animation_DuplicatedBone;

	static FName Animation_MissingBones;

	static FName Animation_InvalidData;

	static FName Animation_TransformError;

	static FName Animation_DifferentLength;

	static FName Animation_CurveNotFound;
};

/**
 * Map error specific message token.
 */
class FFbxErrorToken : public FDocumentationToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	CORE_API static TSharedRef<FFbxErrorToken> Create( const FName& InErrorName )
	{
		return MakeShareable(new FFbxErrorToken(InErrorName));
	}

private:
	/** Private constructor */
	CORE_API FFbxErrorToken( const FName& InErrorName );
};
