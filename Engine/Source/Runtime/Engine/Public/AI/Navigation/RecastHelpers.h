// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 * Coord system utilities
 *
 * Translates between Unreal and Recast coords.
 * Unreal: x, y, z
 * Recast: -x, z, -y
 */

#pragma once

#include "CoreMinimal.h"

extern ENGINE_API FVector Unreal2RecastPoint(const float* UnrealPoint);
extern ENGINE_API FVector Unreal2RecastPoint(const FVector& UnrealPoint);
extern ENGINE_API FBox Unreal2RecastBox(const FBox& UnrealBox);
extern ENGINE_API FMatrix Unreal2RecastMatrix();

extern ENGINE_API FVector Recast2UnrealPoint(const float* RecastPoint);
extern ENGINE_API FVector Recast2UnrealPoint(const FVector& RecastPoint);
extern ENGINE_API FBox Recast2UnrealBox(const float* RecastMin, const float* RecastMax);
extern ENGINE_API FBox Recast2UnrealBox(const FBox& RecastBox);
