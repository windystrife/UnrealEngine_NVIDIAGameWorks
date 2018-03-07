// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Draw functions for debugging trace/sweeps/overlaps


#pragma once 

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"

#if WITH_PHYSX
/** Draw PhysX geom with overlaps */
void DrawGeomOverlaps(const UWorld* InWorld, const PxGeometry& PGeom, const PxTransform& PGeomPose, TArray<struct FOverlapResult>& Overlaps, float Lifetime);
/** Draw PhysX geom being swept with hits */
void DrawGeomSweeps(const UWorld* InWorld, const FVector& Start, const FVector& End, const PxGeometry& PGeom, const PxQuat& Rotation, const TArray<FHitResult>& Hits, float Lifetime);
#endif
