// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

namespace ImmediatePhysics
{
struct FSimulation;

/** handle associated with a physics joint. This is the proper way to read/write to the physics simulation */
struct IMMEDIATEPHYSICS_API FJointHandle
{
private:
	FSimulation& OwningSimulation;
	int32 JointDataIndex;

	friend FSimulation;
	FJointHandle(FSimulation& InOwningSimulation, int32 InJointDataIndex)
		: OwningSimulation(InOwningSimulation)
		, JointDataIndex(InJointDataIndex)
	{
	}

	~FJointHandle()
	{
	}

	FJointHandle(const FJointHandle&);	//Ensure no copying of handles

};

}