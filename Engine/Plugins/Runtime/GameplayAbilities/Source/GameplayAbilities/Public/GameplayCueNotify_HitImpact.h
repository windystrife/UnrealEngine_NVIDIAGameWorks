// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueNotify_HitImpact.generated.h"

class UParticleSystem;
class USoundBase;

/**
 *	Non instanced GAmeplayCueNotify for spawning particle and sound FX.
 *	Still WIP - needs to be fleshed out more.
 */

UCLASS()
class GAMEPLAYABILITIES_API UGameplayCueNotify_HitImpact : public UGameplayCueNotify_Static
{
	GENERATED_UCLASS_BODY()

	/** Does this GameplayCueNotify handle this type of GameplayCueEvent? */
	virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;
	
	virtual void HandleGameplayCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	USoundBase* Sound;

	/** Effects to play for weapon attacks against specific surfaces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameplayCue)
	UParticleSystem* ParticleSystem;
};
