// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Layout/Clipping.h"
#include "AssertionMacros.h"
#include "LogVerbosity.h"
#include "SlateGlobals.h"

FSlateClippingZone::FSlateClippingZone(const FShortRect& AxisAlignedRect)
	: bIsAxisAligned(true)
	, bIntersect(true)
	, bAlwaysClip(false)
{
	int16 Left = FMath::Min(AxisAlignedRect.Left, AxisAlignedRect.Right);
	int16 Right = FMath::Max(AxisAlignedRect.Left, AxisAlignedRect.Right);
	int16 Top = FMath::Min(AxisAlignedRect.Top, AxisAlignedRect.Bottom);
	int16 Bottom = FMath::Max(AxisAlignedRect.Top, AxisAlignedRect.Bottom);

	TopLeft = FVector2D(Left, Top);
	TopRight = FVector2D(Right, Top);
	BottomLeft = FVector2D(Left, Bottom);
	BottomRight = FVector2D(Right, Bottom);
}

FSlateClippingZone::FSlateClippingZone(const FSlateRect& AxisAlignedRect)
	: bIsAxisAligned(true)
	, bIntersect(true)
	, bAlwaysClip(false)
{
	FSlateRect RoundedAxisAlignedRect = AxisAlignedRect.Round();
	float Left = FMath::Min(RoundedAxisAlignedRect.Left, RoundedAxisAlignedRect.Right);
	float Right = FMath::Max(RoundedAxisAlignedRect.Left, RoundedAxisAlignedRect.Right);
	float Top = FMath::Min(RoundedAxisAlignedRect.Top, RoundedAxisAlignedRect.Bottom);
	float Bottom = FMath::Max(RoundedAxisAlignedRect.Top, RoundedAxisAlignedRect.Bottom);

	TopLeft = FVector2D(Left, Top);
	TopRight = FVector2D(Right, Top);
	BottomLeft = FVector2D(Left, Bottom);
	BottomRight = FVector2D(Right, Bottom);
}

FSlateClippingZone::FSlateClippingZone(const FGeometry& BooundingGeometry)
	: bIntersect(true)
	, bAlwaysClip(false)
{
	const FSlateRenderTransform& Transform = BooundingGeometry.GetAccumulatedRenderTransform();
	const FVector2D& LocalSize = BooundingGeometry.GetLocalSize();

	InitializeFromArbitraryPoints(
		Transform.TransformPoint(FVector2D(0,0)),
		Transform.TransformPoint(FVector2D(LocalSize.X, 0)),
		Transform.TransformPoint(FVector2D(0, LocalSize.Y)),
		Transform.TransformPoint(LocalSize)
	);
}

FSlateClippingZone::FSlateClippingZone(const FPaintGeometry& PaintingGeometry)
	: bIntersect(true)
	, bAlwaysClip(false)
{
	const FSlateRenderTransform& Transform = PaintingGeometry.GetAccumulatedRenderTransform();
	const FVector2D& LocalSize = PaintingGeometry.GetLocalSize();

	InitializeFromArbitraryPoints(
		Transform.TransformPoint(FVector2D(0, 0)),
		Transform.TransformPoint(FVector2D(LocalSize.X, 0)),
		Transform.TransformPoint(FVector2D(0, LocalSize.Y)),
		Transform.TransformPoint(LocalSize)
	);
}

FSlateClippingZone::FSlateClippingZone(const FVector2D& InTopLeft, const FVector2D& InTopRight, const FVector2D& InBottomLeft, const FVector2D& InBottomRight)
	: bIntersect(true)
	, bAlwaysClip(false)
{
	InitializeFromArbitraryPoints(InTopLeft, InTopRight, InBottomLeft, InBottomRight);
}

void FSlateClippingZone::InitializeFromArbitraryPoints(const FVector2D& InTopLeft, const FVector2D& InTopRight, const FVector2D& InBottomLeft, const FVector2D& InBottomRight)
{
	bIsAxisAligned =
		FMath::RoundToInt(InTopLeft.X) == FMath::RoundToInt(InBottomLeft.X) &&
		FMath::RoundToInt(InTopRight.X) == FMath::RoundToInt(InBottomRight.X) &&
		FMath::RoundToInt(InTopLeft.Y) == FMath::RoundToInt(InTopRight.Y) &&
		FMath::RoundToInt(InBottomLeft.Y) == FMath::RoundToInt(InBottomRight.Y);

	if ( bIsAxisAligned )
	{
		// Determine the true left, right, top bottom points.
		const FSlateRect RoundedAxisAlignedRect = FSlateRect(InTopLeft.X, InTopLeft.Y, InBottomRight.X, InBottomRight.Y).Round();
		const float Left = FMath::Min(RoundedAxisAlignedRect.Left, RoundedAxisAlignedRect.Right);
		const float Right = FMath::Max(RoundedAxisAlignedRect.Left, RoundedAxisAlignedRect.Right);
		const float Top = FMath::Min(RoundedAxisAlignedRect.Top, RoundedAxisAlignedRect.Bottom);
		const float Bottom = FMath::Max(RoundedAxisAlignedRect.Top, RoundedAxisAlignedRect.Bottom);

		TopLeft = FVector2D(Left, Top);
		TopRight = FVector2D(Right, Top);
		BottomLeft = FVector2D(Left, Bottom);
		BottomRight = FVector2D(Right, Bottom);
	}
	else
	{
		TopLeft = InTopLeft;
		TopRight = InTopRight;
		BottomLeft = InBottomLeft;
		BottomRight = InBottomRight;
	}
}

FSlateClippingZone FSlateClippingZone::Intersect(const FSlateClippingZone& Other) const
{
	check(IsAxisAligned());
	check(Other.IsAxisAligned());

	FSlateRect Intersected(
		FMath::Max(TopLeft.X, Other.TopLeft.X),
		FMath::Max(TopLeft.Y, Other.TopLeft.Y), 
		FMath::Min(BottomRight.X, Other.BottomRight.X), 
		FMath::Min(BottomRight.Y, Other.BottomRight.Y));

	if ( ( Intersected.Bottom < Intersected.Top ) || ( Intersected.Right < Intersected.Left ) )
	{
		return FSlateClippingZone(FSlateRect(0, 0, 0, 0));
	}
	else
	{
		return FSlateClippingZone(Intersected);
	}
}

FSlateRect FSlateClippingZone::GetBoundingBox() const
{
	FVector2D Points[4] =
	{
		TopLeft,
		TopRight,
		BottomLeft,
		BottomRight
	};

	return FSlateRect(
		FMath::Min(Points[0].X, FMath::Min3(Points[1].X, Points[2].X, Points[3].X)),
		FMath::Min(Points[0].Y, FMath::Min3(Points[1].Y, Points[2].Y, Points[3].Y)),
		FMath::Max(Points[0].X, FMath::Max3(Points[1].X, Points[2].X, Points[3].X)),
		FMath::Max(Points[0].Y, FMath::Max3(Points[1].Y, Points[2].Y, Points[3].Y))
	);
}

static float VectorSign(const FVector2D& Vec, const FVector2D& A, const FVector2D& B)
{
	return FMath::Sign((B.X - A.X) * (Vec.Y - A.Y) - (B.Y - A.Y) * (Vec.X - A.X));
}

// Returns true when the point is inside the triangle
// Should not return true when the point is on one of the edges
static bool IsPointInTriangle(const FVector2D& TestPoint, const FVector2D& A, const FVector2D& B, const FVector2D& C)
{
	float BA = VectorSign(B, A, TestPoint);
	float CB = VectorSign(C, B, TestPoint);
	float AC = VectorSign(A, C, TestPoint);

	// point is in the same direction of all 3 tri edge lines
	// must be inside, regardless of tri winding
	return BA == CB && CB == AC;
}

bool FSlateClippingZone::IsPointInside(const FVector2D& Point) const
{
	if (IsAxisAligned())
	{
		return Point.X >= TopLeft.X && Point.X <= TopRight.X && Point.Y >= TopLeft.Y && Point.Y <= BottomLeft.Y;
	}
	else
	{
		if (IsPointInTriangle(Point, TopLeft, TopRight, BottomLeft) || IsPointInTriangle(Point, BottomLeft, TopRight, BottomRight))
		{
			return true;
		}

		return false;
	}
}

//-------------------------------------------------------------------

FSlateClippingState::FSlateClippingState(bool InbAlwaysClips)
	: StateIndex(INDEX_NONE)
	, bAlwaysClips(InbAlwaysClips)
{
}

bool FSlateClippingState::IsPointInside(const FVector2D& Point) const
{
	if (ScissorRect.IsSet())
	{
		return ScissorRect->IsPointInside(Point);
	}
	
	check(StencilQuads.Num() > 0);
	for (const FSlateClippingZone& Quad : StencilQuads)
	{
		if (!Quad.IsPointInside(Point))
		{
			return false;
		}
	}

	return true;
}

//-------------------------------------------------------------------

FSlateClippingManager::FSlateClippingManager()
{
}

int32 FSlateClippingManager::PushClip(const FSlateClippingZone& InClipRect)
{
	FSlateClippingState NewClippingState(InClipRect.GetAlwaysClip());

	const FSlateClippingState* PreviousClippingState = nullptr;

	if (!InClipRect.GetShouldIntersectParent())
	{
		for (int32 StackIndex = ClippingStack.Num() - 1; StackIndex >= 0; StackIndex--)
		{
			const FSlateClippingState& ClippingState = ClippingStates[ClippingStack[StackIndex]];

			if (ClippingState.GetAlwaysClip())
			{
				PreviousClippingState = &ClippingState;
				break;
			}
		}
	}
	else if (ClippingStack.Num() > 0)
	{
		PreviousClippingState = &ClippingStates[ClippingStack.Top()];
	}

	if (PreviousClippingState == nullptr)
	{
		if (InClipRect.IsAxisAligned())
		{
			NewClippingState.ScissorRect = InClipRect;
		}
		else
		{
			NewClippingState.StencilQuads.Add(InClipRect);
		}
	}
	else
	{
		if (PreviousClippingState->ScissorRect.IsSet())
		{
			if (InClipRect.IsAxisAligned())
			{
				NewClippingState.ScissorRect = PreviousClippingState->ScissorRect->Intersect(InClipRect);
			}
			else
			{
				NewClippingState.StencilQuads.Add(PreviousClippingState->ScissorRect.GetValue());
				NewClippingState.StencilQuads.Add(InClipRect);
			}
		}
		else
		{
			ensure(PreviousClippingState->StencilQuads.Num() > 0);
			NewClippingState.StencilQuads = PreviousClippingState->StencilQuads;
			NewClippingState.StencilQuads.Add(InClipRect);
		}
	}

	return PushClippingState(NewClippingState);
}

int32 FSlateClippingManager::PushClippingState(FSlateClippingState& NewClippingState)
{
	NewClippingState.SetStateIndex(ClippingStates.Num());

	ClippingStack.Add(NewClippingState.GetStateIndex());
	ClippingStates.Add(NewClippingState);

	return NewClippingState.GetStateIndex();
}

int32 FSlateClippingManager::GetClippingIndex() const
{
	return ClippingStack.Num() > 0 ? ClippingStack.Top() : INDEX_NONE;
}

const TArray< FSlateClippingState >& FSlateClippingManager::GetClippingStates() const
{
	return ClippingStates;
}

void FSlateClippingManager::PopClip()
{
#ifdef WITH_EDITOR
	if (ClippingStack.Num() == 0)
	{
		UE_LOG(LogSlate, Log, TEXT("Attempting to pop clipping stack of size 0 due to a breakpoint."));
	}
	else
#endif
	{
		ClippingStack.Pop();
	}
}

int32 FSlateClippingManager::MergeClippingStates(const TArray< FSlateClippingState >& States)
{
	int32 Offset = ClippingStates.Num();
	ClippingStates.Append(States);
	return Offset;
}

void FSlateClippingManager::ResetClippingState()
{
	ClippingStates.Reset();
	ClippingStack.Reset();
}