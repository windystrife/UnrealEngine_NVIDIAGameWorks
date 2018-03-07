// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "BrainComponent.h"
#include "HTNPlanner.h"
#include "HTNBrainComponent.generated.h"

UCLASS()
class HTNPLANNER_API UHTNBrainComponent : public UBrainComponent
{
	GENERATED_BODY()

public:
	UHTNBrainComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	FHTNPlanner Planner;
};