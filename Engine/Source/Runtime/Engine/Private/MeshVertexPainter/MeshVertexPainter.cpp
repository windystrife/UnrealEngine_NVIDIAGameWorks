// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshVertexPainter/MeshVertexPainter.h"
#include "Components.h"
#include "StaticMeshResources.h"
#include "Engine/StaticMesh.h"


void FMeshVertexPainter::PaintVerticesSingleColor(UStaticMeshComponent* StaticMeshComponent, const FLinearColor& FillColor, bool bConvertToSRGB)
{
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
	{
		return;
	}

	const int32 NumMeshLODs = StaticMeshComponent->GetStaticMesh()->GetNumLODs();
	StaticMeshComponent->SetLODDataCount(NumMeshLODs, NumMeshLODs);

	const FColor Color = FillColor.ToFColor(bConvertToSRGB);

	uint32 LODIndex = 0;
	for (FStaticMeshComponentLODInfo& LODInfo : StaticMeshComponent->LODData)
	{
		StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
		check(LODInfo.OverrideVertexColors == nullptr);

		const FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[LODIndex];
		const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;
		const uint32 NumVertices = PositionVertexBuffer.GetNumVertices();

		LODInfo.OverrideVertexColors = new FColorVertexBuffer;
		LODInfo.OverrideVertexColors->InitFromSingleColor(Color, NumVertices);

		BeginInitResource(LODInfo.OverrideVertexColors);

		LODIndex++;
	}

	StaticMeshComponent->CachePaintedDataIfNecessary();
	StaticMeshComponent->MarkRenderStateDirty();
	StaticMeshComponent->bDisallowMeshPaintPerInstance = true;
}

void FMeshVertexPainter::PaintVerticesLerpAlongAxis(UStaticMeshComponent* StaticMeshComponent, const FLinearColor& StartColor, const FLinearColor& EndColor, EVertexPaintAxis Axis, bool bConvertToSRGB)
{
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
	{
		return;
	}

	const FBoxSphereBounds Bounds = StaticMeshComponent->GetStaticMesh()->GetBounds();
	const FBox Box = Bounds.GetBox();
	static_assert(static_cast<int32>(EVertexPaintAxis::X) == 0, "EVertexPaintAxis not correctly defined");
	const float AxisMin = Box.Min.Component(static_cast<int32>(Axis));
	const float AxisMax = Box.Max.Component(static_cast<int32>(Axis));

	const int32 NumMeshLODs = StaticMeshComponent->GetStaticMesh()->GetNumLODs();
	StaticMeshComponent->SetLODDataCount(NumMeshLODs, NumMeshLODs);

	uint32 LODIndex = 0;
	for (FStaticMeshComponentLODInfo& LODInfo : StaticMeshComponent->LODData)
	{
		StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
		check(LODInfo.OverrideVertexColors == nullptr);

		const FStaticMeshLODResources& LODModel = StaticMeshComponent->GetStaticMesh()->RenderData->LODResources[LODIndex];
		const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;
		const uint32 NumVertices = PositionVertexBuffer.GetNumVertices();

		TArray<FColor> VertexColors;
		VertexColors.AddZeroed(NumVertices);

		for (uint32 VertexIndex = 0; VertexIndex < NumVertices; ++VertexIndex)
		{
			const FVector& VertexPosition = PositionVertexBuffer.VertexPosition(VertexIndex);
			const FLinearColor Color = FMath::Lerp(StartColor, EndColor, (VertexPosition.Component(static_cast<int32>(Axis)) - AxisMin) / (AxisMax - AxisMin));
			VertexColors[VertexIndex] = Color.ToFColor(bConvertToSRGB);
		}

		LODInfo.OverrideVertexColors = new FColorVertexBuffer;
		LODInfo.OverrideVertexColors->InitFromColorArray(VertexColors);

		BeginInitResource(LODInfo.OverrideVertexColors);

		LODIndex++;
	}

	StaticMeshComponent->CachePaintedDataIfNecessary();
	StaticMeshComponent->MarkRenderStateDirty();
	StaticMeshComponent->bDisallowMeshPaintPerInstance = true;
}

void FMeshVertexPainter::RemovePaintedVertices(UStaticMeshComponent* StaticMeshComponent)
{
	if (!StaticMeshComponent || !StaticMeshComponent->GetStaticMesh())
	{
		return;
	}

	uint32 LODIndex = 0;
	for (FStaticMeshComponentLODInfo& LODInfo : StaticMeshComponent->LODData)
	{
		StaticMeshComponent->RemoveInstanceVertexColorsFromLOD(LODIndex);
		check(LODInfo.OverrideVertexColors == nullptr);
		LODIndex++;
	}

	StaticMeshComponent->MarkRenderStateDirty();
	StaticMeshComponent->bDisallowMeshPaintPerInstance = false;
}
