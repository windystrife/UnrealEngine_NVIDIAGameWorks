// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SoundSourceBusSend.generated.h"

class USoundSourceBus;

USTRUCT(BlueprintType)
struct ENGINE_API FSoundSourceBusSendInfo
{
	GENERATED_USTRUCT_BODY()

	// The amount of audio to send to the source bus
	UPROPERTY(EditAnywhere, Category = SourceBusSend)
	float SendLevel;

	// The source bus to send the audio to
	UPROPERTY(EditAnywhere, Category = SourceBusSend)
	USoundSourceBus* SoundSourceBus;
};
