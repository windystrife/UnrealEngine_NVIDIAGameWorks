// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameFramework/Actor.h"
#include "ScriptTestActor.generated.h"

/**
* Test Actor. To be removed at some point.
*/
UCLASS(BlueprintType, MinimalAPI, EarlyAccessPreview, notplaceable)
class AScriptTestActor : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category = Test, EditAnywhere, BlueprintReadWrite)
	FString TestString;

	UPROPERTY(Category = Test, EditAnywhere, BlueprintReadWrite)
	float TestValue;

	UPROPERTY(Category = Test, EditAnywhere, BlueprintReadWrite)
	bool TestBool;

	UFUNCTION()
	float TestFunction(float InValue, float InFactor, bool bMultiply);
};
