// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig.h"
#include "Perception/AISense_Damage.h"
#include "AISenseConfig_Damage.generated.h"

UCLASS(meta = (DisplayName = "AI Damage sense config"))
class AIMODULE_API UAISenseConfig_Damage : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sense", NoClear, config)
	TSubclassOf<UAISense_Damage> Implementation;

	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
};
