// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"

/**
 * A convenient representation of a marquee selection
 */
struct FMarqueeRect
{
	/** Where the user began the marquee selection */
	FVector2D StartPoint;
	/** Where the user has dragged to so far */
	FVector2D EndPoint;

	/** Make a default marquee selection */
	FMarqueeRect( FVector2D InStartPoint = FVector2D::ZeroVector )
		: StartPoint( InStartPoint )
		, EndPoint( InStartPoint )
	{
	}

	/**
	 * Update the location to which the user has dragged the marquee selection so far
	 *
	 * @param NewEndPoint   Where the user has dragged so far.
	 */
	void UpdateEndPoint( FVector2D NewEndPoint )
	{
		EndPoint = NewEndPoint;
	}

	/** @return true if this marquee selection is not too small to be considered real */
	bool IsValid() const
	{
		return ! (EndPoint - StartPoint).IsNearlyZero();
	}

	/** @return the upper left point of the selection */
	FVector2D GetUpperLeft() const
	{
		return FVector2D( FMath::Min(StartPoint.X, EndPoint.X), FMath::Min( StartPoint.Y, EndPoint.Y ) );
	}

	/** @return the lower right of the selection */
	FVector2D GetLowerRight() const
	{
		return  FVector2D( FMath::Max(StartPoint.X, EndPoint.X), FMath::Max( StartPoint.Y, EndPoint.Y ) );
	}

	/** The size of the selection */
	FVector2D GetSize() const
	{
		FVector2D SignedSize = EndPoint - StartPoint;
		return FVector2D( FMath::Abs(SignedSize.X), FMath::Abs(SignedSize.Y) );
	}

	/** @return This marquee rectangle as a well-formed SlateRect */
	FSlateRect ToSlateRect() const
	{
		return FSlateRect( FMath::Min(StartPoint.X, EndPoint.X), FMath::Min(StartPoint.Y, EndPoint.Y), FMath::Max(StartPoint.X, EndPoint.X), FMath::Max( StartPoint.Y, EndPoint.Y ) );
	}
};
