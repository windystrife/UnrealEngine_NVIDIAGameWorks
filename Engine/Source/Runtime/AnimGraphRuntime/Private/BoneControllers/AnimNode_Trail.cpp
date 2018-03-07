// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneControllers/AnimNode_Trail.h"
#include "Animation/AnimInstanceProxy.h"

/////////////////////////////////////////////////////
// FAnimNode_Trail

DECLARE_CYCLE_STAT(TEXT("Trail Eval"), STAT_Trail_Eval, STATGROUP_Anim);

FAnimNode_Trail::FAnimNode_Trail()
	: ChainLength(2)
	, ChainBoneAxis(EAxis::X)
	, bInvertChainBoneAxis(false)
	, TrailRelaxation_DEPRECATED(10.f)
	, bLimitStretch(false)
	, StretchLimit(0)
	, FakeVelocity(FVector::ZeroVector)
	, bActorSpaceFakeVel(false)
	, bHadValidStrength(false)
{
	FRichCurve* TrailRelaxRichCurve = TrailRelaxationSpeed.GetRichCurve();
	TrailRelaxRichCurve->AddKey(0.f, 10.f);
	TrailRelaxRichCurve->AddKey(1.f, 5.f);
}

void FAnimNode_Trail::UpdateInternal(const FAnimationUpdateContext& Context)
{
	FAnimNode_SkeletalControlBase::UpdateInternal(Context);

	ThisTimstep = Context.GetDeltaTime();
}

void FAnimNode_Trail::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);

	DebugLine += "(";
	AddDebugNodeData(DebugLine);
	DebugLine += FString::Printf(TEXT(" Active: %s)"), *TrailBone.BoneName.ToString());
	DebugData.AddDebugItem(DebugLine);

	ComponentPose.GatherDebugData(DebugData);
}

void FAnimNode_Trail::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output, TArray<FBoneTransform>& OutBoneTransforms)
{
	SCOPE_CYCLE_COUNTER(STAT_Trail_Eval);

	check(OutBoneTransforms.Num() == 0);

	if( ChainBoneIndices.Num() <= 0 )
	{
		return;
	}

	checkSlow (ChainBoneIndices.Num() == ChainLength);
	checkSlow (PerJointTrailData.Num() == ChainLength);
	// The incoming BoneIndex is the 'end' of the spline chain. We need to find the 'start' by walking SplineLength bones up hierarchy.
	// Fail if we walk past the root bone.
	const FBoneContainer& BoneContainer = Output.Pose.GetPose().GetBoneContainer();
	const FTransform& ComponentTransform = Output.AnimInstanceProxy->GetComponentTransform();
	FTransform BaseTransform;
 	if (BaseJoint.IsValidToEvaluate(BoneContainer))
 	{
		FCompactPoseBoneIndex BasePoseIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BaseJoint.BoneIndex));
		FTransform BaseBoneTransform = Output.Pose.GetComponentSpaceTransform(BasePoseIndex);
 		BaseTransform = BaseBoneTransform * ComponentTransform;
 	}
	else
	{
		BaseTransform = ComponentTransform;
	}

	OutBoneTransforms.AddZeroed(ChainLength);

	// this should be checked outside
	checkSlow (TrailBone.IsValidToEvaluate(BoneContainer));

	// If we have >0 this frame, but didn't last time, record positions of all the bones.
	// Also do this if number has changed or array is zero.
	//@todo I don't think this will work anymore. if Alpha is too small, it won't call evaluate anyway
	// so this has to change. AFAICT, this will get called only FIRST TIME
	bool bHasValidStrength = (Alpha > 0.f);
	if(bHasValidStrength && !bHadValidStrength)
	{
		for(int32 i=0; i<ChainBoneIndices.Num(); i++)
		{
			if (BoneContainer.Contains(ChainBoneIndices[i]))
			{
				FCompactPoseBoneIndex ChildIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(ChainBoneIndices[i]));
				const FTransform& ChainTransform = Output.Pose.GetComponentSpaceTransform(ChildIndex);
				TrailBoneLocations[i] = ChainTransform.GetTranslation();
			}
			else
			{
				TrailBoneLocations[i] = FVector::ZeroVector;
			}
		}
		OldBaseTransform = BaseTransform;
	}
	bHadValidStrength = bHasValidStrength;

	// transform between last frame and now.
	FTransform OldToNewTM = OldBaseTransform.GetRelativeTransform(BaseTransform);

	// Add fake velocity if present to all but root bone
	if(!FakeVelocity.IsZero())
	{
		FVector FakeMovement = -FakeVelocity * ThisTimstep;

		if (bActorSpaceFakeVel)
		{
			FTransform BoneToWorld(Output.AnimInstanceProxy->GetActorTransform());
			BoneToWorld.RemoveScaling();
			FakeMovement = BoneToWorld.TransformVector(FakeMovement);
		}

		FakeMovement = BaseTransform.InverseTransformVector(FakeMovement);
		// Then add to each bone
		for(int32 i=1; i<TrailBoneLocations.Num(); i++)
		{
			TrailBoneLocations[i] += FakeMovement;
		}
	}

	// Root bone of trail is not modified.
	FCompactPoseBoneIndex RootIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(ChainBoneIndices[0])); 
	const FTransform& ChainTransform = Output.Pose.GetComponentSpaceTransform(RootIndex);
	OutBoneTransforms[0] = FBoneTransform(RootIndex, ChainTransform);
	TrailBoneLocations[0] = ChainTransform.GetTranslation();

	// Starting one below head of chain, move bones.
	// this Parent/Child relationship is backward. From start joint (from bottom) to end joint(higher parent )
	for(int32 i=1; i<ChainBoneIndices.Num(); i++)
	{
		// Parent bone position in component space.
		FCompactPoseBoneIndex ParentIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(ChainBoneIndices[i - 1]));
		FVector ParentPos = TrailBoneLocations[i-1];
		FVector ParentAnimPos = Output.Pose.GetComponentSpaceTransform(ParentIndex).GetTranslation();

		// Child bone position in component space.
		FCompactPoseBoneIndex ChildIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(ChainBoneIndices[i]));
		FVector ChildPos = OldToNewTM.TransformPosition(TrailBoneLocations[i]); // move from 'last frames component' frame to 'this frames component' frame
		FVector ChildAnimPos = Output.Pose.GetComponentSpaceTransform(ChildIndex).GetTranslation();

		// Desired parent->child offset.
		FVector TargetDelta = (ChildAnimPos - ParentAnimPos);

		// Desired child position.
		FVector ChildTarget = ParentPos + TargetDelta;

		// Find vector from child to target
		FVector Error = (ChildTarget - ChildPos);

		// Calculate how much to push the child towards its target
		float Correction = FMath::Clamp<float>(ThisTimstep * PerJointTrailData[i].TrailRelaxationSpeedPerSecond, 0.f, 1.f);

		// Scale correction vector and apply to get new world-space child position.
		TrailBoneLocations[i] = ChildPos + (Error * Correction);

		// If desired, prevent bones stretching too far.
		if(bLimitStretch)
		{
			float RefPoseLength = TargetDelta.Size();
			FVector CurrentDelta = TrailBoneLocations[i] - TrailBoneLocations[i-1];
			float CurrentLength = CurrentDelta.Size();

			// If we are too far - cut it back (just project towards parent particle).
			if( (CurrentLength - RefPoseLength > StretchLimit) && CurrentLength > SMALL_NUMBER )
			{
				FVector CurrentDir = CurrentDelta / CurrentLength;
				TrailBoneLocations[i] = TrailBoneLocations[i-1] + (CurrentDir * (RefPoseLength + StretchLimit));
			}
		}

		// Modify child matrix
		OutBoneTransforms[i] = FBoneTransform(ChildIndex, Output.Pose.GetComponentSpaceTransform(ChildIndex));
		OutBoneTransforms[i].Transform.SetTranslation(TrailBoneLocations[i]);

		// Modify rotation of parent matrix to point at this one.

		// Calculate the direction that parent bone is currently pointing.
		FVector CurrentBoneDir = OutBoneTransforms[i-1].Transform.TransformVector( GetAlignVector(ChainBoneAxis, bInvertChainBoneAxis) );
		CurrentBoneDir = CurrentBoneDir.GetSafeNormal(SMALL_NUMBER);

		// Calculate vector from parent to child.
		FVector NewBoneDir = FVector(OutBoneTransforms[i].Transform.GetTranslation() - OutBoneTransforms[i - 1].Transform.GetTranslation()).GetSafeNormal(SMALL_NUMBER);

		// Calculate a quaternion that gets us from our current rotation to the desired one.
		FQuat DeltaLookQuat = FQuat::FindBetweenNormals(CurrentBoneDir, NewBoneDir);
		FTransform DeltaTM( DeltaLookQuat, FVector(0.f) );

		// Apply to the current parent bone transform.
		FTransform TmpTransform = FTransform::Identity;
		TmpTransform.CopyRotationPart(OutBoneTransforms[i - 1].Transform);
		TmpTransform = TmpTransform * DeltaTM;
		OutBoneTransforms[i - 1].Transform.CopyRotationPart(TmpTransform);
	}

	// For the last bone in the chain, use the rotation from the bone above it.
	OutBoneTransforms[ChainLength - 1].Transform.CopyRotationPart(OutBoneTransforms[ChainLength - 2].Transform);

	// Update OldBaseTransform
	OldBaseTransform = BaseTransform;
}

bool FAnimNode_Trail::IsValidToEvaluate(const USkeleton* Skeleton, const FBoneContainer& RequiredBones) 
{
	// if bones are valid
	if (TrailBone.IsValidToEvaluate(RequiredBones))
	{
		for (auto& ChainIndex : ChainBoneIndices)
		{
			if (ChainIndex == INDEX_NONE)
			{
				// unfortunately there is no easy way to communicate this back to user other than spamming here because this gets called every frame
				// originally tried in AnimGraphNode, but that doesn't know hierarchy so I can't verify it there. Maybe should try with USkeleton asset there. @todo
				return false;
			}
			else if (RequiredBones.Contains(ChainIndex) == false)
			{
				return false;
			}
		}
	}

	return true;
}

void FAnimNode_Trail::InitializeBoneReferences(const FBoneContainer& RequiredBones) 
{
	TrailBone.Initialize(RequiredBones);
	BaseJoint.Initialize(RequiredBones);

	// initialize chain bone indices
	ChainBoneIndices.Reset();
	if (ChainLength > 2 && TrailBone.IsValidToEvaluate(RequiredBones))
	{
		ChainBoneIndices.AddZeroed(ChainLength);

		int32 WalkBoneIndex = TrailBone.BoneIndex;
		ChainBoneIndices[ChainLength - 1] = WalkBoneIndex;

		for(int32 i = 1; i < ChainLength; i++)
		{
			//Insert indices at the start of array, so that parents are before children in the array.
			int32 TransformIndex = ChainLength - (i + 1);

			// if reached to root or invalid, invalidate the data
			if(WalkBoneIndex == INDEX_NONE || WalkBoneIndex == 0)
			{
				ChainBoneIndices[TransformIndex] = INDEX_NONE;
			}
			else
			{
				// Get parent bone.
				WalkBoneIndex = RequiredBones.GetParentBoneIndex(WalkBoneIndex);
				ChainBoneIndices[TransformIndex] = WalkBoneIndex;
			}
		}
	}
}

FVector FAnimNode_Trail::GetAlignVector(EAxis::Type AxisOption, bool bInvert)
{
	FVector AxisDir;

	if (AxisOption == EAxis::X)
	{
		AxisDir = FVector(1, 0, 0);
	}
	else if (AxisOption == EAxis::Y)
	{
		AxisDir = FVector(0, 1, 0);
	}
	else
	{
		AxisDir = FVector(0, 0, 1);
	}

	if (bInvert)
	{
		AxisDir *= -1.f;
	}

	return AxisDir;
}

void FAnimNode_Trail::PostLoad()
{
	if (TrailRelaxation_DEPRECATED != 10.f)
	{
		FRichCurve* TrailRelaxRichCurve = TrailRelaxationSpeed.GetRichCurve();
		TrailRelaxRichCurve->Reset();
		TrailRelaxRichCurve->AddKey(0.f, TrailRelaxation_DEPRECATED);
		TrailRelaxRichCurve->AddKey(1.f, TrailRelaxation_DEPRECATED);
		// since we don't know if it's same as default or not, we have to keep default
		// if default, the default constructor will take care of it. If not, we'll reset
		TrailRelaxation_DEPRECATED = 10.f;
	}
}
void FAnimNode_Trail::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_SkeletalControlBase::Initialize_AnyThread(Context);

	// allocated all memory here in initialize
	PerJointTrailData.Reset();
	TrailBoneLocations.Reset();
	if(ChainLength > 2)
	{
		PerJointTrailData.AddZeroed(ChainLength);
		TrailBoneLocations.AddZeroed(ChainLength);

		float Interval = (ChainLength > 1)? (1.f/(ChainLength-1)) : 0.f;
		const FRichCurve* TrailRelaxRichCurve = TrailRelaxationSpeed.GetRichCurveConst();
		check(TrailRelaxRichCurve);
		for(int32 Idx=0; Idx<ChainLength; ++Idx)
		{
			PerJointTrailData[Idx].TrailRelaxationSpeedPerSecond = TrailRelaxRichCurve->Eval(Interval * Idx);
		}
	}
}
