// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNodeSpaceConversions.h"

/////////////////////////////////////////////////////
// FAnimNode_ConvertComponentToLocalSpace

FAnimNode_ConvertComponentToLocalSpace::FAnimNode_ConvertComponentToLocalSpace()
{
}

void FAnimNode_ConvertComponentToLocalSpace::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	ComponentPose.Initialize(Context);
}

void FAnimNode_ConvertComponentToLocalSpace::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	ComponentPose.CacheBones(Context);
}

void FAnimNode_ConvertComponentToLocalSpace::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	ComponentPose.Update(Context);
}

void FAnimNode_ConvertComponentToLocalSpace::Evaluate_AnyThread(FPoseContext & Output)
{
	// Evaluate the child and convert
	FComponentSpacePoseContext InputCSPose(Output.AnimInstanceProxy);
	ComponentPose.EvaluateComponentSpace(InputCSPose);

	checkSlow( InputCSPose.Pose.GetPose().IsValid() );
	InputCSPose.Pose.ConvertToLocalPoses(Output.Pose);
	Output.Curve = InputCSPose.Curve;
}

void FAnimNode_ConvertComponentToLocalSpace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
	ComponentPose.GatherDebugData(DebugData);
}

/////////////////////////////////////////////////////
// FAnimNode_ConvertLocalToComponentSpace

FAnimNode_ConvertLocalToComponentSpace::FAnimNode_ConvertLocalToComponentSpace()
{
}

void FAnimNode_ConvertLocalToComponentSpace::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	LocalPose.Initialize(Context);
}

void FAnimNode_ConvertLocalToComponentSpace::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	LocalPose.CacheBones(Context);
}

void FAnimNode_ConvertLocalToComponentSpace::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	LocalPose.Update(Context);
}

void FAnimNode_ConvertLocalToComponentSpace::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugData.AddDebugItem(DebugLine);
	LocalPose.GatherDebugData(DebugData);
}

void FAnimNode_ConvertLocalToComponentSpace::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext & OutputCSPose)
{
	// Evaluate the child and convert
	FPoseContext InputPose(OutputCSPose.AnimInstanceProxy);
	LocalPose.Evaluate(InputPose);

	OutputCSPose.Pose.InitPose(InputPose.Pose);
	OutputCSPose.Curve = InputPose.Curve;
}
