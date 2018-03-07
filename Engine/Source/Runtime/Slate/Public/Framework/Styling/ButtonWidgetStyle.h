// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Styling/SlateTypes.h"
#include "ButtonWidgetStyle.generated.h"

/**
 */
UCLASS(BlueprintType, hidecategories=Object, MinimalAPI)
class UButtonWidgetStyle : public USlateWidgetStyleContainerBase
{
public:
	GENERATED_BODY()

public:
	/** The actual data describing the button's appearance. */
	UPROPERTY(Category=Appearance, EditAnywhere, BlueprintReadWrite, meta=( ShowOnlyInnerProperties ))
	FButtonStyle ButtonStyle;

	virtual const struct FSlateWidgetStyle* const GetStyle() const override
	{
		return static_cast< const struct FSlateWidgetStyle* >( &ButtonStyle );
	}
};
