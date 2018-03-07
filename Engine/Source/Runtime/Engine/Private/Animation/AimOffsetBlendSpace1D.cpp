// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AimOffsetBlendSpace1D.cpp: AimOffsetBlendSpace functionality
=============================================================================*/ 

#include "Animation/AimOffsetBlendSpace1D.h"

UAimOffsetBlendSpace1D::UAimOffsetBlendSpace1D(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRotationBlendInMeshSpace = true;
}

bool UAimOffsetBlendSpace1D::IsValidAdditiveType(EAdditiveAnimationType AdditiveType) const
{
	return (AdditiveType == AAT_RotationOffsetMeshSpace);
}

bool UAimOffsetBlendSpace1D::IsValidAdditive() const
{
	return ContainsMatchingSamples(AAT_RotationOffsetMeshSpace);
}
