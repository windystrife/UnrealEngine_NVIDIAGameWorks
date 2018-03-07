// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "SequenceRecorderActorFilter.generated.h"

USTRUCT()
struct FSequenceRecorderActorFilter
{
	GENERATED_BODY()

	/** Actor classes to accept for recording */
	UPROPERTY(EditAnywhere, Category="Actor Filter")
	TArray<TSubclassOf<AActor>> ActorClassesToRecord;
};
