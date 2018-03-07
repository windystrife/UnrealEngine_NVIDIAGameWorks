// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Types/SlateConstants.h"
#include "Styling/SlateWidgetStyle.h"

/** How much to scroll for each click of the mouse wheel (in Slate Screen Units). */
TAutoConsoleVariable<float> GlobalScrollAmount(
	TEXT("Slate.GlobalScrollAmount"),
	32.0f,
	TEXT("How much to scroll for each click of the mouse wheel (in Slate Screen Units)."));

FSlateWidgetStyle::FSlateWidgetStyle( )
{ }
