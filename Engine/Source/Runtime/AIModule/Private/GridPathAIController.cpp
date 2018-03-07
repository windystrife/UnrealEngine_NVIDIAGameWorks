// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GridPathAIController.h"
#include "Navigation/GridPathFollowingComponent.h"

AGridPathAIController::AGridPathAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGridPathFollowingComponent>(TEXT("PathFollowingComponent")))
{

}
