// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "FlexRopeActor.generated.h"

/** An actor that renders a simulated FlexRope */
UCLASS(hidecategories=(Input,Collision,Replication), showcategories=("Input|MouseInput", "Input|TouchInput"))
class AFlexRopeActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** FlexRope component that performs simulation and rendering */
	UPROPERTY(Category=FlexRope, VisibleAnywhere, BlueprintReadOnly)
	class UFlexRopeComponent* FlexRopeComponent;
};
