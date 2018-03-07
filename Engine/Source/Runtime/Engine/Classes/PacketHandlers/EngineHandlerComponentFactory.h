// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "HandlerComponentFactory.h"

#include "EngineHandlerComponentFactory.generated.h"

class HandlerComponent;

/**
 * Factory class for loading HandlerComponent's contained within Engine
 */
UCLASS()
class UEngineHandlerComponentFactory : public UHandlerComponentFactory
{
	GENERATED_UCLASS_BODY()

public:
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};
