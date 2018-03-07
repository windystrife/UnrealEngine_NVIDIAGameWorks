// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"

namespace FAISystem
{
	FVector FindClosestLocation(const FVector& Origin, const TArray<FVector>& Locations)
	{
		FVector BestLocation = FAISystem::InvalidLocation;
		float BestDistance = FLT_MAX;

		for (const FVector& Candidate : Locations)
		{
			const float CurrentDistanceSq = FVector::DistSquared(Origin, Candidate);
			if (CurrentDistanceSq < BestDistance)
			{
				BestDistance = CurrentDistanceSq;
				BestLocation = Candidate;
			}
		}

		return BestLocation;
	}
}
