// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/SlateRect.h"
#include "Layout/Margin.h"


/* FSlateRect interface
 *****************************************************************************/

FSlateRect FSlateRect::InsetBy( const FMargin& InsetAmount ) const
{
	return FSlateRect(Left + InsetAmount.Left, Top + InsetAmount.Top, Right - InsetAmount.Right, Bottom - InsetAmount.Bottom);
}

FSlateRect FSlateRect::ExtendBy(const FMargin& InsetAmount) const
{
	return FSlateRect(Left - InsetAmount.Left, Top - InsetAmount.Top, Right + InsetAmount.Right, Bottom + InsetAmount.Bottom);
}

FSlateRect FSlateRect::OffsetBy(const FVector2D& OffsetAmount) const
{
	return FSlateRect(GetTopLeft() + OffsetAmount, GetBottomRight() + OffsetAmount);
}
