// Copyright 2017 Google Inc.

#pragma once

#include "GoogleARCorePrimitives.h"
#include "GameFramework/Actor.h"

#include "GoogleARCoreMapConfigurationActor.generated.h"

/**
 * An Actor that is used to configure the GoogleARCore tracking session in a certain map.
 */
UCLASS()
class GOOGLEARCOREBASE_API AGoogleARCoreMapConfigurationActor : public AActor
{
	GENERATED_BODY()

public:
	/** The configuration that will be used when the map is loaded. */
	UPROPERTY(EditAnywhere, Category = "ARCoreMapConfiguration",  meta = (ShowOnlyInnerProperties))
	FGoogleARCoreSessionConfig GoogleARCoreSessionConfiguration;

private:
	// AActor interface.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
