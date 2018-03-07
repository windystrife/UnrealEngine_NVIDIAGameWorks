// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallMaterialParameterCollectionFunction.generated.h"

UCLASS(MinimalAPI)
class UK2Node_CallMaterialParameterCollectionFunction : public UK2Node_CallFunction
{
	GENERATED_UCLASS_BODY()

	//~ Begin EdGraphNode Interface
	virtual void PinDefaultValueChanged(UEdGraphPin* Pin) override;
	//~ End EdGraphNode Interface
};
