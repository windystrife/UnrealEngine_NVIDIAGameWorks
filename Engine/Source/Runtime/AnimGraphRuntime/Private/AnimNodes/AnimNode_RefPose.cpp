// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_RefPose.h"

/////////////////////////////////////////////////////
// FAnimRefPoseNode

void FAnimNode_RefPose::Evaluate_AnyThread(FPoseContext& Output)
{
	// I don't have anything to evaluate. Should this be even here?
	// EvaluateGraphExposedInputs.Execute(Context);

	switch (RefPoseType) 
	{
	case EIT_LocalSpace:
		Output.ResetToRefPose();
		break;

	case EIT_Additive:
	default:
		Output.ResetToAdditiveIdentity();
		break;
	}
}

void FAnimNode_MeshSpaceRefPose::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	Output.ResetToRefPose();
}

/** Helper for enum output... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

const TCHAR* GetRefPostTypeText(ERefPoseType RefPose)
{
	switch (RefPose)
	{
		FOREACH_ENUM_EREFPOSETYPE(CASE_ENUM_TO_TEXT)
	}
	return TEXT("Unknown Ref Pose Type");
}

void FAnimNode_RefPose::GatherDebugData(FNodeDebugData& DebugData)
{
	FString DebugLine = DebugData.GetNodeName(this);
	DebugLine += FString::Printf(TEXT("(Ref Pose Type: %s)"), GetRefPostTypeText(RefPoseType));
	DebugData.AddDebugItem(DebugLine, true);
}
