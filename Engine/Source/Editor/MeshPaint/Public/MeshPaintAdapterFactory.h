// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshPaintModule.h"

class UMeshComponent;
class IMeshPaintGeometryAdapterFactory;

class MESHPAINT_API FMeshPaintAdapterFactory
{
public:
	static TArray<TSharedPtr<IMeshPaintGeometryAdapterFactory>> FactoryList;

public:
	static TSharedPtr<class IMeshPaintGeometryAdapter> CreateAdapterForMesh(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex);
	static void InitializeAdapterGlobals();
};
