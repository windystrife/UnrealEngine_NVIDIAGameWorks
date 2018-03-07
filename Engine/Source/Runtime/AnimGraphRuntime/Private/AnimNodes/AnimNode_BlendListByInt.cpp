// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_BlendListByInt.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendListByInt

int32 FAnimNode_BlendListByInt::GetActiveChildIndex()
{
	const int NumPoses = BlendPose.Num();
	return FMath::Clamp<int32>(ActiveChildIndex, 0, NumPoses - 1);
}
