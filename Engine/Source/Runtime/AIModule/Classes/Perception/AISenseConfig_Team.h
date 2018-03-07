// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Perception/AISense.h"
#include "Perception/AISenseConfig.h"
#include "Perception/AISense_Team.h"
#include "AISenseConfig_Team.generated.h"

UCLASS(meta = (DisplayName = "AI Team sense config"))
class AIMODULE_API UAISenseConfig_Team : public UAISenseConfig
{
	GENERATED_UCLASS_BODY()
public:	
	virtual TSubclassOf<UAISense> GetSenseImplementation() const override { return UAISense_Team::StaticClass(); }
};
