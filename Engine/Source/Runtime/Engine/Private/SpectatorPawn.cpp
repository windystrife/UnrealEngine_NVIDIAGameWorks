// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "GameFramework/SpectatorPawn.h"
#include "Components/SphereComponent.h"
#include "GameFramework/SpectatorPawnMovement.h"

ASpectatorPawn::ASpectatorPawn(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer
	.SetDefaultSubobjectClass<USpectatorPawnMovement>(Super::MovementComponentName)
	.DoNotCreateDefaultSubobject(Super::MeshComponentName)
	)
{
	bCanBeDamaged = false;
	//SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	//bReplicates = true;

	BaseEyeHeight = 0.0f;
	bCollideWhenPlacing = false;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	static FName CollisionProfileName(TEXT("Spectator"));
	GetCollisionComponent()->SetCollisionProfileName(CollisionProfileName);
}


void ASpectatorPawn::PossessedBy(class AController* NewController)
{
	if (bReplicates)
	{
		Super::PossessedBy(NewController);
	}
	else
	{
		// We don't want the automatic changing of net role in Pawn code since we don't replicate, so don't call Super.
		AController* const OldController = Controller;
		Controller = NewController;

		// dispatch Blueprint event if necessary
		if (OldController != NewController)
		{
			ReceivePossessed(Controller);
		}
	}
}

