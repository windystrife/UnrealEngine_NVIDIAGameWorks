// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_BlendListByBool.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendListByBool

int32 FAnimNode_BlendListByBool::GetActiveChildIndex()
{
	// Note: Intentionally flipped boolean sense (the true input is #0, and the false input is #1)
	return bActiveValue ? 0 : 1;
}
