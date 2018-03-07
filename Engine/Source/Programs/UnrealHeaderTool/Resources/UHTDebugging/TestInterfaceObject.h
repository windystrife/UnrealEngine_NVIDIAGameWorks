// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TestInterfaceObject.generated.h"

UCLASS()
class UTestInterfaceObject : public UObject, public ITestInterface
{
	GENERATED_BODY()
public:
	UTestInterfaceObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

//	UFUNCTION(BlueprintNativeEvent)
	FString SomeFunction(int32 Val) const;
};
