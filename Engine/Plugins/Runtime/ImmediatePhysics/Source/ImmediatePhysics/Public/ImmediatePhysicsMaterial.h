// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "PhysicsEngine/PhysicsSettingsEnums.h"

namespace physx
{
	class PxMaterial;
}

namespace ImmediatePhysics
{
	struct FMaterial
	{
		FMaterial()
			: StaticFriction(0.7f)
			, DynamicFriction(0.7f)
			, Restitution(0.3f)
		{
		}

		FMaterial(physx::PxMaterial* InPxMaterial);

		float StaticFriction;
		float DynamicFriction;
		float Restitution;

		EFrictionCombineMode::Type FrictionCombineMode;
		EFrictionCombineMode::Type RestitutionCombineMode;
	};
}