// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputScaleBias.generated.h"

// Input scaling struct
USTRUCT(BlueprintType)
struct ENGINE_API FInputScaleBias
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Bias;

public:
	FInputScaleBias()
		: Scale(1.0f)
		, Bias(0.0f)
	{
	}

	// Apply scale and bias to value
	float ApplyTo(float Value) const;
};

