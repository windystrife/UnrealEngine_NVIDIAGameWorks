// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EdGraphSchema_K2.h"
#include "WidgetGraphSchema.generated.h"

UCLASS(MinimalAPI)
class UWidgetGraphSchema : public UEdGraphSchema_K2
{
	GENERATED_UCLASS_BODY()

public:
	virtual void BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const override;
};
