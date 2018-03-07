// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

#include "ImmediatePhysicsMaterial.h"

namespace ImmediatePhysics
{
/** Holds shape data*/
struct FShape
{
#if WITH_PHYSX
	const PxTransform LocalTM;
	const FMaterial Material;
	const PxGeometry* Geometry;
	const PxVec3 BoundsOffset;
	const float BoundsMagnitude;

	FShape(const PxTransform& InLocalTM, const PxVec3& InBoundsOffset, const float InBoundsMagnitude, const PxGeometry* InGeometry, const FMaterial& InMaterial)
		: LocalTM(InLocalTM)
		, Material(InMaterial)
		, Geometry(InGeometry)
		, BoundsOffset(InBoundsOffset)
		, BoundsMagnitude(InBoundsMagnitude)
	{
	}
#endif
};

}