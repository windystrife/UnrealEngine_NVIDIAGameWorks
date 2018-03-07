// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	NavMeshRenderingComponent.cpp: A component that renders a nav mesh.
 =============================================================================*/

#include "AI/Navigation/NavMeshRenderingComponent.h"
#include "EngineGlobals.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Canvas.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "AI/NavigationOctree.h"
#include "AI/Navigation/RecastHelpers.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/RecastNavMeshGenerator.h"
#include "Debug/DebugDrawService.h"
#include "SceneManagement.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

static const FColor NavMeshRenderColor_Recast_TriangleEdges(255, 255, 255);
static const FColor NavMeshRenderColor_Recast_TileEdges(16, 16, 16, 32);
static const FColor NavMeshRenderColor_Recast_NavMeshEdges(32, 63, 0, 220);
static const FColor NavMeshRenderColor_RecastMesh(140, 255, 0, 164);
static const FColor NavMeshRenderColor_TileBounds(255, 255, 64, 255);
static const FColor NavMeshRenderColor_PathCollidingGeom(255, 255, 255, 40);
static const FColor NavMeshRenderColor_RecastTileBeingRebuilt(255, 0, 0, 64);
static const FColor NavMeshRenderColor_OffMeshConnectionInvalid(64, 64, 64);

static const float DefaultEdges_LineThickness = 0.0f;
static const float PolyEdges_LineThickness = 1.5f;
static const float NavMeshEdges_LineThickness = 3.5f;
static const float LinkLines_LineThickness = 2.0f;
static const float ClusterLinkLines_LineThickness = 2.0f;

namespace FNavMeshRenderingHelpers
{
	bool LineInView(const FVector& Start, const FVector& End, const FSceneView* View, bool bUseDistanceCheck)
	{
		if (FVector::DistSquared(Start, View->ViewMatrices.GetViewOrigin()) > ARecastNavMesh::GetDrawDistanceSq() ||
			FVector::DistSquared(End, View->ViewMatrices.GetViewOrigin()) > ARecastNavMesh::GetDrawDistanceSq())
		{
			return false;
		}

		for (int32 PlaneIdx = 0; PlaneIdx < View->ViewFrustum.Planes.Num(); ++PlaneIdx)
		{
			const FPlane& CurPlane = View->ViewFrustum.Planes[PlaneIdx];
			if (CurPlane.PlaneDot(Start) > 0.f && CurPlane.PlaneDot(End) > 0.f)
			{
				return false;
			}
		}

		return true;
	}

	bool LineInCorrectDistance(const FVector& Start, const FVector& End, const FSceneView* View, float CorrectDistance = -1)
	{
		const float MaxDistanceSq = (CorrectDistance > 0) ? FMath::Square(CorrectDistance) : ARecastNavMesh::GetDrawDistanceSq();
		return FVector::DistSquared(Start, View->ViewMatrices.GetViewOrigin()) < MaxDistanceSq &&
			FVector::DistSquared(End, View->ViewMatrices.GetViewOrigin()) < MaxDistanceSq;
	}

	FVector EvalArc(const FVector& Org, const FVector& Dir, const float h, const float u)
	{
		FVector Pt = Org + Dir * u;
		Pt.Z += h * (1 - (u * 2 - 1)*(u * 2 - 1));

		return Pt;
	}

	void CacheArc(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Start, const FVector& End, const float Height, const uint32 Segments, const FLinearColor& Color, float LineThickness = 0)
	{
		if (Segments == 0)
		{
			return;
		}

		const float ArcPtsScale = 1.0f / (float)Segments;
		const FVector Dir = End - Start;
		const float Length = Dir.Size();

		FVector Prev = Start;
		for (uint32 i = 1; i <= Segments; ++i)
		{
			const float u = i * ArcPtsScale;
			const FVector Pt = EvalArc(Start, Dir, Length*Height, u);

			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Prev, Pt, Color.ToFColor(true)));
			Prev = Pt;
		}
	}

	void CacheArrowHead(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Tip, const FVector& Origin, const float Size, const FLinearColor& Color, float LineThickness = 0)
	{
		FVector Ax, Ay, Az(0, 1, 0);
		Ay = Origin - Tip;
		Ay.Normalize();
		Ax = FVector::CrossProduct(Az, Ay);

		FHitProxyId HitProxyId;
		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Tip, FVector(Tip.X + Ay.X*Size + Ax.X*Size / 3, Tip.Y + Ay.Y*Size + Ax.Y*Size / 3, Tip.Z + Ay.Z*Size + Ax.Z*Size / 3), Color.ToFColor(true)));
		DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(Tip, FVector(Tip.X + Ay.X*Size - Ax.X*Size / 3, Tip.Y + Ay.Y*Size - Ax.Y*Size / 3, Tip.Z + Ay.Z*Size - Ax.Z*Size / 3), Color.ToFColor(true)));
	}

	void DrawWireCylinder(TArray<FDebugRenderSceneProxy::FDebugLine>& DebugLines, const FVector& Base, const FVector& X, const FVector& Y, const FVector& Z, FColor Color, float Radius, float HalfHeight, int32 NumSides, uint8 DepthPriority, float LineThickness = 0)
	{
		const float	AngleDelta = 2.0f * PI / NumSides;
		FVector	LastVertex = Base + X * Radius;

		FHitProxyId HitProxyId;
		for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
		{
			const FVector Vertex = Base + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;

			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex - Z * HalfHeight, Vertex - Z * HalfHeight, Color));
			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex + Z * HalfHeight, Vertex + Z * HalfHeight, Color));
			DebugLines.Add(FDebugRenderSceneProxy::FDebugLine(LastVertex - Z * HalfHeight, LastVertex + Z * HalfHeight, Color));

			LastVertex = Vertex;
		}
	}

	inline uint8 GetBit(int32 v, uint8 bit)
	{
		return (v & (1 << bit)) >> bit;
	}

	FColor GetClusterColor(int32 Idx)
	{
		uint8 r = 1 + GetBit(Idx, 1) + GetBit(Idx, 3) * 2;
		uint8 g = 1 + GetBit(Idx, 2) + GetBit(Idx, 4) * 2;
		uint8 b = 1 + GetBit(Idx, 0) + GetBit(Idx, 5) * 2;
		return FColor(r * 63, g * 63, b * 63, 164);
	}

	FColor DarkenColor(const FColor& Base)
	{
		const uint32 Col = Base.DWColor();
		return FColor(((Col >> 1) & 0x007f7f7f) | (Col & 0xff000000));
	}

	void AddVertex(FNavMeshSceneProxyData::FDebugMeshData& MeshData, const FVector& Pos, const FColor Color)
	{
		const int32 VertexIndex = MeshData.Vertices.Num();
		FDynamicMeshVertex* Vertex = new(MeshData.Vertices) FDynamicMeshVertex;
		Vertex->Position = Pos;
		Vertex->TextureCoordinate = FVector2D::ZeroVector;
		Vertex->TangentX = FVector(1.0f, 0.0f, 0.0f);
		Vertex->TangentZ = FVector(0.0f, 1.0f, 0.0f);
		// store the sign of the determinant in TangentZ.W (-1=0,+1=255)
		Vertex->TangentZ.Vector.W = 255;
		Vertex->Color = Color;
	}

	void AddTriangle(FNavMeshSceneProxyData::FDebugMeshData& MeshData, int32 V0, int32 V1, int32 V2)
	{
		MeshData.Indices.Add(V0);
		MeshData.Indices.Add(V1);
		MeshData.Indices.Add(V2);
	}

	void AddRecastGeometry(TArray<FVector>& OutVertexBuffer, TArray<int32>& OutIndexBuffer, const float* Coords, int32 NumVerts, const int32* Faces, int32 NumFaces)
	{
		const int32 VertIndexBase = OutVertexBuffer.Num();
		for (int32 VertIdx = 0; VertIdx < NumVerts * 3; VertIdx += 3)
		{
			OutVertexBuffer.Add(Recast2UnrealPoint(&Coords[VertIdx]));
		}

		const int32 FirstNewFaceVertexIndex = OutIndexBuffer.Num();
		const uint32 NumIndices = NumFaces * 3;
		OutIndexBuffer.AddUninitialized(NumIndices);
		for (uint32 Index = 0; Index < NumIndices; ++Index)
		{
			OutIndexBuffer[FirstNewFaceVertexIndex + Index] = VertIndexBase + Faces[Index];
		}
	}

	inline bool HasFlag(int32 Flags, ENavMeshDetailFlags TestFlag)
	{
		return (Flags & (1 << static_cast<int32>(TestFlag))) != 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// FNavMeshSceneProxyData

void FNavMeshSceneProxyData::Reset()
{
	MeshBuilders.Reset();
	ThickLineItems.Reset();
	TileEdgeLines.Reset();
	NavMeshEdgeLines.Reset();
	NavLinkLines.Reset();
	ClusterLinkLines.Reset();
	DebugLabels.Reset();
	PathCollidingGeomIndices.Reset();
	PathCollidingGeomVerts.Reset();
	OctreeBounds.Reset();
	Bounds.Init();

	bNeedsNewData = true;
	bDataGathered = false;
	NavDetailFlags = 0;
}

void FNavMeshSceneProxyData::Serialize(FArchive& Ar)
{
	int32 NumMeshBuilders = MeshBuilders.Num();
	Ar << NumMeshBuilders;
	if (Ar.IsLoading())
	{
		MeshBuilders.SetNum(NumMeshBuilders);
	}

	for (int32 Idx = 0; Idx < NumMeshBuilders; Idx++)
	{
		FDebugMeshData& MeshBuilder = MeshBuilders[Idx];

		int32 NumVerts = MeshBuilder.Vertices.Num();
		Ar << NumVerts;
		if (Ar.IsLoading())
		{
			MeshBuilder.Vertices.SetNum(NumVerts);
		}

		for (int32 VertIdx = 0; VertIdx < NumVerts; VertIdx++)
		{
			FVector SerializedVert = MeshBuilder.Vertices[VertIdx].Position;
			Ar << SerializedVert;

			if (Ar.IsLoading())
			{
				MeshBuilder.Vertices[VertIdx] = FDynamicMeshVertex(SerializedVert);
			}
		}

		Ar << MeshBuilder.Indices;
		Ar << MeshBuilder.ClusterColor;
	}

	TArray<FDebugRenderSceneProxy::FDebugLine>* LineArraysToSerialize[] = { &ThickLineItems, &TileEdgeLines, &NavMeshEdgeLines, &NavLinkLines, &ClusterLinkLines };
	for (int32 ArrIdx = 0; ArrIdx < ARRAY_COUNT(LineArraysToSerialize); ArrIdx++)
	{
		int32 NumItems = LineArraysToSerialize[ArrIdx]->Num();
		Ar << NumItems;
		if (Ar.IsLoading())
		{
			LineArraysToSerialize[ArrIdx]->Reset(NumItems);
			LineArraysToSerialize[ArrIdx]->AddUninitialized(NumItems);
		}

		for (int32 Idx = 0; Idx < NumItems; Idx++)
		{
			Ar << (*LineArraysToSerialize[ArrIdx])[Idx].Thickness;
			Ar << (*LineArraysToSerialize[ArrIdx])[Idx].Start;
			Ar << (*LineArraysToSerialize[ArrIdx])[Idx].End;
			Ar << (*LineArraysToSerialize[ArrIdx])[Idx].Color;
		}
	}

	int32 NumLabels = DebugLabels.Num();
	Ar << NumLabels;
	if (Ar.IsLoading())
	{
		DebugLabels.SetNum(NumLabels);
	}

	for (int32 Idx = 0; Idx < NumLabels; Idx++)
	{
		Ar << DebugLabels[Idx].Location;
		Ar << DebugLabels[Idx].Text;
	}

	Ar << PathCollidingGeomIndices;

	int32 NumVerts = PathCollidingGeomVerts.Num();
	Ar << NumVerts;
	if (Ar.IsLoading())
	{
		PathCollidingGeomVerts.SetNum(NumVerts);
	}

	for (int32 Idx = 0; Idx < NumVerts; Idx++)
	{
		FVector SerializedVert = PathCollidingGeomVerts[Idx].Position;
		Ar << SerializedVert;

		if (Ar.IsLoading())
		{
			PathCollidingGeomVerts[Idx] = FDynamicMeshVertex(SerializedVert);
		}
	}

	int32 NumBounds = OctreeBounds.Num();
	Ar << NumBounds;
	if (Ar.IsLoading())
	{
		OctreeBounds.SetNum(NumBounds);
	}

	for (int32 Idx = 0; Idx < NumBounds; Idx++)
	{
		Ar << OctreeBounds[Idx].Center;
		Ar << OctreeBounds[Idx].Extent;
	}

	Ar << Bounds;
	Ar << NavMeshDrawOffset;
	Ar << NavDetailFlags;

	int32 BitFlags = ((bDataGathered ? 1 : 0) << 0) | ((bNeedsNewData ? 1 : 0) << 1);
	Ar << BitFlags;
	bDataGathered = (BitFlags & (1 << 0)) != 0;
	bNeedsNewData = (BitFlags & (1 << 1)) != 0;
}

uint32 FNavMeshSceneProxyData::GetAllocatedSize() const
{
	return MeshBuilders.GetAllocatedSize() +
		ThickLineItems.GetAllocatedSize() +
		TileEdgeLines.GetAllocatedSize() +
		NavMeshEdgeLines.GetAllocatedSize() +
		NavLinkLines.GetAllocatedSize() +
		ClusterLinkLines.GetAllocatedSize() +
		DebugLabels.GetAllocatedSize() +
		PathCollidingGeomIndices.GetAllocatedSize() +
		PathCollidingGeomVerts.GetAllocatedSize() +
		OctreeBounds.GetAllocatedSize();
}

#if WITH_RECAST

int32 FNavMeshSceneProxyData::GetDetailFlags(const ARecastNavMesh* NavMesh) const
{
	return (NavMesh == nullptr) ? 0 : 0 |
		(NavMesh->bDrawTriangleEdges ? (1 << static_cast<int32>(ENavMeshDetailFlags::TriangleEdges)) : 0) |
		(NavMesh->bDrawPolyEdges ? (1 << static_cast<int32>(ENavMeshDetailFlags::PolyEdges)) : 0) |
		(NavMesh->bDrawFilledPolys ? (1 << static_cast<int32>(ENavMeshDetailFlags::FilledPolys)) : 0) |
		(NavMesh->bDrawNavMeshEdges ? (1 << static_cast<int32>(ENavMeshDetailFlags::BoundaryEdges)) : 0) |
		(NavMesh->bDrawTileBounds ? (1 << static_cast<int32>(ENavMeshDetailFlags::TileBounds)) : 0) |
		(NavMesh->bDrawPathCollidingGeometry ? (1 << static_cast<int32>(ENavMeshDetailFlags::PathCollidingGeometry)) : 0) |
		(NavMesh->bDrawTileLabels ? (1 << static_cast<int32>(ENavMeshDetailFlags::TileLabels)) : 0) |
		(NavMesh->bDrawPolygonLabels ? (1 << static_cast<int32>(ENavMeshDetailFlags::PolygonLabels)) : 0) |
		(NavMesh->bDrawDefaultPolygonCost ? (1 << static_cast<int32>(ENavMeshDetailFlags::PolygonCost)) : 0) |
		(NavMesh->bDrawLabelsOnPathNodes ? (1 << static_cast<int32>(ENavMeshDetailFlags::PathLabels)) : 0) |
		(NavMesh->bDrawNavLinks ? (1 << static_cast<int32>(ENavMeshDetailFlags::NavLinks)) : 0) |
		(NavMesh->bDrawFailedNavLinks ? (1 << static_cast<int32>(ENavMeshDetailFlags::FailedNavLinks)) : 0) |
		(NavMesh->bDrawClusters ? (1 << static_cast<int32>(ENavMeshDetailFlags::Clusters)) : 0) |
		(NavMesh->bDrawOctree ? (1 << static_cast<int32>(ENavMeshDetailFlags::NavOctree)) : 0);
}

void FNavMeshSceneProxyData::GatherData(const ARecastNavMesh* NavMesh, int32 InNavDetailFlags, const TArray<int32>& TileSet)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NavMesh_GatherDebugDrawingGeometry);
	Reset();

	NavDetailFlags = InNavDetailFlags;
	if (NavMesh && NavDetailFlags)
	{
		bNeedsNewData = false;
		bDataGathered = true;

		FHitProxyId HitProxyId = FHitProxyId();
		NavMeshDrawOffset.Z = NavMesh->DrawOffset;

		FRecastDebugGeometry NavMeshGeometry;
		NavMeshGeometry.bGatherPolyEdges = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::PolyEdges);
		NavMeshGeometry.bGatherNavMeshEdges = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::BoundaryEdges);

		const FNavDataConfig& NavConfig = NavMesh->GetConfig();
		TArray<FColor> NavMeshColors;
		NavMeshColors.AddDefaulted(RECAST_MAX_AREAS);

		for (uint8 Idx = 0; Idx < RECAST_MAX_AREAS; Idx++)
		{
			NavMeshColors[Idx] = NavMesh->GetAreaIDColor(Idx);
		}
		NavMeshColors[RECAST_DEFAULT_AREA] = NavConfig.Color.DWColor() > 0 ? NavConfig.Color : NavMeshRenderColor_RecastMesh;

		// just a little trick to make sure navmeshes with different sized are not drawn with same offset
		NavMeshDrawOffset.Z += NavMesh->GetConfig().AgentRadius / 10.f;

		NavMesh->BeginBatchQuery();
		if (TileSet.Num() > 0)
		{
			for (int32 Idx = 0; Idx < TileSet.Num(); Idx++)
			{
				NavMesh->GetDebugGeometry(NavMeshGeometry, TileSet[Idx]);
			}
		}
		else
		{
			NavMesh->GetDebugGeometry(NavMeshGeometry);
		}

		const TArray<FVector>& MeshVerts = NavMeshGeometry.MeshVerts;

		// @fixme, this is going to double up on lots of interior lines
		const bool bGatherTriEdges = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::TriangleEdges);
		if (bGatherTriEdges)
		{
			for (int32 AreaIdx = 0; AreaIdx < RECAST_MAX_AREAS; ++AreaIdx)
			{
				const TArray<int32>& MeshIndices = NavMeshGeometry.AreaIndices[AreaIdx];
				for (int32 Idx = 0; Idx < MeshIndices.Num(); Idx += 3)
				{
					ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(MeshVerts[MeshIndices[Idx + 0]] + NavMeshDrawOffset, MeshVerts[MeshIndices[Idx + 1]] + NavMeshDrawOffset, NavMeshRenderColor_Recast_TriangleEdges, DefaultEdges_LineThickness));
					ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(MeshVerts[MeshIndices[Idx + 1]] + NavMeshDrawOffset, MeshVerts[MeshIndices[Idx + 2]] + NavMeshDrawOffset, NavMeshRenderColor_Recast_TriangleEdges, DefaultEdges_LineThickness));
					ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(MeshVerts[MeshIndices[Idx + 2]] + NavMeshDrawOffset, MeshVerts[MeshIndices[Idx + 0]] + NavMeshDrawOffset, NavMeshRenderColor_Recast_TriangleEdges, DefaultEdges_LineThickness));
				}
			}
		}

		// make lines for tile edges
		const bool bGatherPolyEdges = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::PolyEdges);
		if (bGatherPolyEdges)
		{
			const TArray<FVector>& TileEdgeVerts = NavMeshGeometry.PolyEdges;
			for (int32 Idx = 0; Idx < TileEdgeVerts.Num(); Idx += 2)
			{
				TileEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(TileEdgeVerts[Idx] + NavMeshDrawOffset, TileEdgeVerts[Idx + 1] + NavMeshDrawOffset, NavMeshRenderColor_Recast_TileEdges));
			}
		}

		// make lines for navmesh edges
		const bool bGatherBoundaryEdges = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::BoundaryEdges);
		if (bGatherBoundaryEdges)
		{
			const FColor EdgesColor = FNavMeshRenderingHelpers::DarkenColor(NavMeshColors[RECAST_DEFAULT_AREA]);
			const TArray<FVector>& NavMeshEdgeVerts = NavMeshGeometry.NavMeshEdges;
			for (int32 Idx = 0; Idx < NavMeshEdgeVerts.Num(); Idx += 2)
			{
				NavMeshEdgeLines.Add(FDebugRenderSceneProxy::FDebugLine(NavMeshEdgeVerts[Idx] + NavMeshDrawOffset, NavMeshEdgeVerts[Idx + 1] + NavMeshDrawOffset, EdgesColor));
			}
		}

		// offset all navigation-link positions
		const bool bGatherClusters = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::Clusters);
		const bool bGatherNavLinks = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::NavLinks);
		const bool bGatherFailedNavLinks = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::FailedNavLinks);

		if (bGatherClusters == false)
		{
			for (int32 OffMeshLineIndex = 0; OffMeshLineIndex < NavMeshGeometry.OffMeshLinks.Num(); ++OffMeshLineIndex)
			{
				FRecastDebugGeometry::FOffMeshLink& Link = NavMeshGeometry.OffMeshLinks[OffMeshLineIndex];
				const bool bLinkValid = (Link.ValidEnds & FRecastDebugGeometry::OMLE_Left) && (Link.ValidEnds & FRecastDebugGeometry::OMLE_Right);

				if (bGatherFailedNavLinks || (bGatherNavLinks && bLinkValid))
				{
					const FVector V0 = Link.Left + NavMeshDrawOffset;
					const FVector V1 = Link.Right + NavMeshDrawOffset;
					const FColor LinkColor = ((Link.Direction && Link.ValidEnds) || (Link.ValidEnds & FRecastDebugGeometry::OMLE_Left)) ? FNavMeshRenderingHelpers::DarkenColor(NavMeshColors[Link.AreaID]) : NavMeshRenderColor_OffMeshConnectionInvalid;

					FNavMeshRenderingHelpers::CacheArc(NavLinkLines, V0, V1, 0.4f, 4, LinkColor, LinkLines_LineThickness);

					const FVector VOffset(0, 0, FVector::Dist(V0, V1) * 1.333f);
					FNavMeshRenderingHelpers::CacheArrowHead(NavLinkLines, V1, V0 + VOffset, 30.f, LinkColor, LinkLines_LineThickness);
					if (Link.Direction)
					{
						FNavMeshRenderingHelpers::CacheArrowHead(NavLinkLines, V0, V1 + VOffset, 30.f, LinkColor, LinkLines_LineThickness);
					}

					// if the connection as a whole is valid check if there are any of ends is invalid
					if (LinkColor != NavMeshRenderColor_OffMeshConnectionInvalid)
					{
						if (Link.Direction && (Link.ValidEnds & FRecastDebugGeometry::OMLE_Left) == 0)
						{
							// left end invalid - mark it
							FNavMeshRenderingHelpers::DrawWireCylinder(NavLinkLines, V0, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), NavMeshRenderColor_OffMeshConnectionInvalid, Link.Radius, NavMesh->AgentMaxStepHeight, 16, 0, DefaultEdges_LineThickness);
						}
						if ((Link.ValidEnds & FRecastDebugGeometry::OMLE_Right) == 0)
						{
							FNavMeshRenderingHelpers::DrawWireCylinder(NavLinkLines, V1, FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), NavMeshRenderColor_OffMeshConnectionInvalid, Link.Radius, NavMesh->AgentMaxStepHeight, 16, 0, DefaultEdges_LineThickness);
						}
					}
				}
			}
		}

		const bool bGatherTileLabels = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::TileLabels);
		const bool bGatherTileBounds = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::TileBounds);
		const bool bGatherPolygonLabels = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::PolygonLabels);
		const bool bGatherPolygonCost = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::PolygonCost);

		if (bGatherTileLabels || bGatherTileBounds || bGatherPolygonLabels || bGatherPolygonCost)
		{
			TArray<int32> UseTileIndices;
			if (TileSet.Num() > 0)
			{
				UseTileIndices = TileSet;
			}
			else
			{
				const int32 TilesCount = NavMesh->GetNavMeshTilesCount();
				for (int32 Idx = 0; Idx < TilesCount; Idx++)
				{
					UseTileIndices.Add(Idx);
				}
			}

			// calculate appropriate points for displaying debug labels
			DebugLabels.Reserve(UseTileIndices.Num());
			for (int32 TileSetIdx = 0; TileSetIdx < UseTileIndices.Num(); TileSetIdx++)
			{
				const int32 TileIndex = UseTileIndices[TileSetIdx];
				int32 X, Y, Layer;
				if (NavMesh->GetNavMeshTileXY(TileIndex, X, Y, Layer))
				{
					const FBox TileBoundingBox = NavMesh->GetNavMeshTileBounds(TileIndex);
					FVector TileLabelLocation = TileBoundingBox.GetCenter();
					TileLabelLocation.Z = TileBoundingBox.Max.Z;

					FNavLocation NavLocation(TileLabelLocation);
					if (!NavMesh->ProjectPoint(TileLabelLocation, NavLocation, FVector(NavMesh->TileSizeUU / 100, NavMesh->TileSizeUU / 100, TileBoundingBox.Max.Z - TileBoundingBox.Min.Z)))
					{
						NavMesh->ProjectPoint(TileLabelLocation, NavLocation, FVector(NavMesh->TileSizeUU / 2, NavMesh->TileSizeUU / 2, TileBoundingBox.Max.Z - TileBoundingBox.Min.Z));
					}

					if (bGatherTileLabels)
					{
						DebugLabels.Add(FDebugText(NavLocation.Location + NavMeshDrawOffset, FString::Printf(TEXT("(%d,%d:%d)"), X, Y, Layer)));
					}

					if (bGatherPolygonLabels || bGatherPolygonCost)
					{
						TArray<FNavPoly> Polys;
						NavMesh->GetPolysInTile(TileIndex, Polys);

						if (bGatherPolygonCost)
						{
							float DefaultCosts[RECAST_MAX_AREAS];
							float FixedCosts[RECAST_MAX_AREAS];

							NavMesh->GetDefaultQueryFilter()->GetAllAreaCosts(DefaultCosts, FixedCosts, RECAST_MAX_AREAS);

							for (int k = 0; k < Polys.Num(); ++k)
							{
								const uint32 AreaID = NavMesh->GetPolyAreaID(Polys[k].Ref);
								DebugLabels.Add(FDebugText(Polys[k].Center + NavMeshDrawOffset, FString::Printf(TEXT("\\%.3f; %.3f\\"), DefaultCosts[AreaID], FixedCosts[AreaID])));
							}
						}
						else
						{
							for (int k = 0; k < Polys.Num(); ++k)
							{
								uint32 NavPolyIndex = 0;
								uint32 NavTileIndex = 0;
								NavMesh->GetPolyTileIndex(Polys[k].Ref, NavPolyIndex, NavTileIndex);

								DebugLabels.Add(FDebugText(Polys[k].Center + NavMeshDrawOffset, FString::Printf(TEXT("[%X:%X]"), NavTileIndex, NavPolyIndex)));
							}
						}
					}

					if (bGatherTileBounds)
					{
						const FBox TileBox = NavMesh->GetNavMeshTileBounds(TileIndex);
						const float DrawZ = (TileBox.Min.Z + TileBox.Max.Z) * 0.5f;
						const FVector LL(TileBox.Min.X, TileBox.Min.Y, DrawZ);
						const FVector UR(TileBox.Max.X, TileBox.Max.Y, DrawZ);
						const FVector UL(LL.X, UR.Y, DrawZ);
						const FVector LR(UR.X, LL.Y, DrawZ);

						ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(LL, UL, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
						ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(UL, UR, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
						ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(UR, LR, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
						ThickLineItems.Add(FDebugRenderSceneProxy::FDebugLine(LR, LL, NavMeshRenderColor_TileBounds, DefaultEdges_LineThickness));
					}
				}
			}
		}

		NavMesh->FinishBatchQuery();

		// Draw Mesh
		const bool bGatherFilledPolys = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::FilledPolys);
		if (bGatherFilledPolys)
		{
			if (bGatherClusters)
			{
				for (int32 Idx = 0; Idx < NavMeshGeometry.Clusters.Num(); ++Idx)
				{
					const TArray<int32>& MeshIndices = NavMeshGeometry.Clusters[Idx].MeshIndices;
					if (MeshIndices.Num() == 0)
					{
						continue;
					}

					FDebugMeshData DebugMeshData;
					DebugMeshData.ClusterColor = FNavMeshRenderingHelpers::GetClusterColor(Idx);
					for (int32 VertIdx = 0; VertIdx < MeshVerts.Num(); ++VertIdx)
					{
						FNavMeshRenderingHelpers::AddVertex(DebugMeshData, MeshVerts[VertIdx] + NavMeshDrawOffset, DebugMeshData.ClusterColor);
					}
					for (int32 TriIdx = 0; TriIdx < MeshIndices.Num(); TriIdx += 3)
					{
						FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, MeshIndices[TriIdx], MeshIndices[TriIdx + 1], MeshIndices[TriIdx + 2]);
					}

					MeshBuilders.Add(DebugMeshData);
				}
			}
			else
			{
				for (int32 AreaType = 0; AreaType < RECAST_MAX_AREAS; ++AreaType)
				{
					const TArray<int32>& MeshIndices = NavMeshGeometry.AreaIndices[AreaType];
					if (MeshIndices.Num() == 0)
					{
						continue;
					}

					FDebugMeshData DebugMeshData;
					for (int32 VertIdx = 0; VertIdx < MeshVerts.Num(); ++VertIdx)
					{
						FNavMeshRenderingHelpers::AddVertex(DebugMeshData, MeshVerts[VertIdx] + NavMeshDrawOffset, NavMeshColors[AreaType]);
					}
					for (int32 TriIdx = 0; TriIdx < MeshIndices.Num(); TriIdx += 3)
					{
						FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, MeshIndices[TriIdx], MeshIndices[TriIdx + 1], MeshIndices[TriIdx + 2]);
					}

					DebugMeshData.ClusterColor = NavMeshColors[AreaType];
					MeshBuilders.Add(DebugMeshData);
				}
			}
		}

		const bool bGatherOctree = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::NavOctree);
		const bool bGatherPathCollidingGeometry = FNavMeshRenderingHelpers::HasFlag(NavDetailFlags, ENavMeshDetailFlags::PathCollidingGeometry);
		if (bGatherOctree || bGatherPathCollidingGeometry)
		{
			const UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(NavMesh->GetWorld());
			const FNavigationOctree* NavOctree = NavSys ? NavSys->GetNavOctree() : nullptr;
			if (NavOctree)
			{
				if (bGatherOctree)
				{
					for (FNavigationOctree::TConstIterator<> NodeIt(*NavOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
					{
						const FNavigationOctree::FNode& CurrentNode = NodeIt.GetCurrentNode();
						const FOctreeNodeContext& CorrentContext = NodeIt.GetCurrentContext();
						OctreeBounds.Add(CorrentContext.Bounds);

						FOREACH_OCTREE_CHILD_NODE(ChildRef)
						{
							if (CurrentNode.HasChild(ChildRef))
							{
								NodeIt.PushChild(ChildRef);
							}
						}
					}
				}

				if (bGatherPathCollidingGeometry)
				{
					TArray<FVector> CollidingVerts;

					for (FNavigationOctree::TConstIterator<> It(*NavOctree); It.HasPendingNodes(); It.Advance())
					{
						const FNavigationOctree::FNode& Node = It.GetCurrentNode();
						for (FNavigationOctree::ElementConstIt ElementIt(Node.GetElementIt()); ElementIt; ElementIt++)
						{
							const FNavigationOctreeElement& Element = *ElementIt;
							if (Element.ShouldUseGeometry(NavMesh->GetConfig()) && Element.Data->CollisionData.Num())
							{
								const FRecastGeometryCache CachedGeometry(Element.Data->CollisionData.GetData());
								FNavMeshRenderingHelpers::AddRecastGeometry(CollidingVerts, PathCollidingGeomIndices, CachedGeometry.Verts, CachedGeometry.Header.NumVerts, CachedGeometry.Indices, CachedGeometry.Header.NumFaces);
							}
						}
						FOREACH_OCTREE_CHILD_NODE(ChildRef)
						{
							if (Node.HasChild(ChildRef))
							{
								It.PushChild(ChildRef);
							}
						}
					}

					int32 NumVerts = CollidingVerts.Num();
					PathCollidingGeomVerts.SetNum(NumVerts, false);
					for (int32 Idx = 0; Idx < NumVerts; Idx++)
					{
						PathCollidingGeomVerts[Idx] = FDynamicMeshVertex(CollidingVerts[Idx]);
					}
				}
			}
		}

		if (NavMeshGeometry.BuiltMeshIndices.Num() > 0)
		{
			FDebugMeshData DebugMeshData;
			for (int32 VertIdx = 0; VertIdx < MeshVerts.Num(); ++VertIdx)
			{
				FNavMeshRenderingHelpers::AddVertex(DebugMeshData, MeshVerts[VertIdx] + NavMeshDrawOffset, NavMeshRenderColor_RecastTileBeingRebuilt);
			}
			DebugMeshData.Indices.Append(NavMeshGeometry.BuiltMeshIndices);
			DebugMeshData.ClusterColor = NavMeshRenderColor_RecastTileBeingRebuilt;
			MeshBuilders.Add(DebugMeshData);
		}

		if (bGatherClusters)
		{
			for (int32 Idx = 0; Idx < NavMeshGeometry.ClusterLinks.Num(); Idx++)
			{
				const FRecastDebugGeometry::FClusterLink& CLink = NavMeshGeometry.ClusterLinks[Idx];
				const FVector V0 = CLink.FromCluster + NavMeshDrawOffset;
				const FVector V1 = CLink.ToCluster + NavMeshDrawOffset + FVector(0, 0, 20.0f);

				FNavMeshRenderingHelpers::CacheArc(ClusterLinkLines, V0, V1, 0.4f, 4, FColor::Black, ClusterLinkLines_LineThickness);
				const FVector VOffset(0, 0, FVector::Dist(V0, V1) * 1.333f);
				FNavMeshRenderingHelpers::CacheArrowHead(ClusterLinkLines, V1, V0 + VOffset, 30.f, FColor::Black, ClusterLinkLines_LineThickness);
			}
		}

		// cache segment links
		if (bGatherNavLinks)
		{
			for (int32 AreaIdx = 0; AreaIdx < RECAST_MAX_AREAS; AreaIdx++)
			{
				const TArray<int32>& Indices = NavMeshGeometry.OffMeshSegmentAreas[AreaIdx];
				FNavMeshSceneProxyData::FDebugMeshData DebugMeshData;
				int32 VertBase = 0;

				for (int32 Idx = 0; Idx < Indices.Num(); Idx++)
				{
					FRecastDebugGeometry::FOffMeshSegment& SegInfo = NavMeshGeometry.OffMeshSegments[Indices[Idx]];
					const FVector A0 = SegInfo.LeftStart + NavMeshDrawOffset;
					const FVector A1 = SegInfo.LeftEnd + NavMeshDrawOffset;
					const FVector B0 = SegInfo.RightStart + NavMeshDrawOffset;
					const FVector B1 = SegInfo.RightEnd + NavMeshDrawOffset;
					const FVector Edge0 = B0 - A0;
					const FVector Edge1 = B1 - A1;
					const float Len0 = Edge0.Size();
					const float Len1 = Edge1.Size();
					const FColor SegColor = FNavMeshRenderingHelpers::DarkenColor(NavMeshColors[SegInfo.AreaID]);
					const FColor ColA = (SegInfo.ValidEnds & FRecastDebugGeometry::OMLE_Left) ? FColor::White : FColor::Black;
					const FColor ColB = (SegInfo.ValidEnds & FRecastDebugGeometry::OMLE_Right) ? FColor::White : FColor::Black;

					const int32 NumArcPoints = 8;
					const float ArcPtsScale = 1.0f / NumArcPoints;

					FVector Prev0 = FNavMeshRenderingHelpers::EvalArc(A0, Edge0, Len0*0.25f, 0);
					FVector Prev1 = FNavMeshRenderingHelpers::EvalArc(A1, Edge1, Len1*0.25f, 0);
					FNavMeshRenderingHelpers::AddVertex(DebugMeshData, Prev0, ColA);
					FNavMeshRenderingHelpers::AddVertex(DebugMeshData, Prev1, ColA);
					for (int32 ArcIdx = 1; ArcIdx <= NumArcPoints; ArcIdx++)
					{
						const float u = ArcIdx * ArcPtsScale;
						FVector Pt0 = FNavMeshRenderingHelpers::EvalArc(A0, Edge0, Len0*0.25f, u);
						FVector Pt1 = FNavMeshRenderingHelpers::EvalArc(A1, Edge1, Len1*0.25f, u);

						FNavMeshRenderingHelpers::AddVertex(DebugMeshData, Pt0, (ArcIdx == NumArcPoints) ? ColB : FColor::White);
						FNavMeshRenderingHelpers::AddVertex(DebugMeshData, Pt1, (ArcIdx == NumArcPoints) ? ColB : FColor::White);

						FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, VertBase + 0, VertBase + 2, VertBase + 1);
						FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, VertBase + 2, VertBase + 3, VertBase + 1);
						FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, VertBase + 0, VertBase + 1, VertBase + 2);
						FNavMeshRenderingHelpers::AddTriangle(DebugMeshData, VertBase + 2, VertBase + 1, VertBase + 3);

						VertBase += 2;
						Prev0 = Pt0;
						Prev1 = Pt1;
					}
					VertBase += 2;

					DebugMeshData.ClusterColor = SegColor;
				}

				if (DebugMeshData.Indices.Num())
				{
					MeshBuilders.Add(DebugMeshData);
				}
			}
		}
	}
}

#endif

//////////////////////////////////////////////////////////////////////////
// FNavMeshSceneProxy

void FNavMeshSceneProxy::FNavMeshIndexBuffer::InitRHI()
{
	if (Indices.Num() > 0)
	{
		FRHIResourceCreateInfo CreateInfo;
		void* Buffer = nullptr;
		IndexBufferRHI = RHICreateAndLockIndexBuffer(sizeof(int32), Indices.Num() * sizeof(int32), BUF_Static, CreateInfo, Buffer);

		// Write the indices to the index buffer.			
		FMemory::Memcpy(Buffer, Indices.GetData(), Indices.Num() * sizeof(int32));
		RHIUnlockIndexBuffer(IndexBufferRHI);
	}
}

void FNavMeshSceneProxy::FNavMeshVertexBuffer::InitRHI()
{
	if (Vertices.Num() > 0)
	{
		FRHIResourceCreateInfo CreateInfo;
		void* VertexBufferData = nullptr;
		VertexBufferRHI = RHICreateAndLockVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex), BUF_Static, CreateInfo, VertexBufferData);

		// Copy the vertex data into the vertex buffer.			
		FMemory::Memcpy(VertexBufferData, Vertices.GetData(), Vertices.Num() * sizeof(FDynamicMeshVertex));
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
}

void FNavMeshSceneProxy::FNavMeshVertexFactory::Init(const FNavMeshVertexBuffer* InVertexBuffer)
{
	if (IsInRenderingThread())
	{
		// Initialize the vertex factory's stream components.
		FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(InVertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
		NewData.TextureCoordinates.Add(
			FVertexStreamComponent(InVertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
			);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(InVertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
		NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(InVertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
		SetData(NewData);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitNavMeshVertexFactory,
			FNavMeshVertexFactory*, InVertexFactory, this,
			const FNavMeshVertexBuffer*, InVertexBuffer, InVertexBuffer,
			{
				// Initialize the vertex factory's stream components.
				FDataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(InVertexBuffer, FDynamicMeshVertex, Position, VET_Float3);
		NewData.TextureCoordinates.Add(
			FVertexStreamComponent(InVertexBuffer, STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), sizeof(FDynamicMeshVertex), VET_Float2)
			);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(InVertexBuffer, FDynamicMeshVertex, TangentX, VET_PackedNormal);
		NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(InVertexBuffer, FDynamicMeshVertex, TangentZ, VET_PackedNormal);
		InVertexFactory->SetData(NewData);
			});
	}
}

FNavMeshSceneProxy::FNavMeshSceneProxy(const UPrimitiveComponent* InComponent, FNavMeshSceneProxyData* InProxyData, bool ForceToRender)
	: FDebugRenderSceneProxy(InComponent)
	, bRequestedData(false)
	, bForceRendering(ForceToRender)
{
	if (InProxyData)
	{
		ProxyData = *InProxyData;
	}

	RenderingComponent = Cast<const UNavMeshRenderingComponent>(InComponent);
	bSkipDistanceCheck = GIsEditor && (GEngine->GetDebugLocalPlayer() == nullptr);
	bUseThickLines = GIsEditor;

	const int32 NumberOfMeshes = ProxyData.MeshBuilders.Num();
	if (!NumberOfMeshes)
	{
		return;
	}

	MeshColors.Reserve(NumberOfMeshes + (ProxyData.PathCollidingGeomVerts.Num() ? 1 : 0));
	MeshBatchElements.Reserve(NumberOfMeshes);
	const FMaterialRenderProxy* ParentMaterial = GEngine->DebugMeshMaterial->GetRenderProxy(false);
	for (int32 Index = 0; Index < NumberOfMeshes; ++Index)
	{
		const auto& CurrentMeshBuilder = ProxyData.MeshBuilders[Index];

		FMeshBatchElement Element;
		Element.FirstIndex = IndexBuffer.Indices.Num();
		Element.NumPrimitives = FMath::FloorToInt(CurrentMeshBuilder.Indices.Num() / 3);
		Element.MinVertexIndex = VertexBuffer.Vertices.Num();
		Element.MaxVertexIndex = Element.MinVertexIndex + CurrentMeshBuilder.Vertices.Num() - 1;
		Element.IndexBuffer = &IndexBuffer;
		MeshBatchElements.Add(Element);

		MeshColors.Add(FColoredMaterialRenderProxy(ParentMaterial, CurrentMeshBuilder.ClusterColor));

		VertexBuffer.Vertices.Append(CurrentMeshBuilder.Vertices);
		IndexBuffer.Indices.Append(CurrentMeshBuilder.Indices);
	}

	MeshColors.Add(FColoredMaterialRenderProxy(ParentMaterial, NavMeshRenderColor_PathCollidingGeom));

	VertexFactory.Init(&VertexBuffer);
	BeginInitResource(&IndexBuffer);
	BeginInitResource(&VertexBuffer);
	BeginInitResource(&VertexFactory);
}

FNavMeshSceneProxy::~FNavMeshSceneProxy()
{
	VertexBuffer.ReleaseResource();
	IndexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
}

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void FNavMeshDebugDrawDelegateHelper::RegisterDebugDrawDelgate()
{
	ensureMsgf(State != RegisteredState, TEXT("RegisterDebugDrawDelgate is already Registered!"));
	if (State == InitializedState)
	{
		DebugTextDrawingDelegate = FDebugDrawDelegate::CreateRaw(this, &FNavMeshDebugDrawDelegateHelper::DrawDebugLabels);
		DebugTextDrawingDelegateHandle = UDebugDrawService::Register(TEXT("Navigation"), DebugTextDrawingDelegate);
		State = RegisteredState;
	}
}

void FNavMeshDebugDrawDelegateHelper::UnregisterDebugDrawDelgate()
{
	ensureMsgf(State != InitializedState, TEXT("UnegisterDebugDrawDelgate is in an invalid State: %i !"), State);
	if (State == RegisteredState)
	{
		check(DebugTextDrawingDelegate.IsBound());
		UDebugDrawService::Unregister(DebugTextDrawingDelegateHandle);
		State = InitializedState;
	}
}
#endif

void FNavMeshSceneProxy::DrawDebugBox(FPrimitiveDrawInterface* PDI, FVector const& Center, FVector const& Box, FColor const& Color) const
{
	// no debug line drawing on dedicated server
	if (PDI != NULL)
	{
		PDI->DrawLine(Center + FVector(Box.X, Box.Y, Box.Z), Center + FVector(Box.X, -Box.Y, Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, -Box.Y, Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(-Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, Box.Y, Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(-Box.X, Box.Y, Box.Z), Center + FVector(Box.X, Box.Y, Box.Z), Color, SDPG_World);

		PDI->DrawLine(Center + FVector(Box.X, Box.Y, -Box.Z), Center + FVector(Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(-Box.X, -Box.Y, -Box.Z), Center + FVector(-Box.X, Box.Y, -Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(-Box.X, Box.Y, -Box.Z), Center + FVector(Box.X, Box.Y, -Box.Z), Color, SDPG_World);

		PDI->DrawLine(Center + FVector(Box.X, Box.Y, Box.Z), Center + FVector(Box.X, Box.Y, -Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(Box.X, -Box.Y, Box.Z), Center + FVector(Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(-Box.X, -Box.Y, Box.Z), Center + FVector(-Box.X, -Box.Y, -Box.Z), Color, SDPG_World);
		PDI->DrawLine(Center + FVector(-Box.X, Box.Y, Box.Z), Center + FVector(-Box.X, Box.Y, -Box.Z), Color, SDPG_World);
	}
}

void FNavMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastRenderingSceneProxy_GetDynamicMeshElements);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];
			const bool bVisible = !!View->Family->EngineShowFlags.Navigation || bForceRendering;
			if (!bVisible)
			{
				continue;
			}
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			for (int32 Index = 0; Index < ProxyData.OctreeBounds.Num(); ++Index)
			{
				const FBoxCenterAndExtent& ProxyBounds = ProxyData.OctreeBounds[Index];
				DrawDebugBox(PDI, ProxyBounds.Center, ProxyBounds.Extent, FColor::White);
			}

			// Draw Mesh
			if (MeshBatchElements.Num())
			{
				for (int32 Index = 0; Index < MeshBatchElements.Num(); ++Index)
				{
					if (MeshBatchElements[Index].NumPrimitives == 0)
					{
						continue;
					}

					FMeshBatch& Mesh = Collector.AllocateMesh();
					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement = MeshBatchElements[Index];
					BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(FMatrix::Identity, GetBounds(), GetLocalBounds(), false, UseEditorDepthTest());

					Mesh.bWireframe = false;
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = &MeshColors[Index];
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = false;
					Collector.AddMesh(ViewIndex, Mesh);
				}

				if (ProxyData.PathCollidingGeomIndices.Num() > 2)
				{
					FDynamicMeshBuilder MeshBuilder;
					MeshBuilder.AddVertices(ProxyData.PathCollidingGeomVerts);
					MeshBuilder.AddTriangles(ProxyData.PathCollidingGeomIndices);

					MeshBuilder.GetMesh(FMatrix::Identity, &MeshColors[MeshBatchElements.Num()], SDPG_World, false, false, ViewIndex, Collector);
				}
			}

			int32 Num = ProxyData.NavMeshEdgeLines.Num();
			PDI->AddReserveLines(SDPG_World, Num, false, false);
			PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const FDebugLine &Line = ProxyData.NavMeshEdgeLines[Index];
				if (FNavMeshRenderingHelpers::LineInView(Line.Start, Line.End, View, false))
				{
					if (FNavMeshRenderingHelpers::LineInCorrectDistance(Line.Start, Line.End, View))
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, NavMeshEdges_LineThickness, 0, true);
					}
					else if (bUseThickLines)
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
					}
				}
			}

			Num = ProxyData.ClusterLinkLines.Num();
			PDI->AddReserveLines(SDPG_World, Num, false, false);
			PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const FDebugLine &Line = ProxyData.ClusterLinkLines[Index];
				if (FNavMeshRenderingHelpers::LineInView(Line.Start, Line.End, View, false))
				{
					if (FNavMeshRenderingHelpers::LineInCorrectDistance(Line.Start, Line.End, View))
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, ClusterLinkLines_LineThickness, 0, true);
					}
					else if (bUseThickLines)
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
					}
				}
			}

			Num = ProxyData.TileEdgeLines.Num();
			PDI->AddReserveLines(SDPG_World, Num, false, false);
			PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const FDebugLine &Line = ProxyData.TileEdgeLines[Index];
				if (FNavMeshRenderingHelpers::LineInView(Line.Start, Line.End, View, false))
				{
					if (FNavMeshRenderingHelpers::LineInCorrectDistance(Line.Start, Line.End, View))
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, PolyEdges_LineThickness, 0, true);
					}
					else if (bUseThickLines)
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
					}
				}
			}

			Num = ProxyData.NavLinkLines.Num();
			PDI->AddReserveLines(SDPG_World, Num, false, false);
			PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const FDebugLine &Line = ProxyData.NavLinkLines[Index];
				if (FNavMeshRenderingHelpers::LineInView(Line.Start, Line.End, View, false))
				{
					if (FNavMeshRenderingHelpers::LineInCorrectDistance(Line.Start, Line.End, View))
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, LinkLines_LineThickness, 0, true);
					}
					else if (bUseThickLines)
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
					}
				}
			}

			Num = ProxyData.ThickLineItems.Num();
			PDI->AddReserveLines(SDPG_Foreground, Num, false, true);
			for (int32 Index = 0; Index < Num; ++Index)
			{
				const auto &Line = ProxyData.ThickLineItems[Index];
				if (FNavMeshRenderingHelpers::LineInView(Line.Start, Line.End, View, false))
				{
					if (FNavMeshRenderingHelpers::LineInCorrectDistance(Line.Start, Line.End, View))
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World, Line.Thickness, 0, true);
					}
					else if (bUseThickLines)
					{
						PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_Foreground, DefaultEdges_LineThickness, 0, true);
					}
				}
			}
		}
	}
}

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
void FNavMeshDebugDrawDelegateHelper::DrawDebugLabels(UCanvas* Canvas, APlayerController*)
{
	if (!Canvas)
	{
		return;
	}

	const bool bVisible = (Canvas->SceneView && !!Canvas->SceneView->Family->EngineShowFlags.Navigation) || bForceRendering;
	if (!bVisible || bNeedsNewData || DebugLabels.Num() == 0)
	{
		return;
	}

	const FColor OldDrawColor = Canvas->DrawColor;
	Canvas->SetDrawColor(FColor::White);
	const FSceneView* View = Canvas->SceneView;
	UFont* Font = GEngine->GetSmallFont();
	const FNavMeshSceneProxyData::FDebugText* DebugText = DebugLabels.GetData();
	for (int32 Idx = 0; Idx < DebugLabels.Num(); ++Idx, ++DebugText)
	{
		if (View->ViewFrustum.IntersectSphere(DebugText->Location, 1.0f))
		{
			const FVector ScreenLoc = Canvas->Project(DebugText->Location);
			Canvas->DrawText(Font, DebugText->Text, ScreenLoc.X, ScreenLoc.Y);
		}
	}

	Canvas->SetDrawColor(OldDrawColor);
}
#endif

FPrimitiveViewRelevance FNavMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	const bool bVisible = !!View->Family->EngineShowFlags.Navigation || bForceRendering;
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bVisible && IsShown(View);
	Result.bDynamicRelevance = true;
	// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
	Result.bSeparateTranslucencyRelevance = Result.bNormalTranslucencyRelevance = bVisible && IsShown(View);
	return Result;
}

uint32 FNavMeshSceneProxy::GetAllocatedSize() const
{
	return FDebugRenderSceneProxy::GetAllocatedSize() +
		ProxyData.GetAllocatedSize() +
		IndexBuffer.Indices.GetAllocatedSize() +
		VertexBuffer.Vertices.GetAllocatedSize() +
		MeshColors.GetAllocatedSize() +
		MeshBatchElements.GetAllocatedSize();
}

//////////////////////////////////////////////////////////////////////////
// NavMeshRenderingComponent

#if WITH_EDITOR
namespace
{
	bool AreAnyViewportsRelevant(const UWorld* World)
	{
		FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World);
		if (WorldContext && WorldContext->GameViewport)
		{
			return true;
		}
		
		for (FEditorViewportClient* CurrentViewport : GEditor->AllViewportClients)
		{
			if (CurrentViewport && CurrentViewport->IsVisible())
			{
				return true;
			}
		}

		return false;
	}
}
#endif

UNavMeshRenderingComponent::UNavMeshRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bIsEditorOnly = true;
	bSelectable = false;
	bCollectNavigationData = false;
}

bool UNavMeshRenderingComponent::IsNavigationShowFlagSet(const UWorld* World)
{
	bool bShowNavigation = false;

	FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(World);

#if WITH_EDITOR
	if (GEditor && WorldContext && WorldContext->WorldType != EWorldType::Game)
	{
		bShowNavigation = WorldContext->GameViewport != nullptr && WorldContext->GameViewport->EngineShowFlags.Navigation;
		if (bShowNavigation == false)
		{
			// we have to check all viewports because we can't to distinguish between SIE and PIE at this point.
			for (FEditorViewportClient* CurrentViewport : GEditor->AllViewportClients)
			{
				if (CurrentViewport && CurrentViewport->EngineShowFlags.Navigation)
				{
					bShowNavigation = true;
					break;
				}
			}
		}
	}
	else
#endif //WITH_EDITOR
	{
		bShowNavigation = WorldContext && WorldContext->GameViewport && WorldContext->GameViewport->EngineShowFlags.Navigation;
	}

	return bShowNavigation;
}

void UNavMeshRenderingComponent::TimerFunction()
{
	const UWorld* World = GetWorld();
#if WITH_EDITOR
	if (GEditor && (AreAnyViewportsRelevant(World) == false))
	{
		// unable to tell if the flag is on or not
		return;
	}
#endif // WITH_EDITOR

	const bool bShowNavigation = bForceUpdate || IsNavigationShowFlagSet(World);

	if (bShowNavigation != !!bCollectNavigationData && bShowNavigation == true)
	{
		bForceUpdate = false;
		bCollectNavigationData = bShowNavigation;
		MarkRenderStateDirty();
	}
}

void UNavMeshRenderingComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// it's a kind of HACK but there is no event or other information that show flag was changed by user => we have to check it periodically
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->GetTimerManager()->SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &UNavMeshRenderingComponent::TimerFunction), 1, true);
	}
	else
#endif //WITH_EDITOR
	{
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, FTimerDelegate::CreateUObject(this, &UNavMeshRenderingComponent::TimerFunction), 1, true);
	}
#endif //WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
}

void UNavMeshRenderingComponent::OnUnregister()
{
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// it's a kind of HACK but there is no event or other information that show flag was changed by user => we have to check it periodically
#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandle);
	}
	else
#endif //WITH_EDITOR
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
	}
#endif //WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	Super::OnUnregister();
}

FPrimitiveSceneProxy* UNavMeshRenderingComponent::CreateSceneProxy()
{
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	FNavMeshSceneProxy* NavMeshSceneProxy = nullptr;

	const bool bShowNavigation = IsNavigationShowFlagSet(GetWorld());

	bCollectNavigationData = bShowNavigation;

	if (bCollectNavigationData && IsVisible())
	{
		const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetOwner());
		if (NavMesh && NavMesh->IsDrawingEnabled())
		{
			FNavMeshSceneProxyData ProxyData;

			const int32 DetailFlags = ProxyData.GetDetailFlags(NavMesh);
			TArray<int32> EmptyTileSet;
			ProxyData.GatherData(NavMesh, DetailFlags, EmptyTileSet);

			NavMeshSceneProxy = new FNavMeshSceneProxy(this, &ProxyData);
		}
	}

	if (NavMeshSceneProxy)
	{
		NavMeshDebugDrawDelgateManager.InitDelegateHelper(NavMeshSceneProxy);
		NavMeshDebugDrawDelgateManager.ReregisterDebugDrawDelgate();
	}
	return NavMeshSceneProxy;
#else
	return nullptr;
#endif //WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
}

void UNavMeshRenderingComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	NavMeshDebugDrawDelgateManager.RegisterDebugDrawDelgate();
#endif
}

void UNavMeshRenderingComponent::DestroyRenderState_Concurrent()
{
#if WITH_RECAST && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	NavMeshDebugDrawDelgateManager.UnregisterDebugDrawDelgate();
#endif

	Super::DestroyRenderState_Concurrent();
}

FBoxSphereBounds UNavMeshRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(ForceInit);
#if WITH_RECAST
	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetOwner());
	if (NavMesh)
	{
		BoundingBox = NavMesh->GetNavMeshBounds();
		if (NavMesh->bDrawOctree)
		{
			const UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
			const FNavigationOctree* NavOctree = NavSys ? NavSys->GetNavOctree() : nullptr;
			if (NavOctree)
			{
				for (FNavigationOctree::TConstIterator<> NodeIt(*NavOctree); NodeIt.HasPendingNodes(); NodeIt.Advance())
				{
					const FOctreeNodeContext& CorrentContext = NodeIt.GetCurrentContext();
					BoundingBox += CorrentContext.Bounds.GetBox();
				}
			}
		}
	}
#endif
	return FBoxSphereBounds(BoundingBox);
}
