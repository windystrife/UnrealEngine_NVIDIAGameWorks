// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Abilities/GameplayAbilityTargetActor_Radius.h"
#include "GameFramework/Pawn.h"
#include "WorldCollision.h"
#include "Abilities/GameplayAbility.h"

// --------------------------------------------------------------------------------------------------------------------------------------------------------
//
//	AGameplayAbilityTargetActor_Radius
//
// --------------------------------------------------------------------------------------------------------------------------------------------------------

AGameplayAbilityTargetActor_Radius::AGameplayAbilityTargetActor_Radius(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	ShouldProduceTargetDataOnServer = true;
}

void AGameplayAbilityTargetActor_Radius::StartTargeting(UGameplayAbility* InAbility)
{
	Super::StartTargeting(InAbility);
	SourceActor = InAbility->GetCurrentActorInfo()->AvatarActor.Get();
}

void AGameplayAbilityTargetActor_Radius::ConfirmTargetingAndContinue()
{
	check(ShouldProduceTargetData());
	if (SourceActor)
	{
		FVector Origin = StartLocation.GetTargetingTransform().GetLocation();
		FGameplayAbilityTargetDataHandle Handle = MakeTargetData(PerformOverlap(Origin), Origin);
		TargetDataReadyDelegate.Broadcast(Handle);
	}
}

FGameplayAbilityTargetDataHandle AGameplayAbilityTargetActor_Radius::MakeTargetData(const TArray<TWeakObjectPtr<AActor>>& Actors, const FVector& Origin) const
{
	if (OwningAbility)
	{
		/** Use the source location instead of the literal origin */
		return StartLocation.MakeTargetDataHandleFromActors(Actors, false);
	}

	return FGameplayAbilityTargetDataHandle();
}

TArray<TWeakObjectPtr<AActor> >	AGameplayAbilityTargetActor_Radius::PerformOverlap(const FVector& Origin)
{
	bool bTraceComplex = false;
	
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RadiusTargetingOverlap), bTraceComplex);
	Params.bReturnPhysicalMaterial = false;
	Params.bTraceAsyncScene = false;

	TArray<FOverlapResult> Overlaps;

	SourceActor->GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius), Params);

	TArray<TWeakObjectPtr<AActor>>	HitActors;

	for (int32 i = 0; i < Overlaps.Num(); ++i)
	{
		//Should this check to see if these pawns are in the AimTarget list?
		APawn* PawnActor = Cast<APawn>(Overlaps[i].GetActor());
		if (PawnActor && !HitActors.Contains(PawnActor) && Filter.FilterPassesForActor(PawnActor))
		{
			HitActors.Add(PawnActor);
		}
	}

	return HitActors;
}
