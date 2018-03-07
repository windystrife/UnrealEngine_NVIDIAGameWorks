// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxSceneImportOptions.h"


UFbxSceneImportOptions::UFbxSceneImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTransformVertexToAbsolute = false;
	bBakePivotInVertex = false;
	bCreateContentFolderHierarchy = false;
	bImportAsDynamic = false;
	HierarchyType = FBXSOCHT_CreateBlueprint;
	bForceFrontXAxis = false;
	bImportStaticMeshLODs = false;
	bImportSkeletalMeshLODs = false;
	bInvertNormalMaps = false;
	ImportTranslation = FVector(0);
	ImportRotation = FRotator(0);
	ImportUniformScale = 1.0f;
}

