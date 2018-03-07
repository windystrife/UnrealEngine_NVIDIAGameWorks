// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintConnectionDrawingPolicy.h"

class FSlateWindowElementList;
class UEdGraph;

/////////////////////////////////////////////////////
// FAnimGraphConnectionDrawingPolicy

// This class draws the connections for an UEdGraph with an animation schema
class FAnimGraphConnectionDrawingPolicy : public FKismetConnectionDrawingPolicy
{
public:
	// Constructor
	FAnimGraphConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float ZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements, UEdGraph* InGraphObj);

	// FKismetConnectionDrawingPolicy interface
	virtual bool TreatWireAsExecutionPin(UEdGraphPin* InputPin, UEdGraphPin* OutputPin) const override;
	virtual void BuildExecutionRoadmap() override;
	virtual void DetermineStyleOfExecWire(float& Thickness, FLinearColor& WireColor, bool& bDrawBubbles, const FTimePair& Times) override;
	// End of FKismetConnectionDrawingPolicy interface
};

