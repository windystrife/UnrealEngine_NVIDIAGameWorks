#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "BlastAssetImportOptions.generated.h"

USTRUCT()
struct FBlastAssetImportOptions
{
	GENERATED_USTRUCT_BODY()

	/* Root name of any generated or imported assets. Skeletal meshes and collision will be this root name, plus suffixes. */
	UPROPERTY(EditAnywhere, Category = "General")
	FName	RootName;

	/* The filesystem path of the FBX skeletal mesh to import. */
	UPROPERTY(EditAnywhere, Transient, Category = "Blast Mesh")
	FFilePath SkeletalMeshPath;

	UPROPERTY(EditAnywhere, Category = "Blast Mesh")
	bool bImportCollisionData;

	FBlastAssetImportOptions()
		: bImportCollisionData(true)
	{
	}
};
