// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/LayoutUtils.h"

FVector2D ComputePopupFitInRect(const FSlateRect& InAnchor, const FSlateRect& PopupRect, const EOrientation Orientation, const FSlateRect RectToFit)
{
	const bool bAdjustmentNeeded = PopupRect.IntersectionWith(RectToFit) != PopupRect;
	if (bAdjustmentNeeded)
	{
		const FVector2D PopupPosition = PopupRect.GetTopLeft();
		const FVector2D& PopupSize = PopupRect.GetSize();

		// In the direction we are opening, see if there is enough room. If there is not, flip the opening direction along the same axis.
		FVector2D NewPosition = FVector2D::ZeroVector;
		if (Orientation == Orient_Horizontal)
		{
			const bool bFitsRight = InAnchor.Right + PopupSize.X < RectToFit.Right;
			const bool bFitsLeft = InAnchor.Left - PopupSize.X >= RectToFit.Left;

			if (bFitsRight || !bFitsLeft)
			{
				// The menu fits to the right of the anchor or it does not fit to the left, display to the right
				NewPosition = FVector2D(InAnchor.Right, InAnchor.Top);
			}
			else
			{
				// The menu does not fit to the right of the anchor but it does fit to the left, display to the left
				NewPosition = FVector2D(InAnchor.Left - PopupSize.X, InAnchor.Top);
			}
		}
		else
		{
			const bool bFitsDown = InAnchor.Bottom + PopupSize.Y < RectToFit.Bottom;
			const bool bFitsUp = InAnchor.Top - PopupSize.Y >= RectToFit.Top;

			if (bFitsDown || !bFitsUp)
			{
				// The menu fits below the anchor or it does not fit above, display below
				NewPosition = FVector2D(InAnchor.Left, InAnchor.Bottom);
			}
			else
			{
				// The menu does not fit below the anchor but it does fit above, display above
				NewPosition = FVector2D(InAnchor.Left, InAnchor.Top - PopupSize.Y);
			}

			if (!bFitsDown && !bFitsUp)
			{
				NewPosition.X = InAnchor.Right;
			}
		}

		// Adjust the position of popup windows so they do not go out of the visible area of the monitor(s)
		// This can happen along the opposite axis that we are opening with
		// Assumes this window has a valid size
		// Adjust any menus that my not fit on the screen where they are opened
		FVector2D StartPos = NewPosition;
		FVector2D EndPos = NewPosition + PopupSize;
		FVector2D Adjust = FVector2D::ZeroVector;
		if (StartPos.X < RectToFit.Left)
		{
			// Window is clipped by the left side of the work area
			Adjust.X = RectToFit.Left - StartPos.X;
		}

		if (StartPos.Y < RectToFit.Top)
		{
			// Window is clipped by the top of the work area
			Adjust.Y = RectToFit.Top - StartPos.Y;
		}

		if (EndPos.X > RectToFit.Right)
		{
			// Window is clipped by the right side of the work area
			Adjust.X = RectToFit.Right - EndPos.X;
		}

		if (EndPos.Y > RectToFit.Bottom)
		{
			// Window is clipped by the bottom of the work area
			Adjust.Y = RectToFit.Bottom - EndPos.Y;
		}

		NewPosition += Adjust;

		return NewPosition;
	}
	else
	{
		return PopupRect.GetTopLeft();
	}
}
