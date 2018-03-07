// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorFramework/AssetImportData.h"
#include "FbxAssetImportData.generated.h"

class UFbxSceneImportData;

/**
 * Base class for import data and options used when importing any asset from FBX
 */
UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, abstract)
class UNREALED_API UFbxAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FVector ImportTranslation;

	UPROPERTY(EditAnywhere, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FRotator ImportRotation;

	UPROPERTY(EditAnywhere, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	float ImportUniformScale;

	/** Whether to convert scene from FBX scene. */
	UPROPERTY(EditAnywhere, config, Category = Miscellaneous, meta = (ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX coordinate system to UE4 coordinate system"))
	bool bConvertScene;

	/** Whether to force the front axis to be align with X instead of -Y. */
	UPROPERTY(EditAnywhere, config, Category = Miscellaneous, meta = (editcondition = "bConvertScene", ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX coordinate system to UE4 coordinate system with front X axis instead of -Y"))
	bool bForceFrontXAxis;

	/** Whether to convert the scene from FBX unit to UE4 unit (centimeter). */
	UPROPERTY(EditAnywhere, config, Category = Miscellaneous, meta = (ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX unit to UE4 unit (centimeter)."))
	bool bConvertSceneUnit;

	/* Use by the reimport factory to answer CanReimport, if true only factory for scene reimport will return true */
	UPROPERTY()
	bool bImportAsScene;

	/* Use by the reimport factory to answer CanReimport, if true only factory for scene reimport will return true */
	UPROPERTY()
	UFbxSceneImportData* FbxSceneImportDataReference;

	/* We want to change the last dialog state but not the CDO, so we cannot call UObject::SaveConfig here.
	 * The problem is the class is use to store the import settings with the imported asset and the serialization
	 * is serializing the diff with the CDO. Data will be lost if the CDO change.
	*/
	virtual void SaveOptions();
	virtual void LoadOptions();
};
