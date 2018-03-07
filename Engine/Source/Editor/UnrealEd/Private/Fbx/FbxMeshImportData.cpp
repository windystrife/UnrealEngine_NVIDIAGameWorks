// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxMeshImportData.h"
#include "UObject/UnrealType.h"

UFbxMeshImportData::UFbxMeshImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NormalImportMethod = FBXNIM_ComputeNormals;
	NormalGenerationMethod = EFBXNormalGenerationMethod::MikkTSpace;
	bBakePivotInVertex = false;
}

bool UFbxMeshImportData::CanEditChange(const UProperty* InProperty) const
{
	bool bMutable = Super::CanEditChange(InProperty);
	UObject* Outer = GetOuter();
	if(Outer && bMutable)
	{
		// Let the parent object handle the editability of our properties
		bMutable = Outer->CanEditChange(InProperty);
	}

	static FName NAME_NormalGenerationMethod("NormalGenerationMethod");
	if( bMutable && InProperty->GetFName() == NAME_NormalGenerationMethod )
	{
		// Normal generation method is ignored if we are importing both tangents and normals
		return NormalImportMethod == FBXNIM_ImportNormalsAndTangents ? false : true;
	}


	return bMutable;
}
