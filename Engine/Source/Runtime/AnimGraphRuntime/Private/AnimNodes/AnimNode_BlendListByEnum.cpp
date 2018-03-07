// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_BlendListByEnum.h"

/////////////////////////////////////////////////////
// FAnimNode_BlendListByEnum

int32 FAnimNode_BlendListByEnum::GetActiveChildIndex()
{
	if (EnumToPoseIndex.IsValidIndex(ActiveEnumValue))
	{
		return EnumToPoseIndex[ActiveEnumValue];
	}
	else
	{
		return 0;
	}
}

