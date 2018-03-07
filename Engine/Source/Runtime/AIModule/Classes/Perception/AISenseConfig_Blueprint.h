// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig.h"
#include "AISenseConfig_Blueprint.generated.h"

class UAISense_Blueprint;

UCLASS(Blueprintable, Abstract)
class AIMODULE_API UAISenseConfig_Blueprint : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Blueprint> Implementation;

	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
};
