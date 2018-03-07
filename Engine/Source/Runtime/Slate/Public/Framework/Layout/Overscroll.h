// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Geometry.h"

enum class EAllowOverscroll : uint8
{
	Yes,
	No
};

/**
 * Handles overscroll management.
 */
struct SLATE_API FOverscroll
{
public:

	/** The amount to scale the logarithm by to make it more loose */
	static float Looseness;
	/** The "max" used to perform the interpolation snap back, and make it faster the further away it is. */
	static float OvershootLooseMax;
	/** The bounce back rate when the overscroll stops. */
	static float OvershootBounceRate;

	FOverscroll();

	/** @return The Amount actually scrolled */
	float ScrollBy(const FGeometry& AllottedGeometry, float LocalDeltaScroll);

	/** How far the user scrolled above/below the beginning/end of the list. */
	float GetOverscroll(const FGeometry& AllottedGeometry) const;

	/** Ticks the overscroll manager so it can animate. */
	void UpdateOverscroll(float InDeltaTime);

	/**
	 * Should ScrollDelta be applied to overscroll or to regular item scrolling.
	 *
	 * @param bIsAtStartOfList  Are we at the very beginning of the list (i.e. showing the first item at the top of the view)?
	 * @param bIsAtEndOfList    Are we showing the last item on the screen completely?
	 * @param ScrollDelta       How much the user is trying to scroll in Slate Units.
	 *
	 * @return true if the user's scrolling should be applied toward overscroll.
	 */
	bool ShouldApplyOverscroll(const bool bIsAtStartOfList, const bool bIsAtEndOfList, const float ScrollDelta) const;
	
	/** Resets the overscroll amout. */
	void ResetOverscroll();
private:
	/** How much we've over-scrolled above/below the beginning/end of the list, stored in log form */
	float OverscrollAmount;
};
