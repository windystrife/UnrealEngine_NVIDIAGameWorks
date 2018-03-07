// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/GameplayAbilityTargetActor_SingleLineTrace.h"
#include "DrawDebugHelpers.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	AGameplayAbilityTargetActor_SingleLineTrace
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

AGameplayAbilityTargetActor_SingleLineTrace::AGameplayAbilityTargetActor_SingleLineTrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FHitResult AGameplayAbilityTargetActor_SingleLineTrace::PerformTrace(AActor* InSourceActor)
{
	bool bTraceComplex = false;
	TArray<AActor*> ActorsToIgnore;

	ActorsToIgnore.Add(InSourceActor);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AGameplayAbilityTargetActor_SingleLineTrace), bTraceComplex);
	Params.bReturnPhysicalMaterial = true;
	Params.bTraceAsyncScene = true;
	Params.AddIgnoredActors(ActorsToIgnore);

	FVector TraceStart = StartLocation.GetTargetingTransform().GetLocation();// InSourceActor->GetActorLocation();
	FVector TraceEnd;
	AimWithPlayerController(InSourceActor, Params, TraceStart, TraceEnd);		//Effective on server and launching client only

	// ------------------------------------------------------

	FHitResult ReturnHitResult;
	LineTraceWithFilter(ReturnHitResult, InSourceActor->GetWorld(), Filter, TraceStart, TraceEnd, TraceProfile.Name, Params);
	//Default to end of trace line if we don't hit anything.
	if (!ReturnHitResult.bBlockingHit)
	{
		ReturnHitResult.Location = TraceEnd;
	}
	if (AGameplayAbilityWorldReticle* LocalReticleActor = ReticleActor.Get())
	{
		const bool bHitActor = (ReturnHitResult.bBlockingHit && (ReturnHitResult.Actor != NULL));
		const FVector ReticleLocation = (bHitActor && LocalReticleActor->bSnapToTargetedActor) ? ReturnHitResult.Actor->GetActorLocation() : ReturnHitResult.Location;

		LocalReticleActor->SetActorLocation(ReticleLocation);
		LocalReticleActor->SetIsTargetAnActor(bHitActor);
	}

#if ENABLE_DRAW_DEBUG
	if (bDebug)
	{
		DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green);
		DrawDebugSphere(GetWorld(), TraceEnd, 100.0f, 16, FColor::Green);
	}
#endif // ENABLE_DRAW_DEBUG
	return ReturnHitResult;
}
