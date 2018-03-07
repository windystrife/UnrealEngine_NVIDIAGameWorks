
// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationHierarchy.h"

FTransformConstraint* FConstraintNodeData::FindConstraint(const FName& TargetNode)
{
	for (int32 ConstraintIdx = 0; ConstraintIdx < Constraints.Num(); ++ConstraintIdx)
	{
		if (Constraints[ConstraintIdx].TargetNode == TargetNode)
		{
			return &Constraints[ConstraintIdx];
		}
	}

	return nullptr;
}

void FConstraintNodeData::AddConstraint(const FTransformConstraint& TransformConstraint)
{
	FTransformConstraint* ExistingConstraint = FindConstraint(TransformConstraint.TargetNode);
	if (ExistingConstraint)
	{
		*ExistingConstraint = TransformConstraint;
	}
	else
	{
		Constraints.Add(TransformConstraint);
	}
}

void FConstraintNodeData::DeleteConstraint(const FName& TargetNode)
{
	for (int32 ConstraintIdx = 0; ConstraintIdx < Constraints.Num(); ++ConstraintIdx)
	{
		if (Constraints[ConstraintIdx].TargetNode == TargetNode)
		{
			Constraints.RemoveAt(ConstraintIdx);
			--ConstraintIdx;
		}
	}
}