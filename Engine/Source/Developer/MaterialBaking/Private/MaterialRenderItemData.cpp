// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MaterialRenderItemData.h"

TGlobalResource<FMeshVertexFactory> FMeshVertexFactory::MeshVertexFactory;
TGlobalResource<FMaterialMeshVertexBuffer> FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer;

FMeshVertexFactory::FMeshVertexFactory()
{
	FLocalVertexFactory::FDataType VertexData;

	// position
	VertexData.PositionComponent = FVertexStreamComponent(
		&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
		STRUCT_OFFSET(FMaterialMeshVertex, Position),
		sizeof(FMaterialMeshVertex),
		VET_Float3
	);
	// tangents
	VertexData.TangentBasisComponents[0] = FVertexStreamComponent(
		&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
		STRUCT_OFFSET(FMaterialMeshVertex, TangentX),
		sizeof(FMaterialMeshVertex),
		VET_PackedNormal
	);
	VertexData.TangentBasisComponents[1] = FVertexStreamComponent(
		&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
		STRUCT_OFFSET(FMaterialMeshVertex, TangentZ),
		sizeof(FMaterialMeshVertex),
		VET_PackedNormal
	);
	// color
	VertexData.ColorComponent = FVertexStreamComponent(
		&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
		STRUCT_OFFSET(FMaterialMeshVertex, Color),
		sizeof(FMaterialMeshVertex),
		VET_Color
	);
	// UVs
	int32 UVIndex;
	for (UVIndex = 0; UVIndex < MAX_STATIC_TEXCOORDS - 1; UVIndex += 2)
	{
		VertexData.TextureCoordinates.Add(FVertexStreamComponent(
			&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, TextureCoordinate) + sizeof(FVector2D)* UVIndex,
			sizeof(FMaterialMeshVertex),
			VET_Float4
		));
	}
	// possible last UV channel if we have an odd number (by the way, MAX_STATIC_TEXCOORDS is even value, so most
	// likely the following code will never be executed)
	if (UVIndex < MAX_STATIC_TEXCOORDS)
	{
		VertexData.TextureCoordinates.Add(FVertexStreamComponent(
			&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
			STRUCT_OFFSET(FMaterialMeshVertex, TextureCoordinate) + sizeof(FVector2D)* UVIndex,
			sizeof(FMaterialMeshVertex),
			VET_Float2
		));
	}

	VertexData.LightMapCoordinateComponent = FVertexStreamComponent(
		&FMaterialMeshVertexBuffer::DummyMeshRendererVertexBuffer,
		STRUCT_OFFSET(FMaterialMeshVertex, LightMapCoordinate),
		sizeof(FMaterialMeshVertex),
		VET_Float2
	);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FMeshVertexFactoryConstructor,
		FMeshVertexFactory*, FactoryParam, this,
		FLocalVertexFactory::FDataType, DataParam, VertexData,
		{
			FactoryParam->SetData(DataParam);
		}
	);

	FlushRenderingCommands();
}
