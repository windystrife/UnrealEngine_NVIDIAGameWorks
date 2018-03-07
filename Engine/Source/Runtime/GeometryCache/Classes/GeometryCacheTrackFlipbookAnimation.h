// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GeometryCacheTrack.h"
#include "GeometryCacheMeshData.h"

#include "GeometryCacheTrackFlipbookAnimation.generated.h"

/** Derived GeometryCacheTrack class, used for Transform animation. */
UCLASS(collapsecategories, hidecategories = Object, BlueprintType, config = Engine)
class GEOMETRYCACHE_API UGeometryCacheTrack_FlipbookAnimation : public UGeometryCacheTrack
{
	GENERATED_UCLASS_BODY()

	virtual ~UGeometryCacheTrack_FlipbookAnimation();

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	//~ End UObject Interface.

	//~ Begin UGeometryCacheTrack Interface.
	virtual const bool UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData) override;
	virtual const float GetMaxSampleTime() const override;
	//~ End UGeometryCacheTrack Interface.

	/**
	* Add a GeometryCacheMeshData sample to the Track
	*
	* @param MeshData - Holds the mesh data for the specific sample
	* @param SampleTime - SampleTime for the specific sample being added
	* @return void
	*/
	UFUNCTION()
	void AddMeshSample(const FGeometryCacheMeshData& MeshData, const float SampleTime);

private:
	/** Number of Mesh Sample within this track */
	UPROPERTY(VisibleAnywhere, Category = GeometryCache)
	uint32 NumMeshSamples;

	/** Stored data for each Mesh sample */
	TArray<FGeometryCacheMeshData> MeshSamples;
	TArray<float> MeshSampleTimes;
};

