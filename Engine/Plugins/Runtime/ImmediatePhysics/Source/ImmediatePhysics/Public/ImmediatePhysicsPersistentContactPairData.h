// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifndef PERSISTENT_CONTACT_PAIRS
#define PERSISTENT_CONTACT_PAIRS 1
#endif

namespace ImmediatePhysics
{

#if PERSISTENT_CONTACT_PAIRS
/** Persistent data like contact manifold, friction patches, etc... */
struct FPersistentContactPairData
{
#if WITH_PHYSX
	PxCache Cache;
	PxU8* Frictions;
	PxU32 NumFrictions;
#endif

	uint32 SimCount;

	void Clear()
	{
		FPlatformMemory::Memzero(this, sizeof(FPersistentContactPairData));
	}
};
#endif


}