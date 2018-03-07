// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationPath.h"
#include "EngineStats.h"
#include "EngineGlobals.h"
#include "AI/Navigation/NavAgentInterface.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "DrawDebugHelpers.h"
#include "VisualLogger/VisualLoggerTypes.h"
#include "AI/Navigation/RecastNavMesh.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "Debug/DebugDrawService.h"

#define DEBUG_DRAW_OFFSET 0
#define PATH_OFFSET_KEEP_VISIBLE_POINTS 1

//----------------------------------------------------------------------//
// FNavPathType
//----------------------------------------------------------------------//
uint32 FNavPathType::NextUniqueId = 0;

//----------------------------------------------------------------------//
// FNavigationPath
//----------------------------------------------------------------------//
const FNavPathType FNavigationPath::Type;

FNavigationPath::FNavigationPath()
	: GoalActorAsNavAgent(nullptr)
	, SourceActorAsNavAgent(nullptr)
	, PathType(FNavigationPath::Type)
	, bDoAutoUpdateOnInvalidation(true)
	, bIgnoreInvalidation(false)
	, bUpdateStartPointOnRepath(true)
	, bUpdateEndPointOnRepath(true)
	, bWaitingForRepath(false)
	, bUseOnPathUpdatedNotify(false)
	, LastUpdateTimeStamp(-1.f)	// indicates that it has not been set
	, GoalActorLocationTetherDistanceSq(-1.f)
	, GoalActorLastLocation(FVector::ZeroVector)
{
	InternalResetNavigationPath();
}

FNavigationPath::FNavigationPath(const TArray<FVector>& Points, AActor* InBase)
	: GoalActorAsNavAgent(nullptr)
	, SourceActorAsNavAgent(nullptr)
	, PathType(FNavigationPath::Type)
	, bDoAutoUpdateOnInvalidation(true)
	, bIgnoreInvalidation(false)
	, bUpdateStartPointOnRepath(true)
	, bUpdateEndPointOnRepath(true)
	, bWaitingForRepath(false)
	, bUseOnPathUpdatedNotify(false)
	, LastUpdateTimeStamp(-1.f)	// indicates that it has not been set
	, GoalActorLocationTetherDistanceSq(-1.f)
	, GoalActorLastLocation(FVector::ZeroVector)
{
	InternalResetNavigationPath();
	MarkReady();

	Base = InBase;

	PathPoints.AddZeroed(Points.Num());
	for (int32 i = 0; i < Points.Num(); i++)
	{
		FBasedPosition BasedPoint(InBase, Points[i]);
		PathPoints[i] = FNavPathPoint(*BasedPoint);
	}
}

void FNavigationPath::InternalResetNavigationPath()
{
	ShortcutNodeRefs.Reset();
	PathPoints.Reset();
	Base.Reset();

	bUpToDate = true;
	bIsReady = false;
	bIsPartial = false;
	bReachedSearchLimit = false;

	// keep:
	// - GoalActor
	// - GoalActorAsNavAgent
	// - SourceActor
	// - SourceActorAsNavAgent
	// - Querier
	// - Filter
	// - PathType
	// - ObserverDelegate
	// - bDoAutoUpdateOnInvalidation
	// - bIgnoreInvalidation
	// - bUpdateStartPointOnRepath
	// - bUpdateEndPointOnRepath
	// - bWaitingForRepath
	// - NavigationDataUsed
	// - LastUpdateTimeStamp
	// - GoalActorLocationTetherDistanceSq
	// - GoalActorLastLocation
}

FVector FNavigationPath::GetGoalLocation() const
{
	return GoalActor != NULL ? (GoalActorAsNavAgent != NULL ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation()) : GetEndLocation();
}

FVector FNavigationPath::GetPathFindingStartLocation() const
{
	return SourceActor != NULL ? (SourceActorAsNavAgent != NULL ? SourceActorAsNavAgent->GetNavAgentLocation() : SourceActor->GetActorLocation()) : GetStartLocation();
}

void FNavigationPath::SetGoalActorObservation(const AActor& ActorToObserve, float TetherDistance)
{
	if (NavigationDataUsed.IsValid() == false)
	{
		// this mechanism is available only for navigation-generated paths
		UE_LOG(LogNavigation, Warning, TEXT("Updating navigation path on goal actor's location change is available only for navigation-generated paths. Called for %s")
			, *GetNameSafe(&ActorToObserve));
		return;
	}

	// register for path observing only if we weren't registered already
	const bool RegisterForPathUpdates = (GoalActor.IsValid() == false);
	GoalActor = &ActorToObserve;
	checkSlow(GoalActor.IsValid());
	GoalActorAsNavAgent = Cast<INavAgentInterface>(&ActorToObserve);
	GoalActorLocationTetherDistanceSq = FMath::Square(TetherDistance);
	UpdateLastRepathGoalLocation();

	if (RegisterForPathUpdates)
	{
		NavigationDataUsed->RegisterObservedPath(AsShared());
	}
}

void FNavigationPath::SetSourceActor(const AActor& InSourceActor)
{
	SourceActor = &InSourceActor;
	SourceActorAsNavAgent = Cast<INavAgentInterface>(&InSourceActor);
}

void FNavigationPath::UpdateLastRepathGoalLocation()
{
	if (GoalActor.IsValid())
	{
		GoalActorLastLocation = GoalActorAsNavAgent ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
	}
}

EPathObservationResult::Type FNavigationPath::TickPathObservation()
{
	if (GoalActor.IsValid() == false)
	{
		return EPathObservationResult::NoLongerObserving;
	}

	const FVector GoalLocation = GoalActorAsNavAgent != NULL ? GoalActorAsNavAgent->GetNavAgentLocation() : GoalActor->GetActorLocation();
	return FVector::DistSquared(GoalLocation, GoalActorLastLocation) <= GoalActorLocationTetherDistanceSq ? EPathObservationResult::NoChange : EPathObservationResult::RequestRepath;
}

void FNavigationPath::DisableGoalActorObservation()
{
	GoalActor = NULL;
	GoalActorAsNavAgent = NULL;
	GoalActorLocationTetherDistanceSq = -1.f;
}

void FNavigationPath::Invalidate()
{
	if (!bIgnoreInvalidation)
	{
		bUpToDate = false;
		ObserverDelegate.Broadcast(this, ENavPathEvent::Invalidated);
		if (bDoAutoUpdateOnInvalidation && NavigationDataUsed.IsValid())
		{
			bWaitingForRepath = true;
			NavigationDataUsed->RequestRePath(AsShared(), ENavPathUpdateType::NavigationChanged);
		}
	}
}

void FNavigationPath::RePathFailed()
{
	ObserverDelegate.Broadcast(this, ENavPathEvent::RePathFailed);
	bWaitingForRepath = false;
}

void FNavigationPath::ResetForRepath()
{
	InternalResetNavigationPath();
}

void FNavigationPath::DebugDraw(const ANavigationData* NavData, FColor PathColor, UCanvas* Canvas, bool bPersistent, const uint32 NextPathPointIndex) const
{
#if ENABLE_DRAW_DEBUG

	static const FColor Grey(100,100,100);
	const int32 NumPathVerts = PathPoints.Num();

	UWorld* World = NavData->GetWorld();

	for (int32 VertIdx = 0; VertIdx < NumPathVerts-1; ++VertIdx)
	{
		// draw box at vert
		FVector const VertLoc = PathPoints[VertIdx].Location + NavigationDebugDrawing::PathOffset;
		DrawDebugSolidBox(World, VertLoc, NavigationDebugDrawing::PathNodeBoxExtent, VertIdx < int32(NextPathPointIndex) ? Grey : PathColor, bPersistent);

		// draw line to next loc
		FVector const NextVertLoc = PathPoints[VertIdx+1].Location + NavigationDebugDrawing::PathOffset;
		DrawDebugLine(World, VertLoc, NextVertLoc, VertIdx < int32(NextPathPointIndex)-1 ? Grey : PathColor, bPersistent
			, /*LifeTime*/-1.f, /*DepthPriority*/0
			, /*Thickness*/NavigationDebugDrawing::PathLineThickness);
	}

	// draw last vert
	if (NumPathVerts > 0)
	{
		DrawDebugBox(World, PathPoints[NumPathVerts-1].Location + NavigationDebugDrawing::PathOffset, FVector(15.f), PathColor, bPersistent);
	}

	// if observing goal actor draw a radius and a line to the goal
	if (GoalActor.IsValid())
	{
		const FVector GoalLocation = GetGoalLocation() + NavigationDebugDrawing::PathOffset;
		const FVector EndLocation = GetEndLocation() + NavigationDebugDrawing::PathOffset;
		static const FVector CylinderHalfHeight = FVector::UpVector * 10.f;
		DrawDebugCylinder(World, EndLocation - CylinderHalfHeight, EndLocation + CylinderHalfHeight, FMath::Sqrt(GoalActorLocationTetherDistanceSq), 16, PathColor, bPersistent);
		DrawDebugLine(World, EndLocation, GoalLocation, Grey, bPersistent);
	}

#endif
}

bool FNavigationPath::ContainsNode(NavNodeRef NodeRef) const
{
	for (int32 Index = 0; Index < PathPoints.Num(); Index++)
	{
		if (PathPoints[Index].NodeRef == NodeRef)
		{
			return true;
		}
	}

	return ShortcutNodeRefs.Find(NodeRef) != INDEX_NONE;
}

float FNavigationPath::GetLengthFromPosition(FVector SegmentStart, uint32 NextPathPointIndex) const
{
	if (NextPathPointIndex >= (uint32)PathPoints.Num())
	{
		return 0;
	}
	
	const uint32 PathPointsCount = PathPoints.Num();
	float PathDistance = 0.f;

	for (uint32 PathIndex = NextPathPointIndex; PathIndex < PathPointsCount; ++PathIndex)
	{
		const FVector SegmentEnd = PathPoints[PathIndex].Location;
		PathDistance += FVector::Dist(SegmentStart, SegmentEnd);
		SegmentStart = SegmentEnd;
	}

	return PathDistance;
}

bool FNavigationPath::ContainsCustomLink(uint32 LinkUniqueId) const
{
	for (int32 i = 0; i < PathPoints.Num(); i++)
	{
		if (PathPoints[i].CustomLinkId == LinkUniqueId && LinkUniqueId)
		{
			return true;
		}
	}

	return false;
}

bool FNavigationPath::ContainsAnyCustomLink() const
{
	for (int32 i = 0; i < PathPoints.Num(); i++)
	{
		if (PathPoints[i].CustomLinkId)
		{
			return true;
		}
	}

	return false;
}

FORCEINLINE bool FNavigationPath::DoesPathIntersectBoxImplementation(const FBox& Box, const FVector& StartLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{	
	bool bIntersects = false;

	FVector Start = StartLocation;
	for (int32 PathPointIndex = int32(StartingIndex); PathPointIndex < PathPoints.Num(); ++PathPointIndex)
	{
		const FVector End = PathPoints[PathPointIndex].Location;
		if (FVector::DistSquared(Start, End) > SMALL_NUMBER)
		{
			const FVector Direction = (End - Start);

			FVector HitLocation, HitNormal;
			float HitTime;

			// If we have a valid AgentExtent, then we use an extent box to represent the path
			// Otherwise we use a line to represent the path
			if ((AgentExtent && FMath::LineExtentBoxIntersection(Box, Start, End, *AgentExtent, HitLocation, HitNormal, HitTime)) ||
				(!AgentExtent && FMath::LineBoxIntersection(Box, Start, End, Direction)))
			{
				bIntersects = true;
				if (IntersectingSegmentIndex != NULL)
				{
					*IntersectingSegmentIndex = PathPointIndex;
				}
				break;
			}
		}

		Start = End;
	}

	return bIntersects;
}

bool FNavigationPath::DoesIntersectBox(const FBox& Box, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	// iterate over all segments and check if any intersects with given box
	bool bIntersects = false;
	int32 PathPointIndex = INDEX_NONE;

	if (PathPoints.Num() > 1 && PathPoints.IsValidIndex(int32(StartingIndex)))
	{
		bIntersects = DoesPathIntersectBoxImplementation(Box, PathPoints[StartingIndex].Location, StartingIndex + 1, IntersectingSegmentIndex, AgentExtent);
	}

	return bIntersects;
}

bool FNavigationPath::DoesIntersectBox(const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	// iterate over all segments and check if any intersects with given box
	bool bIntersects = false;
	int32 PathPointIndex = INDEX_NONE;

	if (PathPoints.Num() > 1 && PathPoints.IsValidIndex(int32(StartingIndex)))
	{
		bIntersects = DoesPathIntersectBoxImplementation(Box, AgentLocation, StartingIndex, IntersectingSegmentIndex, AgentExtent);
	}

	return bIntersects;
}

FVector FNavigationPath::GetSegmentDirection(uint32 SegmentEndIndex) const
{
	FVector Result = FNavigationSystem::InvalidLocation;

	// require at least two points
	if (PathPoints.Num() > 1)
	{
		if (PathPoints.IsValidIndex(SegmentEndIndex))
		{
			if (SegmentEndIndex > 0)
			{
				Result = (PathPoints[SegmentEndIndex].Location - PathPoints[SegmentEndIndex - 1].Location).GetSafeNormal();
			}
			else
			{
				// for '0'-th segment returns same as for 1st segment 
				Result = (PathPoints[1].Location - PathPoints[0].Location).GetSafeNormal();
			}
		}
		else if (SegmentEndIndex >= uint32(GetPathPoints().Num()))
		{
			// in this special case return direction of last segment
			Result = (PathPoints[PathPoints.Num() - 1].Location - PathPoints[PathPoints.Num() - 2].Location).GetSafeNormal();
		}
	}

	return Result;
}

FBasedPosition FNavigationPath::GetPathPointLocation(uint32 Index) const
{
	FBasedPosition BasedPt;
	if (PathPoints.IsValidIndex(Index))
	{
		BasedPt.Base = Base.Get();
		BasedPt.Position = PathPoints[Index].Location;
	}

	return BasedPt;
}

#if ENABLE_VISUAL_LOG

void FNavigationPath::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const 
{
	if (Snapshot == nullptr)
	{
		return;
	}

	const int32 NumPathVerts = PathPoints.Num();
	FVisualLogShapeElement Element(EVisualLoggerShapeElement::Path);
	Element.Category = LogNavigation.GetCategoryName();
	Element.SetColor(FColorList::Green);
	Element.Points.Reserve(NumPathVerts);
	Element.Thicknes = 3.f;
	
	for (int32 VertIdx = 0; VertIdx < NumPathVerts; ++VertIdx)
	{
		Element.Points.Add(PathPoints[VertIdx].Location + NavigationDebugDrawing::PathOffset);
	}

	Snapshot->ElementsToDraw.Add(Element);
}

FString FNavigationPath::GetDescription() const
{
	return FString::Printf(TEXT("NotifyPathUpdate points:%d valid:%s")
		, PathPoints.Num()
		, IsValid() ? TEXT("yes") : TEXT("no"));
}

#endif // ENABLE_VISUAL_LOG
//----------------------------------------------------------------------//
// FNavMeshPath
//----------------------------------------------------------------------//
const FNavPathType FNavMeshPath::Type;
	
FNavMeshPath::FNavMeshPath()
	: bWantsStringPulling(true)
	, bWantsPathCorridor(false)
{
	PathType = FNavMeshPath::Type;
	InternalResetNavMeshPath();
}

void FNavMeshPath::ResetForRepath()
{
	Super::ResetForRepath();
	InternalResetNavMeshPath();
}

void FNavMeshPath::InternalResetNavMeshPath()
{
	PathCorridor.Reset();
	PathCorridorCost.Reset();
	CustomLinkIds.Reset();
	PathCorridorEdges.Reset();

	bCorridorEdgesGenerated = false;
	bDynamic = false;
	bStringPulled = false;

	// keep:
	// - bWantsStringPulling
	// - bWantsPathCorridor
}

float FNavMeshPath::GetStringPulledLength(const int32 StartingPoint) const
{
	if (IsValid() == false || StartingPoint >= PathPoints.Num())
	{
		return 0.f;
	}

	float TotalLength = 0.f;
	const FNavPathPoint* PrevPoint = PathPoints.GetData() + StartingPoint;
	const FNavPathPoint* PathPoint = PrevPoint + 1;

	for (int32 PathPointIndex = StartingPoint + 1; PathPointIndex < PathPoints.Num(); ++PathPointIndex, ++PathPoint, ++PrevPoint)
	{
		TotalLength += FVector::Dist(PrevPoint->Location, PathPoint->Location);
	}

	return TotalLength;
}

float FNavMeshPath::GetPathCorridorLength(const int32 StartingEdge) const
{
	if (bCorridorEdgesGenerated == false)
	{
		return 0.f;
	}
	else if (StartingEdge >= PathCorridorEdges.Num())
	{
		return StartingEdge == 0 && PathPoints.Num() > 1 ? FVector::Dist(PathPoints[0].Location, PathPoints[PathPoints.Num()-1].Location) : 0.f;
	}

	
	const FNavigationPortalEdge* PrevEdge = PathCorridorEdges.GetData() + StartingEdge;
	const FNavigationPortalEdge* CorridorEdge = PrevEdge + 1;
	FVector PrevEdgeMiddle = PrevEdge->GetMiddlePoint();

	float TotalLength = StartingEdge == 0 ? FVector::Dist(PathPoints[0].Location, PrevEdgeMiddle)
		: FVector::Dist(PrevEdgeMiddle, PathCorridorEdges[StartingEdge - 1].GetMiddlePoint());

	for (int32 PathPolyIndex = StartingEdge + 1; PathPolyIndex < PathCorridorEdges.Num(); ++PathPolyIndex, ++PrevEdge, ++CorridorEdge)
	{
		const FVector CurrentEdgeMiddle = CorridorEdge->GetMiddlePoint();
		TotalLength += FVector::Dist(CurrentEdgeMiddle, PrevEdgeMiddle);
		PrevEdgeMiddle = CurrentEdgeMiddle;
	}
	// @todo add distance to last point here!
	return TotalLength;
}

const TArray<FNavigationPortalEdge>& FNavMeshPath::GeneratePathCorridorEdges() const
{
#if WITH_RECAST
	// mz@todo the underlying recast function queries the navmesh a portal at a time, 
	// which is a waste of performance. A batch-query function has to be added.
	const int32 CorridorLength = PathCorridor.Num();
	if (CorridorLength != 0 && IsInGameThread() && NavigationDataUsed.IsValid())
	{
		const ARecastNavMesh* MyOwner = Cast<ARecastNavMesh>(GetNavigationDataUsed());
		MyOwner->GetEdgesForPathCorridor(&PathCorridor, &PathCorridorEdges);
		bCorridorEdgesGenerated = PathCorridorEdges.Num() > 0;
	}
#endif // WITH_RECAST
	return PathCorridorEdges;
}

void FNavMeshPath::PerformStringPulling(const FVector& StartLoc, const FVector& EndLoc)
{
#if WITH_RECAST
	const ARecastNavMesh* MyOwner = Cast<ARecastNavMesh>(GetNavigationDataUsed());
	if (PathCorridor.Num())
	{
		bStringPulled = MyOwner->FindStraightPath(StartLoc, EndLoc, PathCorridor, PathPoints, &CustomLinkIds);
	}
#endif	// WITH_RECAST
}


#if DEBUG_DRAW_OFFSET
	UWorld* GInternalDebugWorld_ = NULL;
#endif

namespace
{
	struct FPathPointInfo
	{
		FPathPointInfo() 
		{

		}
		FPathPointInfo( const FNavPathPoint& InPoint, const FVector& InEdgePt0, const FVector& InEdgePt1) 
			: Point(InPoint)
			, EdgePt0(InEdgePt0)
			, EdgePt1(InEdgePt1) 
		{ 
			/** Empty */ 
		}

		FNavPathPoint Point;
		FVector EdgePt0;
		FVector EdgePt1;
	};

	FORCEINLINE bool CheckVisibility(const FPathPointInfo* StartPoint, const FPathPointInfo* EndPoint,  TArray<FNavigationPortalEdge>& PathCorridorEdges, float OffsetDistannce, FPathPointInfo* LastVisiblePoint)
	{
		FVector IntersectionPoint = FVector::ZeroVector;
		FVector StartTrace = StartPoint->Point.Location;
		FVector EndTrace = EndPoint->Point.Location;

		// find closest edge to StartPoint
		float BestDistance = FLT_MAX;
		FNavigationPortalEdge* CurrentEdge = NULL;

		float BestEndPointDistance = FLT_MAX;
		FNavigationPortalEdge* EndPointEdge = NULL;
		for (int32 EdgeIndex =0; EdgeIndex < PathCorridorEdges.Num(); ++EdgeIndex)
		{
			float DistToEdge = FLT_MAX;
			FNavigationPortalEdge* Edge = &PathCorridorEdges[EdgeIndex];
			if (BestDistance > FMath::Square(KINDA_SMALL_NUMBER))
			{
				DistToEdge= FMath::PointDistToSegmentSquared(StartTrace, Edge->Left, Edge->Right);
				if (DistToEdge < BestDistance)
				{
					BestDistance = DistToEdge;
					CurrentEdge = Edge;
#if DEBUG_DRAW_OFFSET
					DrawDebugLine( GInternalDebugWorld_, Edge->Left, Edge->Right, FColor::White, true );
#endif
				}
			}

			if (BestEndPointDistance > FMath::Square(KINDA_SMALL_NUMBER))
			{
				DistToEdge= FMath::PointDistToSegmentSquared(EndTrace, Edge->Left, Edge->Right);
				if (DistToEdge < BestEndPointDistance)
				{
					BestEndPointDistance = DistToEdge;
					EndPointEdge = Edge;
				}
			}
		}

		if (CurrentEdge == NULL || EndPointEdge == NULL )
		{
			LastVisiblePoint->Point.Location = FVector::ZeroVector;
			return false;
		}


		if (BestDistance <= FMath::Square(KINDA_SMALL_NUMBER))
		{
			CurrentEdge++;
		}

		if (CurrentEdge == EndPointEdge)
		{
			return true;
		}

		const FVector RayNormal = (StartTrace-EndTrace) .GetSafeNormal() * OffsetDistannce;
		StartTrace = StartTrace + RayNormal;
		EndTrace = EndTrace - RayNormal;

		bool bIsVisible = true;
#if DEBUG_DRAW_OFFSET
		DrawDebugLine( GInternalDebugWorld_, StartTrace, EndTrace, FColor::Yellow, true );
#endif
		const FNavigationPortalEdge* LaseEdge = &PathCorridorEdges[PathCorridorEdges.Num()-1];
		while (CurrentEdge <= EndPointEdge)
		{
			FVector Left = CurrentEdge->Left;
			FVector Right = CurrentEdge->Right;

#if DEBUG_DRAW_OFFSET
			DrawDebugLine( GInternalDebugWorld_, Left, Right, FColor::White, true );
#endif
			bool bIntersected = FMath::SegmentIntersection2D(Left, Right, StartTrace, EndTrace, IntersectionPoint);
			if ( !bIntersected)
			{
				const float EdgeHalfLength = (CurrentEdge->Left - CurrentEdge->Right).Size() * 0.5;
				const float Distance = FMath::Min(OffsetDistannce, EdgeHalfLength) *  0.1;
				Left = CurrentEdge->Left + Distance * (CurrentEdge->Right - CurrentEdge->Left).GetSafeNormal();
				Right = CurrentEdge->Right + Distance * (CurrentEdge->Left - CurrentEdge->Right).GetSafeNormal();
				FVector ClosestPointOnRay, ClosestPointOnEdge;
				FMath::SegmentDistToSegment(StartTrace, EndTrace, Right, Left, ClosestPointOnRay, ClosestPointOnEdge);
#if DEBUG_DRAW_OFFSET
				DrawDebugSphere( GInternalDebugWorld_, ClosestPointOnEdge, 10, 8, FColor::Red, true );
#endif
				LastVisiblePoint->Point.Location = ClosestPointOnEdge;
				LastVisiblePoint->EdgePt0= CurrentEdge->Left ;
				LastVisiblePoint->EdgePt1= CurrentEdge->Right ;
				return false;
			}
#if DEBUG_DRAW_OFFSET
			DrawDebugSphere( GInternalDebugWorld_, IntersectionPoint, 8, 8, FColor::White, true );
#endif
			CurrentEdge++;
			bIsVisible = true;
		}

		return bIsVisible;
	}
}

void FNavMeshPath::ApplyFlags(int32 NavDataFlags)
{
	if (NavDataFlags & ERecastPathFlags::SkipStringPulling)
	{
		bWantsStringPulling = false;
	}

	if (NavDataFlags & ERecastPathFlags::GenerateCorridor)
	{
		bWantsPathCorridor = true;
	}
}

void AppendPathPointsHelper(TArray<FNavPathPoint>& PathPoints, const TArray<FPathPointInfo>& SourcePoints, int32 Index)
{
	if (SourcePoints.IsValidIndex(Index) && SourcePoints[Index].Point.NodeRef != 0)
	{
		PathPoints.Add(SourcePoints[Index].Point);
	}
}

void FNavMeshPath::OffsetFromCorners(float Distance)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_OffsetFromCorners);

	const ARecastNavMesh* MyOwner = Cast<ARecastNavMesh>(GetNavigationDataUsed());
	if (PathPoints.Num() == 0 || PathPoints.Num() > 100)
	{
		// skip it, there is not need to offset that path from performance point of view
		return;
	}

#if DEBUG_DRAW_OFFSET
	GInternalDebugWorld_ = MyOwner->GetWorld();
	FlushDebugStrings(GInternalDebugWorld_);
	FlushPersistentDebugLines(GInternalDebugWorld_);
#endif

	if (bCorridorEdgesGenerated == false)
	{
		GeneratePathCorridorEdges(); 
	}
	const float DistanceSq = Distance * Distance;
	int32 CurrentEdge = 0;
	bool bNeedToCopyResults = false;
	int32 SingleNodePassCount = 0;

	FNavPathPoint* PathPoint = PathPoints.GetData();
	// it's possible we'll be inserting points into the path, so we need to buffer the result
	TArray<FPathPointInfo> FirstPassPoints;
	FirstPassPoints.Reserve(PathPoints.Num() + 2);
	FirstPassPoints.Add(FPathPointInfo(*PathPoint, FVector::ZeroVector, FVector::ZeroVector));
	++PathPoint;

	// for every point on path find a related corridor edge
	for (int32 PathNodeIndex = 1; PathNodeIndex < PathPoints.Num()-1 && CurrentEdge < PathCorridorEdges.Num();)
	{
		if (FNavMeshNodeFlags(PathPoint->Flags).PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION)
		{
			// put both ends
			FirstPassPoints.Add(FPathPointInfo(*PathPoint, FVector(0), FVector(0)));
			FirstPassPoints.Add(FPathPointInfo(*(PathPoint+1), FVector(0), FVector(0)));
			PathNodeIndex += 2;
			PathPoint += 2;
			continue;
		}

		int32 CloserPoint = -1;
		const FNavigationPortalEdge* Edge = &PathCorridorEdges[CurrentEdge];
		for (int32 EdgeIndex = CurrentEdge; EdgeIndex < PathCorridorEdges.Num(); ++Edge, ++EdgeIndex)
		{
			const float DistToSequence = FMath::PointDistToSegmentSquared(PathPoint->Location, Edge->Left, Edge->Right);
			if (DistToSequence <= FMath::Square(KINDA_SMALL_NUMBER))
			{
				const float LeftDistanceSq = FVector::DistSquared(PathPoint->Location, Edge->Left);
				const float RightDistanceSq = FVector::DistSquared(PathPoint->Location, Edge->Right);
				if (LeftDistanceSq > DistanceSq && RightDistanceSq > DistanceSq)
				{
					++CurrentEdge;
				}
				else
				{
					CloserPoint = LeftDistanceSq < RightDistanceSq ? 0 : 1;
					CurrentEdge = EdgeIndex;
				}
				break;
			}
		}

		if (CloserPoint >= 0)
		{
			bNeedToCopyResults = true;

			Edge = &PathCorridorEdges[CurrentEdge];
			const float ActualOffset = FPlatformMath::Min(Edge->GetLength()/2, Distance);

			FNavPathPoint NewPathPoint = *PathPoint;
			// apply offset 

			const FVector EdgePt0 = Edge->GetPoint(CloserPoint);
			const FVector EdgePt1 = Edge->GetPoint((CloserPoint+1)%2);
			const FVector EdgeDir = EdgePt1 - EdgePt0;
			const FVector EdgeOffset = EdgeDir.GetSafeNormal() * ActualOffset;
			NewPathPoint.Location = EdgePt0 + EdgeOffset;
			// update NodeRef (could be different if this is n-th pass on the same PathPoint
			NewPathPoint.NodeRef = Edge->ToRef;
			FirstPassPoints.Add(FPathPointInfo(NewPathPoint, EdgePt0, EdgePt1));

			// if we've found a matching edge it's possible there's also another one there using the same edge. 
			// that's why we need to repeat the process with the same path point and next edge
			++CurrentEdge;

			// we need to know if we did more than one iteration on a given point
			// if so then we should not add that point in following "else" statement
			++SingleNodePassCount;
		}
		else
		{
			if (SingleNodePassCount == 0)
			{
				// store unchanged
				FirstPassPoints.Add(FPathPointInfo(*PathPoint, FVector(0), FVector(0)));
			}
			else
			{
				SingleNodePassCount = 0;
			}

			++PathNodeIndex;
			++PathPoint;
		}
	}

	if (bNeedToCopyResults)
	{
		if (FirstPassPoints.Num() < 3 || !MyOwner->bUseBetterOffsetsFromCorners)
		{
			FNavPathPoint EndPt = PathPoints.Last();

			PathPoints.Reset();
			for (int32 Index=0; Index < FirstPassPoints.Num(); ++Index)
			{
				PathPoints.Add(FirstPassPoints[Index].Point);
			}

			PathPoints.Add(EndPt);
			return;
		}

		TArray<FNavPathPoint> DestinationPathPoints;
		DestinationPathPoints.Reserve(FirstPassPoints.Num() + 2);

		// don't forget the last point
		FirstPassPoints.Add(FPathPointInfo(PathPoints[PathPoints.Num()-1], FVector::ZeroVector, FVector::ZeroVector));

		int32 StartPointIndex = 0;
		int32 LastVisiblePointIndex = 0;
		int32 TestedPointIndex = 1;
		int32 LastPointIndex = FirstPassPoints.Num()-1;

		const int32 MaxSteps = 200;
		for (int32 StepsLeft = MaxSteps; StepsLeft >= 0; StepsLeft--)
		{ 
			if (StartPointIndex == TestedPointIndex || StepsLeft == 0)
			{
				// something went wrong, or exceeded limit of steps (= went even more wrong)
				DestinationPathPoints.Reset();
				break;
			}

			const FNavMeshNodeFlags LastVisibleFlags(FirstPassPoints[LastVisiblePointIndex].Point.Flags);
			const FNavMeshNodeFlags StartPointFlags(FirstPassPoints[StartPointIndex].Point.Flags);
			bool bWantsVisibilityInsert = true;

			if (StartPointFlags.PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION) 
			{
				AppendPathPointsHelper(DestinationPathPoints, FirstPassPoints, StartPointIndex);
				AppendPathPointsHelper(DestinationPathPoints, FirstPassPoints, StartPointIndex + 1);

				StartPointIndex++;
				LastVisiblePointIndex = StartPointIndex;
				TestedPointIndex = LastVisiblePointIndex + 1;
				
				// skip inserting new points
				bWantsVisibilityInsert = false;
			}
			
			bool bVisible = false; 
			if (((LastVisibleFlags.PathFlags & RECAST_STRAIGHTPATH_OFFMESH_CONNECTION) == 0) && (StartPointFlags.Area == LastVisibleFlags.Area))
			{
				FPathPointInfo LastVisiblePoint;
				bVisible = CheckVisibility( &FirstPassPoints[StartPointIndex], &FirstPassPoints[TestedPointIndex], PathCorridorEdges, Distance, &LastVisiblePoint );
				if (!bVisible)
				{
					if (LastVisiblePoint.Point.Location.IsNearlyZero())
					{
						DestinationPathPoints.Reset();
						break;
					}
					else if (StartPointIndex == LastVisiblePointIndex)
					{
						/** add new point only if we don't see our next location otherwise use last visible point*/
						LastVisiblePoint.Point.Flags = FirstPassPoints[LastVisiblePointIndex].Point.Flags;
						LastVisiblePointIndex = FirstPassPoints.Insert( LastVisiblePoint, StartPointIndex+1 );
						LastPointIndex = FirstPassPoints.Num()-1;

						// TODO: potential infinite loop - keeps inserting point without visibility
					}
				}
			}

			if (bWantsVisibilityInsert)
			{
				if (bVisible) 
				{ 
#if PATH_OFFSET_KEEP_VISIBLE_POINTS
					AppendPathPointsHelper(DestinationPathPoints, FirstPassPoints, StartPointIndex);
					LastVisiblePointIndex = TestedPointIndex;
					StartPointIndex = LastVisiblePointIndex;
					TestedPointIndex++;
#else
					LastVisiblePointIndex = TestedPointIndex;
					TestedPointIndex++;
#endif
				} 
				else
				{ 
					AppendPathPointsHelper(DestinationPathPoints, FirstPassPoints, StartPointIndex);
					StartPointIndex = LastVisiblePointIndex;
					TestedPointIndex = LastVisiblePointIndex + 1;
				} 
			}

			// if reached end of path, add current and last points to close it and leave loop
			if (TestedPointIndex > LastPointIndex) 
			{
				AppendPathPointsHelper(DestinationPathPoints, FirstPassPoints, StartPointIndex);
				AppendPathPointsHelper(DestinationPathPoints, FirstPassPoints, LastPointIndex);
				break; 
			} 
		} 

		if (DestinationPathPoints.Num())
		{
			PathPoints = DestinationPathPoints;
		}
	}
}

bool FNavMeshPath::IsPathSegmentANavLink(const int32 PathSegmentStartIndex) const
{
	return PathPoints.IsValidIndex(PathSegmentStartIndex)
		&& FNavMeshNodeFlags(PathPoints[PathSegmentStartIndex].Flags).IsNavLink();
}

void FNavMeshPath::DebugDraw(const ANavigationData* NavData, FColor PathColor, UCanvas* Canvas, bool bPersistent, const uint32 NextPathPointIndex) const
{
	Super::DebugDraw(NavData, PathColor, Canvas, bPersistent, NextPathPointIndex);

#if WITH_RECAST && ENABLE_DRAW_DEBUG
	const ARecastNavMesh* RecastNavMesh = Cast<const ARecastNavMesh>(NavData);		
	const TArray<FNavigationPortalEdge>& Edges = (const_cast<FNavMeshPath*>(this))->GetPathCorridorEdges();	
	const int32 CorridorEdgesCount = Edges.Num();
	const UWorld* World = NavData->GetWorld();

	for (int32 EdgeIndex = 0; EdgeIndex < CorridorEdgesCount; ++EdgeIndex)
	{
		DrawDebugLine(World, Edges[EdgeIndex].Left + NavigationDebugDrawing::PathOffset, Edges[EdgeIndex].Right + NavigationDebugDrawing::PathOffset
			, FColor::Blue, bPersistent, /*LifeTime*/-1.f, /*DepthPriority*/0
			, /*Thickness*/NavigationDebugDrawing::PathLineThickness);
	}

	if (Canvas && RecastNavMesh && RecastNavMesh->bDrawLabelsOnPathNodes)
	{
		UFont* RenderFont = GEngine->GetSmallFont();
		for (int32 VertIdx = 0; VertIdx < PathPoints.Num(); ++VertIdx)
		{
			// draw box at vert
			FVector const VertLoc = PathPoints[VertIdx].Location 
				+ FVector(0, 0, NavigationDebugDrawing::PathNodeBoxExtent.Z*2)
				+ NavigationDebugDrawing::PathOffset;
			const FVector ScreenLocation = Canvas->Project(VertLoc);

			FNavMeshNodeFlags NodeFlags(PathPoints[VertIdx].Flags);
			const UClass* NavAreaClass = RecastNavMesh->GetAreaClass(NodeFlags.Area);

			Canvas->DrawText(RenderFont, FString::Printf(TEXT("%d: %s"), VertIdx, *GetNameSafe(NavAreaClass)), ScreenLocation.X, ScreenLocation.Y );
		}
	}
#endif // WITH_RECAST && ENABLE_DRAW_DEBUG
}

bool FNavMeshPath::ContainsWithSameEnd(const FNavMeshPath* Other) const
{
	if (PathCorridor.Num() < Other->PathCorridor.Num())
	{
		return false;
	}

	const NavNodeRef* ThisPathNode = &PathCorridor[PathCorridor.Num()-1];
	const NavNodeRef* OtherPathNode = &Other->PathCorridor[Other->PathCorridor.Num()-1];
	bool bAreTheSame = true;

	for (int32 NodeIndex = Other->PathCorridor.Num() - 1; NodeIndex >= 0 && bAreTheSame; --NodeIndex, --ThisPathNode, --OtherPathNode)
	{
		bAreTheSame = *ThisPathNode == *OtherPathNode;
	}	

	return bAreTheSame;
}

namespace
{
	FORCEINLINE
	bool CheckIntersectBetweenPoints(const FBox& Box, const FVector* AgentExtent, const FVector& Start, const FVector& End)
	{
		if (FVector::DistSquared(Start, End) > SMALL_NUMBER)
		{
			const FVector Direction = (End - Start);

			FVector HitLocation, HitNormal;
			float HitTime;

			// If we have a valid AgentExtent, then we use an extent box to represent the path
			// Otherwise we use a line to represent the path
			if ((AgentExtent && FMath::LineExtentBoxIntersection(Box, Start, End, *AgentExtent, HitLocation, HitNormal, HitTime)) ||
				(!AgentExtent && FMath::LineBoxIntersection(Box, Start, End, Direction)))
			{
				return true;
			}
		}

		return false;
	}
}
bool FNavMeshPath::DoesPathIntersectBoxImplementation(const FBox& Box, const FVector& StartLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	bool bIntersects = false;	
	const TArray<FNavigationPortalEdge>& CorridorEdges = GetPathCorridorEdges();
	const uint32 NumCorridorEdges = CorridorEdges.Num();

	// if we have a valid corridor, but the index is out of bounds, we could
	// be checking just the last point, but that would be inconsistent with 
	// FNavMeshPath::DoesPathIntersectBoxImplementation implementation
	// so in this case we just say "Nope, doesn't intersect"
	if (NumCorridorEdges <= 0 || StartingIndex > NumCorridorEdges)
	{
		return false;
	}

	// note that it's a bit simplified. It works
	FVector Start = StartLocation;
	if (CorridorEdges.IsValidIndex(StartingIndex))
	{
		// make sure that Start is initialized correctly when testing from the middle of path (StartingIndex > 0)
		if (CorridorEdges.IsValidIndex(StartingIndex - 1))
		{
			const FNavigationPortalEdge& Edge = CorridorEdges[StartingIndex - 1];
			Start = Edge.Right + (Edge.Left - Edge.Right) / 2 + (AgentExtent ? FVector(0.f, 0.f, AgentExtent->Z) : FVector::ZeroVector);
		}

		for (uint32 PortalIndex = StartingIndex; PortalIndex < NumCorridorEdges; ++PortalIndex)
		{
			const FNavigationPortalEdge& Edge = CorridorEdges[PortalIndex];
			const FVector End = Edge.Right + (Edge.Left - Edge.Right) / 2 + (AgentExtent ? FVector(0.f, 0.f, AgentExtent->Z) : FVector::ZeroVector);
			
			if (CheckIntersectBetweenPoints(Box, AgentExtent, Start, End))
			{
				bIntersects = true;
				if (IntersectingSegmentIndex != NULL)
				{
					*IntersectingSegmentIndex = PortalIndex;
				}
				break;
			}

			Start = End;
		}

		// test the last portal->path end line. 
		if (bIntersects == false)
		{
			ensure(PathPoints.Num() == 2);
			const FVector End = PathPoints.Last().Location + (AgentExtent ? FVector(0.f, 0.f, AgentExtent->Z) : FVector::ZeroVector);

			if (CheckIntersectBetweenPoints(Box, AgentExtent, Start, End))
			{
				bIntersects = true;
				if (IntersectingSegmentIndex != NULL)
				{
					*IntersectingSegmentIndex = NumCorridorEdges;
				}
			}
		}
	}
	else if (NumCorridorEdges > 0 && StartingIndex == NumCorridorEdges) //at last polygon, just after last edge so direct line check 
	{
		const FVector End = PathPoints.Last().Location + (AgentExtent ? FVector(0.f, 0.f, AgentExtent->Z) : FVector::ZeroVector);
			
		if (CheckIntersectBetweenPoints(Box, AgentExtent, Start, End))
		{
			bIntersects = true;
			if (IntersectingSegmentIndex != NULL)
			{
				*IntersectingSegmentIndex = CorridorEdges.Num();
			}
		}
	}
	
	// just check if path's end is inside the tested box
	if (bIntersects == false && Box.IsInside(PathPoints.Last().Location))
	{
		bIntersects = true;
		if (IntersectingSegmentIndex != NULL)
		{
			*IntersectingSegmentIndex = CorridorEdges.Num();
		}
	}

	return bIntersects;
}

bool FNavMeshPath::DoesIntersectBox(const FBox& Box, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	if (IsStringPulled())
	{
		return Super::DoesIntersectBox(Box, StartingIndex, IntersectingSegmentIndex);
	}
	
	bool bParametersValid = true;
	FVector StartLocation = PathPoints[0].Location;
	
	const TArray<FNavigationPortalEdge>& CorridorEdges = GetPathCorridorEdges();
	if (StartingIndex < uint32(CorridorEdges.Num()))
	{
		StartLocation = CorridorEdges[StartingIndex].Right + (CorridorEdges[StartingIndex].Left - CorridorEdges[StartingIndex].Right) / 2;
		++StartingIndex;
	}
	else if (StartingIndex > uint32(CorridorEdges.Num()))
	{
		bParametersValid = false;
	}
	// else will be handled by DoesPathIntersectBoxImplementation

	return bParametersValid && DoesPathIntersectBoxImplementation(Box, StartLocation, StartingIndex, IntersectingSegmentIndex, AgentExtent);
}

bool FNavMeshPath::DoesIntersectBox(const FBox& Box, const FVector& AgentLocation, uint32 StartingIndex, int32* IntersectingSegmentIndex, FVector* AgentExtent) const
{
	if (IsStringPulled())
	{
		return Super::DoesIntersectBox(Box, AgentLocation, StartingIndex, IntersectingSegmentIndex, AgentExtent);
	}

	return DoesPathIntersectBoxImplementation(Box, AgentLocation, StartingIndex, IntersectingSegmentIndex, AgentExtent);
}

bool FNavMeshPath::GetNodeFlags(int32 NodeIdx, FNavMeshNodeFlags& Flags) const
{
	bool bResult = false;

	if (IsStringPulled())
	{
		if (PathPoints.IsValidIndex(NodeIdx))
		{
			Flags = FNavMeshNodeFlags(PathPoints[NodeIdx].Flags);
			bResult = true;
		}
	}
	else
	{
		if (PathCorridor.IsValidIndex(NodeIdx))
		{
#if WITH_RECAST
			const ARecastNavMesh* MyOwner = Cast<ARecastNavMesh>(GetNavigationDataUsed());
			MyOwner->GetPolyFlags(PathCorridor[NodeIdx], Flags);
			bResult = true;
#endif	// WITH_RECAST
		}
	}

	return bResult;
}

FVector FNavMeshPath::GetSegmentDirection(uint32 SegmentEndIndex) const
{
	if (IsStringPulled())
	{
		return Super::GetSegmentDirection(SegmentEndIndex);
	}
	
	FVector Result = FNavigationSystem::InvalidLocation;
	const TArray<FNavigationPortalEdge>& Corridor = GetPathCorridorEdges();

	if (Corridor.Num() > 0 && PathPoints.Num() > 1)
	{
		if (Corridor.IsValidIndex(SegmentEndIndex))
		{
			if (SegmentEndIndex > 0)
			{
				Result = (Corridor[SegmentEndIndex].GetMiddlePoint() - Corridor[SegmentEndIndex - 1].GetMiddlePoint()).GetSafeNormal();
			}
			else
			{
				Result = (Corridor[0].GetMiddlePoint() - GetPathPoints()[0].Location).GetSafeNormal();
			}
		}
		else if (SegmentEndIndex >= uint32(Corridor.Num()))
		{
			// in this special case return direction of last segment
			Result = (Corridor[Corridor.Num() - 1].GetMiddlePoint() - GetPathPoints()[0].Location).GetSafeNormal();
		}
	}

	return Result;
}

#if ENABLE_VISUAL_LOG

void FNavMeshPath::DescribeSelfToVisLog(FVisualLogEntry* Snapshot) const
{
	if (Snapshot == nullptr)
	{
		return;
	}

	if (IsStringPulled())
	{
		// draw path points only for string pulled paths
		Super::DescribeSelfToVisLog(Snapshot);
	}

	// draw corridor
#if WITH_RECAST
	FVisualLogShapeElement CorridorPoly(EVisualLoggerShapeElement::Polygon);
	CorridorPoly.SetColor(FColorList::Cyan.WithAlpha(100));
	CorridorPoly.Category = LogNavigation.GetCategoryName();
	CorridorPoly.Verbosity = ELogVerbosity::Verbose;
	CorridorPoly.Points.Reserve(PathCorridor.Num() * 6);

	const FVector CorridorOffset = NavigationDebugDrawing::PathOffset * 1.25f;
	int32 NumAreaMark = 1;

	ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(GetNavigationDataUsed());
	NavMesh->BeginBatchQuery();

	TArray<FVector> Verts;
	for (int32 Idx = 0; Idx < PathCorridor.Num(); Idx++)
	{
		const uint8 AreaID = NavMesh->GetPolyAreaID(PathCorridor[Idx]);
		const UClass* AreaClass = NavMesh->GetAreaClass(AreaID);
		
		Verts.Reset();
		const bool bPolyResult = NavMesh->GetPolyVerts(PathCorridor[Idx], Verts);
		if (!bPolyResult || Verts.Num() == 0)
		{
			// probably invalidated polygon, etc. (time sensitive and rare to reproduce issue)
			continue;
		}

		const UNavArea* DefArea = AreaClass ? ((UClass*)AreaClass)->GetDefaultObject<UNavArea>() : NULL;
		const FColor PolygonColor = AreaClass != UNavigationSystem::GetDefaultWalkableArea() ? (DefArea ? DefArea->DrawColor : NavMesh->GetConfig().Color) : FColorList::Cyan;

		CorridorPoly.SetColor(PolygonColor.WithAlpha(100));
		CorridorPoly.Points.Reset();
		CorridorPoly.Points.Append(Verts);
		Snapshot->ElementsToDraw.Add(CorridorPoly);

		if (AreaClass && AreaClass != UNavigationSystem::GetDefaultWalkableArea())
		{
			FVector CenterPt = FVector::ZeroVector;
			for (int32 VIdx = 0; VIdx < Verts.Num(); VIdx++)
			{
				CenterPt += Verts[VIdx];
			}
			CenterPt /= Verts.Num();

			FVisualLogShapeElement AreaMarkElem(EVisualLoggerShapeElement::Segment);
			AreaMarkElem.SetColor(FColorList::Orange);
			AreaMarkElem.Category = LogNavigation.GetCategoryName();
			AreaMarkElem.Verbosity = ELogVerbosity::Verbose;
			AreaMarkElem.Thicknes = 2;
			AreaMarkElem.Description = AreaClass->GetName();

			AreaMarkElem.Points.Add(CenterPt + CorridorOffset);
			AreaMarkElem.Points.Add(CenterPt + CorridorOffset + FVector(0,0,100.0f + NumAreaMark * 50.0f));
			Snapshot->ElementsToDraw.Add(AreaMarkElem);

			NumAreaMark = (NumAreaMark + 1) % 5;
		}
	}

	NavMesh->FinishBatchQuery();
	//Snapshot->ElementsToDraw.Add(CorridorElem);
#endif
}

FString FNavMeshPath::GetDescription() const
{
	return FString::Printf(TEXT("NotifyPathUpdate points:%d corridor length %d valid:%s")
		, PathPoints.Num()
		, PathCorridor.Num()
		, IsValid() ? TEXT("yes") : TEXT("no"));
}

#endif // ENABLE_VISUAL_LOG

//----------------------------------------------------------------------//
// UNavigationPath
//----------------------------------------------------------------------//

UNavigationPath::UNavigationPath(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsValid(false)
	, bDebugDrawingEnabled(false)
	, DebugDrawingColor(FColor::White)
	, SharedPath(NULL)
{	
	if (HasAnyFlags(RF_ClassDefaultObject) == false)
	{
		PathObserver = FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UNavigationPath::OnPathEvent);
	}
}

void UNavigationPath::BeginDestroy()
{
	if (SharedPath.IsValid())
	{
		SharedPath->RemoveObserver(PathObserverDelegateHandle);
	}
	Super::BeginDestroy();
}

void UNavigationPath::OnPathEvent(FNavigationPath* UpdatedPath, ENavPathEvent::Type PathEvent)
{
	if (UpdatedPath == SharedPath.Get())
	{
		PathUpdatedNotifier.Broadcast(this, PathEvent);
		if (SharedPath.IsValid() && SharedPath->IsValid())
		{
			bIsValid = true;
			SetPathPointsFromPath(*UpdatedPath);
		}
		else
		{
			bIsValid = false;
		}
	}
}

FString UNavigationPath::GetDebugString() const
{
	check((SharedPath.IsValid() && SharedPath->IsValid()) == !!bIsValid);
	if (!bIsValid)
	{
		return TEXT("Invalid path");
	}

	return FString::Printf(TEXT("Path: points %d%s%s"), SharedPath->GetPathPoints().Num()
		, SharedPath->IsPartial() ? TEXT(", partial") : TEXT("")
		, SharedPath->IsUpToDate() ? TEXT("") : TEXT(", OUT OF DATE!")
		);
}

void UNavigationPath::DrawDebug(UCanvas* Canvas, APlayerController*)
{
	if (SharedPath.IsValid())
	{
		SharedPath->DebugDraw(SharedPath->GetNavigationDataUsed(), DebugDrawingColor, Canvas, /*bPersistent=*/false);
	}
}

void UNavigationPath::EnableDebugDrawing(bool bShouldDrawDebugData, FLinearColor PathColor)
{
	DebugDrawingColor = PathColor.ToFColor(true);

	if (bDebugDrawingEnabled == bShouldDrawDebugData)
	{
		return;
	}

	bDebugDrawingEnabled = bShouldDrawDebugData;
	if (bShouldDrawDebugData)
	{
		DrawDebugDelegateHandle = UDebugDrawService::Register(TEXT("Navigation"), FDebugDrawDelegate::CreateUObject(this, &UNavigationPath::DrawDebug));
	}
	else
	{
		UDebugDrawService::Unregister(DrawDebugDelegateHandle);
	}
}

void UNavigationPath::EnableRecalculationOnInvalidation(TEnumAsByte<ENavigationOptionFlag::Type> DoRecalculation)
{
	if (DoRecalculation != RecalculateOnInvalidation)
	{
		RecalculateOnInvalidation = DoRecalculation;
		if (!!bIsValid && RecalculateOnInvalidation != ENavigationOptionFlag::Default)
		{
			SharedPath->EnableRecalculationOnInvalidation(RecalculateOnInvalidation == ENavigationOptionFlag::Enable);
		}
	}
}

float UNavigationPath::GetPathLength() const
{
	check((SharedPath.IsValid() && SharedPath->IsValid()) == !!bIsValid);
	return !!bIsValid ? SharedPath->GetLength() : -1.f;
}

float UNavigationPath::GetPathCost() const
{
	check((SharedPath.IsValid() && SharedPath->IsValid()) == !!bIsValid);
	return !!bIsValid ? SharedPath->GetCost() : -1.f;
}

bool UNavigationPath::IsPartial() const
{
	check((SharedPath.IsValid() && SharedPath->IsValid()) == !!bIsValid);
	return !!bIsValid && SharedPath->IsPartial();
}

bool UNavigationPath::IsValid() const
{
	check((SharedPath.IsValid() && SharedPath->IsValid()) == !!bIsValid);
	return !!bIsValid;
}

bool UNavigationPath::IsStringPulled() const
{
	return false;
}

void UNavigationPath::SetPath(FNavPathSharedPtr NewSharedPath)
{
	FNavigationPath* NewPath = NewSharedPath.Get();
	if (SharedPath.Get() != NewPath)
	{
		if (SharedPath.IsValid())
		{
			SharedPath->RemoveObserver(PathObserverDelegateHandle);
		}
		SharedPath = NewSharedPath;
		if (NewPath != NULL)
		{
			PathObserverDelegateHandle = NewPath->AddObserver(PathObserver);

			if (RecalculateOnInvalidation != ENavigationOptionFlag::Default)
			{
				NewPath->EnableRecalculationOnInvalidation(RecalculateOnInvalidation == ENavigationOptionFlag::Enable);
			}
			
			SetPathPointsFromPath(*NewPath);
		}
		else
		{
			PathPoints.Reset();
		}

		OnPathEvent(NewPath, NewPath != NULL ? ENavPathEvent::NewPath : ENavPathEvent::Cleared);
	}
}

void UNavigationPath::SetPathPointsFromPath(FNavigationPath& NativePath)
{
	PathPoints.Reset(NativePath.GetPathPoints().Num());
	for (const auto& PathPoint : NativePath.GetPathPoints())
	{
		PathPoints.Add(PathPoint.Location);
	}
}
