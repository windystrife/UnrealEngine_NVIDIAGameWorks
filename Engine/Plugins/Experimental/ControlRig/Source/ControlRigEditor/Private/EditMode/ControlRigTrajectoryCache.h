// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Curves/RichCurve.h"
#include "DynamicMeshBuilder.h"

class FControlRigTrajectoryFrame;
class ISequencer;
class UHierarchicalRig;
class UMaterialInstanceDynamic;

/**
 * Control Rig trajectory frame, which keeps cached locations of nodes
 */
class FControlRigTrajectoryFrame : public TSharedFromThis<FControlRigTrajectoryFrame>
{
public:
	struct FTrajectorySegment
	{
		FVector Location;
		FVector Tangent;
	};

	/** Create and initialize a new instance. */
	FControlRigTrajectoryFrame(float InTime);

public:

	/** Calculates the frame for the current position. */
	void CalculateFrame(UHierarchicalRig* HierarchicalRig);

	/** Get the time this frame is a snapshot of */
	float GetTime() const { return Time; }

	/** Get the time this frame is a snapshot of */
	const FTrajectorySegment& GetSegment(int32 Index) { return Segments[Index]; }

	/** Check whether this frame is valid */
	bool IsValid() const { return Segments.Num() > 0; }

private:

	/** Where in time this frame is a snapshot of */
	float Time;
	
	/** The locations we have generated for this frame */
	TArray<FTrajectorySegment> Segments;
};

/** Cache data */
struct FTrajectoryCacheData
{
	FTrajectoryCacheData() : TimeRange(0.f), FrameSnap(0.0f) {}

	bool operator==(const FTrajectoryCacheData& RHS) const
	{
		return
			TimeRange == RHS.TimeRange &&
			FrameSnap == RHS.FrameSnap;
	}

	bool operator!=(const FTrajectoryCacheData& RHS) const
	{
		return
			TimeRange != RHS.TimeRange ||
			FrameSnap != RHS.FrameSnap;
	}

	/** The total range to generate frames for */
	TRange<float> TimeRange;

	/** The current frame snap we are using */
	float FrameSnap;
};

class FControlRigTrajectoryCache
{
public:
	FControlRigTrajectoryCache();

	/** Force the cahce to recalcualte next frame */
	void ForceRecalc() { bForceRecalc = true; }

	/** Per-frame update */
	void Update(TSharedRef<ISequencer> Sequencer, const FGuid& InObjectBinding, const TRange<float>& NewRange, float FrameSnap, float DeltaTime, double InCurrentTime);

	/** Render our trajectories */
	void RenderTrajectories(const FTransform& ComponentTransform, FPrimitiveDrawInterface* PDI);

	/** Rebuild the mesh for our trajectories based on the selected nodes passed in */
	void RebuildMesh(const TArray<int32>& InNodeIndices);

protected:

	/** Rebuild our mesh */
	void RebuildMesh();

	/** Validate the cache against the current set of conditions */
	void Revalidate(double InCurrentTime);

	/** Compute new trajectory frames */
	void ComputeNewTrajectoryFrames();

	/** Fill the frames that we need to build for our current range */
	void UpdateFilledTrajectoryFrames();

	/** Actually do the calculations to build the frames (amortized over time) */
	void ComputeQueuedFrames();

protected:

	/** 'Vertex buffer' we use to render with */
	TArray<FDynamicMeshVertex> Vertices;

	/** 'Index buffer' we use to render with */
	TArray<int32> Indices;

	/** Currently selected indices */
	TArray<int32> NodeIndices;

	/** The sequencer we are bound to */
	TWeakPtr<ISequencer> SequencerPtr;

	/** The object binding we are showing */
	FGuid ObjectBinding;

	/** Time based data to used to validate the cached data */
	FTrajectoryCacheData CurrentCache;
	FTrajectoryCacheData PreviousCache;

	/** Current range snapped to frame intervals */
	TRange<float> CurrentSnappedRange;

	/** All the frames we currently represent - note these may not all be valid */
	TArray<TSharedPtr<FControlRigTrajectoryFrame>> TrajectoryFrames;

	/** All the frames queued for update - these frames are not valid yet */
	TArray<TSharedPtr<FControlRigTrajectoryFrame>> QueuedTrajectoryFrames;

	/** Last time we computed new frames */
	double LastComputationTime;

	/** Whether we need to recalculate our frames */
	bool bNeedsNewTrajectoryFrames;

	/** Whether to force a recalc or not */
	bool bForceRecalc;

	/** Whether rebuild our mesh */
	bool bNeedsMeshRebuild;

	/** Current display time */
	float CurrentDisplayTime;

	/** Material used to render trajectories */
	TWeakObjectPtr<UMaterialInstanceDynamic> Material;
};
