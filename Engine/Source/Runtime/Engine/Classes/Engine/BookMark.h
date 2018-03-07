// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BookMark.generated.h"


/**
 * A camera position the current level.
 */
UCLASS(hidecategories=Object, MinimalAPI)
class UBookMark : public UObject
{
	GENERATED_UCLASS_BODY()

	/** Camera position */
	UPROPERTY(EditAnywhere, Category=BookMark)
	FVector Location;

	/** Camera rotation */
	UPROPERTY(EditAnywhere, Category=BookMark)
	FRotator Rotation;

	/** Array of levels that are hidden */
	UPROPERTY(EditAnywhere, Category=BookMark)
	TArray<FString> HiddenLevels;
};
