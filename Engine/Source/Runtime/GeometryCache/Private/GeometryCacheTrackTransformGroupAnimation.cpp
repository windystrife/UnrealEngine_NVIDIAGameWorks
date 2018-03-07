// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheTrackTransformGroupAnimation.h"


GEOMETRYCACHE_API UGeometryCacheTrack_TransformGroupAnimation::UGeometryCacheTrack_TransformGroupAnimation(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/) : UGeometryCacheTrack(ObjectInitializer)
{
}

void UGeometryCacheTrack_TransformGroupAnimation::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	MeshData.GetResourceSizeEx(CumulativeResourceSize);
}

void UGeometryCacheTrack_TransformGroupAnimation::Serialize(FArchive& Ar)
{
	UGeometryCacheTrack::Serialize(Ar);
	Ar << MeshData;
}

const bool UGeometryCacheTrack_TransformGroupAnimation::UpdateMeshData(const float Time, const bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData*& OutMeshData)
{
	// If InOutMeshSampleIndex equals -1 (first creation) update the OutVertices and InOutMeshSampleIndex
	if (InOutMeshSampleIndex == -1)
	{
		OutMeshData = &MeshData;
		InOutMeshSampleIndex = 0;
		return true;
	}

	return false;
}

void UGeometryCacheTrack_TransformGroupAnimation::SetMesh(const FGeometryCacheMeshData& NewMeshData)
{
	MeshData = NewMeshData;
	NumMaterials = NewMeshData.BatchesInfo.Num();
}
