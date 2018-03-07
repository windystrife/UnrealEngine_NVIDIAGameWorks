// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Modified version of Recast/Detour's source file

//
// Copyright (c) 2009-2010 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//

#include "Detour/DetourNavMesh.h"
#include "Detour/DetourCommon.h"
#include "Detour/DetourAssert.h"

enum ESlabOverlapFlag
{
	SLABOVERLAP_Cross = 1,
	SLABOVERLAP_Min = 2,
	SLABOVERLAP_Max = 4,
};

inline bool overlapSlabs(const float* amin, const float* amax,
						 const float* bmin, const float* bmax,
						 const float px, const float py, unsigned char* mode)
{
	// Check for horizontal overlap.
	// The segment is shrunken a little so that slabs which touch
	// at end points are not connected.
	const float minx = dtMax(amin[0]+px,bmin[0]+px);
	const float maxx = dtMin(amax[0]-px,bmax[0]-px);
	if (minx > maxx)
		return false;
	
	// Check vertical overlap.
	const float ad = (amax[1]-amin[1]) / (amax[0]-amin[0]);
	const float ak = amin[1] - ad*amin[0];
	const float bd = (bmax[1]-bmin[1]) / (bmax[0]-bmin[0]);
	const float bk = bmin[1] - bd*bmin[0];
	const float aminy = ad*minx + ak;
	const float amaxy = ad*maxx + ak;
	const float bminy = bd*minx + bk;
	const float bmaxy = bd*maxx + bk;
	const float dmin = bminy - aminy;
	const float dmax = bmaxy - amaxy;
		
	// Crossing segments always overlap.
	if (dmin*dmax < 0)
	{
		*mode = SLABOVERLAP_Cross;
		return true;
	}
		
	// Check for overlap at endpoints.
	const float thr = dtSqr(py*2);
	if (dmin*dmin <= thr)
	{
		*mode |= SLABOVERLAP_Min;
	}

	if (dmax*dmax <= thr)
	{
		*mode |= SLABOVERLAP_Max;
	}
		
	return (*mode != 0);
}

static float getSlabCoord(const float* va, const int side)
{
	if (side == 0 || side == 4)
		return va[0];
	else if (side == 2 || side == 6)
		return va[2];
	return 0;
}

static void calcSlabEndPoints(const float* va, const float* vb, float* bmin, float* bmax, const int side)
{
	if (side == 0 || side == 4)
	{
		if (va[2] < vb[2])
		{
			bmin[0] = va[2];
			bmin[1] = va[1];
			bmax[0] = vb[2];
			bmax[1] = vb[1];
		}
		else
		{
			bmin[0] = vb[2];
			bmin[1] = vb[1];
			bmax[0] = va[2];
			bmax[1] = va[1];
		}
	}
	else if (side == 2 || side == 6)
	{
		if (va[0] < vb[0])
		{
			bmin[0] = va[0];
			bmin[1] = va[1];
			bmax[0] = vb[0];
			bmax[1] = vb[1];
		}
		else
		{
			bmin[0] = vb[0];
			bmin[1] = vb[1];
			bmax[0] = va[0];
			bmax[1] = va[1];
		}
	}
}

static float getHeightFromDMesh(const dtMeshTile* tile, int polyIdx, float* pos)
{
	if (tile == 0 || polyIdx < 0 || polyIdx >= tile->header->detailMeshCount)
		return 0.0f;

	const dtPolyDetail* pd = &tile->detailMeshes[polyIdx];
	const dtPoly* poly = &tile->polys[polyIdx];
	for (int j = 0; j < pd->triCount; ++j)
	{
		const unsigned char* t = &tile->detailTris[(pd->triBase+j)*4];
		const float* v[3];
		for (int k = 0; k < 3; ++k)
		{
			if (t[k] < poly->vertCount)
				v[k] = &tile->verts[poly->verts[t[k]]*3];
			else
				v[k] = &tile->detailVerts[(pd->vertBase+(t[k]-poly->vertCount))*3];
		}
		float h;
		if (dtClosestHeightPointTriangle(pos, v[0], v[1], v[2], h))
		{			
			return h;
		}
	}

	return 0.0f;
}

inline int computeTileHash(int x, int y, const int mask)
{
	const unsigned int h1 = 0x8da6b343; // Large multiplicative constants;
	const unsigned int h2 = 0xd8163841; // here arbitrarily chosen primes
	unsigned int n = h1 * x + h2 * y;
	return (int)(n & mask);
}

enum ELinkAllocationType
{
	CREATE_LINK_PREALLOCATED,
	CREATE_LINK_DYNAMIC_OFFMESH,
	CREATE_LINK_DYNAMIC_CLUSTER,
};

inline unsigned int allocLink(dtMeshTile* tile, char LinkAllocMode)
{
	unsigned int newLink = DT_NULL_LINK;

	if (LinkAllocMode == CREATE_LINK_PREALLOCATED)
	{
		if (tile->linksFreeList != DT_NULL_LINK)
		{
			newLink = tile->linksFreeList;
			tile->linksFreeList = tile->links[newLink].next;
		}
	}
	else if (LinkAllocMode == CREATE_LINK_DYNAMIC_OFFMESH)
	{
		if (tile->dynamicFreeListO == DT_NULL_LINK)
		{
			dtLink emptyLink;
			memset(&emptyLink, 0, sizeof(dtLink));
			emptyLink.next = DT_NULL_LINK;

			tile->dynamicFreeListO = tile->dynamicLinksO.size();
			tile->dynamicLinksO.push(emptyLink);		
		}

		newLink = tile->dynamicFreeListO;
		tile->dynamicFreeListO = tile->dynamicLinksO[newLink].next;

		newLink += tile->header->maxLinkCount;
	}
	else if (LinkAllocMode == CREATE_LINK_DYNAMIC_CLUSTER)
	{
		if (tile->dynamicFreeListC == DT_NULL_LINK)
		{
			dtClusterLink emptyLink;
			memset(&emptyLink, 0, sizeof(dtClusterLink));
			emptyLink.next = DT_NULL_LINK;

			tile->dynamicFreeListC = tile->dynamicLinksC.size();
			tile->dynamicLinksC.push(emptyLink);		
		}

		newLink = tile->dynamicFreeListC;
		tile->dynamicFreeListC = tile->dynamicLinksC[newLink].next;

		newLink += DT_CLINK_FIRST;
	}

	return newLink;
}

inline void freeLink(dtMeshTile* tile, unsigned int link)
{
	if (link < (unsigned int)tile->header->maxLinkCount)
	{
		tile->links[link].next = tile->linksFreeList;
		tile->linksFreeList = link;
	}
	else if (link < DT_CLINK_FIRST)
	{
		const unsigned int linkIdx = link - tile->header->maxLinkCount;
		tile->dynamicLinksO[linkIdx].next = tile->dynamicFreeListO;
		tile->dynamicFreeListO = linkIdx;
	}
	else
	{
		const unsigned int linkIdx = link - DT_CLINK_FIRST;
		tile->dynamicLinksC[linkIdx].next = tile->dynamicFreeListC;
		tile->dynamicFreeListC = linkIdx;
	}
}

dtNavMesh* dtAllocNavMesh()
{
	void* mem = dtAlloc(sizeof(dtNavMesh), DT_ALLOC_PERM);
	if (!mem) return 0;
	return new(mem) dtNavMesh;
}

/// @par
///
/// This function will only free the memory for tiles with the #DT_TILE_FREE_DATA
/// flag set.
void dtFreeNavMesh(dtNavMesh* navmesh)
{
	if (!navmesh) return;
	navmesh->~dtNavMesh();
	dtFree(navmesh);
}

void dtFreeNavMeshTileRuntimeData(dtMeshTile* tile)
{
	tile->dynamicLinksO.~dtChunkArray();
	tile->dynamicLinksC.~dtChunkArray();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Segment type offmesh links

static const unsigned int DT_INVALID_SEGMENT = 0xffffffff;
static const int DT_MAX_OFFMESH_SEGMENT_POINTS = 32;

struct dtOffMeshSegmentIntersection
{
	dtMeshTile* tile;
	unsigned int poly;
	float t;
};

struct dtOffMeshSegmentTileIntersection
{
	dtOffMeshSegmentIntersection points[DT_MAX_OFFMESH_SEGMENT_POINTS];
	int npoints;
};

struct dtOffMeshSegmentIntersectionLink
{
	float t;
	unsigned int polyA, polyB;
	dtMeshTile* tileA;
	dtMeshTile* tileB;
};

struct dtOffMeshSegmentPart
{
	float t0, t1;
	unsigned short vA0, vA1, vB0, vB1;
	unsigned int polyA, polyB;
	dtMeshTile* tileA;
	dtMeshTile* tileB;
};

struct dtOffMeshSegmentData
{
	dtOffMeshSegmentTileIntersection listA;
	dtOffMeshSegmentTileIntersection listB;
};

inline bool isIntersectionPointEqual(float t0, float t1)
{
	return dtAbs(t0 - t1) < 0.001f;
}

static bool isPolyIntersectingSegment(const dtMeshTile* tile, int polyIdx,
	const float* spos, const float* epos, float& tmin, float& tmax)
{
	dtPoly* poly = &tile->polys[polyIdx];
	float verts[DT_VERTS_PER_POLYGON*3];
	for (int i = 0; i < poly->vertCount; i++)
		dtVcopy(&verts[i*3], &tile->verts[poly->verts[i]*3]);

	int smin, smax;
	return dtIntersectSegmentPoly2D(spos, epos, verts, poly->vertCount, tmin, tmax, smin, smax);
}

static void addSegmentIntersections(dtOffMeshSegmentIntersection* isec, dtOffMeshSegmentIntersection* list, int& nlist)
{
	int minIdx = -1;
	int maxIdx = -1;
	for (int i = 0; i < nlist; i++)
	{
		if (isIntersectionPointEqual(list[i].t, isec[0].t)) minIdx = i;
		if (isIntersectionPointEqual(list[i].t, isec[1].t)) maxIdx = i;
	}

	// min = overwrite if exists or add new one
	if (minIdx < 0)
	{
		list[nlist] = isec[0];
		nlist++;
	}
	else
	{
		list[minIdx] = isec[0];
	}

	// max = skip if exists or add new one
	if (maxIdx < 0 && nlist < DT_MAX_OFFMESH_SEGMENT_POINTS)
	{
		list[nlist] = isec[1];
		nlist++;
	}
}

int segmentIntersectionSorter(const void* i1, const void* i2)
{
	// lesser "t" goes first
	const dtOffMeshSegmentIntersection* a = (const dtOffMeshSegmentIntersection*)i1;
	const dtOffMeshSegmentIntersection* b = (const dtOffMeshSegmentIntersection*)i2;
	return (a->t < b->t) ? -1 : (a->t > b->t) ? 1 : 0;
}

static void gatherSegmentIntersections(dtMeshTile* tile,
	const float* spos, const float* epos, const float radius,
	dtOffMeshSegmentTileIntersection& list)
{
	// get all polys intersecting with segment
	float segBMin[3], segBMax[3], segRad[3] = { radius, tile->header->walkableClimb, radius };
	dtVcopy(segBMin, spos);
	dtVcopy(segBMax, spos);
	dtVmin(segBMin, epos);
	dtVmax(segBMax, epos);
	dtVsub(segBMin, segBMin, segRad);
	dtVadd(segBMax, segBMax, segRad);

	if (!dtOverlapBounds(segBMin, segBMax, tile->header->bmin, tile->header->bmax))
		return;

	dtOffMeshSegmentIntersection intersec[2];
	intersec[0].tile = tile;
	intersec[1].tile = tile;

	float bmin[3], bmax[3];
	for (int i = 0; i < tile->header->offMeshBase; i++)
	{
		dtPoly* poly = &tile->polys[i];
		dtVcopy(bmin, &tile->verts[poly->verts[0]*3]);
		dtVcopy(bmax, &tile->verts[poly->verts[0]*3]);
		for (int j = 1; j < poly->vertCount; j++)
		{
			dtVmin(bmin, &tile->verts[poly->verts[j]*3]);
			dtVmax(bmax, &tile->verts[poly->verts[j]*3]);
		}

		// use simple AABB overlap test first
		if (dtOverlapBounds(segBMin, segBMax, bmin, bmax))
		{
			// mark intersection
			if (isPolyIntersectingSegment(tile, i, spos, epos, intersec[0].t, intersec[1].t))
			{
				intersec[0].poly = i;
				intersec[1].poly = i;
				addSegmentIntersections(intersec, list.points, list.npoints);

				if (list.npoints >= DT_MAX_OFFMESH_SEGMENT_POINTS)
					break;
			}
		}
	}
}

static dtOffMeshSegmentData* initSegmentIntersection(dtMeshTile* tile)
{
	const int segCount = tile->header->offMeshSegConCount;
	if (segCount <= 0)
		return NULL;

	dtOffMeshSegmentData* segs = (dtOffMeshSegmentData*)dtAlloc(sizeof(dtOffMeshSegmentData)*segCount, DT_ALLOC_TEMP);
	if (segs == NULL)
		return NULL;

	memset(segs, 0, sizeof(dtOffMeshSegmentData)*segCount);
	for (int i = 0; i < segCount; i++)
	{
		dtOffMeshSegmentConnection& con = tile->offMeshSeg[i];
		
		CA_SUPPRESS(6385);
		gatherSegmentIntersections(tile, con.startA, con.endA, con.rad, segs[i].listA);
		gatherSegmentIntersections(tile, con.startB, con.endB, con.rad, segs[i].listB);
	}

	return segs;
}

static void appendSegmentIntersection(dtOffMeshSegmentData* seg, dtMeshTile* tile, dtMeshTile* nei)
{
	if (seg == NULL)
		return;

	for (int i = 0; i < tile->header->offMeshSegConCount; i++)
	{
		dtOffMeshSegmentConnection& con = tile->offMeshSeg[i];

		gatherSegmentIntersections(nei, con.startA, con.endA, con.rad, seg[i].listA);
		gatherSegmentIntersections(nei, con.startB, con.endB, con.rad, seg[i].listB);
	}
}

int segmentIntersectionLinkSorter(const void* i1, const void* i2)
{
	// lesser "t" goes first
	const dtOffMeshSegmentIntersectionLink* a = (const dtOffMeshSegmentIntersectionLink*)i1;
	const dtOffMeshSegmentIntersectionLink* b = (const dtOffMeshSegmentIntersectionLink*)i2;
	return (a->t < b->t) ? -1 : (a->t > b->t) ? 1 : 0;
}

int segmentPartSorter(const void* i1, const void* i2)
{
	// higher length (t1-t0) goes first
	const dtOffMeshSegmentPart* a = (const dtOffMeshSegmentPart*)i1;
	const dtOffMeshSegmentPart* b = (const dtOffMeshSegmentPart*)i2;
	const float lenA = a->t1 - a->t0;
	const float lenB = b->t1 - b->t0;
	return (lenA > lenB) ? -1 : (lenA < lenB) ? 1 : 0;
}

static unsigned int findMatchingSegmentIntersection(const float t, const dtOffMeshSegmentIntersection* points, const int npoints, bool bAllowExisting)
{
	if (npoints < 1 || t < points[0].t || t > points[npoints - 1].t)
		return DT_INVALID_SEGMENT;

	unsigned int seg = DT_INVALID_SEGMENT;
	for (int i = 1; i < npoints; i++)
	{
		if (t <= points[i].t)
		{
			if (bAllowExisting || (!isIntersectionPointEqual(t, points[i].t) && !isIntersectionPointEqual(t, points[i-1].t)))
			{
				if (i < 2 || points[i - 2].poly != points[i - 1].poly)
					seg = i - 1;
			}

			return seg;
		}
	}

	return seg;
}

static bool canConnectSegmentPart(unsigned int startPoly, unsigned int endPoly,
	const dtMeshTile* startTile, const dtMeshTile* endTile,
	const dtOffMeshSegmentIntersection* points, const int npoints)
{
	if ((startPoly != endPoly || startTile != endTile) && (npoints > 1))
	{
		for (int i = 1; i < npoints; i++)
		{
			if (points[i-1].poly == points[i].poly &&
				points[i-1].tile == points[i].tile &&
				startPoly == points[i].poly &&
				startTile == points[i].tile)
			{
				return false;
			}
		}
	}

	return true;
}

static unsigned short findOrAddUniqueValue(float v, float* arr, unsigned short& narr)
{
	for (unsigned short i = 0; i < narr; i++)
	{
		if (arr[i] == v)
			return i;
	}

	const unsigned short pos = narr;
	arr[pos] = v;
	narr++;
	return pos;
}

static void createSegmentParts(dtMeshTile* tile, const dtOffMeshSegmentData& segData,
	dtOffMeshSegmentPart* parts, const int maxParts, int& nparts, unsigned short& nverts)
{
	if (segData.listA.npoints <= 0 && segData.listB.npoints <= 0)
		return;

	const int maxLinks = DT_MAX_OFFMESH_SEGMENT_POINTS * 2;
	dtOffMeshSegmentIntersectionLink links[maxLinks];
	memset(links, 0, sizeof(dtOffMeshSegmentIntersectionLink)*maxLinks);
	int nlinks = 0;

	// match from A to B
	for (int i = 0; i < segData.listA.npoints; i++)
	{
		unsigned int idxB = findMatchingSegmentIntersection(segData.listA.points[i].t, segData.listB.points, segData.listB.npoints, true);
		if (idxB != DT_INVALID_SEGMENT)
		{
			links[nlinks].t = segData.listA.points[i].t;
			links[nlinks].polyA = segData.listA.points[i].poly;
			links[nlinks].polyB = segData.listB.points[idxB].poly;
			links[nlinks].tileA = segData.listA.points[i].tile;
			links[nlinks].tileB = segData.listB.points[idxB].tile;
			nlinks++;
		}
	}

	// match from B to A
	for (int i = 0; i < segData.listA.npoints; i++)
	{
		unsigned int idxA = findMatchingSegmentIntersection(segData.listB.points[i].t, segData.listA.points, segData.listA.npoints, false);
		if (idxA != DT_INVALID_SEGMENT)
		{
			links[nlinks].t = segData.listB.points[i].t;
			links[nlinks].polyA = segData.listA.points[idxA].poly;
			links[nlinks].polyB = segData.listB.points[i].poly;
			links[nlinks].tileA = segData.listA.points[idxA].tile;
			links[nlinks].tileB = segData.listB.points[i].tile;
			nlinks++;
		}
	}

	if (nlinks < 2)
		return;

	// sort positions
	qsort(links, nlinks, sizeof(dtOffMeshSegmentIntersectionLink), segmentIntersectionLinkSorter);

	// create segments
	memset(parts, 0, sizeof(dtOffMeshSegmentPart)*maxParts);
	nparts = 0;

	for (int i = 1; i < nlinks; i++)
	{
		if (links[i-1].tileA == tile || links[i-1].tileB == tile ||
			links[i  ].tileA == tile || links[i  ].tileB == tile ||
			canConnectSegmentPart(links[i-1].polyA, links[i].polyA, links[i-1].tileA, links[i].tileA, segData.listA.points, segData.listA.npoints))
		{
			parts[nparts].t0 = links[i-1].t;
			parts[nparts].t1 = links[i].t;
			parts[nparts].polyA = links[i-1].polyA;
			parts[nparts].polyB = links[i-1].polyB;
			parts[nparts].tileA = links[i-1].tileA;
			parts[nparts].tileB = links[i-1].tileB;
			nparts++;
		}
	}

	// sort positions
	if (nparts > DT_MAX_OFFMESH_SEGMENT_PARTS)
	{
		qsort(parts, nparts, sizeof(dtOffMeshSegmentPart), segmentPartSorter);
		nparts = DT_MAX_OFFMESH_SEGMENT_PARTS;
	}

	// count unique verts
	float uniquePos[DT_MAX_OFFMESH_SEGMENT_PARTS*2];
	unsigned short nPos = 0;
	for (int i = 0; i < nparts; i++)
	{
		parts[i].vA0 = findOrAddUniqueValue(parts[i].t0, uniquePos, nPos);
		parts[i].vA1 = findOrAddUniqueValue(parts[i].t1, uniquePos, nPos);
	}

	for (int i = 0; i < nparts; i++)
	{
		parts[i].vB0 = parts[i].vA0 + nPos;
		parts[i].vB1 = parts[i].vA1 + nPos;
	}

	nverts = nPos * 2;
}

static void createSegmentPolys(dtNavMesh* nav, dtMeshTile* tile, dtOffMeshSegmentConnection* con,
	dtOffMeshSegmentPart* parts, int nparts, unsigned short vertBase, int polyBase)
{
	float lenA[3], lenB[3];
	dtVsub(lenA, con->endA, con->startA);
	dtVsub(lenB, con->endB, con->startB);

	unsigned char sideFwd = DT_LINK_FLAG_OFFMESH_CON | (con->getBiDirectional() ? DT_LINK_FLAG_OFFMESH_CON_BIDIR : 0);
	unsigned char sideBck = sideFwd | DT_LINK_FLAG_OFFMESH_CON_BACKTRACKER;
	con->firstPoly = (unsigned short)(polyBase - tile->header->offMeshSegPolyBase);
	con->npolys = (nparts > 0 && nparts < 256) ? (unsigned char)nparts : 0;

	for (int i = 0; i < nparts; i++)
	{
		dtOffMeshSegmentPart* it = &parts[i];

		// add verts
		dtVmad(&tile->verts[(vertBase+it->vA0)*3], con->startA, lenA, it->t0);
		dtVmad(&tile->verts[(vertBase+it->vA1)*3], con->startA, lenA, it->t1);
		dtVmad(&tile->verts[(vertBase+it->vB0)*3], con->startB, lenB, it->t0);
		dtVmad(&tile->verts[(vertBase+it->vB1)*3], con->startB, lenB, it->t1);

		// add poly
		dtPoly* poly = &tile->polys[polyBase + i];
		poly->vertCount = 4;
		poly->verts[0] = vertBase + it->vA0;
		poly->verts[1] = vertBase + it->vA1;
		poly->verts[2] = vertBase + it->vB0;
		poly->verts[3] = vertBase + it->vB1;
		poly->firstLink = DT_NULL_LINK;

		// add links
		const unsigned char sideA = (tile == it->tileA) ? DT_CONNECTION_INTERNAL : 0;
		const unsigned char sideB = (tile == it->tileB) ? DT_CONNECTION_INTERNAL : 0;
		nav->linkOffMeshHelper(tile, polyBase + i, it->tileA, it->polyA, sideBck | sideA, 0);
		nav->linkOffMeshHelper(tile, polyBase + i, it->tileB, it->polyB, sideFwd | sideB, 1);
		nav->linkOffMeshHelper(it->tileA, it->polyA, tile, polyBase + i, sideFwd | sideA, 0xff);
		nav->linkOffMeshHelper(it->tileB, it->polyB, tile, polyBase + i, sideBck | sideB, 0xff);
	}
}

static void createSegmentLinks(dtNavMesh* nav, dtOffMeshSegmentData* seg, dtMeshTile* tile)
{
	if (seg == NULL)
		return;

	unsigned short vertBase = (unsigned short)tile->header->offMeshSegVertBase;
	int polyBase = tile->header->offMeshSegPolyBase;
	for (int i = 0; i < tile->header->offMeshSegConCount; i++)
	{
		dtOffMeshSegmentConnection& con = tile->offMeshSeg[i];
		dtOffMeshSegmentData& segData = seg[i];

		qsort(segData.listA.points, segData.listA.npoints, sizeof(dtOffMeshSegmentIntersection), segmentIntersectionSorter);
		qsort(segData.listB.points, segData.listB.npoints, sizeof(dtOffMeshSegmentIntersection), segmentIntersectionSorter);

		const int maxParts = (DT_MAX_OFFMESH_SEGMENT_POINTS * 2) - 1;
		dtOffMeshSegmentPart parts[maxParts];
		int nparts = 0;
		unsigned short nverts = 0;
		createSegmentParts(tile, segData, parts, maxParts, nparts, nverts);

		createSegmentPolys(nav, tile, &con, parts, nparts, vertBase, polyBase);
		vertBase += nverts;
		polyBase += nparts;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////

/**
@class dtNavMesh

The navigation mesh consists of one or more tiles defining three primary types of structural data:

A polygon mesh which defines most of the navigation graph. (See rcPolyMesh for its structure.)
A detail mesh used for determining surface height on the polygon mesh. (See rcPolyMeshDetail for its structure.)
Off-mesh connections, which define custom point-to-point edges within the navigation graph.

The general build process is as follows:

-# Create rcPolyMesh and rcPolyMeshDetail data using the Recast build pipeline.
-# Optionally, create off-mesh connection data.
-# Combine the source data into a dtNavMeshCreateParams structure.
-# Create a tile data array using dtCreateNavMeshData().
-# Allocate at dtNavMesh object and initialize it. (For single tile navigation meshes,
   the tile data is loaded during this step.)
-# For multi-tile navigation meshes, load the tile data using dtNavMesh::addTile().

Notes:

- This class is usually used in conjunction with the dtNavMeshQuery class for pathfinding.
- Technically, all navigation meshes are tiled. A 'solo' mesh is simply a navigation mesh initialized 
  to have only a single tile.
- This class does not implement any asynchronous methods. So the ::dtStatus result of all methods will 
  always contain either a success or failure flag.

@see dtNavMeshQuery, dtCreateNavMeshData, dtNavMeshCreateParams, #dtAllocNavMesh, #dtFreeNavMesh
*/

dtNavMesh::dtNavMesh() :
	m_tileWidth(0),
	m_tileHeight(0),
	m_maxTiles(0),
	m_tileLutSize(0),
	m_tileLutMask(0),
	m_posLookup(0),
	m_nextFree(0),
	m_tiles(0),
	m_saltBits(0),
	m_tileBits(0),
	m_polyBits(0)
{
	memset(&m_params, 0, sizeof(dtNavMeshParams));
	memset(m_areaCostOrder, 0, sizeof(m_areaCostOrder));
	m_orig[0] = 0;
	m_orig[1] = 0;
	m_orig[2] = 0;
}

dtNavMesh::~dtNavMesh()
{
	for (int i = 0; i < m_maxTiles; ++i)
	{
		if (m_tiles[i].flags & DT_TILE_FREE_DATA)
		{
			dtFree(m_tiles[i].data);
			m_tiles[i].data = 0;
			m_tiles[i].dataSize = 0;
		}

		// cleanup runtime data (not serialized by navmesh owners)
		dtFreeNavMeshTileRuntimeData(&m_tiles[i]);
	}
	dtFree(m_posLookup);
	dtFree(m_tiles);
}
		
dtStatus dtNavMesh::init(const dtNavMeshParams* params)
{
	memcpy(&m_params, params, sizeof(dtNavMeshParams));
	dtVcopy(m_orig, params->orig);
	m_tileWidth = params->tileWidth;
	m_tileHeight = params->tileHeight;
	
	// Init tiles
	m_maxTiles = params->maxTiles;
	m_tileLutSize = dtNextPow2(params->maxTiles/4);
	if (!m_tileLutSize) m_tileLutSize = 1;
	m_tileLutMask = m_tileLutSize-1;
	
	m_tiles = (dtMeshTile*)dtAlloc(sizeof(dtMeshTile)*m_maxTiles, DT_ALLOC_PERM);
	if (!m_tiles)
		return DT_FAILURE | DT_OUT_OF_MEMORY;
	m_posLookup = (dtMeshTile**)dtAlloc(sizeof(dtMeshTile*)*m_tileLutSize, DT_ALLOC_PERM);
	if (!m_posLookup)
		return DT_FAILURE | DT_OUT_OF_MEMORY;
	memset(m_tiles, 0, sizeof(dtMeshTile)*m_maxTiles);
	memset(m_posLookup, 0, sizeof(dtMeshTile*)*m_tileLutSize);
	m_nextFree = 0;
	for (int i = m_maxTiles-1; i >= 0; --i)
	{
		m_tiles[i].salt = DT_SALT_BASE;
		m_tiles[i].next = m_nextFree;
		m_nextFree = &m_tiles[i];
	}

	// Init ID generator values.
	m_tileBits = dtIlog2(dtNextPow2((unsigned int)params->maxTiles));
	m_polyBits = dtIlog2(dtNextPow2((unsigned int)params->maxPolys));
	// Only allow 31 salt bits, since the salt mask is calculated using 32bit uint and it will overflow.
#if USE_64BIT_ADDRESS
	m_saltBits = dtMin((unsigned int)31, 64 - m_tileBits - m_polyBits);
#else
	m_saltBits = dtMin((unsigned int)31, 32 - m_tileBits - m_polyBits);
#endif
	if (m_saltBits < DT_MIN_SALT_BITS)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	return DT_SUCCESS;
}

dtStatus dtNavMesh::init(unsigned char* data, const int dataSize, const int flags)
{
	// Make sure the data is in right format.
	dtMeshHeader* header = (dtMeshHeader*)data;
	if (header->magic != DT_NAVMESH_MAGIC)
		return DT_FAILURE | DT_WRONG_MAGIC;
	if (header->version != DT_NAVMESH_VERSION)
		return DT_FAILURE | DT_WRONG_VERSION;

	dtNavMeshParams params;
	dtVcopy(params.orig, header->bmin);
	params.tileWidth = header->bmax[0] - header->bmin[0];
	params.tileHeight = header->bmax[2] - header->bmin[2];
	params.maxTiles = 1;
	params.maxPolys = header->polyCount;
	
	dtStatus status = init(&params);
	if (dtStatusFailed(status))
		return status;

	return addTile(data, dataSize, flags, 0, 0);
}

/// @par
///
/// @note The parameters are created automatically when the single tile
/// initialization is performed.
const dtNavMeshParams* dtNavMesh::getParams() const
{
	return &m_params;
}

//////////////////////////////////////////////////////////////////////////////////////////
int dtNavMesh::findConnectingPolys(const float* va, const float* vb,
	const dtMeshTile* fromTile, int fromPolyIdx,
	const dtMeshTile* tile, int side,
	dtChunkArray<FConnectingPolyData>& cons) const
{
	if (!tile) return 0;

	float amin[2], amax[2], apt[3];
	calcSlabEndPoints(va, vb, amin, amax, side);
	const float apos = getSlabCoord(va, side);
	dtVcopy(apt, va);

	// Remove links pointing to 'side' and compact the links array. 
	float bmin[2], bmax[2], bpt[3];
	unsigned short m = DT_EXT_LINK | (unsigned short)side;
	int n = 0;

	dtPolyRef base = getPolyRefBase(tile);

	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		dtPoly* poly = &tile->polys[i];
		const int nv = poly->vertCount;
		for (int j = 0; j < nv; ++j)
		{
			// Skip edges which do not point to the right side.
			if (poly->neis[j] != m) continue;

			const float* vc = &tile->verts[poly->verts[j] * 3];
			const float* vd = &tile->verts[poly->verts[(j + 1) % nv] * 3];
			const float bpos = getSlabCoord(vc, side);

			// Segments are not close enough.
			if (dtAbs(apos - bpos) > 0.01f)
				continue;

			// Check if the segments touch.
			calcSlabEndPoints(vc, vd, bmin, bmax, side);

			unsigned char overlapMode = 0;
			if (!overlapSlabs(amin, amax, bmin, bmax, 0.01f, tile->header->walkableClimb, &overlapMode)) continue;

			// if overlapping with only one side, verify height difference using detailed mesh
			if (overlapMode == SLABOVERLAP_Max || overlapMode == SLABOVERLAP_Min)
			{
				dtVcopy(bpt, vc);
				const int coordIdx = (side == 0 || side == 4) ? 2 : 0;
				apt[coordIdx] = (overlapMode == SLABOVERLAP_Min) ? dtMax(amin[0], bmin[0]) : dtMin(amax[0], bmax[0]);
				bpt[coordIdx] = apt[coordIdx];

				const float aH = getHeightFromDMesh(fromTile, fromPolyIdx, apt);
				const float bH = getHeightFromDMesh(tile, i, bpt);
				const float heightDiff = dtAbs(aH - bH);
				if (heightDiff > tile->header->walkableClimb)
					continue;
			}

			// Add return value.
			FConnectingPolyData NewPolyData;
			NewPolyData.min = dtMax(amin[0], bmin[0]);
			NewPolyData.max = dtMin(amax[0], bmax[0]);
			NewPolyData.ref = base | (dtPolyRef)i;
			cons.push(NewPolyData);
			n++;
			break;
		}
	}

	return n;
}

void dtNavMesh::unconnectExtLinks(dtMeshTile* tile, dtMeshTile* target)
{
	if (!tile || !target) return;

	const unsigned int targetNum = decodePolyIdTile(getTileRef(target));

	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		dtPoly* poly = &tile->polys[i];
		unsigned int j = poly->firstLink;
		unsigned int pj = DT_NULL_LINK;
		while (j != DT_NULL_LINK)
		{
			dtLink& testLink = getLink(tile, j);
//@UE4 BEGIN
			if ((testLink.side & DT_CONNECTION_INTERNAL) == 0 &&
//@UE4 END
				decodePolyIdTile(testLink.ref) == targetNum)
			{
				// Remove link.
				unsigned int nj = testLink.next;
				if (pj == DT_NULL_LINK)
				{
					poly->firstLink = nj;
				}
				else
				{
					dtLink& prevLink = getLink(tile, pj);
					prevLink.next = nj;
				}
				freeLink(tile, j);
				j = nj;
			}
			else
			{
				// Advance
				pj = j;
				j = testLink.next;
			}
		}
	}

	unconnectClusterLinks(tile, target);
}

void dtNavMesh::connectExtLinks(dtMeshTile* tile, dtMeshTile* target, int side, bool updateCLinks)
{
	if (!tile) return;
	
	dtChunkArray<FConnectingPolyData> cons(16);

	// Connect border links.
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		dtPoly* poly = &tile->polys[i];

		// Create new links.
//		unsigned short m = DT_EXT_LINK | (unsigned short)side;
		
		const int nv = poly->vertCount;
		for (int j = 0; j < nv; ++j)
		{
			// Skip non-portal edges.
			if ((poly->neis[j] & DT_EXT_LINK) == 0)
				continue;
			
			const int dir = (int)(poly->neis[j] & 0xff);
			if (side != -1 && dir != side)
				continue;
			
			// Create new links
			const float* va = &tile->verts[poly->verts[j]*3];
			const float* vb = &tile->verts[poly->verts[(j+1) % nv]*3];

			// reset array before using
			cons.resize(0);
			findConnectingPolys(va,vb, tile, i, target, dtOppositeTile(dir), cons);

			for (int k = 0; k < cons.size(); ++k)
			{
				const FConnectingPolyData& NeiData = cons[k];
				unsigned int idx = allocLink(tile, CREATE_LINK_PREALLOCATED);
				if (idx != DT_NULL_LINK)
				{
					dtLink* link = &tile->links[idx];
					link->ref = NeiData.ref;
					link->edge = (unsigned char)j;
					link->side = (unsigned char)dir;
					
					link->next = poly->firstLink;
					poly->firstLink = idx;

					// Compress portal limits to a byte value.
					if (dir == 0 || dir == 4)
					{
						float tmin = (NeiData.min-va[2]) / (vb[2]-va[2]);
						float tmax = (NeiData.max-va[2]) / (vb[2]-va[2]);
						if (tmin > tmax)
							dtSwap(tmin,tmax);
						link->bmin = (unsigned char)(dtClamp(tmin, 0.0f, 1.0f)*255.0f);
						link->bmax = (unsigned char)(dtClamp(tmax, 0.0f, 1.0f)*255.0f);
					}
					else if (dir == 2 || dir == 6)
					{
						float tmin = (NeiData.min-va[0]) / (vb[0]-va[0]);
						float tmax = (NeiData.max-va[0]) / (vb[0]-va[0]);
						if (tmin > tmax)
							dtSwap(tmin,tmax);
						link->bmin = (unsigned char)(dtClamp(tmin, 0.0f, 1.0f)*255.0f);
						link->bmax = (unsigned char)(dtClamp(tmax, 0.0f, 1.0f)*255.0f);
					}
				}

				if (updateCLinks)
				{
					unsigned int targetIdx = decodePolyIdPoly(NeiData.ref);
					if (tile->polyClusters && target->polyClusters &&
						i < tile->header->offMeshBase &&
						targetIdx < (unsigned int)target->header->offMeshBase)
					{
						connectClusterLink(tile, tile->polyClusters[i], target, target->polyClusters[targetIdx], DT_CLINK_VALID_FWD);
						connectClusterLink(target, target->polyClusters[targetIdx], tile, tile->polyClusters[i], DT_CLINK_VALID_BCK);
					}
				}
			}
		}
	}
}

void dtNavMesh::linkOffMeshHelper(dtMeshTile* tile0, unsigned int polyIdx0, dtMeshTile* tile1, unsigned int polyIdx1, unsigned char side, unsigned char edge)
{
	dtPoly* poly0 = &tile0->polys[polyIdx0];

	const unsigned int idx = allocLink(tile0, CREATE_LINK_DYNAMIC_OFFMESH);
	dtLink& link = getLink(tile0, idx);

	link.ref = getPolyRefBase(tile1) | (dtPolyRef)polyIdx1;
	link.edge = edge;
	link.side = side;
	link.bmin = link.bmax = 0;
	link.next = poly0->firstLink;
	poly0->firstLink = idx;
}

void dtNavMesh::connectExtOffMeshLinks(dtMeshTile* tile, dtMeshTile* target, int side, bool updateCLinks)
{
	if (!tile) return;
	
	// Connect off-mesh links.
	// We are interested on links which land from target tile to this tile.
//@UE4 BEGIN
	const unsigned char oppositeSide = (side == -1) ? DT_CONNECTION_INTERNAL : (unsigned char)dtOppositeTile(side);

	for (int i = 0; i < target->header->offMeshConCount; ++i)
	{
		dtOffMeshConnection* targetCon = &target->offMeshCons[i];
		if (targetCon->side != oppositeSide)
			continue;

		const unsigned char biDirFlag = targetCon->getBiDirectional() ? DT_LINK_FLAG_OFFMESH_CON_BIDIR : 0;

		dtPoly* targetPoly = &target->polys[targetCon->poly];
		// Skip off-mesh connections which start location could not be connected at all.
		if (targetPoly->firstLink == DT_NULL_LINK)
			continue;
		
		const dtLink& targetLink = getLink(target, targetPoly->firstLink);
		const dtPolyRef targetLandPoly = targetLink.ref;
		const float ext[3] = { targetCon->rad, targetCon->height, targetCon->rad };

		// Find polygon to connect to.
		const float* p = &targetCon->pos[3];
		float nearestPt[3];
		dtPolyRef ref = 0;

		// [UE4] try finding cheapest, but it that's outside requested radius, fallback to nearest one
		// findNearestPoly may return too optimistic results, further check to make sure. 
		if (targetCon->getSnapToCheapestArea())
		{
			ref = findCheapestNearPolyInTile(tile, p, ext, nearestPt);
			if (!ref || (ref == targetLandPoly) || (dtSqr(nearestPt[0] - p[0]) + dtSqr(nearestPt[2] - p[2]) > dtSqr(targetCon->rad)))
			{
				ref = 0;
			}
		}

		if (!ref)
		{
			ref = findNearestPolyInTile(tile, p, ext, nearestPt, true);
			if (!ref || (ref == targetLandPoly) || (dtSqr(nearestPt[0] - p[0]) + dtSqr(nearestPt[2] - p[2]) > dtSqr(targetCon->rad)))
			{
				ref = 0;
			}
		}

		// Avoid linking back into the same ground poly
		if (!ref || (targetLandPoly == ref))
			continue;
		// Make sure the location is on current mesh.
		float* v = &target->verts[targetPoly->verts[1]*3];
		dtVcopy(v, nearestPt);
		
		unsigned char linkSide = oppositeSide | DT_LINK_FLAG_OFFMESH_CON | biDirFlag;
		if (tile != target)
		{
			linkSide &= ~DT_CONNECTION_INTERNAL;
		}

		// Link off-mesh connection to target poly.
		const unsigned int landPolyIdx = decodePolyIdPoly(ref);
		linkOffMeshHelper(target, targetCon->poly, tile, landPolyIdx, linkSide, 1);

		// Link target poly to off-mesh connection.
		linkSide = (side == -1 ? DT_CONNECTION_INTERNAL : side) | DT_LINK_FLAG_OFFMESH_CON | biDirFlag;
		if (tile != target)
		{
			linkSide &= ~DT_CONNECTION_INTERNAL;
		}

		if (biDirFlag == 0)
		{
			// if it's not a bi-directional link put it in anyway
			// just annotate it accordingly
			linkSide |= DT_LINK_FLAG_OFFMESH_CON_BACKTRACKER;
		}

		linkOffMeshHelper(tile, landPolyIdx, target, targetCon->poly, linkSide, 0xff);

		if (updateCLinks)
		{
			unsigned int targetPolyIdx = decodePolyIdPoly(targetLandPoly);
			unsigned int thisPolyIdx = landPolyIdx;
			if (thisPolyIdx < (unsigned int)tile->header->offMeshBase &&
				targetPolyIdx < (unsigned int)target->header->offMeshBase &&
				tile->polyClusters && target->polyClusters)
			{
				unsigned int targetClusterIdx = target->polyClusters[targetPolyIdx];
				unsigned int thisClusterIdx = tile->polyClusters[thisPolyIdx];
				const bool bUniqueCheck = true;

				const unsigned char flagsFwd = DT_CLINK_VALID_FWD | (biDirFlag ? DT_CLINK_VALID_BCK : 0);
				const unsigned char flagsBck = DT_CLINK_VALID_BCK | (biDirFlag ? DT_CLINK_VALID_FWD : 0);

				connectClusterLink(target, targetClusterIdx, tile, thisClusterIdx, flagsFwd, bUniqueCheck);				
				connectClusterLink(tile, thisClusterIdx, target, targetClusterIdx, flagsBck, bUniqueCheck);
			}
		}
//@UE4 END
	}

}

void dtNavMesh::connectIntLinks(dtMeshTile* tile)
{
	if (!tile) return;

	dtPolyRef base = getPolyRefBase(tile);

	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		dtPoly* poly = &tile->polys[i];
		poly->firstLink = DT_NULL_LINK;

		if (poly->getType() != DT_POLYTYPE_GROUND)
			continue;
			
		// Build edge links backwards so that the links will be
		// in the linked list from lowest index to highest.
		for (int j = poly->vertCount-1; j >= 0; --j)
		{
			// Skip hard and non-internal edges.
			if (poly->neis[j] == 0 || (poly->neis[j] & DT_EXT_LINK)) continue;

			unsigned int idx = allocLink(tile, CREATE_LINK_PREALLOCATED);
			if (idx != DT_NULL_LINK)
			{
				dtLink* link = &tile->links[idx];
				link->ref = base | (dtPolyRef)(poly->neis[j]-1);
				link->edge = (unsigned char)j;
//@UE4 BEGIN 
				link->side = DT_CONNECTION_INTERNAL;
//@UE4 END
				link->bmin = link->bmax = 0;
				// Add to linked list.
				link->next = poly->firstLink;
				poly->firstLink = idx;
			}
		}			
	}
}

void dtNavMesh::baseOffMeshLinks(dtMeshTile* tile)
{
	if (!tile) return;
	
	// Base off-mesh connection start points.
	for (int i = 0; i < tile->header->offMeshConCount; ++i)
	{
		dtOffMeshConnection* con = &tile->offMeshCons[i];
		dtPoly* poly = &tile->polys[con->poly];
	
		const float ext[3] = { con->rad, con->height, con->rad };
		
		// Find polygon to connect to.
		const float* p = &con->pos[0]; // First vertex
		float nearestPt[3];
		dtPolyRef ref = 0;

		// [UE4] try finding cheapest, but it that's outside requested radius, fallback to nearest one
		// findNearestPoly may return too optimistic results, further check to make sure. 
		if (con->getSnapToCheapestArea())
		{
			ref = findCheapestNearPolyInTile(tile, p, ext, nearestPt);
			if (!ref || (dtSqr(nearestPt[0] - p[0]) + dtSqr(nearestPt[2] - p[2]) > dtSqr(con->rad)))
			{
				ref = 0;
			}
		}

		if (!ref)
		{
			ref = findNearestPolyInTile(tile, p, ext, nearestPt, true);
			if (!ref || (dtSqr(nearestPt[0] - p[0]) + dtSqr(nearestPt[2] - p[2]) > dtSqr(con->rad)))
			{
				ref = 0;
			}
		}

		if (!ref) continue;

		// Make sure the location is on current mesh.
		float* v = &tile->verts[poly->verts[0]*3];
		dtVcopy(v, nearestPt);

		unsigned char sideFwd = DT_CONNECTION_INTERNAL | DT_LINK_FLAG_OFFMESH_CON | (con->getBiDirectional() ? DT_LINK_FLAG_OFFMESH_CON_BIDIR : 0);
		unsigned char sideBck = sideFwd | DT_LINK_FLAG_OFFMESH_CON_BACKTRACKER;

		// Link off-mesh connection to target poly.
		linkOffMeshHelper(tile, con->poly, tile, decodePolyIdPoly(ref), sideBck, 0);

		// Start end-point is always connect back to off-mesh connection.
		linkOffMeshHelper(tile, decodePolyIdPoly(ref), tile, con->poly, sideFwd, 0xff);
	}
}

void dtNavMesh::connectClusterLink(dtMeshTile* tile0, unsigned int clusterIdx0, dtMeshTile* tile1, unsigned int clusterIdx1, unsigned char flags, bool bCheckExisting)
{
	if (tile0 == tile1 && clusterIdx0 == clusterIdx1)
		return;

	dtCluster& cluster0 = tile0->clusters[clusterIdx0];
	dtClusterRef cluster1Ref = getClusterRefBase(tile1) | (dtClusterRef)clusterIdx1;

	dtClusterLink* link = 0;

	// check if already connected
	if (bCheckExisting)
	{
		unsigned int i = cluster0.firstLink;
		while (i != DT_NULL_LINK)
		{
			dtClusterLink& testLink = getClusterLink(tile0, i);
			if (testLink.ref == cluster1Ref)
			{
				link = &testLink;
				break;
			}

			i = testLink.next;
		}
	}

	if (!link)
	{
		// add new link
		unsigned int linkIdx = allocLink(tile0, CREATE_LINK_DYNAMIC_CLUSTER);
		dtClusterLink& testLink = getClusterLink(tile0, linkIdx);

		testLink.ref = cluster1Ref;
		testLink.next = cluster0.firstLink;
		cluster0.firstLink = linkIdx;

		link = &testLink;
	}

	// assign cost and side properties
	link->flags = link->flags | flags;
}

void dtNavMesh::unconnectClusterLinks(dtMeshTile* tile0, dtMeshTile* tile1)
{
	unsigned int tile1Num = decodeClusterIdTile(getTileRef(tile1));
	unsigned int nclusters = (unsigned int)tile0->header->clusterCount;
	for (unsigned int i = 0; i < nclusters; i++)
	{
		dtCluster& cluster = tile0->clusters[i];

		unsigned int pj = DT_NULL_LINK;
		unsigned int j = cluster.firstLink;
		while (j != DT_NULL_LINK)
		{
			dtClusterLink& link = getClusterLink(tile0, j);
			unsigned int linkTileNum = decodeClusterIdTile(link.ref);
			if (linkTileNum == tile1Num)
			{
				unsigned int nj = link.next;
				if (pj == DT_NULL_LINK)
				{
					cluster.firstLink = nj;
				}
				else
				{
					dtClusterLink& prevLink = getClusterLink(tile0, pj);
					prevLink.next = nj;
				}
				freeLink(tile0, j);
				j = nj;
			}
			else
			{
				pj = j;
				j = link.next;
			}
		}
	}
}

void dtNavMesh::closestPointOnPolyInTile(const dtMeshTile* tile, unsigned int ip,
										 const float* pos, float* closest) const
{
	const dtPoly* poly = &tile->polys[ip];
	// Off-mesh connections don't have detail polygons.
	if (poly->getType() == DT_POLYTYPE_OFFMESH_POINT)
	{
		const float* v0 = &tile->verts[poly->verts[0]*3];
		const float* v1 = &tile->verts[poly->verts[1]*3];
		const float d0 = dtVdist(pos, v0);
		const float d1 = dtVdist(pos, v1);
		const float u = d0 / (d0+d1);
		dtVlerp(closest, v0, v1, u);
		return;
	}

	// Clamp point to be inside the polygon.
	float verts[DT_VERTS_PER_POLYGON*3];	
	float edged[DT_VERTS_PER_POLYGON];
	float edget[DT_VERTS_PER_POLYGON];
	const int nv = poly->vertCount;
	for (int i = 0; i < nv; ++i)
		dtVcopy(&verts[i*3], &tile->verts[poly->verts[i]*3]);
	
	dtVcopy(closest, pos);
	if (!dtDistancePtPolyEdgesSqr(pos, verts, nv, edged, edget))
	{
		// Point is outside the polygon, dtClamp to nearest edge.
		float dmin = FLT_MAX;
		int imin = -1;
		for (int i = 0; i < nv; ++i)
		{
			if (edged[i] < dmin)
			{
				dmin = edged[i];
				imin = i;
			}
		}
		const float* va = &verts[imin*3];
		const float* vb = &verts[((imin+1)%nv)*3];
		CA_SUPPRESS(6385);
		dtVlerp(closest, va, vb, edget[imin]);
	}
	
	// Find height at the location.
	if (poly->getType() == DT_POLYTYPE_GROUND)
	{
		const dtPolyDetail* pd = &tile->detailMeshes[ip];

		for (int j = 0; j < pd->triCount; ++j)
		{
			const unsigned char* t = &tile->detailTris[(pd->triBase+j)*4];
			const float* v[3];
			for (int k = 0; k < 3; ++k)
			{
				if (t[k] < poly->vertCount)
				{
					CA_SUPPRESS(6385);
					v[k] = &tile->verts[poly->verts[t[k]]*3];
				}
				else
				{
					v[k] = &tile->detailVerts[(pd->vertBase+(t[k]-poly->vertCount))*3];
				}
			}
			float h;
			if (dtClosestHeightPointTriangle(pos, v[0], v[1], v[2], h))
			{
				closest[1] = h;
				break;
			}
		}
	}
	else
	{
		float h;
		if (dtClosestHeightPointTriangle(closest, &verts[0], &verts[6], &verts[3], h))
		{
			closest[1] = h;
		}
		else if (dtClosestHeightPointTriangle(closest, &verts[3], &verts[6], &verts[9], h))
		{
			closest[1] = h;
		}
	}
}

dtPolyRef dtNavMesh::findNearestPolyInTile(const dtMeshTile* tile,
										   const float* center, const float* extents,
										   float* nearestPt, bool bExcludeUnwalkable) const
{
	dtAssert(nearestPt);

	float bmin[3], bmax[3];
	dtVsub(bmin, center, extents);
	dtVadd(bmax, center, extents);
	
	// Get nearby polygons from proximity grid.
	dtPolyRef polys[128];
	int polyCount = queryPolygonsInTile(tile, bmin, bmax, polys, 128, bExcludeUnwalkable);
	
	// Find nearest polygon amongst the nearby polygons.
	dtPolyRef nearest = 0;
	float nearestDistanceSqr = FLT_MAX;
	dtVcopy(nearestPt, center);
	for (int i = 0; i < polyCount; ++i)
	{
		dtPolyRef ref = polys[i];
		float closestPtPoly[3];
		closestPointOnPolyInTile(tile, decodePolyIdPoly(ref), center, closestPtPoly);
		float d = dtVdistSqr(center, closestPtPoly);
		if (d < nearestDistanceSqr)
		{
			dtVcopy(nearestPt, closestPtPoly);
			nearestDistanceSqr = d;
			nearest = ref;
		}
	}

	// Verify if the point is actually within requested height, caller is performing 2D check anyway (radius)
	if (dtAbs(nearestPt[1]-center[1]) > extents[1])
	{
		nearest = 0;
	}
	
	return nearest;
}

dtPolyRef dtNavMesh::findCheapestNearPolyInTile(const dtMeshTile* tile, const float* center,
												const float* extents, float* nearestPt) const
{
	dtAssert(nearestPt);

	float bmin[3], bmax[3];
	dtVsub(bmin, center, extents);
	dtVadd(bmax, center, extents);

	// Get nearby polygons from proximity grid.
	dtPolyRef polys[128];
	const bool bExcludeUnwalkable = true;
	int polyCount = queryPolygonsInTile(tile, bmin, bmax, polys, 128, bExcludeUnwalkable);

	// Find nearest polygon amongst the nearby polygons.
	dtPolyRef nearest = 0;
	float nearestDistanceSqr = FLT_MAX;
	unsigned char cheapestAreaCostOrder = 0xff;
	for (int i = 0; i < polyCount; ++i)
	{
		dtPolyRef ref = polys[i];
		
		const int polyIdx = decodePolyIdPoly(ref);
		dtPoly* poly = &tile->polys[polyIdx];
		const unsigned char polyAreaCostOrder = m_areaCostOrder[poly->getArea()];
		if (polyAreaCostOrder < cheapestAreaCostOrder)
		{
			cheapestAreaCostOrder = polyAreaCostOrder;
			nearestDistanceSqr = FLT_MAX;
			nearest = 0;
		}

		if (polyAreaCostOrder == cheapestAreaCostOrder)
		{
			float closestPtPoly[3];
			closestPointOnPolyInTile(tile, polyIdx, center, closestPtPoly);
			float d = dtVdistSqr(center, closestPtPoly);
			if (d < nearestDistanceSqr)
			{
				dtVcopy(nearestPt, closestPtPoly);
				nearestDistanceSqr = d;
				nearest = ref;
			}
		}
	}

	// Verify if the point is actually within requested height, caller is performing 2D check anyway (radius)
	if (dtAbs(nearestPt[1] - center[1]) > extents[1])
	{
		nearest = 0;
	}

	return nearest;
}

int dtNavMesh::queryPolygonsInTile(const dtMeshTile* tile, const float* qmin, const float* qmax,
									dtPolyRef* polys, const int maxPolys, bool bExcludeUnwalkable) const
{
	if (tile->bvTree)
	{
		const dtBVNode* node = &tile->bvTree[0];
		const dtBVNode* end = &tile->bvTree[tile->header->bvNodeCount];
		const float* tbmin = tile->header->bmin;
		const float* tbmax = tile->header->bmax;
		const float qfac = tile->header->bvQuantFactor;
		
		// Calculate quantized box
		unsigned short bmin[3], bmax[3];
		// dtClamp query box to world box.
		float minx = dtClamp(qmin[0], tbmin[0], tbmax[0]) - tbmin[0];
		float miny = dtClamp(qmin[1], tbmin[1], tbmax[1]) - tbmin[1];
		float minz = dtClamp(qmin[2], tbmin[2], tbmax[2]) - tbmin[2];
		float maxx = dtClamp(qmax[0], tbmin[0], tbmax[0]) - tbmin[0];
		float maxy = dtClamp(qmax[1], tbmin[1], tbmax[1]) - tbmin[1];
		float maxz = dtClamp(qmax[2], tbmin[2], tbmax[2]) - tbmin[2];
		// Quantize
		bmin[0] = (unsigned short)(qfac * minx) & 0xfffe;
		bmin[1] = (unsigned short)(qfac * miny) & 0xfffe;
		bmin[2] = (unsigned short)(qfac * minz) & 0xfffe;
		bmax[0] = (unsigned short)(qfac * maxx + 1) | 1;
		bmax[1] = (unsigned short)(qfac * maxy + 1) | 1;
		bmax[2] = (unsigned short)(qfac * maxz + 1) | 1;
		
		// Traverse tree
		dtPolyRef base = getPolyRefBase(tile);
		int n = 0;
		while (node < end)
		{
			const bool overlap = dtOverlapQuantBounds(bmin, bmax, node->bmin, node->bmax);
			const bool isLeafNode = node->i >= 0;
			
			if (isLeafNode && overlap)
			{
				if (n < maxPolys)
				{
					if (!bExcludeUnwalkable || tile->polys[node->i].flags)
					{
						polys[n++] = base | (dtPolyRef)node->i;
					}
				}
			}
			
			if (overlap || isLeafNode)
				node++;
			else
			{
				const int escapeIndex = -node->i;
				node += escapeIndex;
			}
		}
		
		return n;
	}
	else
	{
		float bmin[3], bmax[3];
		int n = 0;
		dtPolyRef base = getPolyRefBase(tile);
		for (int i = 0; i < tile->header->polyCount; ++i)
		{
			dtPoly* p = &tile->polys[i];
			// Do not return off-mesh connection polygons.
			if (p->getType() != DT_POLYTYPE_GROUND)
				continue;
			if (p->flags == 0 && bExcludeUnwalkable)
				continue;

			// Calc polygon bounds.
			const float* v = &tile->verts[p->verts[0]*3];
			dtVcopy(bmin, v);
			dtVcopy(bmax, v);
			for (int j = 1; j < p->vertCount; ++j)
			{
				v = &tile->verts[p->verts[j]*3];
				dtVmin(bmin, v);
				dtVmax(bmax, v);
			}
			if (dtOverlapBounds(qmin,qmax, bmin,bmax))
			{
				if (n < maxPolys)
					polys[n++] = base | (dtPolyRef)i;
			}
		}
		return n;
	}
}

/// @par
///
/// The add operation will fail if the data is in the wrong format, the allocated tile
/// space is full, or there is a tile already at the specified reference.
///
/// The lastRef parameter is used to restore a tile with the same tile
/// reference it had previously used.  In this case the #dtPolyRef's for the
/// tile will be restored to the same values they were before the tile was 
/// removed.
///
/// @see dtCreateNavMeshData, #removeTile
dtStatus dtNavMesh::addTile(unsigned char* data, int dataSize, int flags,
							dtTileRef lastRef, dtTileRef* result)
{
	// Make sure the data is in right format.
	dtMeshHeader* header = (dtMeshHeader*)data;
	if (header->magic != DT_NAVMESH_MAGIC)
		return DT_FAILURE | DT_WRONG_MAGIC;
	if (header->version != DT_NAVMESH_VERSION)
		return DT_FAILURE | DT_WRONG_VERSION;
		
	// Make sure the location is free.
	if (getTileAt(header->x, header->y, header->layer))
		return DT_FAILURE;
		
	// Allocate a tile.
	dtMeshTile* tile = 0;
	if (!lastRef)
	{
		if (m_nextFree)
		{
			tile = m_nextFree;
			m_nextFree = tile->next;
			tile->next = 0;
		}
	}
	else
	{
		// Try to relocate the tile to specific index with same salt.
		int tileIndex = (int)decodePolyIdTile((dtPolyRef)lastRef);
		if (tileIndex >= m_maxTiles)
			return DT_FAILURE | DT_OUT_OF_MEMORY;
		// Try to find the specific tile id from the free list.
		dtMeshTile* target = &m_tiles[tileIndex];
		dtMeshTile* prev = 0;
		tile = m_nextFree;
		while (tile && tile != target)
		{
			prev = tile;
			tile = tile->next;
		}
		// Could not find the correct location.
		if (tile != target)
			return DT_FAILURE | DT_OUT_OF_MEMORY;
		// Remove from freelist
		if (!prev)
			m_nextFree = tile->next;
		else
			prev->next = tile->next;

		// Restore salt.
		tile->salt = decodePolyIdSalt((dtPolyRef)lastRef);
	}

	// Make sure we could allocate a tile.
	if (!tile)
		return DT_FAILURE | DT_OUT_OF_MEMORY;
	
	// Insert tile into the position lut.
	int h = computeTileHash(header->x, header->y, m_tileLutMask);
	tile->next = m_posLookup[h];
	m_posLookup[h] = tile;
	
	// Patch header pointers.
	const int headerSize = dtAlign4(sizeof(dtMeshHeader));
	const int vertsSize = dtAlign4(sizeof(float)*3*header->vertCount);
	const int polysSize = dtAlign4(sizeof(dtPoly)*header->polyCount);
	const int linksSize = dtAlign4(sizeof(dtLink)*(header->maxLinkCount));
	const int detailMeshesSize = dtAlign4(sizeof(dtPolyDetail)*header->detailMeshCount);
	const int detailVertsSize = dtAlign4(sizeof(float)*3*header->detailVertCount);
	const int detailTrisSize = dtAlign4(sizeof(unsigned char)*4*header->detailTriCount);
	const int bvtreeSize = dtAlign4(sizeof(dtBVNode)*header->bvNodeCount);
	const int offMeshLinksSize = dtAlign4(sizeof(dtOffMeshConnection)*header->offMeshConCount);
	const int offMeshSegsSize = dtAlign4(sizeof(dtOffMeshSegmentConnection)*header->offMeshSegConCount);
	const int clustersSize = dtAlign4(sizeof(dtCluster)*header->clusterCount);
	const int clusterPolysSize = dtAlign4(sizeof(unsigned short)*header->offMeshBase);
 
	unsigned char* d = data + headerSize;
	tile->verts = (float*)d; d += vertsSize;
	tile->polys = (dtPoly*)d; d += polysSize;
	tile->links = (dtLink*)d; d += linksSize;
	tile->detailMeshes = (dtPolyDetail*)d; d += detailMeshesSize;
	tile->detailVerts = (float*)d; d += detailVertsSize;
	tile->detailTris = (unsigned char*)d; d += detailTrisSize;
	tile->bvTree = (dtBVNode*)d; d += bvtreeSize;
	tile->offMeshCons = (dtOffMeshConnection*)d; d += offMeshLinksSize;
	tile->offMeshSeg = (dtOffMeshSegmentConnection*)d; d += offMeshSegsSize;
	tile->clusters = (dtCluster*)d; d += clustersSize;
	tile->polyClusters = (unsigned short*)d; d += clusterPolysSize;

	// If there are no items in the bvtree, reset the tree pointer.
	if (!bvtreeSize)
		tile->bvTree = 0;

	const bool bHasClusters = header->clusterCount > 0;
	if (bHasClusters)
	{
		for (int i = 0; i < header->clusterCount; i++)
		{
			tile->clusters[i].numLinks = 0;
			tile->clusters[i].firstLink = DT_NULL_LINK;
		}
	}
	else
	{
		tile->polyClusters = 0;
	}

	// Build links freelist
	tile->linksFreeList = 0;
	tile->links[header->maxLinkCount-1].next = DT_NULL_LINK;
	for (int i = 0; i < header->maxLinkCount-1; ++i)
		tile->links[i].next = i+1;

	// Initialize dynamic links array
	tile->dynamicFreeListO = DT_NULL_LINK;
	tile->dynamicLinksO.resize(0);
	tile->dynamicFreeListC = DT_NULL_LINK;
	tile->dynamicLinksC.resize(0);

	// Init tile.
	tile->header = header;
	tile->data = data;
	tile->dataSize = dataSize;
	tile->flags = flags;

	connectIntLinks(tile);
	baseOffMeshLinks(tile);
	
	dtOffMeshSegmentData* segList = initSegmentIntersection(tile);

	// Create connections with neighbour tiles.
	ReadTilesHelper TileArray;
	int nneis = getTileCountAt(header->x, header->y);
	dtMeshTile** neis = TileArray.PrepareArray(nneis);
	
	// Connect with layers in current tile.
	getTilesAt(header->x, header->y, neis, nneis);
	for (int j = 0; j < nneis; ++j)
	{
		if (neis[j] != tile)
		{
			connectExtLinks(tile, neis[j], -1, bHasClusters);
			connectExtLinks(neis[j], tile, -1, bHasClusters);
			appendSegmentIntersection(segList, tile, neis[j]);
			connectExtOffMeshLinks(tile, neis[j], -1, bHasClusters);
		}
		connectExtOffMeshLinks(neis[j], tile, -1, bHasClusters);
	}
	
	// Connect with neighbour tiles.
	for (int i = 0; i < 8; ++i)
	{
		nneis = getNeighbourTilesCountAt(header->x, header->y, i);
		neis = TileArray.PrepareArray(nneis);

		getNeighbourTilesAt(header->x, header->y, i, neis, nneis);
		for (int j = 0; j < nneis; ++j)
		{
			connectExtLinks(tile, neis[j], i, bHasClusters);
			connectExtLinks(neis[j], tile, dtOppositeTile(i), bHasClusters);
			appendSegmentIntersection(segList, tile, neis[j]);
			connectExtOffMeshLinks(tile, neis[j], i, bHasClusters);
			connectExtOffMeshLinks(neis[j], tile, dtOppositeTile(i), bHasClusters);
		}
	}

	createSegmentLinks(this, segList, tile);
	dtFree(segList);
	
	if (result)
		*result = getTileRef(tile);
	
	return DT_SUCCESS;
}

const dtMeshTile* dtNavMesh::getTileAt(const int x, const int y, const int layer) const
{
	// Find tile based on hash.
	int h = computeTileHash(x,y,m_tileLutMask);
	dtMeshTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->header &&
			tile->header->x == x &&
			tile->header->y == y &&
			tile->header->layer == layer)
		{
			return tile;
		}
		tile = tile->next;
	}
	return 0;
}

int dtNavMesh::getNeighbourTilesAt(const int x, const int y, const int side, dtMeshTile** tiles, const int maxTiles) const
{
	int nx = x, ny = y;
	switch (side)
	{
		case 0: nx++; break;
		case 1: nx++; ny++; break;
		case 2: ny++; break;
		case 3: nx--; ny++; break;
		case 4: nx--; break;
		case 5: nx--; ny--; break;
		case 6: ny--; break;
		case 7: nx++; ny--; break;
	};

	return getTilesAt(nx, ny, (const dtMeshTile**)tiles, maxTiles);
}

// @UE4 BEGIN
int dtNavMesh::getNeighbourTilesCountAt(const int x, const int y, const int side) const
{
	int nx = x, ny = y;
	switch (side)
	{
		case 0: nx++; break;
		case 1: nx++; ny++; break;
		case 2: ny++; break;
		case 3: nx--; ny++; break;
		case 4: nx--; break;
		case 5: nx--; ny--; break;
		case 6: ny--; break;
		case 7: nx++; ny--; break;
	};

	return getTileCountAt(nx, ny);
}

int dtNavMesh::getTileCountAt(const int x, const int y) const
{
	int n = 0;

	// Find tile based on hash.
	int h = computeTileHash(x,y,m_tileLutMask);
	dtMeshTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->header &&
			tile->header->x == x &&
			tile->header->y == y)
		{
			n++;
		}
		tile = tile->next;
	}

	return n;
}
// @UE4 END

int dtNavMesh::getTilesAt(const int x, const int y, dtMeshTile** tiles, const int maxTiles) const
{
	int n = 0;
	
	// Find tile based on hash.
	int h = computeTileHash(x,y,m_tileLutMask);
	dtMeshTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->header &&
			tile->header->x == x &&
			tile->header->y == y)
		{
			if (n < maxTiles)
				tiles[n++] = tile;
		}
		tile = tile->next;
	}
	
	return n;
}

/// @par
///
/// This function will not fail if the tiles array is too small to hold the
/// entire result set.  It will simply fill the array to capacity.
int dtNavMesh::getTilesAt(const int x, const int y, dtMeshTile const** tiles, const int maxTiles) const
{
	int n = 0;
	
	// Find tile based on hash.
	int h = computeTileHash(x,y,m_tileLutMask);
	dtMeshTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->header &&
			tile->header->x == x &&
			tile->header->y == y)
		{
			if (n < maxTiles)
				tiles[n++] = tile;
		}
		tile = tile->next;
	}
	
	return n;
}


dtTileRef dtNavMesh::getTileRefAt(const int x, const int y, const int layer) const
{
	// Find tile based on hash.
	int h = computeTileHash(x,y,m_tileLutMask);
	dtMeshTile* tile = m_posLookup[h];
	while (tile)
	{
		if (tile->header &&
			tile->header->x == x &&
			tile->header->y == y &&
			tile->header->layer == layer)
		{
			return getTileRef(tile);
		}
		tile = tile->next;
	}
	return 0;
}

const dtMeshTile* dtNavMesh::getTileByRef(dtTileRef ref) const
{
	if (!ref)
		return 0;
	unsigned int tileIndex = decodePolyIdTile((dtPolyRef)ref);
	unsigned int tileSalt = decodePolyIdSalt((dtPolyRef)ref);
	if ((int)tileIndex >= m_maxTiles)
		return 0;
	const dtMeshTile* tile = &m_tiles[tileIndex];
	if (tile->salt != tileSalt)
		return 0;
	return tile;
}

int dtNavMesh::getMaxTiles() const
{
	return m_maxTiles;
}

dtMeshTile* dtNavMesh::getTile(int i)
{
	return &m_tiles[i];
}

const dtMeshTile* dtNavMesh::getTile(int i) const
{
	return &m_tiles[i];
}

void dtNavMesh::calcTileLoc(const float* pos, int* tx, int* ty) const
{
	*tx = (int)floorf((pos[0]-m_orig[0]) / m_tileWidth);
	*ty = (int)floorf((pos[2]-m_orig[2]) / m_tileHeight);
}

dtStatus dtNavMesh::getTileAndPolyByRef(const dtPolyRef ref, const dtMeshTile** tile, const dtPoly** poly) const
{
	if (!ref) return DT_FAILURE;
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return DT_FAILURE | DT_INVALID_PARAM;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return DT_FAILURE | DT_INVALID_PARAM;
	if (ip >= (unsigned int)m_tiles[it].header->polyCount) return DT_FAILURE | DT_INVALID_PARAM;
	*tile = &m_tiles[it];
	*poly = &m_tiles[it].polys[ip];
	return DT_SUCCESS;
}

/// @par
///
/// @warning Only use this function if it is known that the provided polygon
/// reference is valid. This function is faster than #getTileAndPolyByRef, but
/// it does not validate the reference.
void dtNavMesh::getTileAndPolyByRefUnsafe(const dtPolyRef ref, const dtMeshTile** tile, const dtPoly** poly) const
{
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	*tile = &m_tiles[it];
	*poly = &m_tiles[it].polys[ip];
}

bool dtNavMesh::isValidPolyRef(dtPolyRef ref) const
{
	if (!ref) return false;
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return false;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return false;
	if (ip >= (unsigned int)m_tiles[it].header->polyCount) return false;
	return true;
}

/// @par
///
/// This function returns the data for the tile so that, if desired,
/// it can be added back to the navigation mesh at a later point.
///
/// @see #addTile
dtStatus dtNavMesh::removeTile(dtTileRef ref, unsigned char** data, int* dataSize)
{
	if (!ref)
		return DT_FAILURE | DT_INVALID_PARAM;
	unsigned int tileIndex = decodePolyIdTile((dtPolyRef)ref);
	unsigned int tileSalt = decodePolyIdSalt((dtPolyRef)ref);
	if ((int)tileIndex >= m_maxTiles)
		return DT_FAILURE | DT_INVALID_PARAM;
	dtMeshTile* tile = &m_tiles[tileIndex];
	if (tile->salt != tileSalt)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	// Remove tile from hash lookup.
	int h = computeTileHash(tile->header->x,tile->header->y,m_tileLutMask);
	dtMeshTile* prev = 0;
	dtMeshTile* cur = m_posLookup[h];
	while (cur)
	{
		if (cur == tile)
		{
			if (prev)
				prev->next = cur->next;
			else
				m_posLookup[h] = cur->next;
			break;
		}
		prev = cur;
		cur = cur->next;
	}
	
	// Remove connections to neighbour tiles.
	// Create connections with neighbour tiles.
	ReadTilesHelper TileArray;
	int nneis = getTileCountAt(tile->header->x, tile->header->y);
	dtMeshTile** neis = TileArray.PrepareArray(nneis);
	
	// Connect with layers in current tile.
	getTilesAt(tile->header->x, tile->header->y, neis, nneis);
	for (int j = 0; j < nneis; ++j)
	{
		if (neis[j] == tile) continue;
		unconnectExtLinks(neis[j], tile);
	}
	
	// Connect with neighbour tiles.
	for (int i = 0; i < 8; ++i)
	{
		nneis = getNeighbourTilesCountAt(tile->header->x, tile->header->y, i);
		neis = TileArray.PrepareArray(nneis);

		getNeighbourTilesAt(tile->header->x, tile->header->y, i, neis, nneis);
		for (int j = 0; j < nneis; ++j)
			unconnectExtLinks(neis[j], tile);
	}

	// Whether caller wants to own tile data
	bool callerOwnsData = (data && dataSize);

	// Reset tile.
	if ((tile->flags & DT_TILE_FREE_DATA) && !callerOwnsData)
	{
		// Owns data
		dtFree(tile->data);
		tile->data = 0;
		tile->dataSize = 0;
		if (data) *data = 0;
		if (dataSize) *dataSize = 0;
	}
	else
	{
		if (data) *data = tile->data;
		if (dataSize) *dataSize = tile->dataSize;
	}

	tile->header = 0;
	tile->flags = 0;
	tile->linksFreeList = 0;
	tile->polys = 0;
	tile->verts = 0;
	tile->links = 0;
	tile->detailMeshes = 0;
	tile->detailVerts = 0;
	tile->detailTris = 0;
	tile->bvTree = 0;
	tile->offMeshCons = 0;

	// Update salt, salt should never be zero.
	tile->salt = (tile->salt+1) & ((1<<m_saltBits)-1);
	if (tile->salt == 0)
		tile->salt++;

	// Add to free list.
	tile->next = m_nextFree;
	m_nextFree = tile;

	return DT_SUCCESS;
}

dtTileRef dtNavMesh::getTileRef(const dtMeshTile* tile) const
{
	if (!tile) return 0;
	const unsigned int it = (unsigned int)(tile - m_tiles);
	return (dtTileRef)encodePolyId(tile->salt, it, 0);
}

/// @par
///
/// Example use case:
/// @code
///
/// const dtPolyRef base = navmesh->getPolyRefBase(tile);
/// for (int i = 0; i < tile->header->polyCount; ++i)
/// {
///     const dtPoly* p = &tile->polys[i];
///     const dtPolyRef ref = base | (dtPolyRef)i;
///     
///     // Use the reference to access the polygon data.
/// }
/// @endcode
dtPolyRef dtNavMesh::getPolyRefBase(const dtMeshTile* tile) const
{
	if (!tile) return 0;
	const unsigned int it = (unsigned int)(tile - m_tiles);
	return encodePolyId(tile->salt, it, 0);
}

dtClusterRef dtNavMesh::getClusterRefBase(const dtMeshTile* tile) const
{
	if (!tile) return 0;
	const unsigned int it = (unsigned int)(tile - m_tiles);
	return encodePolyId(tile->salt, it, 0);
}

struct dtTileState
{
	int magic;								// Magic number, used to identify the data.
	int version;							// Data version number.
	dtTileRef ref;							// Tile ref at the time of storing the data.
};

struct dtPolyState
{
	unsigned short flags;						// Flags (see dtPolyFlags).
	unsigned char area;							// Area ID of the polygon.
};

///  @see #storeTileState
int dtNavMesh::getTileStateSize(const dtMeshTile* tile) const
{
	if (!tile) return 0;
	const int headerSize = dtAlign4(sizeof(dtTileState));
	const int polyStateSize = dtAlign4(sizeof(dtPolyState) * tile->header->polyCount);
	return headerSize + polyStateSize;
}

/// @par
///
/// Tile state includes non-structural data such as polygon flags, area ids, etc.
/// @note The state data is only valid until the tile reference changes.
/// @see #getTileStateSize, #restoreTileState
dtStatus dtNavMesh::storeTileState(const dtMeshTile* tile, unsigned char* data, const int maxDataSize) const
{
	// Make sure there is enough space to store the state.
	const int sizeReq = getTileStateSize(tile);
	if (maxDataSize < sizeReq)
		return DT_FAILURE | DT_BUFFER_TOO_SMALL;
		
	dtTileState* tileState = (dtTileState*)data; data += dtAlign4(sizeof(dtTileState));
	dtPolyState* polyStates = (dtPolyState*)data; data += dtAlign4(sizeof(dtPolyState) * tile->header->polyCount);
	
	// Store tile state.
	tileState->magic = DT_NAVMESH_STATE_MAGIC;
	tileState->version = DT_NAVMESH_STATE_VERSION;
	tileState->ref = getTileRef(tile);
	
	// Store per poly state.
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		const dtPoly* p = &tile->polys[i];
		dtPolyState* s = &polyStates[i];
		s->flags = p->flags;
		s->area = p->getArea();
	}
	
	return DT_SUCCESS;
}

/// @par
///
/// Tile state includes non-structural data such as polygon flags, area ids, etc.
/// @note This function does not impact the tile's #dtTileRef and #dtPolyRef's.
/// @see #storeTileState
dtStatus dtNavMesh::restoreTileState(dtMeshTile* tile, const unsigned char* data, const int maxDataSize)
{
	// Make sure there is enough space to store the state.
	const int sizeReq = getTileStateSize(tile);
	if (maxDataSize < sizeReq)
		return DT_FAILURE | DT_INVALID_PARAM;
	
	const dtTileState* tileState = (const dtTileState*)data; data += dtAlign4(sizeof(dtTileState));
	const dtPolyState* polyStates = (const dtPolyState*)data; data += dtAlign4(sizeof(dtPolyState) * tile->header->polyCount);
	
	// Check that the restore is possible.
	if (tileState->magic != DT_NAVMESH_STATE_MAGIC)
		return DT_FAILURE | DT_WRONG_MAGIC;
	if (tileState->version != DT_NAVMESH_STATE_VERSION)
		return DT_FAILURE | DT_WRONG_VERSION;
	if (tileState->ref != getTileRef(tile))
		return DT_FAILURE | DT_INVALID_PARAM;
	
	// Restore per poly state.
	for (int i = 0; i < tile->header->polyCount; ++i)
	{
		dtPoly* p = &tile->polys[i];
		const dtPolyState* s = &polyStates[i];
		p->flags = s->flags;
		p->setArea(s->area);
	}
	
	return DT_SUCCESS;
}

/// @par
///
/// Off-mesh connections are stored in the navigation mesh as special 2-vertex 
/// polygons with a single edge. At least one of the vertices is expected to be 
/// inside a normal polygon. So an off-mesh connection is "entered" from a 
/// normal polygon at one of its endpoints. This is the polygon identified by 
/// the prevRef parameter.
dtStatus dtNavMesh::getOffMeshConnectionPolyEndPoints(dtPolyRef prevRef, dtPolyRef polyRef, const float* currentPos, float* startPos, float* endPos) const
{
	unsigned int salt, it, ip;

	if (!polyRef)
		return DT_FAILURE;
	
	// Get current polygon
	decodePolyId(polyRef, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return DT_FAILURE | DT_INVALID_PARAM;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return DT_FAILURE | DT_INVALID_PARAM;
	const dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return DT_FAILURE | DT_INVALID_PARAM;
	const dtPoly* poly = &tile->polys[ip];

	if (poly->getType() == DT_POLYTYPE_GROUND)
		return DT_FAILURE;

	// Figure out which way to hand out the vertices.
	int idx0 = 0, idx1 = 1;

	// Find link that points to first vertex.
	unsigned int i = poly->firstLink;
	while (i != DT_NULL_LINK)
	{
		const dtLink& link = getLink(tile, i);
		if (link.edge == 0)
		{
			if (link.ref != prevRef)
			{
				idx0 = 1;
				idx1 = 0;
			}
			break;
		}

		i = link.next;
	}
	
	if (poly->getType() == DT_POLYTYPE_OFFMESH_SEGMENT)
	{
		idx0 = (idx0 == 0) ? 0 : 2;
		idx1 = (idx1 == 1) ? 1 : 3;
		const int idx2 = (idx0 == 0) ? 2 : 0;
		const int idx3 = (idx1 == 1) ? 3 : 1;

		float start0[3], start1[3];
		dtVcopy(start0, &tile->verts[poly->verts[idx0]*3]);
		dtVcopy(start1, &tile->verts[poly->verts[idx1]*3]);
		float t = 0;
		dtDistancePtSegSqr2D(startPos, start0, start1, t);

		dtVlerp(startPos, start0, start1, t);
		dtVlerp(endPos, &tile->verts[poly->verts[idx2]*3], &tile->verts[poly->verts[idx3]*3], t);
	}
	else
	{
		dtVcopy(startPos, &tile->verts[poly->verts[idx0]*3]);
		dtVcopy(endPos, &tile->verts[poly->verts[idx1]*3]);
	}

	return DT_SUCCESS;
}


const dtOffMeshConnection* dtNavMesh::getOffMeshConnectionByRef(dtPolyRef ref) const
{
	unsigned int salt, it, ip;
	
	if (!ref)
		return 0;
	
	// Get current polygon
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return 0;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return 0;
	const dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return 0;
	const dtPoly* poly = &tile->polys[ip];
	
	// Make sure that the current poly is indeed off-mesh link.
	if (poly->getType() != DT_POLYTYPE_OFFMESH_POINT)
		return 0;

	const unsigned int idx =  ip - tile->header->offMeshBase;
	dtAssert(idx < (unsigned int)tile->header->offMeshConCount);
	return &tile->offMeshCons[idx];
}

const dtOffMeshSegmentConnection* dtNavMesh::getOffMeshSegmentConnectionByRef(dtPolyRef ref) const
{
	unsigned int salt, it, ip;

	if (!ref)
		return 0;

	// Get current polygon
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return 0;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return 0;
	const dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return 0;
	const dtPoly* poly = &tile->polys[ip];

	// Make sure that the current poly is indeed off-mesh link.
	if (poly->getType() != DT_POLYTYPE_OFFMESH_SEGMENT)
		return 0;

	const unsigned int idx = (ip - tile->header->offMeshSegPolyBase) / DT_MAX_OFFMESH_SEGMENT_PARTS;
	dtAssert(idx < (unsigned int)tile->header->offMeshSegConCount);
	return &tile->offMeshSeg[idx];
}

void dtNavMesh::updateOffMeshConnectionByUserId(unsigned int userId, unsigned char newArea, unsigned short newFlags)
{
	for (int it = 0; it < m_maxTiles; it++)
	{
		dtMeshTile* tile = &m_tiles[it];
		if (tile == 0 || tile->header == 0)
			continue;

		for (int ic = 0; ic < tile->header->offMeshConCount; ic++)
		{
			dtOffMeshConnection& con = tile->offMeshCons[ic];
			if (con.userId == userId)
			{
				dtPoly* poly = &tile->polys[con.poly];
				poly->setArea(newArea);
				poly->flags = newFlags;
			}
		}
	}
}

void dtNavMesh::updateOffMeshSegmentConnectionByUserId(unsigned int userId, unsigned char newArea, unsigned short newFlags)
{
	for (int it = 0; it < m_maxTiles; it++)
	{
		dtMeshTile* tile = &m_tiles[it];
		if (tile == 0 || tile->header == 0)
			continue;

		for (int ic = 0; ic < tile->header->offMeshSegConCount; ic++)
		{
			dtOffMeshSegmentConnection& con = tile->offMeshSeg[ic];
			if (con.userId == userId)
			{
				for (int ip = 0; ip < con.npolys; ip++)
				{
					dtPoly* poly = &tile->polys[tile->header->offMeshSegPolyBase + con.firstPoly + ip];
					poly->setArea(newArea);
					poly->flags = newFlags;
				}
			}
		}
	}
}

dtStatus dtNavMesh::setPolyFlags(dtPolyRef ref, unsigned short flags)
{
	if (!ref) return DT_FAILURE;
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return DT_FAILURE | DT_INVALID_PARAM;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return DT_FAILURE | DT_INVALID_PARAM;
	dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return DT_FAILURE | DT_INVALID_PARAM;
	dtPoly* poly = &tile->polys[ip];
	
	// Change flags.
	poly->flags = flags;
	
	return DT_SUCCESS;
}

dtStatus dtNavMesh::getPolyFlags(dtPolyRef ref, unsigned short* resultFlags) const
{
	if (!ref) return DT_FAILURE;
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return DT_FAILURE | DT_INVALID_PARAM;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return DT_FAILURE | DT_INVALID_PARAM;
	const dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return DT_FAILURE | DT_INVALID_PARAM;
	const dtPoly* poly = &tile->polys[ip];

	*resultFlags = poly->flags;
	
	return DT_SUCCESS;
}

dtStatus dtNavMesh::setPolyArea(dtPolyRef ref, unsigned char area)
{
	if (!ref) return DT_FAILURE;
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return DT_FAILURE | DT_INVALID_PARAM;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return DT_FAILURE | DT_INVALID_PARAM;
	dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return DT_FAILURE | DT_INVALID_PARAM;
	dtPoly* poly = &tile->polys[ip];
	
	poly->setArea(area);
	
	return DT_SUCCESS;
}

dtStatus dtNavMesh::getPolyArea(dtPolyRef ref, unsigned char* resultArea) const
{
	if (!ref) return DT_FAILURE;
	unsigned int salt, it, ip;
	decodePolyId(ref, salt, it, ip);
	if (it >= (unsigned int)m_maxTiles) return DT_FAILURE | DT_INVALID_PARAM;
	if (m_tiles[it].salt != salt || m_tiles[it].header == 0) return DT_FAILURE | DT_INVALID_PARAM;
	const dtMeshTile* tile = &m_tiles[it];
	if (ip >= (unsigned int)tile->header->polyCount) return DT_FAILURE | DT_INVALID_PARAM;
	const dtPoly* poly = &tile->polys[ip];
	
	*resultArea = poly->getArea();
	
	return DT_SUCCESS;
}

//@UE4 BEGIN 
void dtNavMesh::applyWorldOffset(const float* offset)
{
	//Shift navmesh origin
	dtVadd(m_params.orig, m_params.orig, offset);
	dtVadd(m_orig, m_orig, offset);
						
	// Iterate over all tiles and apply provided offset
	for (int i = 0; i < m_maxTiles; ++i)
	{
		dtMeshTile& tile = m_tiles[i];
		if (tile.header != NULL)
		{
			// Shift tile bounds
			dtVadd(tile.header->bmin, tile.header->bmin, offset);
			dtVadd(tile.header->bmax, tile.header->bmax, offset);
			
			//Shift tile vertices
			for (int j = 0; j < tile.header->vertCount; ++j)
			{
				dtVadd(&(tile.verts[j*3]), &(tile.verts[j*3]), offset);
			}
			
			//Shift tile details vertices
			for (int j = 0; j < tile.header->detailVertCount; ++j)
			{
				dtVadd(&(tile.detailVerts[j*3]), &(tile.detailVerts[j*3]), offset);
			}

			//Shift off-mesh connections
			for (int j = 0; j < tile.header->offMeshConCount; ++j)
			{
				dtVadd(&(tile.offMeshCons[j].pos[0]), &(tile.offMeshCons[j].pos[0]), offset);
				dtVadd(&(tile.offMeshCons[j].pos[3]), &(tile.offMeshCons[j].pos[3]), offset);
			}
			
			// Shift clusters
			for (int j = 0; j < tile.header->clusterCount; ++j)
			{
				dtVadd(&(tile.clusters[j].center[0]), &(tile.clusters[j].center[0]), offset);
			}
		}
	}
}

void dtNavMesh::applyAreaCostOrder(unsigned char* costOrder)
{
	memcpy(m_areaCostOrder, costOrder, sizeof(m_areaCostOrder));
}
//@UE4 END
