// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

#include "ImmediatePhysicsShape.h"

namespace ImmediatePhysics
{

/** Holds geometry data*/
struct FActor
{
	TArray<FShape> Shapes;

#if WITH_PHYSX
	/** Create geometry data for the entity */
	void CreateGeometry(PxRigidActor* RigidActor, const PxTransform& ActorToBodyTM);
#endif

	/** Ensures all the geometry data has been properly freed */
	void TerminateGeometry();
};

}