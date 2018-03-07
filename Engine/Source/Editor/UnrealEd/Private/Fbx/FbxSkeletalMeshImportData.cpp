// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxSkeletalMeshImportData.h"
#include "Engine/SkeletalMesh.h"

UFbxSkeletalMeshImportData::UFbxSkeletalMeshImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bImportMeshesInBoneHierarchy(true)
{
	bTransformVertexToAbsolute = true;
	bBakePivotInVertex = false;
}

UFbxSkeletalMeshImportData* UFbxSkeletalMeshImportData::GetImportDataForSkeletalMesh(USkeletalMesh* SkeletalMesh, UFbxSkeletalMeshImportData* TemplateForCreation)
{
	check(SkeletalMesh);
	
	UFbxSkeletalMeshImportData* ImportData = Cast<UFbxSkeletalMeshImportData>(SkeletalMesh->AssetImportData);
	if ( !ImportData )
	{
		ImportData = NewObject<UFbxSkeletalMeshImportData>(SkeletalMesh, NAME_None, RF_NoFlags, TemplateForCreation);

		// Try to preserve the source file data if possible
		if ( SkeletalMesh->AssetImportData != NULL )
		{
			ImportData->SourceData = SkeletalMesh->AssetImportData->SourceData;
		}

		SkeletalMesh->AssetImportData = ImportData;
	}

	return ImportData;
}

bool UFbxSkeletalMeshImportData::CanEditChange(const UProperty* InProperty) const
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
