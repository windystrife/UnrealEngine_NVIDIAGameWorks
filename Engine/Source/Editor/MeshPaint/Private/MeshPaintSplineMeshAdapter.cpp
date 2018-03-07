// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintSplineMeshAdapter.h"

#include "StaticMeshResources.h"
#include "Components/SplineMeshComponent.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSplineMeshes

bool FMeshPaintGeometryAdapterForSplineMeshes::InitializeVertexData()
{
	// Cache deformed spline mesh vertices for quick lookup during painting / previewing
	USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(StaticMeshComponent);
	check(SplineMeshComponent);

	bool bValid = false;
	if (LODModel)
	{
		// Retrieve vertex and index data 
		const int32 NumVertices = LODModel->PositionVertexBuffer.GetNumVertices();
		MeshVertices.Reset();
		MeshVertices.AddDefaulted(NumVertices);
		
		// Apply spline vertex deformation to each vertex
		for (int32 Index = 0; Index < NumVertices; Index++)
		{
			FVector Position = LODModel->PositionVertexBuffer.VertexPosition(Index);
			const FTransform SliceTransform = SplineMeshComponent->CalcSliceTransform(USplineMeshComponent::GetAxisValue(Position, SplineMeshComponent->ForwardAxis));
			USplineMeshComponent::GetAxisValue(Position, SplineMeshComponent->ForwardAxis) = 0;
			MeshVertices[Index] = SliceTransform.TransformPosition(Position);
		}

		const int32 NumIndices = LODModel->IndexBuffer.GetNumIndices();
		MeshIndices.Reset();
		MeshIndices.AddDefaulted(NumIndices);
		const FIndexArrayView ArrayView = LODModel->IndexBuffer.GetArrayView();
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			MeshIndices[Index] = ArrayView[Index];
		}

		bValid = (MeshVertices.Num() > 0 && MeshIndices.Num() > 0);
	}

	return bValid;
}


//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSplineMeshesFactory

TSharedPtr<IMeshPaintGeometryAdapter> FMeshPaintGeometryAdapterForSplineMeshesFactory::Construct(class UMeshComponent* InComponent, int32 MeshLODIndex) const
{
	if (USplineMeshComponent* SplineMeshComponent = Cast<USplineMeshComponent>(InComponent))
	{
		if (SplineMeshComponent->GetStaticMesh() != nullptr)
		{
			TSharedRef<FMeshPaintGeometryAdapterForSplineMeshes> Result = MakeShareable(new FMeshPaintGeometryAdapterForSplineMeshes());
			if (Result->Construct(InComponent, MeshLODIndex))
			{
				return Result;
			}
		}
	}

	return nullptr;
}
