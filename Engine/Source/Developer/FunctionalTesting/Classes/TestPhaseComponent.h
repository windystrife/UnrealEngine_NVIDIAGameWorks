// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Components/SceneComponent.h"
#include "TestPhaseComponent.generated.h"

UCLASS(hidecategories=Object, editinlinenew)
class UTestPhaseComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTestPhaseComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//virtual void Start();
};
