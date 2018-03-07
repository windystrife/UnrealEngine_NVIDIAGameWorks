// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/SkeletalMesh.h"

#include "SkeletalMeshReductionSettings.generated.h"

USTRUCT()
struct FSkeletalMeshLODGroupSettings
{
	GENERATED_USTRUCT_BODY()

	FSkeletalMeshLODGroupSettings() {}
	
	/** Get Skeletal mesh optimizations setting structure for the given LOD level */
	ENGINE_API FSkeletalMeshOptimizationSettings GetSettings() const;

	/** Get the correct screen size for the given LOD level */
	ENGINE_API const float GetScreenSize() const;
protected:
	/** FSkeletalMeshLODSettings initializes group entries. */
	friend class USkeletalMeshReductionSettings;

	/** The screen sizes to use for the respective LOD level */
	UPROPERTY(EditAnywhere, Category = Reduction)
	float ScreenSize;
	
	/** The optimization settings to use for the respective LOD level */
	UPROPERTY(EditAnywhere, Category = Reduction)
	FSkeletalMeshOptimizationSettings OptimizationSettings;
};

UCLASS(config=Engine, defaultconfig, MinimalAPI) 
class USkeletalMeshReductionSettings : public UObject
{
	GENERATED_UCLASS_BODY()
protected:
	UPROPERTY(globalconfig)
	TArray<FSkeletalMeshLODGroupSettings> Settings;
public:

	/** Accessor and initializer **/
	ENGINE_API static USkeletalMeshReductionSettings* Get();

	/** Validates and initializes data retrieved from the ini file */
	ENGINE_API void Initialize();

	/** Retrieves the Skeletal mesh LOD group settings for the given name */
	ENGINE_API const FSkeletalMeshLODGroupSettings& GetDefaultSettingsForLODLevel(const int32 LODIndex) const;

	/** Returns whether or not valid settings were retrieved from the ini file */
	ENGINE_API const bool HasValidSettings() const;

	/** Returns the number of settings parsed from the ini file */
	ENGINE_API int32 GetNumberOfSettings() const;
private:
	/** Flag for whether or not valid settings were found parsed form the ini file */
	bool bValidSettings;
};
