// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AimOffsetBlendSpace.cpp: AimOffsetBlendSpace functionality
=============================================================================*/ 

#include "Animation/AimOffsetBlendSpace.h"

UAimOffsetBlendSpace::UAimOffsetBlendSpace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRotationBlendInMeshSpace = true;
}

bool UAimOffsetBlendSpace::IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const
{
	return (AdditiveType == AAT_RotationOffsetMeshSpace);
}

bool UAimOffsetBlendSpace::IsValidAdditive() const
{
	return ContainsMatchingSamples(AAT_RotationOffsetMeshSpace);
}
