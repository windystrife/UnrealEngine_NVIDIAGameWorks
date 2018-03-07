// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "ActorFactoryVolume.generated.h"

UCLASS(Abstract, MinimalAPI, config=Editor, collapsecategories, hidecategories=Object)
class UActorFactoryVolume : public UActorFactory
{
	GENERATED_BODY()
};
