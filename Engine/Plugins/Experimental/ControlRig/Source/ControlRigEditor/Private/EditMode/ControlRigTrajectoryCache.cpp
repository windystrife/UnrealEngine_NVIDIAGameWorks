// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigTrajectoryCache.h"
#include "ISequencer.h"
#include "HierarchicalRig.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "DynamicMeshBuilder.h"
#include "ModuleManager.h"
#include "ControlRigEditorModule.h"

static void EvaluateRig(UHierarchicalRig* HierarchicalRig)
{
	if (HierarchicalRig)
	{
		HierarchicalRig->PreEvaluate();
		HierarchicalRig->Evaluate();
		HierarchicalRig->PostEvaluate();
	}
}

FControlRigTrajectoryFrame::FControlRigTrajectoryFrame(float InTime)
	: Time(InTime)
{
}

void FControlRigTrajectoryFrame::CalculateFrame(UHierarchicalRig* HierarchicalRig)
{
	Segments.Reset();

	// First evaluate rig
	EvaluateRig(HierarchicalRig);

	// then build the segments for each node
	const FAnimationHierarchy& Hierarchy = HierarchicalRig->GetHierarchy();
	const int32 NodeCount = Hierarchy.GetNum();
	for (int32 NodeIndex = 0; NodeIndex < NodeCount; ++NodeIndex)
	{
		FVector Location(FVector::ZeroVector);
		FVector Tangent(0.0f, 0.0f, 0.0f);
		FTransform Transform(FTransform::Identity);

		// first look for the 'driven' node
		FName NodeName = Hierarchy.GetNodeName(NodeIndex);
		FName DrivenName = HierarchicalRig->FindNodeDrivenByNode(NodeName);
		if (DrivenName != NAME_None)
		{
			int32 DrivenNodeIndex = Hierarchy.GetNodeIndex(DrivenName);
			check(DrivenNodeIndex != INDEX_NONE);
			Transform = Hierarchy.GetGlobalTransform(DrivenNodeIndex);
			Location = Transform.GetLocation();

			int32 DrivenParentIndex = Hierarchy.GetParentIndex(DrivenNodeIndex);
			if (DrivenParentIndex != INDEX_NONE)
			{
				FVector ParentLocation = Hierarchy.GetGlobalTransform(DrivenParentIndex).GetLocation();
				Tangent = (ParentLocation - Location).GetSafeNormal();
			}
			else
			{
				// default to the X axis for root nodes
				Tangent = Transform.GetUnitAxis(EAxis::X);
			}
		}
		else
		{
			// otherwise default to just using this node
			Transform = Hierarchy.GetGlobalTransform(NodeIndex);
			Location = Transform.GetLocation();

			int32 ParentIndex = Hierarchy.GetParentIndex(NodeIndex);
			if (ParentIndex != INDEX_NONE)
			{
				FVector ParentLocation = Hierarchy.GetGlobalTransform(ParentIndex).GetLocation();
				Tangent = (ParentLocation - Location).GetSafeNormal();
			}
			else
			{
				// default to the X axis for root nodes
				Tangent = Transform.GetUnitAxis(EAxis::X);
			}
		}

		// for nodes in the same place as their parent, we just default to the X axis
		if (Tangent.IsNearlyZero())
		{
			Tangent = Transform.GetUnitAxis(EAxis::X);
		}

		Segments.Add({ Location, Tangent });
	}
}

FControlRigTrajectoryCache::FControlRigTrajectoryCache()
{
	LastComputationTime = 0;
	bForceRecalc = false;
	bNeedsNewTrajectoryFrames = false;
	CurrentSnappedRange = TRange<float>::Empty();
	CurrentDisplayTime = 0.0f;
}

void FControlRigTrajectoryCache::Update(TSharedRef<ISequencer> Sequencer, const FGuid& InObjectBinding, const TRange<float>& NewRange, float FrameSnap, float DeltaTime, double InCurrentTime)
{
	if (!SequencerPtr.IsValid() || Sequencer != SequencerPtr.Pin().ToSharedRef() ||
		PreviousCache.FrameSnap != CurrentCache.FrameSnap ||
		ObjectBinding != InObjectBinding)
	{
		bForceRecalc = true;
		bNeedsMeshRebuild = true;
	}

	SequencerPtr = Sequencer;
	ObjectBinding = InObjectBinding;

	PreviousCache.TimeRange = CurrentCache.TimeRange;
	PreviousCache.FrameSnap = CurrentCache.FrameSnap;

	CurrentCache.TimeRange = NewRange;
	CurrentCache.FrameSnap = FrameSnap;

	Revalidate(InCurrentTime);

	ComputeQueuedFrames();

	// update current time
	if (CurrentDisplayTime < CurrentSnappedRange.GetLowerBoundValue())
	{
		CurrentDisplayTime = CurrentSnappedRange.GetLowerBoundValue();
	}

	CurrentDisplayTime += DeltaTime;
	if (CurrentDisplayTime >= CurrentSnappedRange.GetUpperBoundValue())
	{
		CurrentDisplayTime = CurrentSnappedRange.GetLowerBoundValue();
	}

	if (bNeedsMeshRebuild)
	{
		RebuildMesh();
	}
}

void FControlRigTrajectoryCache::Revalidate(double InCurrentTime)
{
	if (CurrentCache == PreviousCache && !bForceRecalc && !bNeedsNewTrajectoryFrames)
	{
		return;
	}

	if (FMath::IsNearlyZero(CurrentCache.TimeRange.Size<float>()) || CurrentCache.TimeRange.IsEmpty())
	{
		// Can't generate frames for this
		QueuedTrajectoryFrames.Reset();
		TrajectoryFrames.Reset();
		CurrentSnappedRange = TRange<float>::Empty();
		bNeedsNewTrajectoryFrames = false;
		bNeedsMeshRebuild = true;
		return;
	}

	bNeedsNewTrajectoryFrames = true;

	if (bForceRecalc)
	{
		TrajectoryFrames.Reset();
		CurrentSnappedRange = TRange<float>::Empty();
		bNeedsMeshRebuild = true;
	}
	
	if (InCurrentTime - LastComputationTime > 0.25f)
	{
		ComputeNewTrajectoryFrames();
		LastComputationTime = InCurrentTime;
	}
}

void FControlRigTrajectoryCache::ComputeNewTrajectoryFrames()
{
	UpdateFilledTrajectoryFrames();

	bForceRecalc = false;
	bNeedsNewTrajectoryFrames = false;
}

void FControlRigTrajectoryCache::UpdateFilledTrajectoryFrames()
{
	if (bNeedsNewTrajectoryFrames)
	{
		TrajectoryFrames.Reset();

		TSharedRef<ISequencer> Sequencer = SequencerPtr.Pin().ToSharedRef();

		float FirstFrameTime = FMath::GridSnap(CurrentCache.TimeRange.GetLowerBoundValue(), CurrentCache.FrameSnap);
		float LastFrameTime = FMath::GridSnap(CurrentCache.TimeRange.GetUpperBoundValue(), CurrentCache.FrameSnap);
		int32 FrameCount = FMath::RoundToInt((LastFrameTime - FirstFrameTime) / CurrentCache.FrameSnap);

		CurrentSnappedRange = TRange<float>(FirstFrameTime, LastFrameTime);

		float CurrentFrameTime = FirstFrameTime;
		for (int32 FrameIndex = 0; FrameIndex < FrameCount; ++FrameIndex)
		{
			if (CurrentSnappedRange.Contains(CurrentFrameTime))
			{
				TSharedPtr<FControlRigTrajectoryFrame> NewTrajectory = MakeShareable(new FControlRigTrajectoryFrame(CurrentFrameTime));
				TrajectoryFrames.Add(NewTrajectory);
				QueuedTrajectoryFrames.Add(NewTrajectory);
			}

			CurrentFrameTime += CurrentCache.FrameSnap;
		}
	}
}

void FControlRigTrajectoryCache::ComputeQueuedFrames()
{
	if (QueuedTrajectoryFrames.Num() > 0)
	{
		TSharedRef<ISequencer> Sequencer = SequencerPtr.Pin().ToSharedRef();
		TArrayView<TWeakObjectPtr<>> BoundObjects = Sequencer->FindObjectsInCurrentSequence(ObjectBinding);
		if (BoundObjects.Num() > 0)
		{
			if (UHierarchicalRig* HierarchicalRig = Cast<UHierarchicalRig>(BoundObjects[0].Get()))
			{
				Sequencer->EnterSilentMode();
				EMovieScenePlayerStatus::Type SavedPlaybackStatus = Sequencer->GetPlaybackStatus();
				float PlaybackTime = Sequencer->GetLocalTime();

				// Generate each frame
				int32 FramesCalculatedThisTime = 0;
				const int32 MaxFramesCalculatedPerUpdate = 30;
				while (QueuedTrajectoryFrames.Num() > 0 && FramesCalculatedThisTime < MaxFramesCalculatedPerUpdate)
				{
					TSharedPtr<FControlRigTrajectoryFrame> Frame = QueuedTrajectoryFrames.Pop();

					Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Jumping);
					Sequencer->SetLocalTimeDirectly(Frame->GetTime());

					Sequencer->ForceEvaluate();

					Frame->CalculateFrame(HierarchicalRig);
					FramesCalculatedThisTime++;
				}

				// Reset back to time before we generated
				Sequencer->SetPlaybackStatus(EMovieScenePlayerStatus::Jumping);
				Sequencer->SetLocalTimeDirectly(PlaybackTime);

				// force evaluate at time frame (pushes state to properties)
				Sequencer->ForceEvaluate();
				EvaluateRig(HierarchicalRig);

				Sequencer->SetPlaybackStatus(SavedPlaybackStatus);
				Sequencer->ExitSilentMode();
			}
		}

		bNeedsMeshRebuild = true;
	}
}

void FControlRigTrajectoryCache::RebuildMesh(const TArray<int32>& InNodeIndices)
{
	// check if selection is different
	if (InNodeIndices != NodeIndices)
	{
		NodeIndices = InNodeIndices;
		RebuildMesh();
	}
}

void FControlRigTrajectoryCache::RebuildMesh()
{
	Vertices.Reset();
	Indices.Reset();

	if (NodeIndices.Num() > 0 && TrajectoryFrames.Num() > 0)
	{
		const float Thickness = 4.0f;

		for (int32 NodeIndex : NodeIndices)
		{
			for (int32 FrameIndex = 0; FrameIndex < TrajectoryFrames.Num() - 1; ++FrameIndex)
			{
				const TSharedPtr<FControlRigTrajectoryFrame>& Frame = TrajectoryFrames[FrameIndex];
				const TSharedPtr<FControlRigTrajectoryFrame>& NextFrame = TrajectoryFrames[FrameIndex + 1];

				if (Frame->IsValid() && NextFrame->IsValid())
				{
					const FControlRigTrajectoryFrame::FTrajectorySegment& Segment = Frame->GetSegment(NodeIndex);
					const FControlRigTrajectoryFrame::FTrajectorySegment& NextSegment = NextFrame->GetSegment(NodeIndex);

					// Build tangent basis
					FVector SegmentTangentX = Segment.Tangent;
					FVector SegmentTangentY = (NextSegment.Location - Segment.Location).GetSafeNormal();
					FVector SegmentTangentZ = FVector::CrossProduct(SegmentTangentX, SegmentTangentY);
					FVector::CreateOrthonormalBasis(SegmentTangentX, SegmentTangentY, SegmentTangentZ);

					FVector NextSegmentTangentX = NextSegment.Tangent;
					FVector NextSegmentTangentY = SegmentTangentY;
					FVector NextSegmentTangentZ = FVector::CrossProduct(NextSegmentTangentX, NextSegmentTangentY);
					FVector::CreateOrthonormalBasis(NextSegmentTangentX, NextSegmentTangentY, NextSegmentTangentZ);

					// Setup vertices using tangents to create 'ribbon'
					const FVector FaceVertices[4] =
					{
						Segment.Location,
						NextSegment.Location,
						NextSegment.Location + (NextSegment.Tangent * Thickness),
						Segment.Location + (Segment.Tangent * Thickness),
					};

					// Stuff time in the UVs
					const FVector2D FaceUVs[4] =
					{
						FVector2D(0.0f, Frame->GetTime()),
						FVector2D(0.0f, NextFrame->GetTime()),
						FVector2D(0.0f, NextFrame->GetTime()),
						FVector2D(0.0f, Frame->GetTime()),
					};

					// Use colors as UV coordinates
					const FColor FaceColors[4] =
					{
						FColor(0, 0, 0, 0),
						FColor(0, 255, 0, 0),
						FColor(255, 255, 0, 0),
						FColor(255, 0, 0, 0),
					};

					int32 VertexIndices[4];
					for (int32 VertexIndex = 0; VertexIndex < 4; VertexIndex++)
					{
						VertexIndices[VertexIndex] = Vertices.Num();

						Vertices.Add(FDynamicMeshVertex(
							FaceVertices[VertexIndex],
							SegmentTangentX,
							SegmentTangentZ,
							FaceUVs[VertexIndex],
							FaceColors[VertexIndex]
						));
					}

					Indices.Add(VertexIndices[0]);
					Indices.Add(VertexIndices[1]); 
					Indices.Add(VertexIndices[2]);
					Indices.Add(VertexIndices[0]);
					Indices.Add(VertexIndices[2]);
					Indices.Add(VertexIndices[3]);
				}
			}
		}
	}

	bNeedsMeshRebuild = false;
}

void FControlRigTrajectoryCache::RenderTrajectories(const FTransform& ComponentTransform, FPrimitiveDrawInterface* PDI)
{
	if (Vertices.Num() > 0 && Indices.Num() > 0)
	{
		FDynamicMeshBuilder MeshBuilder;
		MeshBuilder.AddVertices(Vertices);
		MeshBuilder.AddTriangles(Indices);

		if (!Material.IsValid())
		{
			FControlRigEditorModule& ControlRigEditorModule = FModuleManager::GetModuleChecked<FControlRigEditorModule>("ControlRigEditor");
			Material = UMaterialInstanceDynamic::Create(ControlRigEditorModule.GetTrajectoryMaterial(), nullptr);
		}

		if (Material.IsValid())
		{
			Material->SetScalarParameterValue("Time", CurrentDisplayTime);
			MeshBuilder.Draw(PDI, ComponentTransform.ToMatrixWithScale(), Material->GetRenderProxy(false), SDPG_Foreground);
		}
	}
}
