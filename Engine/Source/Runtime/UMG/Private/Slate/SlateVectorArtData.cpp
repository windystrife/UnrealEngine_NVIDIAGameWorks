// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Slate/SlateVectorArtData.h"
#include "RawIndexBuffer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UMGPrivate.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"

static void StaticMeshToSlateRenderData(const UStaticMesh& DataSource, TArray<FSlateMeshVertex>& OutSlateVerts, TArray<uint32>& OutIndexes, FVector2D& OutExtentMin, FVector2D& OutExtentMax )
{
	OutExtentMin = FVector2D(FLT_MAX, FLT_MAX);
	OutExtentMax = FVector2D(-FLT_MAX, -FLT_MAX);

	const FStaticMeshLODResources& LOD = DataSource.RenderData->LODResources[0];
	const int32 NumSections = LOD.Sections.Num();
	if (NumSections > 1)
	{
		UE_LOG(LogUMG, Warning, TEXT("StaticMesh %s has %d sections. SMeshWidget expects a static mesh with 1 section."), *DataSource.GetName(), NumSections);
	}
	else
	{
		// Populate Vertex Data
		{
			const uint32 NumVerts = LOD.PositionVertexBuffer.GetNumVertices();
			OutSlateVerts.Empty();
			OutSlateVerts.Reserve(NumVerts);

			static const int32 MAX_SUPPORTED_UV_SETS = 6;
			const int32 TexCoordsPerVertex = LOD.GetNumTexCoords();
			if (TexCoordsPerVertex > MAX_SUPPORTED_UV_SETS)
			{
				UE_LOG(LogStaticMesh, Warning, TEXT("[%s] has %d UV sets; slate vertex data supports at most %d"), *DataSource.GetName(), TexCoordsPerVertex, MAX_SUPPORTED_UV_SETS);
			}

			for (uint32 i = 0; i < NumVerts; ++i)
			{
				// Copy Position
				const FVector& Position = LOD.PositionVertexBuffer.VertexPosition(i);
				OutExtentMin.X = FMath::Min(Position.X, OutExtentMin.X);
				OutExtentMin.Y = FMath::Min(Position.Y, OutExtentMin.Y);
				OutExtentMax.X = FMath::Max(Position.X, OutExtentMax.X);
				OutExtentMax.Y = FMath::Max(Position.Y, OutExtentMax.Y);
				
				// Copy Color
				FColor Color = (LOD.ColorVertexBuffer.GetNumVertices() > 0) ? LOD.ColorVertexBuffer.VertexColor(i) : FColor::White;
				
				// Copy all the UVs that we have, and as many as we can fit.
				const FVector2D& UV0 = (TexCoordsPerVertex > 0) ? LOD.VertexBuffer.GetVertexUV(i, 0) : FVector2D(1, 1);

				const FVector2D& UV1 = (TexCoordsPerVertex > 1) ? LOD.VertexBuffer.GetVertexUV(i, 1) : FVector2D(1, 1);

				const FVector2D& UV2 = (TexCoordsPerVertex > 2) ? LOD.VertexBuffer.GetVertexUV(i, 2) : FVector2D(1, 1);

				const FVector2D& UV3 = (TexCoordsPerVertex > 3) ? LOD.VertexBuffer.GetVertexUV(i, 3) : FVector2D(1, 1);

				const FVector2D& UV4 = (TexCoordsPerVertex > 4) ? LOD.VertexBuffer.GetVertexUV(i, 4) : FVector2D(1, 1);

				const FVector2D& UV5 = (TexCoordsPerVertex > 5) ? LOD.VertexBuffer.GetVertexUV(i, 5) : FVector2D(1, 1);

				OutSlateVerts.Add(FSlateMeshVertex(
					FVector2D(Position.X, Position.Y),
					Color,
					UV0,
					UV1,
					UV2,
					UV3,
					UV4,
					UV5
				));
			}
		}

		// Populate Index data
		{
			FIndexArrayView SourceIndexes = LOD.IndexBuffer.GetArrayView();
			const int32 NumIndexes = SourceIndexes.Num();
			OutIndexes.Empty();
			OutIndexes.Reserve(NumIndexes);
			for (int32 i = 0; i < NumIndexes; ++i)
			{
				OutIndexes.Add(SourceIndexes[i]);
			}


			// Sort the index buffer such that verts are drawn in Z-order.
			// Assume that all triangles are coplanar with Z == SomeValue.
			ensure(NumIndexes % 3 == 0);
			for (int32 a = 0; a < NumIndexes; a += 3)
			{
				for (int32 b = 0; b < NumIndexes; b += 3)
				{
					const float VertADepth = LOD.PositionVertexBuffer.VertexPosition(OutIndexes[a]).Z;
					const float VertBDepth = LOD.PositionVertexBuffer.VertexPosition(OutIndexes[b]).Z;
					if ( VertADepth < VertBDepth )
					{
						// Swap the order in which triangles will be drawn
						Swap(OutIndexes[a + 0], OutIndexes[b + 0]);
						Swap(OutIndexes[a + 1], OutIndexes[b + 1]);
						Swap(OutIndexes[a + 2], OutIndexes[b + 2]);
					}
				}
			}
		}
	}
}


USlateVectorArtData::USlateVectorArtData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

const TArray<FSlateMeshVertex>& USlateVectorArtData::GetVertexData() const
{
	return VertexData;
}

const TArray<uint32>& USlateVectorArtData::GetIndexData() const
{
	return IndexData;
}

UMaterialInterface* USlateVectorArtData::GetMaterial() const
{
	return Material;
}

UMaterialInstanceDynamic* USlateVectorArtData::ConvertToMaterialInstanceDynamic()
{
	EnsureValidData();
	UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(Material);
	if (ExistingMID == nullptr)
	{
		UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(Material, this);
		Material = NewMID;
		return NewMID;
	}
	else
	{
		return ExistingMID;
	}
}

void USlateVectorArtData::EnsureValidData()
{
#if WITH_EDITORONLY_DATA
	InitFromStaticMesh(*MeshAsset);
#endif
}

void USlateVectorArtData::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
	EnsureValidData();
}

#if WITH_EDITORONLY_DATA
void USlateVectorArtData::InitFromStaticMesh(const UStaticMesh& InSourceMesh)
{
	if (SourceMaterial != InSourceMesh.GetMaterial(0))
	{
		SourceMaterial = InSourceMesh.GetMaterial(0);
		Material = SourceMaterial;
	}

	ensureMsgf(Material != nullptr, TEXT("USlateVectorArtData::InitFromStaticMesh() expected %s to have a material assigned."), *InSourceMesh.GetFullName());

	StaticMeshToSlateRenderData(InSourceMesh, VertexData, IndexData, ExtentMin, ExtentMax);
}
#endif

FVector2D USlateVectorArtData::GetDesiredSize() const
{
	return GetExtentMax() - GetExtentMin();
}

FVector2D USlateVectorArtData::GetExtentMin() const
{
	return ExtentMin;
}

FVector2D USlateVectorArtData::GetExtentMax() const
{
	return ExtentMax;
}
