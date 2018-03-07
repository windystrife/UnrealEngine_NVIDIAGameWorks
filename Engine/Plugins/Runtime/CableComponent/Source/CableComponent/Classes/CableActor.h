// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "CableActor.generated.h"

/** An actor that renders a simulated cable */
UCLASS(hidecategories=(Input,Replication), showcategories=("Input|MouseInput", "Input|TouchInput"))
class ACableActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Cable component that performs simulation and rendering */
	UPROPERTY(Category=Cable, VisibleAnywhere, BlueprintReadOnly)
	class UCableComponent* CableComponent;
};
