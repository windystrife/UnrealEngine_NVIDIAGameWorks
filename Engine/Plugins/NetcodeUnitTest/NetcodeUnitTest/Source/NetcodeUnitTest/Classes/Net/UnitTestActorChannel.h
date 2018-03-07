// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/ActorChannel.h"

#include "UnitTestActorChannel.generated.h"


// Forward declarations
class FInBunch;
class UMinimalClient;


/**
 * An actor net channel override, for hooking ReceivedBunch, to aid in detecting/blocking of remote actors, of a specific class
 */
UCLASS(transient)
class UUnitTestActorChannel : public UActorChannel
{
	GENERATED_UCLASS_BODY()

	virtual void Init(UNetConnection* InConnection, int32 InChIndex, bool InOpenedLocally) override;

	virtual void ReceivedBunch(FInBunch& Bunch) override;

	virtual void Tick() override;

	virtual void NotifyActorChannelOpen(AActor* InActor, FInBunch& InBunch) override;

private:
	/** Cached referenced to the minimal client that owns this actor channel */
	UMinimalClient* MinClient;
};

