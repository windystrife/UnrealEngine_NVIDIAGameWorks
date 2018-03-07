// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// Utilities to convert from PhysX result structs to Unreal ones

#pragma once 

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "EngineDefines.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsPublic.h"
#include "PhysXIncludes.h"

#if WITH_PHYSX


enum class EConvertQueryResult
{
	Valid,
	Invalid
};

/** 
 * Util to convert single physX hit (raycast or sweep) to our hit result
 *
 * @param	PHit		PhysX hit data
 * @param	OutResult	(out) Result converted
 * @param	CheckLength	Distance of trace
 * @param	QueryFilter	Query Filter 
 * @param	StartLoc	Start of trace
 * @param	EndLoc
 * @param	Geom
 * @param	bReturnFaceIndex	True if we want to lookup the face index
 * @param	bReturnPhysMat		True if we want to lookup the physical material
 * @return	Whether result passed NaN/Inf checks.
 */
EConvertQueryResult ConvertQueryImpactHit(const UWorld* World, const PxLocationHit& PHit, FHitResult& OutResult, float CheckLength, const PxFilterData& QueryFilter, const FVector& StartLoc, const FVector& EndLoc, const PxGeometry* const Geom, const PxTransform& QueryTM, bool bReturnFaceIndex, bool bReturnPhysMat);

/** 
 * Util to convert physX raycast results to our hit results
 *
 * @param	OutHasValidBlockingHit set to whether there is a valid blocking hit found in the results.
 * @param	NumHits		Number of Hits
 * @param	Hits		Array of hits
 * @param	CheckLength	Distance of trace
 * @param	QueryFilter	Query Filter 
 * @param	StartLoc	Start of trace
 * @param	EndLoc		End of trace
 * @param	bReturnFaceIndex	True if we want to lookup the face index
 * @param	bReturnPhysMat		True if we want to lookup the physical material
 * @return	Whether all results passed NaN/Inf checks.
 */
EConvertQueryResult ConvertRaycastResults(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, PxRaycastHit* Hits, float CheckLength, const PxFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, bool bReturnFaceIndex, bool bReturnPhysMat);

/** 
 * Util to convert physX sweep results to unreal hit results and add to array
 *
 * @param	OutHasValidBlockingHit set to whether there is a valid blocking hit found in the results.
 * @param	NumHits		Number of Hits
 * @param	Hits		Array of hits
 * @param	CheckLength	Distance of trace
 * @param	QueryFilter	Query Filter 
 * @param	StartLoc	Start of trace
 * @param	EndLoc		End of trace
 * @param	Geom
 * @param	bReturnFaceIndex	True if we want to lookup the face index
 * @param	bReturnPhysMat		True if we want to lookup the physical material
 * @return	Whether all results passed NaN/Inf checks.
 */
EConvertQueryResult AddSweepResults(bool& OutHasValidBlockingHit, const UWorld* World, int32 NumHits, PxSweepHit* Hits, float CheckLength, const PxFilterData& QueryFilter, TArray<FHitResult>& OutHits, const FVector& StartLoc, const FVector& EndLoc, const PxGeometry& Geom, const PxTransform& QueryTM, float MaxDistance, bool bReturnFaceIndex, bool bReturnPhysMat);

/** 
 * Util to convert physX overlap query to our overlap result
 *
 * @param	PShape		Shape that overlaps
 * @param	PActor		Specific actor as PShape might be shared among many actors
 * @param	OutOverlap	(out) Result converted
 * @param	QueryFilter	Query Filter 
 */
void ConvertQueryOverlap(const UWorld* World, const PxShape* PShape, const PxRigidActor* PActor, FOverlapResult& OutOverlap, const PxFilterData& QueryFilter);

/**
 * Util to determine if a shape is deemed blocking based on the query filter
 *
 * @param PShape Shape that overlaps
 * @QueryFilter Query Filter
 * @return true if the query filter and shape filter resolve to be blocking
 */
bool IsBlocking(const PxShape* PShape, const PxFilterData& QueryFilter);


/** 
 * Util to convert a list of overlap hits into FOverlapResult and add them to OutOverlaps, if not already there
 *
 * @param	NumOverlaps	Number Of Overlaps that happened
 * @param	POverlapResults			Overlaps list
 * @param	QueryFilter	Query filter for converting
 * @return	OutOverlaps	Converted data
 */
bool ConvertOverlapResults(int32 NumOverlaps,  PxOverlapHit* POverlapResults, const PxFilterData& QueryFilter, TArray<FOverlapResult>& OutOverlaps);


#endif // WITH_PHYX


#if UE_WITH_PHYSICS

struct FCompareFHitResultTime
{
	FORCEINLINE bool operator()(const FHitResult& A, const FHitResult& B) const
	{
		if (A.Time == B.Time)
		{
			// Sort blocking hits after non-blocking hits, if they are at the same time. Also avoid swaps if they are the same.
			// This is important so initial touches are reported before processing stops on the first blocking hit.
			return (A.bBlockingHit == B.bBlockingHit) ? true : B.bBlockingHit;
		}

		return A.Time < B.Time;
	}
};

#endif
