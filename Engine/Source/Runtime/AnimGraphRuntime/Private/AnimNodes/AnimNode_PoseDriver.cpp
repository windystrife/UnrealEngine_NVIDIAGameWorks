// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseDriver.h"
#include "AnimationRuntime.h"
#include "Serialization/CustomVersion.h"
#include "Animation/AnimInstanceProxy.h"
#include "RBF/RBFSolver.h"


FAnimNode_PoseDriver::FAnimNode_PoseDriver()
{
	RadialScaling_DEPRECATED = 0.25f;
	Type_DEPRECATED = EPoseDriverType::SwingOnly;

	DriveSource = EPoseDriverSource::Rotation;
	DriveOutput = EPoseDriverOutput::DrivePoses;

	RBFParams.DistanceMethod = ERBFDistanceMethod::SwingAngle;
}

void FAnimNode_PoseDriver::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_PoseHandler::Initialize_AnyThread(Context);

	SourcePose.Initialize(Context);

	CacheDrivenIDs(Context.AnimInstanceProxy->GetSkeleton());
}

void FAnimNode_PoseDriver::CacheDrivenIDs(USkeleton* Skeleton)
{
	// Cache UIDs for driving curves
	if (DriveOutput == EPoseDriverOutput::DriveCurves)
	{
		for (FPoseDriverTarget& PoseTarget : PoseTargets)
		{
			PoseTarget.DrivenUID = Skeleton->GetUIDByName(USkeleton::AnimCurveMappingName, PoseTarget.DrivenName);
		}
	}
	// If not driving curves, init to INDEX_NONE
	else
	{
		for (FPoseDriverTarget& PoseTarget : PoseTargets)
		{
			PoseTarget.DrivenUID = INDEX_NONE;
		}
	}
}

void FAnimNode_PoseDriver::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	FAnimNode_PoseHandler::CacheBones_AnyThread(Context);
	// Init pose input
	SourcePose.CacheBones(Context);

	const FBoneContainer& BoneContainer = Context.AnimInstanceProxy->GetRequiredBones();

	// Init bone refs
	for (FBoneReference& SourceBoneRef : SourceBones)
	{
		SourceBoneRef.Initialize(BoneContainer);
	}

	for (FBoneReference& OnlyDriveBoneRef : OnlyDriveBones)
	{
		OnlyDriveBoneRef.Initialize(BoneContainer);
	}

	EvalSpaceBone.Initialize(BoneContainer);

	// Don't want to modify SourceBones, set weight to zero (if weight array is allocated)
	for (FBoneReference& SourceBoneRef : SourceBones)
	{
		const FCompactPoseBoneIndex SourceCompactIndex = SourceBoneRef.GetCompactPoseIndex(BoneContainer);
		if (BoneBlendWeights.IsValidIndex(SourceCompactIndex.GetInt()))
		{
			BoneBlendWeights[SourceCompactIndex.GetInt()] = 0.f;
		}
	}


	// If we are filtering for only specific bones, set blend weight to zero for unwanted bones, and remember which bones to filter
	BonesToFilter.Reset();
	if (bOnlyDriveSelectedBones && CurrentPoseAsset.IsValid())
	{
		// Super call above should init BoneBlendWeights to compact pose size if CurrentPoseAsset is valid
		check(BoneBlendWeights.Num() == BoneContainer.GetBoneIndicesArray().Num());

		const TArray<FName> TrackNames = CurrentPoseAsset.Get()->GetTrackNames();
		for (const auto& TrackName : TrackNames)
		{
			// See if bone is in select list
			if (!IsBoneDriven(TrackName))
			{
				int32 MeshBoneIndex = BoneContainer.GetPoseBoneIndexForBoneName(TrackName);
				FCompactPoseBoneIndex CompactBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshBoneIndex));
				if (CompactBoneIndex != INDEX_NONE)
				{
					BoneBlendWeights[CompactBoneIndex.GetInt()] = 0.f; // Set blend weight for non-additive 
					BonesToFilter.Add(CompactBoneIndex); // Remember bones to filter out for additive
				}
			}
		}
	}
}

void FAnimNode_PoseDriver::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);
	SourcePose.Update(Context);
}

void FAnimNode_PoseDriver::GatherDebugData(FNodeDebugData& DebugData)
{
	FAnimNode_PoseHandler::GatherDebugData(DebugData);
	SourcePose.GatherDebugData(DebugData.BranchFlow(1.f));
}

bool FAnimNode_PoseDriver::IsBoneDriven(FName BoneName) const
{
	// If not filtering, drive all the bones
	if (!bOnlyDriveSelectedBones)
	{
		return true;
	}

	bool bIsDriven = false;
	for (const FBoneReference& BoneRef : OnlyDriveBones)
	{
		if (BoneRef.BoneName == BoneName)
		{
			bIsDriven = true;
			break;
		}
	}

	return bIsDriven;
}


void FAnimNode_PoseDriver::GetRBFTargets(TArray<FRBFTarget>& OutTargets) const
{
	OutTargets.Empty();
	OutTargets.AddZeroed(PoseTargets.Num());

	// Create entry for each target
	for (int32 TargetIdx = 0; TargetIdx < PoseTargets.Num(); TargetIdx++)
	{
		FRBFTarget& RBFTarget = OutTargets[TargetIdx];
		const FPoseDriverTarget& PoseTarget = PoseTargets[TargetIdx];

		// We want to make sure we always have the right number of Values in our RBFTarget. 
		// If bone entries are missing, we fill with zeroes
		for (int32 SourceIdx = 0; SourceIdx < SourceBones.Num(); SourceIdx++)
		{
			if (PoseTarget.BoneTransforms.IsValidIndex(SourceIdx))
			{
				const FPoseDriverTransform& BoneTransform = PoseTarget.BoneTransforms[SourceIdx];
				if (DriveSource == EPoseDriverSource::Translation)
				{
					RBFTarget.AddFromVector(BoneTransform.TargetTranslation);
				}
				else
				{
					RBFTarget.AddFromRotator(BoneTransform.TargetRotation);
				}
			}
			else
			{
				RBFTarget.AddFromVector(FVector::ZeroVector);
			}
		}

		RBFTarget.ScaleFactor = PoseTarget.TargetScale;
		RBFTarget.bApplyCustomCurve = PoseTarget.bApplyCustomCurve;
		RBFTarget.CustomCurve = PoseTarget.CustomCurve;
	}
}


void FAnimNode_PoseDriver::Evaluate_AnyThread(FPoseContext& Output)
{
	// Udpate DrivenIDs if needed
	if (bCachedDrivenIDsAreDirty)
	{
		CacheDrivenIDs(Output.AnimInstanceProxy->GetSkeleton());
	}

	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);

	// Get the index of the source bone
	const FBoneContainer& BoneContainer = SourceData.Pose.GetBoneContainer();

	FRBFEntry Input;

	SourceBoneTMs.Reset();
	bool bFoundAnyBone = false;
	for (const FBoneReference& SourceBoneRef : SourceBones)
	{
		FTransform SourceBoneTM = FTransform::Identity;

		const FCompactPoseBoneIndex SourceCompactIndex = SourceBoneRef.GetCompactPoseIndex(BoneContainer);
		if (SourceCompactIndex.GetInt() != INDEX_NONE)
		{
			// If evaluating in alternative bone space, have to build component space pose
			if (EvalSpaceBone.IsValidToEvaluate(BoneContainer))
			{
				FCSPose<FCompactPose> CSPose;
				CSPose.InitPose(SourceData.Pose);

				const FCompactPoseBoneIndex EvalSpaceCompactIndex = EvalSpaceBone.GetCompactPoseIndex(BoneContainer);
				FTransform EvalSpaceCompSpace = CSPose.GetComponentSpaceTransform(EvalSpaceCompactIndex);
				FTransform SourceBoneCompSpace = CSPose.GetComponentSpaceTransform(SourceCompactIndex);

				SourceBoneTM = SourceBoneCompSpace.GetRelativeTransform(EvalSpaceCompSpace);
			}
			// If just evaluating in local space, just grab from local space pose
			else
			{
				SourceBoneTM = SourceData.Pose[SourceCompactIndex];
			}

			bFoundAnyBone = true;
		}


		// Build RBFInput entry
		if (DriveSource == EPoseDriverSource::Translation)
		{
			Input.AddFromVector(SourceBoneTM.GetTranslation());
		}
		else
		{
			Input.AddFromRotator(SourceBoneTM.Rotator());
		}

		// Record this so we can use it for drawing in edit mode
		SourceBoneTMs.Add(SourceBoneTM);
	}

	// Do nothing if bone is no bones are found/all LOD-ed out
	if (!bFoundAnyBone)
	{
		Output = SourceData;
		return;
	}


	RBFParams.TargetDimensions = SourceBones.Num() * 3;

	// Get target array as RBF types
	TArray<FRBFTarget> RBFTargets;
	GetRBFTargets(RBFTargets);

	// Run RBF solver
	OutputWeights.Empty();
	FRBFSolver::Solve(RBFParams, RBFTargets, Input, OutputWeights);

	// Track if we have filled Output with valid pose
	bool bHaveValidPose = false;

	// Process active targets (if any)
	if (OutputWeights.Num() > 0)
	{
		// If we want to drive poses, and PoseAsset is assigned and compatible
		if (DriveOutput == EPoseDriverOutput::DrivePoses && 
			CurrentPoseAsset.IsValid() && 
			Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton()) )
		{
			FPoseContext CurrentPose(Output);

			// Reset PoseExtractContext..
			check(PoseExtractContext.PoseCurves.Num() == PoseUIDList.Num());		
			FMemory::Memzero(PoseExtractContext.PoseCurves.GetData(), PoseExtractContext.PoseCurves.Num() * sizeof(float));
			// Then fill in weight for any driven poses
			for (const FRBFOutputWeight& Weight : OutputWeights)
			{
				const FPoseDriverTarget& PoseTarget = PoseTargets[Weight.TargetIndex];

				int32 PoseIndex = CurrentPoseAsset.Get()->GetPoseIndexByName(PoseTarget.DrivenName);
				if (PoseIndex != INDEX_NONE)
				{
					PoseExtractContext.PoseCurves[PoseIndex] = Weight.TargetWeight;
				}
			}

			if (CurrentPoseAsset.Get()->GetAnimationPose(CurrentPose.Pose, CurrentPose.Curve, PoseExtractContext))
			{
				// blend by weight
				if (CurrentPoseAsset->IsValidAdditive())
				{
					const FTransform AdditiveIdentity(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);

					// Don't want to modify SourceBones, set additive offset to zero (not identity transform, as need zero scale)
					for (const FBoneReference& SourceBoneRef : SourceBones)
					{
						const FCompactPoseBoneIndex SourceCompactIndex = SourceBoneRef.GetCompactPoseIndex(BoneContainer);
						CurrentPose.Pose[SourceCompactIndex] = AdditiveIdentity;
					}

					// If filtering for specific bones, filter out bones using BonesToFilter array
					if (bOnlyDriveSelectedBones)
					{
						for (FCompactPoseBoneIndex BoneIndex : BonesToFilter)
						{
							CurrentPose.Pose[BoneIndex] = AdditiveIdentity;
						}
					}

					Output = SourceData;
					FAnimationRuntime::AccumulateAdditivePose(Output.Pose, CurrentPose.Pose, Output.Curve, CurrentPose.Curve, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
				}
				else
				{
					FAnimationRuntime::BlendTwoPosesTogetherPerBone(SourceData.Pose, CurrentPose.Pose, SourceData.Curve, CurrentPose.Curve, BoneBlendWeights, Output.Pose, Output.Curve);
				}

				bHaveValidPose = true;
			}
		}
		// Drive curves (morphs, materials etc)
		else if(DriveOutput == EPoseDriverOutput::DriveCurves)
		{
			// Start by copying input
			Output = SourceData;
			
			// Then set curves based on target weights
			for (const FRBFOutputWeight& Weight : OutputWeights)
			{
				FPoseDriverTarget& PoseTarget = PoseTargets[Weight.TargetIndex];
				if (PoseTarget.DrivenUID != SmartName::MaxUID)
				{
					Output.Curve.Set(PoseTarget.DrivenUID, Weight.TargetWeight);
				}
			}

			bHaveValidPose = true;
		}
	}


	// No valid pose, just pass through
	if(!bHaveValidPose)
	{
		Output = SourceData;
	}
}
