// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "AI/Navigation/NavigationTypes.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "Engine/World.h"

class ANavigationData;
class Error;

namespace FEQSHelpers
{
	enum class ETraceMode : uint8
	{
		Keep,
		Discard,
	};

	struct FBatchTrace
	{
		UWorld* World;
		const FVector Extent;
		const FCollisionQueryParams Params;
		enum ECollisionChannel Channel;
		ETraceMode TraceMode;
		TArray<uint8> TraceHits;

		FBatchTrace(UWorld* InWorld, enum ECollisionChannel InChannel, const FCollisionQueryParams& InParams,
			const FVector& InExtent, ETraceMode InTraceMode)
			: World(InWorld), Extent(InExtent), Params(InParams), Channel(InChannel), TraceMode(InTraceMode)
		{

		}

		FORCEINLINE_DEBUGGABLE bool RunLineTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->LineTraceSingleByChannel(OutHit, StartPos, EndPos, Channel, Params);
			HitPos = OutHit.Location;
			return bHit;
		}

		FORCEINLINE_DEBUGGABLE bool RunSphereTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->SweepSingleByChannel(OutHit, StartPos, EndPos, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Extent.X), Params);
			HitPos = OutHit.Location;
			return bHit;
		}

		FORCEINLINE_DEBUGGABLE bool RunCapsuleTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->SweepSingleByChannel(OutHit, StartPos, EndPos, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Extent.X, Extent.Z), Params);
			HitPos = OutHit.Location;
			return bHit;
		}

		FORCEINLINE_DEBUGGABLE bool RunBoxTrace(const FVector& StartPos, const FVector& EndPos, FVector& HitPos)
		{
			FHitResult OutHit;
			const bool bHit = World->SweepSingleByChannel(OutHit, StartPos, EndPos, FQuat((EndPos - StartPos).Rotation()), Channel, FCollisionShape::MakeBox(Extent), Params);
			HitPos = OutHit.Location;
			return bHit;
		}

		template<EEnvTraceShape::Type TraceType>
		void DoSingleSourceMultiDestinations(const FVector& Source, TArray<FNavLocation>& Points)
		{
			UE_LOG(LogEQS, Error, TEXT("FBatchTrace::DoSingleSourceMultiDestinations called with unhandled trace type: %d"), int32(TraceType));
		}

		/** note that his function works slightly different in terms of discarding items. 
		 *	"Accepted" items get added to the OutPoints array*/
		template<EEnvTraceShape::Type TraceType>
		void DoMultiSourceMultiDestinations2D(const TArray<FRayStartEnd>& Rays, TArray<FNavLocation>& OutPoints)
		{
			UE_LOG(LogEQS, Error, TEXT("FBatchTrace::DoMultiSourceMultiDestinations2D called with unhandled trace type: %d"), int32(TraceType));
		}

		template<EEnvTraceShape::Type TraceType>
		void DoProject(TArray<FNavLocation>& Points, float StartOffsetZ, float EndOffsetZ, float HitOffsetZ)
		{
			UE_LOG(LogEQS, Error, TEXT("FBatchTrace::DoSingleSourceMultiDestinations called with unhandled trace type: %d"), int32(TraceType));
		}
	};

	void RunNavRaycasts(const ANavigationData& NavData, const UObject& Querier, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, const ETraceMode TraceMode = ETraceMode::Keep);
	void RunNavProjection(const ANavigationData& NavData, const UObject& Querier, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, const ETraceMode TraceMode = ETraceMode::Discard);
	void RunPhysRaycasts(UWorld* World, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, const TArray<AActor*>& IgnoredActors, const ETraceMode TraceMode = ETraceMode::Keep);
	void RunPhysProjection(UWorld* World, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, const ETraceMode TraceMode = ETraceMode::Discard);
	void RunPhysProjection(UWorld* World, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, TArray<uint8>& TraceHits);
	/** Does initial raycast on navmesh, just like, RunNavRaycasts but
	 *	once it hits navmesh edge it does a geometry trace to determine whether
	 *	it hit a wall or a ledge (empty space). */
	void RunRaycastsOnNavHitOnlyWalls(const ANavigationData& NavData, const UObject& Querier, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, const TArray<AActor*>& IgnoredActors, const ETraceMode TraceMode = ETraceMode::Keep);

	// deprecated

	DEPRECATED_FORGAME(4.12, "This function is now deprecated, please use version with Querier argument instead.")
	void RunNavRaycasts(const ANavigationData& NavData, const FEnvTraceData& TraceData, const FVector& SourcePt, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Keep);

	DEPRECATED_FORGAME(4.12, "This function is now deprecated, please use version with Querier argument instead.")
	void RunNavProjection(const ANavigationData& NavData, const FEnvTraceData& TraceData, TArray<FNavLocation>& Points, ETraceMode TraceMode = ETraceMode::Discard);
}
