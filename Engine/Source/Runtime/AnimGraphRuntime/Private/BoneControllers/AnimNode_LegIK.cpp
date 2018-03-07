// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_LegIK.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Animation/AnimInstanceProxy.h"

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimNodeLegIKDebug(TEXT("a.AnimNode.LegIK.Debug"), 0, TEXT("Turn on debug for FAnimNode_LegIK"));
#endif

TAutoConsoleVariable<int32> CVarAnimLegIKEnable(TEXT("a.AnimNode.LegIK.Enable"), 1, TEXT("Toggle LegIK node."));
TAutoConsoleVariable<int32> CVarAnimLegIKMaxIterations(TEXT("a.AnimNode.LegIK.MaxIterations"), 0, TEXT("Leg IK MaxIterations override. 0 = node default, > 0 override."));
TAutoConsoleVariable<float> CVarAnimLegIKTargetReachStepPercent(TEXT("a.AnimNode.LegIK.TargetReachStepPercent"), 0.7f, TEXT("Leg IK TargetReachStepPercent."));
TAutoConsoleVariable<float> CVarAnimLegIKPullDistribution(TEXT("a.AnimNode.LegIK.PullDistribution"), 0.5f, TEXT("Leg IK PullDistribution. 0 = foot, 0.5 = balanced, 1.f = hip"));

/////////////////////////////////////////////////////
// FAnimAnimNode_LegIK

DECLARE_CYCLE_STAT(TEXT("LegIK Eval"), STAT_LegIK_Eval, STATGROUP_Anim);
DECLARE_CYCLE_STAT(TEXT("LegIK FABRIK Eval"), STAT_LegIK_FABRIK_Eval, STATGROUP_Anim);

FAnimNode_LegIK::FAnimNode_LegIK()
{
	ReachPrecision = 0.01f;
	MaxIterations = 12;
}

void FAnimNode_LegIK::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	// 	DebugLine += "(";
	// 	AddDebugNodeData(DebugLine);
	// 	DebugLine += FString::Printf(TEXT(" Target: %s)"), *BoneToModify.BoneName.ToString());

	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

static FVector GetBoneWorldLocation(const FTransform& InBoneTransform, const USkeletalMeshComponent& InSkelMeshComp)
{
	const FVector MeshCompSpaceLocation = InBoneTransform.GetLocation();
	return InSkelMeshComp.GetComponentTransform().TransformPosition(MeshCompSpaceLocation);
}

#if ENABLE_DRAW_DEBUG
static void DrawDebugLeg(const FAnimLegIKData& InLegData, const USkeletalMeshComponent& InSkelMeshComp, const UWorld* InWorld, const FColor& InColor)
{
	for (int32 Index = 0; Index < InLegData.NumBones - 1; Index++)
	{
		const FVector CurrentBoneWorldLoc = GetBoneWorldLocation(InLegData.FKLegBoneTransforms[Index], InSkelMeshComp);
		const FVector ParentBoneWorldLoc = GetBoneWorldLocation(InLegData.FKLegBoneTransforms[Index + 1], InSkelMeshComp);
		DrawDebugLine(InWorld, CurrentBoneWorldLoc, ParentBoneWorldLoc, InColor, false, -1.f, SDPG_Foreground, 2.f);
	}
}
#endif // ENABLE_DRAW_DEBUG


void FAnimLegIKData::InitializeTransforms(USkeletalMeshComponent* SkelComp, FCSPose<FCompactPose>& MeshBases)
{
	// Initialize bone transforms
	IKFootTransform = MeshBases.GetComponentSpaceTransform(IKFootBoneIndex);

	FKLegBoneTransforms.Reset(NumBones);
	for (auto LegBoneIndex : FKLegBoneIndices)
	{
		FKLegBoneTransforms.Add(MeshBases.GetComponentSpaceTransform(LegBoneIndex));
	}

#if ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
	const bool bShowDebug = (CVarAnimNodeLegIKDebug.GetValueOnAnyThread() == 1);
	if (bShowDebug)
	{
		DrawDebugLeg(*this, *SkelComp, SkelComp->GetWorld(), FColor::Red);
		DrawDebugSphere(SkelComp->GetWorld(), GetBoneWorldLocation(IKFootTransform, *SkelComp), 4.f, 4, FColor::Red, false, -1.f, SDPG_Foreground, 2.f);
	}
#endif // ENABLE_ANIM_DEBUG && ENABLE_DRAW_DEBUG
}

void FAnimNode_LegIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_LegIK_Eval);

#if ENABLE_ANIM_DEBUG
	check(Output.AnimInstanceProxy->GetSkelMeshComponent());
#endif
	check(OutBoneTransforms.Num() == 0);

	// Get transforms for each leg.
	{
		for (FAnimLegIKData& LegData : LegsData)
		{
			LegData.InitializeTransforms(Output.AnimInstanceProxy->GetSkelMeshComponent(), Output.Pose);

			// rotate hips so foot aligns with effector.
			OrientLegTowardsIK(LegData, Output.AnimInstanceProxy->GetSkelMeshComponent());

			// expand/compress leg, so foot reaches effector.
			DoLegReachIK(LegData, Output.AnimInstanceProxy->GetSkelMeshComponent());

			if (LegData.LegDefPtr->bEnableKneeTwistCorrection)
			{
				// Adjust knee twist orientation
				AdjustKneeTwist(LegData, Output.AnimInstanceProxy->GetSkelMeshComponent());
			}

			// Override Foot FK, with IK.
			LegData.FKLegBoneTransforms[0].SetRotation(LegData.IKFootTransform.GetRotation());

			// Add transforms
			for (int32 Index = 0; Index < LegData.NumBones; Index++)
			{
				OutBoneTransforms.Add(FBoneTransform(LegData.FKLegBoneIndices[Index], LegData.FKLegBoneTransforms[Index]));
			}
		}
	}

	// Sort OutBoneTransforms so indices are in increasing order.
	OutBoneTransforms.Sort(FCompareBoneTransformIndex());
}

static bool RotateLegByQuat(const FQuat& InDeltaRotation, FAnimLegIKData& InLegData)
{
	if (!InDeltaRotation.IsIdentity())
	{
		const FVector HipLocation = InLegData.FKLegBoneTransforms.Last().GetLocation();

		// Rotate Leg so it is aligned with IK Target
		for (auto& LegBoneTransform : InLegData.FKLegBoneTransforms)
		{
			LegBoneTransform.SetRotation(InDeltaRotation * LegBoneTransform.GetRotation());

			const FVector BoneLocation = LegBoneTransform.GetLocation();
			LegBoneTransform.SetLocation(HipLocation + InDeltaRotation.RotateVector(BoneLocation - HipLocation));
		}

		return true;
	}

	return false;
}

static bool RotateLegByDeltaNormals(const FVector& InInitialDir, const FVector& InTargetDir, FAnimLegIKData& InLegData)
{
	if (!InInitialDir.IsZero() && !InInitialDir.Equals(InTargetDir))
	{
		// Find Delta Rotation take takes us from Old to New dir
		const FQuat DeltaRotation = FQuat::FindBetweenNormals(InInitialDir, InTargetDir);
		return RotateLegByQuat(DeltaRotation, InLegData);
	}

	return false;
}

void FAnimNode_LegIK::OrientLegTowardsIK(FAnimLegIKData& InLegData, USkeletalMeshComponent* SkelComp)
{
	check(InLegData.NumBones > 1);
	const FVector HipLocation = InLegData.FKLegBoneTransforms.Last().GetLocation();
	const FVector FootFKLocation = InLegData.FKLegBoneTransforms[0].GetLocation();
	const FVector FootIKLocation = InLegData.IKFootTransform.GetLocation();

	const FVector InitialDir = (FootFKLocation - HipLocation).GetSafeNormal();
	const FVector TargetDir = (FootIKLocation - HipLocation).GetSafeNormal();

	if (RotateLegByDeltaNormals(InitialDir, TargetDir, InLegData))
	{
#if ENABLE_ANIM_DEBUG
		const bool bShowDebug = (CVarAnimNodeLegIKDebug.GetValueOnAnyThread() == 1);
		if (bShowDebug)
		{
			DrawDebugLeg(InLegData, *SkelComp, SkelComp->GetWorld(), FColor::Green);
		}
#endif
	}
}

void FIKChain::InitializeFromLegData(const FAnimLegIKData& InLegData, USkeletalMeshComponent* InSkelMeshComp)
{
	Links.Reset(InLegData.NumBones);
	MaximumReach = 0.f;

	check(InLegData.NumBones > 1);
	for (int32 Index = 0; Index < InLegData.NumBones - 1; Index++)
	{
		const FVector BoneLocation = InLegData.FKLegBoneTransforms[Index].GetLocation();
		const FVector ParentLocation = InLegData.FKLegBoneTransforms[Index + 1].GetLocation();
		const float BoneLength = FVector::Dist(BoneLocation, ParentLocation);
		Links.Add(FIKChainLink(BoneLocation, BoneLength));
		MaximumReach += BoneLength;
	}

	// Add root bone last
	const FVector RootLocation = InLegData.FKLegBoneTransforms.Last().GetLocation();

	Links.Add(FIKChainLink(RootLocation, 0.f));
	NumLinks = Links.Num();
	check(NumLinks == InLegData.NumBones);

	if (InLegData.LegDefPtr != nullptr)
	{
		bEnableRotationLimit = InLegData.LegDefPtr->bEnableRotationLimit;
		if (bEnableRotationLimit)
		{
			MinRotationAngleRadians = FMath::DegreesToRadians(FMath::Clamp(InLegData.LegDefPtr->MinRotationAngle, 0.f, 90.f));
		}
	}

	SkelMeshComp = InSkelMeshComp;
	bInitialized = (SkelMeshComp != nullptr);
}

void FIKChain::ReachTarget(const FVector& InTargetLocation, float InReachPrecision, int32 InMaxIterations)
{
	if (!bInitialized)
	{
		return;
	}

	const FVector RootLocation = Links.Last().Location;

	// If we can't reach, we just go in a straight line towards the target,
	if ((NumLinks <= 2) || (FVector::DistSquared(RootLocation, InTargetLocation) >= FMath::Square(GetMaximumReach())))
	{
		const FVector Direction = (InTargetLocation - RootLocation).GetSafeNormal();
		OrientAllLinksToDirection(Direction);
	}
	// Do iterative approach based on FABRIK
	else
	{
		SolveFABRIK(InTargetLocation, InReachPrecision, InMaxIterations);
	}
}

void FIKChain::OrientAllLinksToDirection(const FVector& InDirection)
{
	for (int32 Index = Links.Num() - 2; Index >= 0; Index--)
	{
		Links[Index].Location = Links[Index + 1].Location + InDirection * Links[Index].Length;
	}
}

void FAnimNode_LegIK::DoLegReachIK(FAnimLegIKData& InLegData, USkeletalMeshComponent* SkelComp)
{
	SCOPE_CYCLE_COUNTER(STAT_LegIK_FABRIK_Eval);

	const FVector FootFKLocation = InLegData.FKLegBoneTransforms[0].GetLocation();
	const FVector FootIKLocation = InLegData.IKFootTransform.GetLocation();

	// If we're already reaching our IK Target, we have no work to do.
	if (FootFKLocation.Equals(FootIKLocation, ReachPrecision))
	{
		return;
	}

	FIKChain IKChain;
	IKChain.InitializeFromLegData(InLegData, SkelComp);

	const int32 MaxIterationsOverride = CVarAnimLegIKMaxIterations.GetValueOnAnyThread() > 0 ? CVarAnimLegIKMaxIterations.GetValueOnAnyThread() : MaxIterations;
	IKChain.ReachTarget(FootIKLocation, ReachPrecision, MaxIterationsOverride);

	// Update bone transforms based on IKChain

	// Rotations
	for (int32 LinkIndex = InLegData.NumBones - 2; LinkIndex >= 0; LinkIndex--)
	{
		const FIKChainLink& ParentLink = IKChain.Links[LinkIndex + 1];
		const FIKChainLink& CurrentLink = IKChain.Links[LinkIndex];

		FTransform& ParentTransform = InLegData.FKLegBoneTransforms[LinkIndex + 1];
		FTransform& CurrentTransform = InLegData.FKLegBoneTransforms[LinkIndex];

		// Calculate pre-translation vector between this bone and child
		const FVector InitialDir = (CurrentTransform.GetLocation() - ParentTransform.GetLocation()).GetSafeNormal();

		// Get vector from the post-translation bone to it's child
		const FVector TargetDir = (CurrentLink.Location - ParentLink.Location).GetSafeNormal();

		const FQuat DeltaRotation = FQuat::FindBetweenNormals(InitialDir, TargetDir);
		ParentTransform.SetRotation(DeltaRotation * ParentTransform.GetRotation());
	}

	// Translations
	for (int32 LinkIndex = InLegData.NumBones - 2; LinkIndex >= 0; LinkIndex--)
	{
		const FIKChainLink& CurrentLink = IKChain.Links[LinkIndex];
		FTransform& CurrentTransform = InLegData.FKLegBoneTransforms[LinkIndex];

		CurrentTransform.SetTranslation(CurrentLink.Location);
	}

#if ENABLE_ANIM_DEBUG
	const bool bShowDebug = (CVarAnimNodeLegIKDebug.GetValueOnAnyThread() == 1);
	if (bShowDebug)
	{
		DrawDebugLeg(InLegData, *SkelComp, SkelComp->GetWorld(), FColor::Yellow);
	}
#endif
}

void FIKChain::DrawDebugIKChain(const FIKChain& IKChain, const FColor& InColor)
{
#if ENABLE_DRAW_DEBUG
	if (IKChain.bInitialized && IKChain.SkelMeshComp)
	{
		for (int32 Index = 0; Index < IKChain.NumLinks - 1; Index++)
		{
			const FVector CurrentBoneWorldLoc = GetBoneWorldLocation(FTransform(IKChain.Links[Index].Location), *IKChain.SkelMeshComp);
			const FVector ParentBoneWorldLoc = GetBoneWorldLocation(FTransform(IKChain.Links[Index + 1].Location), *IKChain.SkelMeshComp);
			DrawDebugLine(IKChain.SkelMeshComp->GetWorld(), CurrentBoneWorldLoc, ParentBoneWorldLoc, InColor, false, -1.f, SDPG_Foreground, 1.f);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}

void FIKChain::FABRIK_ApplyLinkConstraints_Forward(FIKChain& IKChain, int32 LinkIndex)
{
	if ((LinkIndex <= 0) || (LinkIndex >= IKChain.NumLinks - 1))
	{
		return;
	}

	const FIKChainLink& ChildLink = IKChain.Links[LinkIndex - 1];
	const FIKChainLink& CurrentLink = IKChain.Links[LinkIndex];
	FIKChainLink& ParentLink = IKChain.Links[LinkIndex + 1];

	const FVector ChildAxisX = (ChildLink.Location - CurrentLink.Location).GetSafeNormal();
	const FVector ChildAxisY = CurrentLink.LinkAxisZ ^ ChildAxisX;
	const FVector ParentAxisX = (ParentLink.Location - CurrentLink.Location).GetSafeNormal();

	const float ParentCos = (ParentAxisX | ChildAxisX);
	const float ParentSin = (ParentAxisX | ChildAxisY);

	const bool bNeedsReorient = (ParentSin < 0.f) || (ParentCos > FMath::Cos(IKChain.MinRotationAngleRadians));

	// Parent Link needs to be reoriented.
	if (bNeedsReorient)
	{
		// folding over itself.
		if (ParentCos > 0.f)
		{
			// Enforce minimum angle.
			ParentLink.Location = CurrentLink.Location + CurrentLink.Length * (FMath::Cos(IKChain.MinRotationAngleRadians) * ChildAxisX + FMath::Sin(IKChain.MinRotationAngleRadians) * ChildAxisY);
		}
		else
		{
			// When opening up leg, allow it to extend in a full straight line.
			ParentLink.Location = CurrentLink.Location - ChildAxisX * CurrentLink.Length;
		}
	}
}

void FIKChain::FABRIK_ApplyLinkConstraints_Backward(FIKChain& IKChain, int32 LinkIndex)
{
	if ((LinkIndex <= 0) || (LinkIndex >= IKChain.NumLinks - 1))
	{
		return;
	}

	FIKChainLink& ChildLink = IKChain.Links[LinkIndex - 1];
	const FIKChainLink& CurrentLink = IKChain.Links[LinkIndex];
	const FIKChainLink& ParentLink = IKChain.Links[LinkIndex + 1];

	const FVector ParentAxisX = (ParentLink.Location - CurrentLink.Location).GetSafeNormal();
	const FVector ParentAxisY = CurrentLink.LinkAxisZ ^ ParentAxisX;
	const FVector ChildAxisX = (ChildLink.Location - CurrentLink.Location).GetSafeNormal();

	const float ChildCos = (ChildAxisX | ParentAxisX);
	const float ChildSin = (ChildAxisX | ParentAxisY);

	const bool bNeedsReorient = (ChildSin > 0.f) || (ChildCos > FMath::Cos(IKChain.MinRotationAngleRadians));

	// Parent Link needs to be reoriented.
	if (bNeedsReorient)
	{
		// folding over itself.
		if (ChildCos > 0.f)
		{
			// Enforce minimum angle.
			ChildLink.Location = CurrentLink.Location + ChildLink.Length * (FMath::Cos(IKChain.MinRotationAngleRadians) * ParentAxisX - FMath::Sin(IKChain.MinRotationAngleRadians) * ParentAxisY);
		}
		else
		{
			// When opening up leg, allow it to extend in a full straight line.
			ChildLink.Location = CurrentLink.Location - ParentAxisX * ChildLink.Length;
		}
	}
}

void FIKChain::FABRIK_ForwardReach(const FVector& InTargetLocation, FIKChain& IKChain)
{
	// Move end effector towards target
	// If we are compressing the chain, limit displacement.
	// Due to how FABRIK works, if we push the target past the parent's joint, we flip the bone.
	{
		FVector EndEffectorToTarget = InTargetLocation - IKChain.Links[0].Location;

		FVector EndEffectorToTargetDir;
		float EndEffectToTargetSize;
		EndEffectorToTarget.ToDirectionAndLength(EndEffectorToTargetDir, EndEffectToTargetSize);

		const float ReachStepAlpha = FMath::Clamp(CVarAnimLegIKTargetReachStepPercent.GetValueOnAnyThread(), 0.01f, 0.99f);

		float Displacement = EndEffectToTargetSize;
		for (int32 LinkIndex = 1; LinkIndex < IKChain.NumLinks; LinkIndex++)
		{
			FVector EndEffectorToParent = IKChain.Links[LinkIndex].Location - IKChain.Links[0].Location;
			float ParentDisplacement = (EndEffectorToParent | EndEffectorToTargetDir);

			Displacement = (ParentDisplacement > 0.f) ? FMath::Min(Displacement, ParentDisplacement * ReachStepAlpha) : Displacement;
		}

		IKChain.Links[0].Location += EndEffectorToTargetDir * Displacement;
	}

	// "Forward Reaching" stage - adjust bones from end effector.
	for (int32 LinkIndex = 1; LinkIndex < IKChain.NumLinks; LinkIndex++)
	{
		FIKChainLink& ChildLink = IKChain.Links[LinkIndex - 1];
		FIKChainLink& CurrentLink = IKChain.Links[LinkIndex];

		CurrentLink.Location = ChildLink.Location + (CurrentLink.Location - ChildLink.Location).GetSafeNormal() * ChildLink.Length;

		if (IKChain.bEnableRotationLimit)
		{
			FABRIK_ApplyLinkConstraints_Forward(IKChain, LinkIndex);
		}
	}
}

void FIKChain::FABRIK_BackwardReach(const FVector& InRootTargetLocation, FIKChain& IKChain)
{
	// Move Root back towards RootTarget
	// If we are compressing the chain, limit displacement.
	// Due to how FABRIK works, if we push the target past the parent's joint, we flip the bone.
	{
		FVector RootToRootTarget = InRootTargetLocation - IKChain.Links.Last().Location;

		FVector RootToRootTargetDir;
		float RootToRootTargetSize;
		RootToRootTarget.ToDirectionAndLength(RootToRootTargetDir, RootToRootTargetSize);

		const float ReachStepAlpha = FMath::Clamp(CVarAnimLegIKTargetReachStepPercent.GetValueOnAnyThread(), 0.01f, 0.99f);

		float Displacement = RootToRootTargetSize;
		for (int32 LinkIndex = IKChain.NumLinks - 2; LinkIndex >= 0; LinkIndex--)
		{
			FVector RootToChild = IKChain.Links[IKChain.NumLinks - 2].Location - IKChain.Links.Last().Location;
			float ChildDisplacement = (RootToChild | RootToRootTargetDir);

			Displacement = (ChildDisplacement > 0.f) ? FMath::Min(Displacement, ChildDisplacement * ReachStepAlpha) : Displacement;
		}

		IKChain.Links.Last().Location += RootToRootTargetDir * Displacement;
	}

	// "Backward Reaching" stage - adjust bones from root.
	for (int32 LinkIndex = IKChain.NumLinks - 1; LinkIndex >= 1; LinkIndex--)
	{
		FIKChainLink& CurrentLink = IKChain.Links[LinkIndex];
		FIKChainLink& ChildLink = IKChain.Links[LinkIndex - 1];

		ChildLink.Location = CurrentLink.Location + (ChildLink.Location - CurrentLink.Location).GetSafeNormal() * ChildLink.Length;

		if (IKChain.bEnableRotationLimit)
		{
			FABRIK_ApplyLinkConstraints_Backward(IKChain, LinkIndex);
		}
	}
}

static FVector FindPlaneNormal(const TArray<FIKChainLink>& Links, const FVector& RootLocation, const FVector& TargetLocation)
{
	const FVector AxisX = (TargetLocation - RootLocation).GetSafeNormal();

	for (int32 LinkIndex = Links.Num() - 2; LinkIndex >= 0; LinkIndex--)
	{
		const FVector AxisY = (Links[LinkIndex].Location - RootLocation).GetSafeNormal();
		const FVector PlaneNormal = AxisX ^ AxisY;

		// Make sure we have a valid normal (Axes were not coplanar).
		if (PlaneNormal.SizeSquared() > SMALL_NUMBER)
		{
			return PlaneNormal.GetUnsafeNormal();
		}
	}

	// All links are co-planar?
	return FVector::UpVector;
}

TAutoConsoleVariable<int32> CVarAnimLegIKAveragePull(TEXT("a.AnimNode.LegIK.AveragePull"), 1, TEXT("Leg IK AveragePull"));

void FIKChain::SolveFABRIK(const FVector& InTargetLocation, float InReachPrecision, int32 InMaxIterations)
{
	// Make sure precision is not too small.
	const float ReachPrecision = FMath::Max(InReachPrecision, KINDA_SMALL_NUMBER);

	const FVector RootTargetLocation = Links.Last().Location;
	const float PullDistributionAlpha = FMath::Clamp(CVarAnimLegIKPullDistribution.GetValueOnAnyThread(), 0.f, 1.f);

	// Check distance between foot and foot target location
	float Slop = FVector::Dist(Links[0].Location, InTargetLocation);
	if (Slop > ReachPrecision)
	{
		if (bEnableRotationLimit)
		{
			// Since we've previously aligned the foot with the IK Target, we're solving IK in 2D space on a single plane.
			// Find Plane Normal, to use in rotation constraints.
			const FVector PlaneNormal = FindPlaneNormal(Links, RootTargetLocation, InTargetLocation);

			for (int32 LinkIndex = 1; LinkIndex < (NumLinks - 1); LinkIndex++)
			{
				const FIKChainLink& ChildLink = Links[LinkIndex - 1];
				FIKChainLink& CurrentLink = Links[LinkIndex];
				const FIKChainLink& ParentLink = Links[LinkIndex + 1];

				const FVector ChildAxisX = (ChildLink.Location - CurrentLink.Location).GetSafeNormal();
				const FVector ChildAxisY = PlaneNormal ^ ChildAxisX;
				const FVector ParentAxisX = (ParentLink.Location - CurrentLink.Location).GetSafeNormal();

				// Orient Z, so that ChildAxisY points 'up' and produces positive Sin values.
				CurrentLink.LinkAxisZ = (ParentAxisX | ChildAxisY) > 0.f ? PlaneNormal : -PlaneNormal;
			}
		}

#if ENABLE_ANIM_DEBUG
		const bool bShowDebug = (CVarAnimNodeLegIKDebug.GetValueOnAnyThread() == 1);
		if (bShowDebug)
		{
			DrawDebugIKChain(*this, FColor::Magenta);
		}
#endif

		// Re-position limb to distribute pull
		const FVector PullDistributionOffset = PullDistributionAlpha * (InTargetLocation - Links[0].Location) + (1.f - PullDistributionAlpha) * (RootTargetLocation - Links.Last().Location);
		for (int32 LinkIndex = 0; LinkIndex < NumLinks; LinkIndex++)
		{
			Links[LinkIndex].Location += PullDistributionOffset;
		}

		int32 IterationCount = 1;
		const int32 MaxIterations = FMath::Max(InMaxIterations, 1);
		do
		{
			const float PreviousSlop = Slop;

#if ENABLE_ANIM_DEBUG
			bool bDrawDebug = bShowDebug && (IterationCount == (MaxIterations - 1));
			if (bDrawDebug) { DrawDebugIKChain(*this, FColor::Red); }
#endif

			if ((CVarAnimLegIKAveragePull.GetValueOnAnyThread() == 1) && (Slop > 1.f))
			{
				FIKChain ForwardPull = *this;
				FABRIK_ForwardReach(InTargetLocation, ForwardPull);

				FIKChain BackwardPull = *this;
				FABRIK_BackwardReach(RootTargetLocation, BackwardPull);

				// Average pulls
				for (int32 LinkIndex = 0; LinkIndex < NumLinks; LinkIndex++)
				{
					Links[LinkIndex].Location = 0.5f * (ForwardPull.Links[LinkIndex].Location + BackwardPull.Links[LinkIndex].Location);
				}

#if ENABLE_ANIM_DEBUG
				if (bDrawDebug)
				{
					DrawDebugIKChain(ForwardPull, FColor::Green);
					DrawDebugIKChain(BackwardPull, FColor::Blue);
				}
#endif
			}
			else
			{
				FABRIK_ForwardReach(InTargetLocation, *this);

#if ENABLE_ANIM_DEBUG
				if (bDrawDebug) { DrawDebugIKChain(*this, FColor::Green); }
#endif

				FABRIK_BackwardReach(RootTargetLocation, *this);
#if ENABLE_ANIM_DEBUG
				if (bDrawDebug) { DrawDebugIKChain(*this, FColor::Blue); }
#endif
			}

			Slop = FVector::Dist(Links[0].Location, InTargetLocation) + FVector::Dist(Links.Last().Location, RootTargetLocation);

			// Abort if we're not getting closer and enter a deadlock.
			if (Slop > PreviousSlop)
			{
				break;
			}

		} while ((Slop > ReachPrecision) && (++IterationCount < MaxIterations));

		// Make sure our root is back at our root target.
		if (!Links.Last().Location.Equals(RootTargetLocation))
		{
			FABRIK_BackwardReach(RootTargetLocation, *this);
		}

		// If we reached, set target precisely
		if (Slop <= ReachPrecision)
		{
			Links[0].Location = InTargetLocation;
		}

#if ENABLE_ANIM_DEBUG
		if (bShowDebug)
		{
			DrawDebugIKChain(*this, FColor::Yellow);

			FString DebugString = FString::Printf(TEXT("FABRIK IterationCount: [%d]/[%d], Slop: [%f]/[%f]")
				, IterationCount, MaxIterations, Slop, ReachPrecision);
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0.f, FColor::Red, DebugString, false);
		}
#endif
	}
}

void FAnimNode_LegIK::AdjustKneeTwist(FAnimLegIKData& InLegData, USkeletalMeshComponent* SkelComp)
{
	const FVector FootFKLocation = InLegData.FKLegBoneTransforms[0].GetLocation();
	const FVector FootIKLocation = InLegData.IKFootTransform.GetLocation();

	const FVector HipLocation = InLegData.FKLegBoneTransforms.Last().GetLocation();
	const FVector FootAxisZ = (FootIKLocation - HipLocation).GetSafeNormal();

	FVector FootFKAxisX = InLegData.FKLegBoneTransforms[0].GetUnitAxis(InLegData.LegDefPtr->FootBoneForwardAxis);
	FVector FootIKAxisX = InLegData.IKFootTransform.GetUnitAxis(InLegData.LegDefPtr->FootBoneForwardAxis);

	// Reorient X Axis to be perpendicular with FootAxisZ
	FootFKAxisX = ((FootAxisZ ^ FootFKAxisX) ^ FootAxisZ);
	FootIKAxisX = ((FootAxisZ ^ FootIKAxisX) ^ FootAxisZ);

	// Compare Axis X to see if we need a rotation to be performed
	if (RotateLegByDeltaNormals(FootFKAxisX, FootIKAxisX, InLegData))
	{
#if ENABLE_ANIM_DEBUG
		const bool bShowDebug = (CVarAnimNodeLegIKDebug.GetValueOnAnyThread() == 1);
		if (bShowDebug)
		{
			DrawDebugLeg(InLegData, *SkelComp, SkelComp->GetWorld(), FColor::Magenta);
		}
#endif
	}
}

bool FAnimNode_LegIK::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones)
{
	const bool bIsEnabled = (CVarAnimLegIKEnable.GetValueOnAnyThread() == 1);
	return bIsEnabled && (LegsData.Num() > 0);
}

static void PopulateLegBoneIndices(FAnimLegIKData& InLegData, const FCompactPoseBoneIndex& InFootBoneIndex, const int32& NumBonesInLimb, const FBoneContainer& RequiredBones)
{
	FCompactPoseBoneIndex BoneIndex = InFootBoneIndex;
	if (BoneIndex != INDEX_NONE)
	{
		InLegData.FKLegBoneIndices.Add(BoneIndex);
		FCompactPoseBoneIndex ParentBoneIndex = RequiredBones.GetParentBoneIndex(BoneIndex);

		int32 NumIterations = NumBonesInLimb;
		while ((NumIterations-- > 0) && (ParentBoneIndex != INDEX_NONE))
		{
			BoneIndex = ParentBoneIndex;
			InLegData.FKLegBoneIndices.Add(BoneIndex);
			ParentBoneIndex = RequiredBones.GetParentBoneIndex(BoneIndex);
		};
	}
}

void FAnimNode_LegIK::InitializeBoneReferences(const FBoneContainer& RequiredBones)
{
	LegsData.Reset();
	for (auto& LegDef : LegsDefinition)
	{
		LegDef.IKFootBone.Initialize(RequiredBones);
		LegDef.FKFootBone.Initialize(RequiredBones);

		FAnimLegIKData LegData;
		LegData.IKFootBoneIndex = LegDef.IKFootBone.GetCompactPoseIndex(RequiredBones);
		const FCompactPoseBoneIndex FKFootBoneIndex = LegDef.FKFootBone.GetCompactPoseIndex(RequiredBones);

		if ((LegData.IKFootBoneIndex != INDEX_NONE) && (FKFootBoneIndex != INDEX_NONE))
		{
			PopulateLegBoneIndices(LegData, FKFootBoneIndex, FMath::Max(LegDef.NumBonesInLimb, 1), RequiredBones);

			// We need at least three joints for this to work (hip, knee and foot).
			if (LegData.FKLegBoneIndices.Num() >= 3)
			{
				LegData.NumBones = LegData.FKLegBoneIndices.Num();
				LegData.LegDefPtr = &LegDef;
				LegsData.Add(LegData);
			}
		}
	}
}
