// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_Fabrik.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// AnimNode_Fabrik
// Implementation of the FABRIK IK Algorithm
// Please see http://www.academia.edu/9165835/FABRIK_A_fast_iterative_solver_for_the_Inverse_Kinematics_problem for more details

FAnimNode_Fabrik::FAnimNode_Fabrik()
	: EffectorTransform(FTransform::Identity)
	, EffectorTransformSpace(BCS_ComponentSpace)
	, EffectorRotationSource(BRS_KeepLocalSpaceRotation)
	, Precision(1.f)
	, MaxIterations(10)
	, bEnableDebugDraw(false)
{
}

FVector FAnimNode_Fabrik::GetCurrentLocation(FCSPose<FCompactPose>& MeshBases, const FCompactPoseBoneIndex& BoneIndex)
{
	return MeshBases.GetComponentSpaceTransform(BoneIndex).GetLocation();
}

FTransform FAnimNode_Fabrik::GetTargetTransform(const FTransform& InComponentTransform, FCSPose<FCompactPose>& MeshBases, FBoneSocketTarget& InTarget, EBoneControlSpace Space, const FTransform& InOffset) 
{
	FTransform OutTransform;
	if (Space == BCS_BoneSpace)
	{
		OutTransform = InTarget.GetTargetTransform(InOffset, MeshBases, InComponentTransform);
	}
	else
	{
		// parent bone space still goes through this way
		// if your target is socket, it will try find parents of joint that socket belongs to
		OutTransform = InOffset;
		FAnimationRuntime::ConvertBoneSpaceTransformToCS(InComponentTransform, MeshBases, OutTransform, InTarget.GetCompactPoseBoneIndex(), Space);
	}

	return OutTransform;
}

void FAnimNode_Fabrik::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();

	// Update EffectorLocation if it is based off a bone position
	FTransform CSEffectorTransform = EffectorTransform;
	CSEffectorTransform = GetTargetTransform(Output.AnimInstanceProxy->GetComponentTransform(), Output.Pose, EffectorTarget, EffectorTransformSpace, EffectorTransform);

	FVector const CSEffectorLocation = CSEffectorTransform.GetLocation();

#if WITH_EDITOR
	CachedEffectorCSTransform = CSEffectorTransform;
#endif

	// Gather all bone indices between root and tip.
	TArray<FCompactPoseBoneIndex> BoneIndices;
	
	{
		const FCompactPoseBoneIndex RootIndex = RootBone.GetCompactPoseIndex(BoneContainer);
		FCompactPoseBoneIndex BoneIndex = TipBone.GetCompactPoseIndex(BoneContainer);
		do
		{
			BoneIndices.Insert(BoneIndex, 0);
			BoneIndex = Output.Pose.GetPose().GetParentBoneIndex(BoneIndex);
		} while (BoneIndex != RootIndex);
		BoneIndices.Insert(BoneIndex, 0);
	}

	// Maximum length of skeleton segment at full extension
	float MaximumReach = 0;

	// Gather transforms
	int32 const NumTransforms = BoneIndices.Num();
	OutBoneTransforms.AddUninitialized(NumTransforms);

	// Gather chain links. These are non zero length bones.
	TArray<FABRIKChainLink> Chain;
	Chain.Reserve(NumTransforms);

	// Start with Root Bone
	{
		const FCompactPoseBoneIndex& RootBoneIndex = BoneIndices[0];
		const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(RootBoneIndex);

		OutBoneTransforms[0] = FBoneTransform(RootBoneIndex, BoneCSTransform);
		Chain.Add(FABRIKChainLink(BoneCSTransform.GetLocation(), 0.f, RootBoneIndex, 0));
	}

	// Go through remaining transforms
	for (int32 TransformIndex = 1; TransformIndex < NumTransforms; TransformIndex++)
	{
		const FCompactPoseBoneIndex& BoneIndex = BoneIndices[TransformIndex];

		const FTransform& BoneCSTransform = Output.Pose.GetComponentSpaceTransform(BoneIndex);
		FVector const BoneCSPosition = BoneCSTransform.GetLocation();

		OutBoneTransforms[TransformIndex] = FBoneTransform(BoneIndex, BoneCSTransform);

		// Calculate the combined length of this segment of skeleton
		float const BoneLength = FVector::Dist(BoneCSPosition, OutBoneTransforms[TransformIndex-1].Transform.GetLocation());

		if (!FMath::IsNearlyZero(BoneLength))
		{
			Chain.Add(FABRIKChainLink(BoneCSPosition, BoneLength, BoneIndex, TransformIndex));
			MaximumReach += BoneLength;
		}
		else
		{
			// Mark this transform as a zero length child of the last link.
			// It will inherit position and delta rotation from parent link.
			FABRIKChainLink & ParentLink = Chain[Chain.Num()-1];
			ParentLink.ChildZeroLengthTransformIndices.Add(TransformIndex);
		}
	}

	bool bBoneLocationUpdated = false;
	float const RootToTargetDistSq = FVector::DistSquared(Chain[0].Position, CSEffectorLocation);
	int32 const NumChainLinks = Chain.Num();

	// FABRIK algorithm - bone translation calculation
	// If the effector is further away than the distance from root to tip, simply move all bones in a line from root to effector location
	if (RootToTargetDistSq > FMath::Square(MaximumReach))
	{
		for (int32 LinkIndex = 1; LinkIndex < NumChainLinks; LinkIndex++)
		{
			FABRIKChainLink const & ParentLink = Chain[LinkIndex - 1];
			FABRIKChainLink & CurrentLink = Chain[LinkIndex];
			CurrentLink.Position = ParentLink.Position + (CSEffectorLocation - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length;
		}
		bBoneLocationUpdated = true;
	}
	else // Effector is within reach, calculate bone translations to position tip at effector location
	{
		int32 const TipBoneLinkIndex = NumChainLinks - 1;

		// Check distance between tip location and effector location
		float Slop = FVector::Dist(Chain[TipBoneLinkIndex].Position, CSEffectorLocation);
		if (Slop > Precision)
		{
			// Set tip bone at end effector location.
			Chain[TipBoneLinkIndex].Position = CSEffectorLocation;

			int32 IterationCount = 0;
			while ((Slop > Precision) && (IterationCount++ < MaxIterations))
			{
				// "Forward Reaching" stage - adjust bones from end effector.
				for (int32 LinkIndex = TipBoneLinkIndex - 1; LinkIndex > 0; LinkIndex--)
				{
					FABRIKChainLink & CurrentLink = Chain[LinkIndex];
					FABRIKChainLink const & ChildLink = Chain[LinkIndex + 1];

					CurrentLink.Position = ChildLink.Position + (CurrentLink.Position - ChildLink.Position).GetUnsafeNormal() * ChildLink.Length;
				}

				// "Backward Reaching" stage - adjust bones from root.
				for (int32 LinkIndex = 1; LinkIndex < TipBoneLinkIndex; LinkIndex++)
				{
					FABRIKChainLink const & ParentLink = Chain[LinkIndex - 1];
					FABRIKChainLink & CurrentLink = Chain[LinkIndex];

					CurrentLink.Position = ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length;
				}

				// Re-check distance between tip location and effector location
				// Since we're keeping tip on top of effector location, check with its parent bone.
				Slop = FMath::Abs(Chain[TipBoneLinkIndex].Length - FVector::Dist(Chain[TipBoneLinkIndex - 1].Position, CSEffectorLocation));
			}

			// Place tip bone based on how close we got to target.
			{
				FABRIKChainLink const & ParentLink = Chain[TipBoneLinkIndex - 1];
				FABRIKChainLink & CurrentLink = Chain[TipBoneLinkIndex];

				CurrentLink.Position = ParentLink.Position + (CurrentLink.Position - ParentLink.Position).GetUnsafeNormal() * CurrentLink.Length;
			}

			bBoneLocationUpdated = true;
		}
	}

	// If we moved some bones, update bone transforms.
	if (bBoneLocationUpdated)
	{
		// First step: update bone transform positions from chain links.
		for (int32 LinkIndex = 0; LinkIndex < NumChainLinks; LinkIndex++)
		{
			FABRIKChainLink const & ChainLink = Chain[LinkIndex];
			OutBoneTransforms[ChainLink.TransformIndex].Transform.SetTranslation(ChainLink.Position);

			// If there are any zero length children, update position of those
			int32 const NumChildren = ChainLink.ChildZeroLengthTransformIndices.Num();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				OutBoneTransforms[ChainLink.ChildZeroLengthTransformIndices[ChildIndex]].Transform.SetTranslation(ChainLink.Position);
			}
		}

		// FABRIK algorithm - re-orientation of bone local axes after translation calculation
		for (int32 LinkIndex = 0; LinkIndex < NumChainLinks - 1; LinkIndex++)
		{
			FABRIKChainLink const & CurrentLink = Chain[LinkIndex];
			FABRIKChainLink const & ChildLink = Chain[LinkIndex + 1];

			// Calculate pre-translation vector between this bone and child
			FVector const OldDir = (GetCurrentLocation(Output.Pose, ChildLink.BoneIndex) - GetCurrentLocation(Output.Pose, CurrentLink.BoneIndex)).GetUnsafeNormal();

			// Get vector from the post-translation bone to it's child
			FVector const NewDir = (ChildLink.Position - CurrentLink.Position).GetUnsafeNormal();

			// Calculate axis of rotation from pre-translation vector to post-translation vector
			FVector const RotationAxis = FVector::CrossProduct(OldDir, NewDir).GetSafeNormal();
			float const RotationAngle = FMath::Acos(FVector::DotProduct(OldDir, NewDir));
			FQuat const DeltaRotation = FQuat(RotationAxis, RotationAngle);
			// We're going to multiply it, in order to not have to re-normalize the final quaternion, it has to be a unit quaternion.
			checkSlow(DeltaRotation.IsNormalized());

			// Calculate absolute rotation and set it
			FTransform& CurrentBoneTransform = OutBoneTransforms[CurrentLink.TransformIndex].Transform;
			CurrentBoneTransform.SetRotation(DeltaRotation * CurrentBoneTransform.GetRotation());
			CurrentBoneTransform.NormalizeRotation();

			// Update zero length children if any
			int32 const NumChildren = CurrentLink.ChildZeroLengthTransformIndices.Num();
			for (int32 ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				FTransform& ChildBoneTransform = OutBoneTransforms[CurrentLink.ChildZeroLengthTransformIndices[ChildIndex]].Transform;
				ChildBoneTransform.SetRotation(DeltaRotation * ChildBoneTransform.GetRotation());
				ChildBoneTransform.NormalizeRotation();
			}
		}
	}

	// Special handling for tip bone's rotation.
	int32 const TipBoneTransformIndex = OutBoneTransforms.Num() - 1;
	switch (EffectorRotationSource)
	{
	case BRS_KeepLocalSpaceRotation:
		OutBoneTransforms[TipBoneTransformIndex].Transform = Output.Pose.GetLocalSpaceTransform(BoneIndices[TipBoneTransformIndex]) * OutBoneTransforms[TipBoneTransformIndex - 1].Transform;
		break;
	case BRS_CopyFromTarget:
		OutBoneTransforms[TipBoneTransformIndex].Transform.SetRotation(CSEffectorTransform.GetRotation());
		break;
	case BRS_KeepComponentSpaceRotation:
		// Don't change the orientation at all
		break;
	default:
		break;
	}
}

bool FAnimNode_Fabrik::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	// Allow evaluation if all parameters are initialized and TipBone is child of RootBone
	return
		(
		TipBone.IsValidToEvaluate(RequiredBones)
		&& RootBone.IsValidToEvaluate(RequiredBones)
		&& Precision > 0
		&& RequiredBones.BoneIsChildOf(TipBone.BoneIndex, RootBone.BoneIndex)
		);
}

void FAnimNode_Fabrik::ConditionalDebugDraw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* PreviewSkelMeshComp) const
{
#if WITH_EDITOR

	if(bEnableDebugDraw && PreviewSkelMeshComp && PreviewSkelMeshComp->GetWorld())
	{
		FVector const CSEffectorLocation = CachedEffectorCSTransform.GetLocation();

		// Show end effector position.
		DrawDebugBox(PreviewSkelMeshComp->GetWorld(), CSEffectorLocation, FVector(Precision), FColor::Green, true, 0.1f);
		DrawDebugCoordinateSystem(PreviewSkelMeshComp->GetWorld(), CSEffectorLocation, CachedEffectorCSTransform.GetRotation().Rotator(), 5.f, true, 0.1f);
	}
#endif
}

void FAnimNode_Fabrik::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	TipBone.Initialize(RequiredBones);
	RootBone.Initialize(RequiredBones);
	EffectorTarget.InitializeBoneReferences(RequiredBones);
}

void FAnimNode_Fabrik::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_Fabrik::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);
	EffectorTarget.Initialize(Context.AnimInstanceProxy);
}
