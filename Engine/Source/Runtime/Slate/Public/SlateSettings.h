// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlateSettings.generated.h"

/**
* Settings that control Slate functionality
*/
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Slate"))
class SLATE_API USlateSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** 
	 * Allow children of SConstraintCanvas to share render layers. Children must set explicit ZOrder on their slots to control render order. 
	 * Recommendation: Enable for mobile platforms.
	 */
	UPROPERTY(config, EditAnywhere, Category="ConstraintCanvas")
	bool bExplicitCanvasChildZOrder;
};
