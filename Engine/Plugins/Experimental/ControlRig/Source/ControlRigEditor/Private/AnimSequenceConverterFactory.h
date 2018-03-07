// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/AnimSequenceFactory.h"
#include "AnimSequenceConverterFactory.generated.h"

UCLASS(MinimalAPI)
class UAnimSequenceConverterFactory : public UAnimSequenceFactory
{
	GENERATED_BODY()

	// Act just like the regular UAnimSequenceFactory, but with a pre-configured skeleton
	virtual bool ConfigureProperties() override { return true; }
};

