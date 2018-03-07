// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateBrush.h"
#include "PaperSpriteBlueprintLibrary.generated.h"

class UPaperSprite;

/**
 *
 */
UCLASS()
class UPaperSpriteBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Widget|Brush")
	static FSlateBrush MakeBrushFromSprite(UPaperSprite* Sprite, int32 Width, int32 Height);
};
