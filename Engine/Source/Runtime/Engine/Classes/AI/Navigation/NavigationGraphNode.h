// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "NavigationGraphNode.generated.h"

UCLASS(config=Engine, MinimalAPI, NotBlueprintable, abstract)
class ANavigationGraphNode : public AActor
{
	GENERATED_UCLASS_BODY()
};
