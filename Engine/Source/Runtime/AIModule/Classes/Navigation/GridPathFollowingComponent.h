// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Navigation/PathFollowingComponent.h"
#include "GridPathFollowingComponent.generated.h"

class UNavLocalGridManager;

/**
 *  Path following augmented with local navigation grids
 *
 *  Keeps track of nearby grids and use them instead of navigation path when agent is inside.
 *  Once outside grid, regular path following is resumed.
 *
 *  This allows creating dynamic navigation obstacles with fully static navigation (e.g. static navmesh),
 *  as long as they are minor modifications for path. Not recommended for blocking off entire corridors.
 *
 *  Does not replace proper avoidance for dynamic obstacles!
 */
UCLASS(BlueprintType, Experimental)
class AIMODULE_API UGridPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_UCLASS_BODY()
public:

	virtual void Initialize() override;
	virtual void UpdatePathSegment() override;
	virtual void Reset() override;
	virtual void ResumeMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest) override;
	virtual void OnPathUpdated();

	bool HasActiveGrid() const { return ActiveGridIdx != INDEX_NONE; }
	int32 GetActiveGridIdx() const { return ActiveGridIdx; }

	const TArray<FVector>& GetGridPathPoints() const { return GridPathPoints; }
	int32 GetNextGridPathIndex() const { return GridMoveSegmentEndIndex; }

protected:

	void UpdateActiveGrid(const FVector& CurrentLocation);

	UPROPERTY(Transient)
	UNavLocalGridManager* GridManager;

	/** index of current grid */
	int32 ActiveGridIdx;

	/** Id of current grid */
	int32 ActiveGridId;

	/** set when end of followed Path ends inside current grid */
	uint32 bIsPathEndInsideGrid : 1;

	/** set when grid path is valid */
	uint32 bHasGridPath : 1;

	/** path points for moving through grid */
	TArray<FVector> GridPathPoints;
	
	/** index of current destination grid path point */
	int32 GridMoveSegmentEndIndex;

	/** expected start of path segment after leaving grid */
	int32 MoveSegmentStartIndexOffGrid;
};
