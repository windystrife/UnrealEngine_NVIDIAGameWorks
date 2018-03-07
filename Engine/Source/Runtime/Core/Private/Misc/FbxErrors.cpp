// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Misc/FbxErrors.h"
	
/** Generic */
FName FFbxErrors::Generic_ImportingNewObjectFailed(TEXT("Generic_ImportingNewObjectFailed"));

FName FFbxErrors::Generic_ReimportingObjectFailed(TEXT("Generic_ReimportingObjectFailed"));

FName FFbxErrors::Generic_LoadingSceneFailed(TEXT("Generic_LoadingSceneFailed"));

FName FFbxErrors::Generic_InvalidCharacterInName(TEXT("Generic_InvalidCharacterInName"));

FName FFbxErrors::Generic_SameNameAssetExists(TEXT("Generic_SameNameAssetExists"));

FName FFbxErrors::Generic_SameNameAssetOverriding(TEXT("Generic_SameNameAssetOverriding"));

FName FFbxErrors::Generic_CannotDeleteReferenced(TEXT("Generic_CannotDeleteReferenced"));

FName FFbxErrors::Generic_FBXFileParseFailed(TEXT("Generic_FBXFileParseFailed"));

FName FFbxErrors::Generic_MeshNotFound(TEXT("Generic_MeshNotFound"));

FName FFbxErrors::Generic_CannotDetectImportType(TEXT("Generic_CannotDetectImportType"));

/** Mesh Generic **/

FName FFbxErrors::Generic_Mesh_NoGeometry(TEXT("Generic_Mesh_NoGeometry"));

FName FFbxErrors::Generic_Mesh_SmallGeometry(TEXT("Generic_Mesh_SmallGeometry"));

FName FFbxErrors::Generic_Mesh_TriangulationFailed(TEXT("Generic_Mesh_TriangulationFailed"));

FName FFbxErrors::Generic_Mesh_ConvertSmoothingGroupFailed(TEXT("Generic_Mesh_ConvertSmoothingGroupFailed"));

FName FFbxErrors::Generic_Mesh_UnsupportingSmoothingGroup(TEXT("Generic_Mesh_UnsupportingSmoothingGroup"));

FName FFbxErrors::Generic_Mesh_MaterialIndexInconsistency(TEXT("Generic_Mesh_MaterialIndexInconsistency"));

FName FFbxErrors::Generic_Mesh_MeshNotFound(TEXT("Generic_Mesh_MeshNotFound"));

FName FFbxErrors::Generic_Mesh_NoSmoothingGroup(TEXT("Generic_Mesh_NoSmoothingGroup"));

FName FFbxErrors::Generic_Mesh_LOD_InvalidIndex(TEXT("Generic_Mesh_LOD_InvalidIndex"));

FName FFbxErrors::Generic_Mesh_LOD_NoFileSelected(TEXT("Generic_Mesh_LOD_NoFileSelected"));

FName FFbxErrors::Generic_Mesh_LOD_MultipleFilesSelected(TEXT("Generic_Mesh_LOD_MultipleFilesSelected"));

FName FFbxErrors::Generic_Mesh_SkinxxNameError(TEXT("Generic_Mesh_SkinxxNameError"));

FName FFbxErrors::Generic_Mesh_TooManyLODs(TEXT("Generic_Mesh_TooManyLODs"));

/** Static Mesh **/
FName FFbxErrors::StaticMesh_TooManyMaterials(TEXT("StaticMesh_TooManyMaterials"));

FName FFbxErrors::StaticMesh_UVSetLayoutProblem(TEXT("StaticMesh_UVSetLayoutProblem"));

FName FFbxErrors::StaticMesh_NoTriangles(TEXT("StaticMesh_NoTriangles"));

FName FFbxErrors::StaticMesh_BuildError(TEXT("StaticMesh_BuildError"));

FName FFbxErrors::StaticMesh_AllTrianglesDegenerate(TEXT("StaticMesh_AllTrianglesDegenerate"));

/** SkeletalMesh **/
FName FFbxErrors::SkeletalMesh_DifferentRoots(TEXT("SkeletalMesh_DifferentRoot"));

FName FFbxErrors::SkeletalMesh_DuplicateBones(TEXT("SkeletalMesh_DuplicateBones"));

FName FFbxErrors::SkeletalMesh_NoInfluences(TEXT("SkeletalMesh_NoInfluences"));

FName FFbxErrors::SkeletalMesh_TooManyInfluences(TEXT("SkeletalMesh_TooManyInfluences"));

FName FFbxErrors::SkeletalMesh_RestoreSortingMismatchedStrips(TEXT("SkeletalMesh_RestoreSortingMismatchedStrips"));

FName FFbxErrors::SkeletalMesh_RestoreSortingNoSectionMatch(TEXT("SkeletalMesh_RestoreSortingNoSectionMatch"));

FName FFbxErrors::SkeletalMesh_RestoreSortingForSectionNumber(TEXT("SkeletalMesh_RestoreSortingForSectionNumber"));

FName FFbxErrors::SkeletalMesh_NoMeshFoundOnRoot(TEXT("SkeletalMesh_NoMeshFoundOnRoot"));

FName FFbxErrors::SkeletalMesh_InvalidRoot(TEXT("SkeletalMesh_InvalidRoot"));

FName FFbxErrors::SkeletalMesh_InvalidBone(TEXT("SkeletalMesh_InvalidBone"));

FName FFbxErrors::SkeletalMesh_InvalidNode(TEXT("SkeletalMesh_InvalidNode"));

FName FFbxErrors::SkeletalMesh_NoWeightsOnDeformer(TEXT("SkeletalMesh_NoWeightsOnDeformer"));

FName FFbxErrors::SkeletalMesh_NoBindPoseInScene(TEXT("SkeletalMesh_NoBindPoseInScene"));

FName FFbxErrors::SkeletalMesh_NoAssociatedCluster(TEXT("SkeletalMesh_NoAssociatedCluster"));

FName FFbxErrors::SkeletalMesh_NoBoneFound(TEXT("SkeletalMesh_NoBoneFound"));

FName FFbxErrors::SkeletalMesh_InvalidBindPose(TEXT("SkeletalMesh_InvalidBindPose"));

FName FFbxErrors::SkeletalMesh_MultipleRoots(TEXT("SkeletalMesh_MultipleRoots"));

FName FFbxErrors::SkeletalMesh_BonesAreMissingFromBindPose(TEXT("SkeletalMesh_BonesAreMissingFromBindPose"));

FName FFbxErrors::SkeletalMesh_VertMissingInfluences(TEXT("SkeletalMesh_VertMissingInfluences"));

FName FFbxErrors::SkeletalMesh_SectionWithNoTriangle(TEXT("SkeletalMesh_SectionWithNoTriangle"));

FName FFbxErrors::SkeletalMesh_TooManyVertices(TEXT("SkeletalMesh_TooManyVertices"));

FName FFbxErrors::SkeletalMesh_FailedToCreatePhyscisAsset(TEXT("SkeletalMesh_FailedToCreatePhyscisAsset"));

FName FFbxErrors::SkeletalMesh_SkeletonRecreateError(TEXT("SkeletalMesh_SkeletonRecreateError"));

FName FFbxErrors::SkeletalMesh_ExceedsMaxBoneCount(TEXT("SkeletalMesh_ExceedsMaxBoneCount"));

FName FFbxErrors::SkeletalMesh_NoUVSet(TEXT("SkeletalMesh_NoUVSet"));

FName FFbxErrors::SkeletalMesh_LOD_MissingBone(TEXT("SkeletalMesh_LOD_MissingBone"));

FName FFbxErrors::SkeletalMesh_LOD_FailedToImport(TEXT("SkeletalMesh_LOD_FailedToImport"));

FName FFbxErrors::SkeletalMesh_LOD_RootNameIncorrect(TEXT("SkeletalMesh_LOD_RootNameIncorrect"));

FName FFbxErrors::SkeletalMesh_LOD_BonesDoNotMatch(TEXT("SkeletalMesh_LOD_BonesDoNotMatch"));

FName FFbxErrors::SkeletalMesh_LOD_IncorrectParent(TEXT("SkeletalMesh_LOD_IncorrectParent"));

FName FFbxErrors::SkeletalMesh_LOD_HasSoftVerts(TEXT("SkeletalMesh_LOD_HasSoftVerts"));

FName FFbxErrors::SkeletalMesh_LOD_MissingSocketBone(TEXT("SkeletalMesh_LOD_MissingSocketBone"));

FName FFbxErrors::SkeletalMesh_LOD_MissingMorphTarget(TEXT("SkeletalMesh_LOD_MissingMorphTarget"));

FName FFbxErrors::SkeletalMesh_FillImportDataFailed(TEXT("SkeletalMesh_FillImportDataFailed"));

FName FFbxErrors::SkeletalMesh_InvalidPosition(TEXT("SkeletalMesh_InvalidPosition"));

/** Animation **/
FName FFbxErrors::Animation_CouldNotFindRootTrack(TEXT("Animation_CouldNotFindRootTrack"));

FName FFbxErrors::Animation_CouldNotBuildSkeleton(TEXT("Animation_CouldNotBuildSkeleton"));

FName FFbxErrors::Animation_CouldNotFindTrack(TEXT("Animation_CouldNotFindTrack"));

FName FFbxErrors::Animation_ZeroLength(TEXT("Animation_ZeroLength"));

FName FFbxErrors::Animation_RootTrackMismatch(TEXT("Animation_RootTrackMismatch"));

FName FFbxErrors::Animation_DuplicatedBone(TEXT("Animation_DuplicatedBone"));

FName FFbxErrors::Animation_MissingBones(TEXT("Animation_MissingBones"));

FName FFbxErrors::Animation_InvalidData(TEXT("Animation_InvalidData"));

FName FFbxErrors::Animation_TransformError(TEXT("Animation_TransformError"));

FName FFbxErrors::Animation_DifferentLength(TEXT("Animation_DifferentLength"));

FName FFbxErrors::Animation_CurveNotFound(TEXT("Animation_CurveNotFound"));

static const FString FbxErrorsPath = TEXT("Shared/Editor/FbxErrors");

FFbxErrorToken::FFbxErrorToken(const FName& InErrorName)
	: FDocumentationToken(FbxErrorsPath, FbxErrorsPath, InErrorName.ToString())
{
}
