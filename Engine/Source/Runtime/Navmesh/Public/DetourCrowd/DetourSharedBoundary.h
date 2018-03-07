// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Detour/DetourNavMeshQuery.h"

struct dtSharedBoundaryEdge
{
	float v0[3];
	float v1[3];
	dtPolyRef p0;
	dtPolyRef p1;
};

struct dtSharedBoundaryData
{
	float Center[3];
	float Radius;
	float AccessTime;
	dtQueryFilter* Filter;
	uint8 SingleAreaId;
	
	TArray<dtSharedBoundaryEdge> Edges;
	TSet<dtPolyRef> Polys;

	dtSharedBoundaryData() : Filter(nullptr) {}
};

class dtSharedBoundary
{
public:
	TSparseArray<dtSharedBoundaryData> Data;
	dtQueryFilter SingleAreaFilter;
	float CurrentTime;
	float NextClearTime;

	void Initialize();
	void Tick(float DeltaTime);

	int32 FindData(float* Center, float Radius, dtPolyRef ReqPoly, dtQueryFilter* NavFilter) const;
	int32 FindData(float* Center, float Radius, dtPolyRef ReqPoly, uint8 SingleAreaId) const;

	int32 CacheData(float* Center, float Radius, dtPolyRef CenterPoly, dtNavMeshQuery* NavQuery, dtQueryFilter* NavFilter);
	int32 CacheData(float* Center, float Radius, dtPolyRef CenterPoly, dtNavMeshQuery* NavQuery, uint8 SingleAreaId);

	void FindEdges(dtSharedBoundaryData& Data, dtPolyRef CenterPoly, dtNavMeshQuery* NavQuery, dtQueryFilter* NavFilter);
	bool HasSample(int32 Idx) const;

private:

	bool IsValid(int32 Idx, dtNavMeshQuery* NavQuery, dtQueryFilter* NavFilter) const;
};
