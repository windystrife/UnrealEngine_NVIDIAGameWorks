// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

class IMeshPaintGeometryAdapter;
class UMeshComponent;

/**
 * Factory for IMeshPaintGeometryAdapter
 */
class IMeshPaintGeometryAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) const = 0;
	virtual void InitializeAdapterGlobals() = 0;
	virtual ~IMeshPaintGeometryAdapterFactory() {}
};