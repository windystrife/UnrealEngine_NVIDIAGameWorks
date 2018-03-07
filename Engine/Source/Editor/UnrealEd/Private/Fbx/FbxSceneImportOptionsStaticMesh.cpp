// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxSceneImportOptionsStaticMesh.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/FbxSceneImportOptions.h"


UFbxSceneImportOptionsStaticMesh::UFbxSceneImportOptionsStaticMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, StaticMeshLODGroup(NAME_None)
	, bAutoGenerateCollision(true)
	, bRemoveDegenerates(true)
	, bBuildAdjacencyBuffer(true)
	, bBuildReversedIndexBuffer(true)
	, bGenerateLightmapUVs(true)
	, bOneConvexHullPerUCX(true)
{
	VertexOverrideColor = FColor(255, 255, 255, 255);
	NormalImportMethod = EFBXSceneNormalImportMethod::FBXSceneNIM_ComputeNormals;
	NormalGenerationMethod = EFBXSceneNormalGenerationMethod::MikkTSpace;
}

void UFbxSceneImportOptionsStaticMesh::FillStaticMeshInmportData(UFbxStaticMeshImportData* StaticMeshImportData, UFbxSceneImportOptions* SceneImportOptions)
{
	check(StaticMeshImportData != nullptr);
	check(SceneImportOptions != nullptr);
	StaticMeshImportData->bAutoGenerateCollision = bAutoGenerateCollision;
	StaticMeshImportData->bBuildAdjacencyBuffer = bBuildAdjacencyBuffer;
	StaticMeshImportData->bBuildReversedIndexBuffer = bBuildReversedIndexBuffer;
	StaticMeshImportData->bGenerateLightmapUVs = bGenerateLightmapUVs;
	StaticMeshImportData->bOneConvexHullPerUCX = bOneConvexHullPerUCX;
	StaticMeshImportData->bRemoveDegenerates = bRemoveDegenerates;
	StaticMeshImportData->StaticMeshLODGroup = StaticMeshLODGroup;
	switch (VertexColorImportOption)
	{
	case EFbxSceneVertexColorImportOption::Ignore:
		StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Ignore;
		break;
	case EFbxSceneVertexColorImportOption::Override:
		StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Override;
		break;
	case EFbxSceneVertexColorImportOption::Replace:
		StaticMeshImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
		break;
	}
	StaticMeshImportData->VertexOverrideColor = VertexOverrideColor;

	//Scene general options
	StaticMeshImportData->bImportMeshLODs = SceneImportOptions->bImportStaticMeshLODs;
	StaticMeshImportData->ImportTranslation = SceneImportOptions->ImportTranslation;
	StaticMeshImportData->ImportRotation = SceneImportOptions->ImportRotation;
	StaticMeshImportData->ImportUniformScale = SceneImportOptions->ImportUniformScale;
	StaticMeshImportData->bTransformVertexToAbsolute = SceneImportOptions->bTransformVertexToAbsolute;
	StaticMeshImportData->bBakePivotInVertex = SceneImportOptions->bBakePivotInVertex;

	StaticMeshImportData->bImportAsScene = true;
}
