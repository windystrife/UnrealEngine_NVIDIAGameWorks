// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

namespace ImmediatePhysics
{
/** Holds shape data*/
struct FKinematicTarget
{
#if WITH_PHYSX
	PxTransform BodyToWorld;
#endif
	bool bTargetSet;
	
	FKinematicTarget()
		: bTargetSet(false)
	{
	}
};

}