// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UnitTest.h"

#include "NetBitsTest.generated.h"


/**
 * @todo JohnB
 */
UCLASS()
class UNetBitsTest : public UUnitTest
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool ExecuteUnitTest() override;
};
