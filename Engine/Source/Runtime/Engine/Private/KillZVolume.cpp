// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/KillZVolume.h"
#include "GameFramework/DamageType.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

AKillZVolume::AKillZVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AKillZVolume::ActorEnteredVolume(AActor* Other)
{
	Super::ActorEnteredVolume(Other);
	
	if ( Other )
	{
		const UDamageType* DamageType = GetDefault<UDamageType>();

		UWorld* World = GetWorld();
		if ( World )
		{
			AWorldSettings* WorldSettings = World->GetWorldSettings( true );
			if ( WorldSettings && WorldSettings->KillZDamageType )
			{
				DamageType = WorldSettings->KillZDamageType->GetDefaultObject<UDamageType>();
			}
		}

		Other->FellOutOfWorld(*DamageType);
	}
}
