// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshPaintAdapterFactory.h"
#include "IMeshPaintGeometryAdapterFactory.h"

TArray<TSharedPtr<class IMeshPaintGeometryAdapterFactory>> FMeshPaintAdapterFactory::FactoryList;

TSharedPtr<class IMeshPaintGeometryAdapter> FMeshPaintAdapterFactory::CreateAdapterForMesh(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex)
{
	TSharedPtr<IMeshPaintGeometryAdapter> Result;

	for (const auto& Factory : FactoryList)
	{
		Result = Factory->Construct(InComponent, InPaintingMeshLODIndex);

		if (Result.IsValid())
		{
			break;
		}
	}

	return Result;
}

void FMeshPaintAdapterFactory::InitializeAdapterGlobals()
{
	for (const auto& Factory : FactoryList)
	{
		Factory->InitializeAdapterGlobals();
	}
}
