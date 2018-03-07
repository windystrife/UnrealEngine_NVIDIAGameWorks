// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_SimpleParallel.generated.h"

class UEdGraphPin;

UCLASS()
class UBehaviorTreeGraphNode_SimpleParallel : public UBehaviorTreeGraphNode_Composite
{
	GENERATED_UCLASS_BODY()

	virtual void AllocateDefaultPins() override;
	virtual void GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const override;
};
