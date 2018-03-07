// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TestInterface.generated.h"

UINTERFACE()
class UTestInterface : public UInterface
{
	GENERATED_BODY()
public:
	UTestInterface(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

class ITestInterface
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintNativeEvent)
	FString SomeFunction(int32 Val) const;
};
