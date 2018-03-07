// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LeapBaseObject.generated.h"

/**
* Optional base class for custom memory management
*/
UCLASS()
class LEAPMOTION_API ULeapBaseObject : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	virtual void CleanupRootReferences() {};
};