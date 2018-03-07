// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxStaticMeshImportData.h"
#include "Engine/StaticMesh.h"

UFbxStaticMeshImportData::UFbxStaticMeshImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StaticMeshLODGroup = NAME_None;
	bRemoveDegenerates = true;
	bBuildAdjacencyBuffer = true;
	bBuildReversedIndexBuffer = true;
	bGenerateLightmapUVs = true;
	bOneConvexHullPerUCX = true;
	bAutoGenerateCollision = true;
	bTransformVertexToAbsolute = true;
	bBakePivotInVertex = false;
	VertexOverrideColor = FColor(255, 255, 255, 255);
}

UFbxStaticMeshImportData* UFbxStaticMeshImportData::GetImportDataForStaticMesh(UStaticMesh* StaticMesh, UFbxStaticMeshImportData* TemplateForCreation)
{
	check(StaticMesh);
	
	UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(StaticMesh->AssetImportData);
	if ( !ImportData )
	{
		ImportData = NewObject<UFbxStaticMeshImportData>(StaticMesh, NAME_None, RF_NoFlags, TemplateForCreation);

		// Try to preserve the source file path if possible
		if ( StaticMesh->AssetImportData != NULL )
		{
			ImportData->SourceData = StaticMesh->AssetImportData->SourceData;
		}

		StaticMesh->AssetImportData = ImportData;
	}

	return ImportData;
}

bool UFbxStaticMeshImportData::CanEditChange(const UProperty* InProperty) const
{
	bool bMutable = Super::CanEditChange(InProperty);
	UObject* Outer = GetOuter();
	if(Outer && bMutable)
	{
		// Let the FbxImportUi object handle the editability of our properties
		bMutable = Outer->CanEditChange(InProperty);
	}
	return bMutable;
}
