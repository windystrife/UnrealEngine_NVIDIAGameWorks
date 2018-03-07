// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/SlateRenderTransform.h"

/**
 * Stores a rectangle that has been transformed by an arbitrary render transform.
 * We provide a ctor that does the work common to slate drawing, but you could technically
 * create this any way you want.
 */
struct SLATECORE_API FSlateRotatedRect
{
public:
	/** Default ctor. */
	FSlateRotatedRect() {}

	/** Construct a rotated rect from a given aligned rect. */
	explicit FSlateRotatedRect(const FSlateRect& AlignedRect)
		: TopLeft(AlignedRect.Left, AlignedRect.Top)
		, ExtentX(AlignedRect.Right - AlignedRect.Left, 0.0f)
		, ExtentY(0.0f, AlignedRect.Bottom - AlignedRect.Top)
	{
	}

	/** Per-element constructor. */
	FSlateRotatedRect(const FVector2D& InTopLeft, const FVector2D& InExtentX, const FVector2D& InExtentY)
		: TopLeft(InTopLeft)
		, ExtentX(InExtentX)
		, ExtentY(InExtentY)
	{
	}

public:

	/** transformed Top-left corner. */
	FVector2D TopLeft;
	/** transformed X extent (right-left). */
	FVector2D ExtentX;
	/** transformed Y extent (bottom-top). */
	FVector2D ExtentY;

public:
	bool operator == (const FSlateRotatedRect& Other) const
	{
		return
			TopLeft == Other.TopLeft &&
			ExtentX == Other.ExtentX &&
			ExtentY == Other.ExtentY;
	}

public:

	/** Convert to a bounding, aligned rect. */
	FSlateRect ToBoundingRect() const;

	/** Point-in-rect test. */
	bool IsUnderLocation(const FVector2D& Location) const;

	static FSlateRotatedRect MakeRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FSlateLayoutTransform& InverseLayoutTransform, const FSlateRenderTransform& RenderTransform)
	{
		return MakeRotatedRect(ClipRectInLayoutWindowSpace, Concatenate(InverseLayoutTransform, RenderTransform));
	}

	static FSlateRotatedRect MakeRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FTransform2D& LayoutToRenderTransform);

	static FSlateRotatedRect MakeSnappedRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FSlateLayoutTransform& InverseLayoutTransform, const FSlateRenderTransform& RenderTransform)
	{
		return MakeSnappedRotatedRect(ClipRectInLayoutWindowSpace, Concatenate(InverseLayoutTransform, RenderTransform));
	}

	/**
	* Used to construct a rotated rect from an aligned clip rect and a set of layout and render transforms from the geometry, snapped to pixel boundaries. Returns a float or float16 version of the rect based on the typedef.
	*/
	static FSlateRotatedRect MakeSnappedRotatedRect(const FSlateRect& ClipRectInLayoutWindowSpace, const FTransform2D& LayoutToRenderTransform);
};

/**
* Transforms a rect by the given transform.
*/
template <typename TransformType>
FSlateRotatedRect TransformRect(const TransformType& Transform, const FSlateRotatedRect& Rect)
{
	return FSlateRotatedRect
	(
		TransformPoint(Transform, Rect.TopLeft),
		TransformVector(Transform, Rect.ExtentX),
		TransformVector(Transform, Rect.ExtentY)
	);
}
