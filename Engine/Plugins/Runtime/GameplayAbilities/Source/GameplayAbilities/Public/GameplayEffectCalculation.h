// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectCalculation.generated.h"

/** Abstract base for specialized gameplay effect calculations; Capable of specifing attribute captures */
UCLASS(BlueprintType, Blueprintable, Abstract)
class GAMEPLAYABILITIES_API UGameplayEffectCalculation : public UObject
{
	GENERATED_UCLASS_BODY()

public:

	/** Simple accessor to capture definitions for attributes */
	virtual const TArray<FGameplayEffectAttributeCaptureDefinition>& GetAttributeCaptureDefinitions() const;

protected:

	/** Attributes to capture that are relevant to the calculation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Attributes)
	TArray<FGameplayEffectAttributeCaptureDefinition> RelevantAttributesToCapture;
};
