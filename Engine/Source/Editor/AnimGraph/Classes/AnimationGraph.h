// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraph/EdGraph.h"
#include "AnimationGraph.generated.h"

class UEdGraphPin;

/** Delegate fired when a pin's default value is changed */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPinDefaultValueChanged, UEdGraphPin* /*InPinThatChanged*/)

UCLASS(MinimalAPI)
class UAnimationGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	/** Delegate fired when a pin's default value is changed */
	FOnPinDefaultValueChanged OnPinDefaultValueChanged;
};

