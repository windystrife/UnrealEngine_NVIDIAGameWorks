// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_PHYSX
#include "PhysXPublic.h"
#endif

namespace ImmediatePhysics
{
	struct FSimulation;
	struct FMaterial;

#if WITH_PHYSX
struct FContactPointRecorder : public immediate::PxContactRecorder
{
	FContactPointRecorder(struct FSimulation& InSimulation, int32 InDynamicActorDataIndex, int32 InOtherActorDataIndex, int32 InPairIdx, const FMaterial& InSimulatedShapeMaterial, const FMaterial& InOtherShapeMaterial)
		: Simulation(InSimulation)
		, SimulatedShapeMaterial(InSimulatedShapeMaterial)
		, OtherShapeMaterial(InOtherShapeMaterial)
		, DynamicActorDataIndex(InDynamicActorDataIndex)
		, OtherActorDataIndex(InOtherActorDataIndex)
		, PairIdx(InPairIdx)
	{
	}

	bool recordContacts(const Gu::ContactPoint* ContactPoints, const PxU32 NumContacts, const PxU32 Index) override;

	FSimulation& Simulation;
	const FMaterial& SimulatedShapeMaterial;
	const FMaterial& OtherShapeMaterial;
	int32 DynamicActorDataIndex;
	int32 OtherActorDataIndex;
	int32 PairIdx;
};
#endif

}