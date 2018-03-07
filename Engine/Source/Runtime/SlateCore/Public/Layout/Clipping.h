// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "SlateRect.h"
#include "Rendering/RenderingCommon.h"
#include "Clipping.generated.h"

/**
 * This enum controls clipping of widgets in Slate.  By default all SWidgets do not need to clip their children.
 * Most of the time, you don't need to clip, the only times it becomes important is when something might become hidden
 * due to panning.  You should use this wisely, as Slate can not batch across clipping areas, so if widget A and widget B
 * are set to EWidgetClipping::Yes, no drawing that happens inside their widget trees will ever be batch together, adding
 * additional GPU overhead.
 */
UENUM(BlueprintType)
enum class EWidgetClipping : uint8
{
	/**
	 * This widget does not clip children, it and all children inherit the clipping area of the last widget that clipped.
	 */
	Inherit,
	/**
	 * This widget clips content the bounds of this widget.  It intersects those bounds with any previous clipping area.
	 */
	ClipToBounds,
	/**
	 * This widget clips to its bounds.  It does NOT intersect with any existing clipping geometry, it pushes a new clipping 
	 * state.  Effectively allowing it to render outside the bounds of hierarchy that does clip.
	 * 
	 * NOTE: This will NOT allow you ignore the clipping zone that is set to [Yes - Always].
	 */
	ClipToBoundsWithoutIntersecting UMETA(DisplayName = "Clip To Bounds - Without Intersecting (Advanced)"),
	/**
	 * This widget clips to its bounds.  It intersects those bounds with any previous clipping area.
	 *
	 * NOTE: This clipping area can NOT be ignored, it will always clip children.  Useful for hard barriers
	 * in the UI where you never want animations or other effects to break this region.
	 */
	ClipToBoundsAlways UMETA(DisplayName = "Clip To Bounds - Always (Advanced)"),
	/**
	 * This widget clips to its bounds when it's Desired Size is larger than the allocated geometry 
	 * the widget is given.  If that occurs, it work like [Yes].
	 * 
	 * NOTE: This mode was primarily added for Text, which is often placed into containers that eventually 
	 * are resized to not be able to support the length of the text.  So rather than needing to tag every 
	 * container that could contain text with [Yes], which would result in almost no batching, this mode 
	 * was added to dynamically adjust the clipping if needed.  The reason not every panel is set to OnDemand, 
	 * is because not every panel returns a Desired Size that matches what it plans to render at.
	 */
	OnDemand UMETA(DisplayName = "On Demand (Advanced)")
};


/**
 * The Clipping Zone represents some arbitrary plane segment that can be used to clip the geometry in Slate.
 */
class SLATECORE_API FSlateClippingZone
{
public:

	FVector2D TopLeft;
	FVector2D TopRight;
	FVector2D BottomLeft;
	FVector2D BottomRight;

	explicit FSlateClippingZone(const FShortRect& AxisAlignedRect);
	explicit FSlateClippingZone(const FSlateRect& AxisAlignedRect);
	explicit FSlateClippingZone(const FGeometry& BoundingGeometry);
	explicit FSlateClippingZone(const FPaintGeometry& PaintingGeometry);
	FSlateClippingZone(const FVector2D& InTopLeft, const FVector2D& InTopRight, const FVector2D& InBottomLeft, const FVector2D& InBottomRight);

	/**  */
	FORCEINLINE bool GetShouldIntersectParent() const
	{
		return bIntersect;
	}

	/**  */
	FORCEINLINE void SetShouldIntersectParent(bool bValue)
	{
		bIntersect = bValue;
	}

	/**  */
	FORCEINLINE bool GetAlwaysClip() const
	{
		return bAlwaysClip;
	}

	/**  */
	FORCEINLINE void SetAlwaysClip(bool bValue)
	{
		bAlwaysClip = bValue;
	}

	/** Is the clipping rect axis aligned?  If it is, we can use a much cheaper clipping solution. */
	FORCEINLINE bool IsAxisAligned() const
	{
		return bIsAxisAligned;
	}

	/**
	 * Indicates if this clipping state has a zero size, aka is empty.  We only possibly report true for
	 * scissor clipping zones.
	 */
	FORCEINLINE bool HasZeroArea() const
	{
		if (bIsAxisAligned)
		{
			FVector2D Difference = TopLeft - BottomRight;
			return FMath::IsNearlyZero(Difference.X) || FMath::IsNearlyZero(Difference.Y);
		}

		return false;
	}

	/** Is a point inside the clipping zone? */
	bool IsPointInside(const FVector2D& Point) const;

	/**
	 * Intersects two clipping zones and returns the new clipping zone that would need to be used.
	 * This can only be called between two axis aligned clipping zones.
	 */
	FSlateClippingZone Intersect(const FSlateClippingZone& Other) const;

	/**
	 * Gets the bounding box of the points making up this clipping zone.
	 */
	FSlateRect GetBoundingBox() const;

	bool operator==(const FSlateClippingZone& Other) const
	{
		return bIsAxisAligned == Other.bIsAxisAligned &&
			bIntersect == Other.bIntersect &&
			bAlwaysClip == Other.bAlwaysClip &&
			TopLeft == Other.TopLeft &&
			TopRight == Other.TopRight &&
			BottomLeft == Other.BottomLeft &&
			BottomRight == Other.BottomRight;
	}

private:
	void InitializeFromArbitraryPoints(const FVector2D& InTopLeft, const FVector2D& InTopRight, const FVector2D& InBottomLeft, const FVector2D& InBottomRight);

private:
	/** Is the clipping zone axis aligned?  Axis aligned clipping zones are much cheaper. */
	bool bIsAxisAligned : 1;
	/** Should this clipping zone intersect the current one? */
	bool bIntersect : 1;
	/** Should this clipping zone always clip, even if another zone wants to ignore intersection? */
	bool bAlwaysClip : 1;
};

template<> struct TIsPODType<FSlateClippingZone> { enum { Value = true }; };

/**
 * Indicates the method of clipping that should be used on the GPU.
 */
enum class EClippingMethod : uint8
{
	Scissor,
	Stencil
};

/**
 * Captures everything about a single draw calls clipping state.
 */
class SLATECORE_API FSlateClippingState
{
public:
	FSlateClippingState(bool InAlwaysClips);
	
	/** Is a point inside the clipping state? */
	bool IsPointInside(const FVector2D& Point) const;

	FORCEINLINE void SetStateIndex(int32 InStateIndex) { StateIndex = InStateIndex; }
	FORCEINLINE int32 GetStateIndex() const { return StateIndex; }

	FORCEINLINE bool GetAlwaysClip() const { return bAlwaysClips; }

	/**
	 * Gets the type of clipping that is required by this clipping state.  The simpler clipping is
	 * scissor clipping, but that's only possible if the clipping rect is axis aligned.
	 */
	FORCEINLINE EClippingMethod GetClippingMethod() const
	{
		return ScissorRect.IsSet() ? EClippingMethod::Scissor : EClippingMethod::Stencil;
	}

	/**
	 * Indicates if this clipping state has a zero size, aka is empty.  We only possibly report true for
	 * scissor clipping zones.
	 */
	FORCEINLINE bool HasZeroArea() const
	{
		if (ScissorRect.IsSet())
		{
			return ScissorRect->HasZeroArea();
		}

		return false;
	}

	bool operator==(const FSlateClippingState& Other) const
	{
		return StateIndex == Other.StateIndex &&
			bAlwaysClips == Other.bAlwaysClips &&
			ScissorRect == Other.ScissorRect &&
			StencilQuads == Other.StencilQuads;
	}

private:
	/** For a given frame, this is a unique index into a state array of clipping zones that have been registered for a window being drawn. */
	int32 StateIndex;
	/** If the clipping state is always clip, we cache it at a higher level. */
	bool bAlwaysClips;

public:
	/** If this is an axis aligned clipping state, this will be filled. */
	TOptional<FSlateClippingZone> ScissorRect;
	/** If this is a more expensive stencil clipping zone, this will be filled. */
	TArray<FSlateClippingZone> StencilQuads;
};


/**
 * The clipping manager maintain the running clip state.  This is used for both maintain and for hit testing.
 */
class SLATECORE_API FSlateClippingManager
{
public:
	FSlateClippingManager();

	int32 PushClip(const FSlateClippingZone& InClippingZone);
	int32 PushClippingState(FSlateClippingState& InClipState);
	int32 GetClippingIndex() const;
	const TArray< FSlateClippingState >& GetClippingStates() const;
	void PopClip();

	int32 MergeClippingStates(const TArray< FSlateClippingState >& States);

	void ResetClippingState();

private:
	/** Maintains the current clipping stack, with the indexes in the array of clipping states.  Pushed and popped throughout the drawing process. */
	TArray< int32 > ClippingStack;

	/** The authoratitive list of clipping states used when rendering.  Any time a clipping state is needed, it's added here. */
	TArray< FSlateClippingState > ClippingStates;
};