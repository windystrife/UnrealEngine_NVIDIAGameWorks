// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProceduralFoliageInstance.h"
#include "FoliageType_InstancedStaticMesh.h"

FProceduralFoliageInstance::FProceduralFoliageInstance()
: Location(ForceInit)
, Rotation(ForceInit)
, Normal(ForceInit)
, Age(0)
, Scale(1)
, Type(nullptr)
, bBlocker(false)
, BaseComponent(nullptr)
, bAlive(true)
{
}

FProceduralFoliageInstance::FProceduralFoliageInstance(const FProceduralFoliageInstance& Other)
: Location(Other.Location)
, Rotation(Other.Rotation)
, Normal(Other.Normal)
, Age(Other.Age)
, Scale(Other.Scale)
, Type(Other.Type)
, bBlocker(Other.bBlocker)
, BaseComponent(Other.BaseComponent)
, bAlive(Other.bAlive)
{

}

FProceduralFoliageInstance* GetLessFit(FProceduralFoliageInstance* A, FProceduralFoliageInstance* B)
{
	//Blocker is used for culling instances when we overlap tiles. It always wins
	if (A->bBlocker){ return B; }
	if (B->bBlocker){ return A; }
	
	//we look at priority, then age, then radius
	if (A->Type->OverlapPriority == B->Type->OverlapPriority)
	{
		if (A->Age == B->Age)
		{
			return A->Scale < B->Scale ? A : B;
		}
		else
		{
			return A->Age < B->Age ? A : B;
		}
	}
	else
	{
		return A->Type->OverlapPriority < B->Type->OverlapPriority ? A : B;
	}
}


FProceduralFoliageInstance* FProceduralFoliageInstance::Domination(FProceduralFoliageInstance* A, FProceduralFoliageInstance* B, ESimulationOverlap::Type OverlapType)
{
	const UFoliageType_InstancedStaticMesh* AType = A->Type;
	const UFoliageType_InstancedStaticMesh* BType = B->Type;

	FProceduralFoliageInstance* Dominated = GetLessFit(A, B);

	if (OverlapType == ESimulationOverlap::ShadeOverlap && Dominated->Type->bCanGrowInShade)
	{
		return nullptr;
	}

	return Dominated;
}

float FProceduralFoliageInstance::GetMaxRadius() const
{
	const float CollisionRadius = GetCollisionRadius();
	const float ShadeRadius = GetShadeRadius();
	return FMath::Max(CollisionRadius, ShadeRadius);
}

float FProceduralFoliageInstance::GetShadeRadius() const
{
	const float ShadeRadius = Type->ShadeRadius * Scale;
	return ShadeRadius;
}

float FProceduralFoliageInstance::GetCollisionRadius() const
{
	const float CollisionRadius = Type->CollisionRadius * Scale;
	return CollisionRadius;
}

void FProceduralFoliageInstance::TerminateInstance()
{
	bAlive = false;
}
