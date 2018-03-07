// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

#include "ImmediatePhysicsLinearBlockAllocator.h"

namespace ImmediatePhysics
{

#if WITH_PHYSX
/** TODO: Use a smarter memory allocator */
class FConstraintAllocator : public PxConstraintAllocator
{
public:
	FConstraintAllocator() : External(0){}

	PxU8* reserveConstraintData(const PxU32 ByteSize) override
	{
		return BlockAllocator[External].Alloc(ByteSize);

	}

	PxU8* reserveFrictionData(const PxU32 ByteSize)
	{
		return BlockAllocator[External].Alloc(ByteSize);
	}

	void Reset()
	{
#if PERSISTENT_CONTACT_PAIRS
		External = 1 - External;	//flip buffer so we maintain cache for 1 extra step
#endif
		BlockAllocator[External].Reset();
	}

	FLinearBlockAllocator BlockAllocator[2];
	int32 External;
};
#endif

}