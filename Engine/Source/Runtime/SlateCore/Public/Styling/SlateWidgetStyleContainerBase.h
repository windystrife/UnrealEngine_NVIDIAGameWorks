// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Styling/SlateWidgetStyleContainerInterface.h"
#include "SlateWidgetStyleContainerBase.generated.h"

SLATECORE_API DECLARE_LOG_CATEGORY_EXTERN(LogSlateStyle, Log, All);

/**
 * Just a wrapper for the struct with real data in it.
 */
UCLASS(hidecategories=Object)
class SLATECORE_API USlateWidgetStyleContainerBase : public UObject, public ISlateWidgetStyleContainerInterface
{
	GENERATED_BODY()

public:

	virtual const struct FSlateWidgetStyle* const GetStyle() const override;
};
