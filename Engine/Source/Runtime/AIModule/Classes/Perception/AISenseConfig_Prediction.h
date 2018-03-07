// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig.h"
#include "AISenseConfig_Prediction.generated.h"

UCLASS(meta = (DisplayName = "AI Prediction sense config"))
class AIMODULE_API UAISenseConfig_Prediction : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()
public:	
	virtual TSubclassOf<UAISense> GetSenseImplementation() const override;
};
