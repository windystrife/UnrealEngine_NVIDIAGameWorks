// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "USDImportOptions.h"
#include "UnrealType.h"

UUSDImportOptions::UUSDImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MeshImportType = EUsdMeshImportType::StaticMesh;
	bApplyWorldTransformToGeometry = true;

}

void UUSDImportOptions::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		SaveConfig();
	}
}

UUSDSceneImportOptions::UUSDSceneImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bFlattenHierarchy = true;
	bImportMeshes = true;
	PathForAssets.Path = TEXT("/Game");
	bGenerateUniqueMeshes = true;
	bGenerateUniquePathPerUSDPrim = true;
	bApplyWorldTransformToGeometry = false;
}

void UUSDSceneImportOptions::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool UUSDSceneImportOptions::CanEditChange(const UProperty* InProperty) const
{
	bool bCanEdit = Super::CanEditChange(InProperty);

	FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;

	if (GET_MEMBER_NAME_CHECKED(UUSDImportOptions, MeshImportType) == PropertyName ||
		GET_MEMBER_NAME_CHECKED(UUSDImportOptions, bApplyWorldTransformToGeometry) == PropertyName || 
		GET_MEMBER_NAME_CHECKED(UUSDImportOptions, bGenerateUniquePathPerUSDPrim) == PropertyName)
	{
		bCanEdit &= bImportMeshes;
	}

	return bCanEdit;
}

