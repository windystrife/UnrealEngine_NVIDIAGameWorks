// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/PhysicsVolume.h"
#include "KillZVolume.generated.h"

/**
* KillZVolume is a volume used to determine when actors should be killed. Killing logic is overridden in FellOutOfWorld
* 
* @see FellOutOfWorld
*/

UCLASS()
class ENGINE_API AKillZVolume : public APhysicsVolume
{
	GENERATED_UCLASS_BODY()
	
	//Begin PhysicsVolume Interface
	virtual void ActorEnteredVolume(class AActor* Other) override;
	//End PhysicsVolume Interface
};



