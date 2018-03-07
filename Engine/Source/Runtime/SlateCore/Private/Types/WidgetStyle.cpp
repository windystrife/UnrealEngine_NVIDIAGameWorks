// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Styling/WidgetStyle.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"


/* Static initialization
 *****************************************************************************/

const float FWidgetStyle::SubdueAmount = 0.6f;


/* FWidgetStyle interface
 *****************************************************************************/

FWidgetStyle& FWidgetStyle::SetForegroundColor( const TAttribute<struct FSlateColor>& InForeground )
{
	ForegroundColor = InForeground.Get().GetColor(*this);

	SubduedForeground = ForegroundColor;
	SubduedForeground.A *= SubdueAmount;

	return *this;
}
