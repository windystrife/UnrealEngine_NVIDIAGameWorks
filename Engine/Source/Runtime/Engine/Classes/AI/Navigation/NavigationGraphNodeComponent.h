// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "AI/Navigation/NavigationGraph.h"
#include "NavigationGraphNodeComponent.generated.h"

UCLASS(config=Engine, MinimalAPI, HideCategories=(Mobility))
class UNavigationGraphNodeComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	FNavGraphNode Node;

	UPROPERTY()
	UNavigationGraphNodeComponent* NextNodeComponent;

	UPROPERTY()
	UNavigationGraphNodeComponent* PrevNodeComponent;

	//~ Begin UObject Interface.
	virtual void BeginDestroy() override;
};
