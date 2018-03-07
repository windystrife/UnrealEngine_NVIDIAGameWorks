// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxSceneImportOptionsSkeletalMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "Factories/FbxSceneImportOptions.h"
#include "Factories/FbxSceneImportOptionsStaticMesh.h"


UFbxSceneImportOptionsSkeletalMesh::UFbxSceneImportOptionsSkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bUpdateSkeletonReferencePose(false)
	, bCreatePhysicsAsset(false)
	, bUseT0AsRefPose(false)
	, bPreserveSmoothingGroups(false)
	, bImportMeshesInBoneHierarchy(true)
	, bImportMorphTargets(false)
	, bKeepOverlappingVertices(false)
	, bImportAnimations(true)
	, AnimationLength(EFbxSceneVertexColorImportOption::Replace)
	, FrameImportRange(0, 0)
	, bUseDefaultSampleRate(false)
	, bImportCustomAttribute(true)
	, bPreserveLocalTransform(false)
	, bDeleteExistingMorphTargetCurves(false)
{
}

void UFbxSceneImportOptionsSkeletalMesh::FillSkeletalMeshInmportData(UFbxSkeletalMeshImportData* SkeletalMeshImportData, UFbxAnimSequenceImportData* AnimSequenceImportData, UFbxSceneImportOptions* SceneImportOptions)
{
	check(SkeletalMeshImportData != nullptr);
	SkeletalMeshImportData->bImportMeshesInBoneHierarchy = bImportMeshesInBoneHierarchy;
	SkeletalMeshImportData->bImportMorphTargets = bImportMorphTargets;
	SkeletalMeshImportData->bKeepOverlappingVertices = bKeepOverlappingVertices;
	SkeletalMeshImportData->bPreserveSmoothingGroups = bPreserveSmoothingGroups;
	SkeletalMeshImportData->bUpdateSkeletonReferencePose = bUpdateSkeletonReferencePose;
	SkeletalMeshImportData->bUseT0AsRefPose = bUseT0AsRefPose;

	SkeletalMeshImportData->bImportMeshLODs = SceneImportOptions->bImportSkeletalMeshLODs;
	SkeletalMeshImportData->ImportTranslation = SceneImportOptions->ImportTranslation;
	SkeletalMeshImportData->ImportRotation = SceneImportOptions->ImportRotation;
	SkeletalMeshImportData->ImportUniformScale = SceneImportOptions->ImportUniformScale;
	SkeletalMeshImportData->bTransformVertexToAbsolute = SceneImportOptions->bTransformVertexToAbsolute;
	SkeletalMeshImportData->bBakePivotInVertex = SceneImportOptions->bBakePivotInVertex;
	
	SkeletalMeshImportData->bImportAsScene = true;

	AnimSequenceImportData->bImportMeshesInBoneHierarchy = bImportMeshesInBoneHierarchy;
	AnimSequenceImportData->AnimationLength = AnimationLength;
	AnimSequenceImportData->bDeleteExistingMorphTargetCurves = bDeleteExistingMorphTargetCurves;
	AnimSequenceImportData->bImportCustomAttribute = bImportCustomAttribute;
	AnimSequenceImportData->bPreserveLocalTransform = bPreserveLocalTransform;
	AnimSequenceImportData->bUseDefaultSampleRate = bUseDefaultSampleRate;
	AnimSequenceImportData->FrameImportRange = FrameImportRange;

	AnimSequenceImportData->bImportAsScene = true;
}
