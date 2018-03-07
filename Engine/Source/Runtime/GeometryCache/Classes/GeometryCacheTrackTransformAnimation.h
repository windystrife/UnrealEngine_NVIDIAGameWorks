// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "GeometryCacheTrack.h"
#include "GeometryCacheMeshData.h"

#include "GeometryCacheTrackTransformAnimation.generated.h"

/** Derived GeometryCacheTrack class, used for Transform animation. */
UCLASS(collapsecategories, hidecategories = Object, BlueprintType, config = Engine)
class GEOMETRYCACHE_API UGeometryCacheTrack_TransformAnimation : public UGeometryCacheTrack
{
	GENERATED_UCLASS_BODY()
		
	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.
	
	//~ Begin UGeometryCacheTrack Interface.
	virtual const bool UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData) override;
	//~ End UGeometryCacheTrack Interface.

	/**
	* Sets/updates the MeshData for this track
	*
	* @param NewMeshData - GeometryCacheMeshData instance later used as the rendered mesh	
	*/
	UFUNCTION()
	void SetMesh(const FGeometryCacheMeshData& NewMeshData);	
private:
	FGeometryCacheMeshData MeshData;
};
