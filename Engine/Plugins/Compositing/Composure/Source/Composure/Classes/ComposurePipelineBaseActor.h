// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ComposurePipelineBaseActor.generated.h"


/**
 * Actor designed to implement compositing pipeline in a blueprint.
 */
UCLASS(BlueprintType, Blueprintable, config=Engine, meta=(ShortTooltip="Actor designed to implement compositing pipeline in a blueprint."))
class COMPOSURE_API AComposurePipelineBaseActor
	: public AActor
{
	GENERATED_UCLASS_BODY()

public:

	// Begins AActor
	void RerunConstructionScripts() override;
	// Ends AActor

};
