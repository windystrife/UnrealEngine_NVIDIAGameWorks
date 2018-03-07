// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "BookMark2D.generated.h"


/**
 * Simple class to store 2D camera information.
 */
UCLASS(hidecategories=Object)
class UBookMark2D
	: public UObject
{
	GENERATED_UCLASS_BODY()

	/** Zoom of the camera */
	UPROPERTY(EditAnywhere, Category=BookMark2D)
	float Zoom2D;

	/** Location of the camera */
	UPROPERTY(EditAnywhere, Category=BookMark2D)
	FIntPoint Location;
};
