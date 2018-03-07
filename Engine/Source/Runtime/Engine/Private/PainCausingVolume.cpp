// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/PainCausingVolume.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"

APainCausingVolume::APainCausingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

	bPainCausing = true;
	DamageType = UDamageType::StaticClass();
	DamagePerSec = 1.0f;
	bEntryPain = true;
	PainInterval = 1.0f;
}

void APainCausingVolume::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	BACKUP_bPainCausing	= bPainCausing;
}

void APainCausingVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	GetWorldTimerManager().ClearTimer(TimerHandle_PainTimer);
}

void APainCausingVolume::Reset()
{
	bPainCausing = BACKUP_bPainCausing;
	ForceNetUpdate();
}

void APainCausingVolume::ActorEnteredVolume(AActor* Other)
{
	Super::ActorEnteredVolume(Other);
	if ( bPainCausing && bEntryPain && Other->bCanBeDamaged )
	{
		CausePainTo(Other);
	}

	// Start timer if none is active
	if (!GetWorldTimerManager().IsTimerActive(TimerHandle_PainTimer))
	{
		GetWorldTimerManager().SetTimer(TimerHandle_PainTimer, this, &APainCausingVolume::PainTimer, PainInterval, true);
	}
}

void APainCausingVolume::PainTimer()
{
	if (bPainCausing)
	{
		TSet<AActor*> TouchingActors;
		GetOverlappingActors(TouchingActors, APawn::StaticClass());

		for (AActor* const A : TouchingActors)
		{
			if (A && A->bCanBeDamaged && !A->IsPendingKill())
			{
				// @todo physicsVolume This won't work for other actor. Need to fix it properly
				APawn* PawnA = Cast<APawn>(A);
				if (PawnA && PawnA->GetPawnPhysicsVolume() == this)
				{
					CausePainTo(A);
				}
			}
		}

		// Stop timer if nothing is overlapping us
		if (TouchingActors.Num() == 0)
		{
			GetWorldTimerManager().ClearTimer(TimerHandle_PainTimer);
		}
	}
}

void APainCausingVolume::CausePainTo(AActor* Other)
{
	if (DamagePerSec > 0.f)
	{
		TSubclassOf<UDamageType> DmgTypeClass = DamageType ? *DamageType : UDamageType::StaticClass();
		Other->TakeDamage(DamagePerSec*PainInterval, FDamageEvent(DmgTypeClass), DamageInstigator, this);
	}
}

