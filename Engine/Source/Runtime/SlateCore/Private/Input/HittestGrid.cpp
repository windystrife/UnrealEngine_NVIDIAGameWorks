// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Input/HittestGrid.h"
#include "Rendering/RenderingCommon.h"
#include "SlateGlobals.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogHittestDebug, Display, All);

int32 SlateVerifyHitTestVisibility = 0;
static FAutoConsoleVariableRef CVarSlateVerifyHitTestVisibility(TEXT("Slate.VerifyHitTestVisibility"), SlateVerifyHitTestVisibility, TEXT("Should we double check the visibility of widgets during hit testing, in case previously resolved hit tests that same frame may have changed state?"), ECVF_Default);

//
// Helper Functions
//

FVector2D ClosestPointOnSlateRotatedRect(const FVector2D &Point, const FSlateRotatedRect& RotatedRect)
{
	//no need to do any testing if we are inside of the rect
	if (RotatedRect.IsUnderLocation(Point))
	{
		return Point;
	}

	const static int32 NumOfCorners = 4;
	FVector2D Corners[NumOfCorners];
	Corners[0] = RotatedRect.TopLeft;
	Corners[1] = Corners[0] + RotatedRect.ExtentX;
	Corners[2] = Corners[1] + RotatedRect.ExtentY;
	Corners[3] = Corners[0] + RotatedRect.ExtentY;

	FVector2D RetPoint;
	float ClosestDistSq = FLT_MAX;
	for (int32 i = 0; i < NumOfCorners; ++i)
	{
		//grab the closest point along the line segment
		const FVector2D ClosestPoint = FMath::ClosestPointOnSegment2D(Point, Corners[i], Corners[(i + 1) % NumOfCorners]);

		//get the distance between the two
		const float TestDist = FVector2D::DistSquared(Point, ClosestPoint);

		//if the distance is smaller than the current smallest, update our closest
		if (TestDist < ClosestDistSq)
		{
			RetPoint = ClosestPoint;
			ClosestDistSq = TestDist;
		}
	}

	return RetPoint;
}

FORCEINLINE float DistanceSqToSlateRotatedRect(const FVector2D &Point, const FSlateRotatedRect& RotatedRect)
{
	return FVector2D::DistSquared(ClosestPointOnSlateRotatedRect(Point, RotatedRect), Point);
}

FORCEINLINE bool IsOverlappingSlateRotatedRect(const FVector2D& Point, const float Radius, const FSlateRotatedRect& RotatedRect)
{
	return DistanceSqToSlateRotatedRect( Point, RotatedRect ) <= (Radius * Radius);
}

bool ContainsInteractableWidget(const TArray<FWidgetAndPointer>& PathToTest)
{
	for (int32 i = PathToTest.Num() - 1; i >= 0; --i)
	{
		const FWidgetAndPointer& WidgetAndPointer = PathToTest[i];
		if (WidgetAndPointer.Widget->IsInteractable())
		{
			return true;
		}
	}
	return false;
}

//
// FHittestGrid
//

const FVector2D CellSize(128.0f, 128.0f);

struct FHittestGrid::FGridTestingParams
{
	/** Ctor */
	FGridTestingParams()
	: CellCoord(-1, -1)
	, CursorPositionInGrid(FVector2D::ZeroVector)
	, Radius(-1.0f)
	, bTestWidgetIsInteractive(false)
	{}

	FIntPoint CellCoord;
	FVector2D CursorPositionInGrid;
	float Radius;
	bool bTestWidgetIsInteractive;
};


struct FHittestGrid::FCachedWidget
{
	FCachedWidget(int32 InParentIndex, const FArrangedWidget& InWidget, int32 InClippingStateIndex, int32 InLayerId)
	: WidgetPtr(InWidget.Widget)
	, CachedGeometry(InWidget.Geometry)
	, ClippingStateIndex(InClippingStateIndex)
	, Children()
	, ParentIndex(InParentIndex)
	, LayerId(InLayerId)
	{}

	void AddChild(const int32 ChildIndex)
	{
		Children.Add(ChildIndex);
	}

	TWeakPtr<SWidget> WidgetPtr;
	/** Allow widgets that implement this interface to insert widgets into the bubble path */
	TWeakPtr<ICustomHitTestPath> CustomPath;
	FGeometry CachedGeometry;
	int32 ClippingStateIndex;
	TArray<int32, TInlineAllocator<16> > Children;
	int32 ParentIndex;
	/** This is needed to be able to pick the best of the widgets within the virtual cursor's radius. */
	int32 LayerId;
};


FHittestGrid::FHittestGrid()
: WidgetsCachedThisFrame( new TArray<FCachedWidget>() )
{
}


FHittestGrid::~FHittestGrid()
{
}

TArray<FWidgetAndPointer> FHittestGrid::GetBubblePath(FVector2D DesktopSpaceCoordinate, float CursorRadius, bool bIgnoreEnabledStatus)
{
	//grab the radius, and if we are non-zero
	const bool bDirectTestingOnly = (CursorRadius <= 0.0f);

	//calculate the cursor position in the grid
	const FVector2D CursorPositionInGrid = DesktopSpaceCoordinate - GridOrigin;

	if (WidgetsCachedThisFrame->Num() > 0 && Cells.Num() > 0)
	{
		//grab the path for direct testing first
		FGridTestingParams DirectTestingParams;
		DirectTestingParams.CursorPositionInGrid = CursorPositionInGrid;
		DirectTestingParams.CellCoord = GetCellCoordinate(CursorPositionInGrid);
		DirectTestingParams.Radius = 0.0f;
		DirectTestingParams.bTestWidgetIsInteractive = false;

		const FWidgetPathAndDist DirectBubblePathInfo = GetWidgetPathAndDist(DirectTestingParams, bIgnoreEnabledStatus);

		//if we aren't doing a radius check, or we already have a direct path, use that
		if (bDirectTestingOnly || ContainsInteractableWidget(DirectBubblePathInfo.BubblePath))
		{
			return DirectBubblePathInfo.BubblePath;
		}

		//if we are here, we need to check other cells
		const FVector2D RadiusVector(CursorRadius, CursorRadius);
		const FIntPoint ULIndex = GetCellCoordinate(CursorPositionInGrid - RadiusVector);
		const FIntPoint LRIndex = GetCellCoordinate(CursorPositionInGrid + RadiusVector);

		//first, find all the overlapping cells
		TArray<FIntPoint, TInlineAllocator<16>> CellIndexes;

		for (int32 YIndex = ULIndex.Y; YIndex <= LRIndex.Y; ++YIndex)
		{
			for (int32 XIndex = ULIndex.X; XIndex <= LRIndex.X; ++XIndex)
			{
				const FIntPoint PointToTest(XIndex, YIndex);
				if (IsValidCellCoord(PointToTest))
				{
					CellIndexes.Add(PointToTest);
				}
			}
		}

		//setup the radius testing params
		FGridTestingParams RadiusTestingParams;
		RadiusTestingParams.CursorPositionInGrid = CursorPositionInGrid;
		RadiusTestingParams.Radius = CursorRadius;
		RadiusTestingParams.bTestWidgetIsInteractive = true;

		//next, collect valid paths from all those cells
		TArray<FWidgetPathAndDist> PathsAndDistances;
		for (const FIntPoint& CellCoord : CellIndexes)
		{
			//update the cell coord property
			RadiusTestingParams.CellCoord = CellCoord;

			const FWidgetPathAndDist TestPath = GetWidgetPathAndDist(RadiusTestingParams, bIgnoreEnabledStatus);
			if ( TestPath.IsValidPath() )
			{
				PathsAndDistances.Add(TestPath);
			}
		}

		// We have paths from multiple cells; use the one that is closest to the cursor's center.
		if (PathsAndDistances.Num() > 0)
		{
			//sort the paths, and get the closest one that is valid
			PathsAndDistances.Sort([](const FWidgetPathAndDist& A, const FWidgetPathAndDist& B)
			{
				return A.DistToTopWidgetSq < B.DistToTopWidgetSq;
			});

			const FWidgetPathAndDist* BestCandidateByDistance = nullptr;
			for (const FWidgetPathAndDist& TestPath : PathsAndDistances)
			{
				if (ContainsInteractableWidget(TestPath.BubblePath))
				{
					BestCandidateByDistance = &TestPath;
					break;
				}
			}

			if (BestCandidateByDistance != nullptr)
			{
				const FGeometry& LeafmostGeometry = BestCandidateByDistance->BubblePath.Last().Geometry;
				const int32 LeafmostLayerId = BestCandidateByDistance->LayerId;
				if (DirectBubblePathInfo.LayerId > LeafmostLayerId)
				{
					return DirectBubblePathInfo.BubblePath;
				}
				else
				{
					return BestCandidateByDistance->BubblePath;
				}
			}
		}


		return DirectBubblePathInfo.BubblePath;
	}
	else
	{
		// We didn't hit anything.
		return TArray<FWidgetAndPointer>();
	}
}

void FHittestGrid::ClearGridForNewFrame(const FSlateRect& HittestArea)
{
	//LogGrid();

	GridOrigin = HittestArea.GetTopLeft();
	const FVector2D GridSize = HittestArea.GetSize();
	NumCells = FIntPoint(FMath::CeilToInt(GridSize.X / CellSize.X), FMath::CeilToInt(GridSize.Y / CellSize.Y));
	WidgetsCachedThisFrame->Reset();

	const int32 NewTotalCells = NumCells.X * NumCells.Y;
	if (NewTotalCells != Cells.Num())
	{
		Cells.Reset(NewTotalCells);
		Cells.SetNum(NewTotalCells);
	}
	else
	{
		// As an optimization, if the number of cells do not change then we will just reset the index list inside of them
		// This will leave slack for indices to be re-added without reallocating.
		for (FCell& Cell : Cells)
		{
			Cell.CachedWidgetIndexes.Reset();
		}
	}

	ClippingManager.ResetClippingState();
}

void FHittestGrid::PushClip(const FSlateClippingZone& ClippingZone)
{
	ClippingManager.PushClip(ClippingZone);
}

void FHittestGrid::PopClip()
{
	ClippingManager.PopClip();
}

int32 FHittestGrid::InsertWidget(const int32 ParentHittestIndex, const EVisibility& Visibility, const FArrangedWidget& ArrangedWidget, const FVector2D InWindowOffset, int32 LayerId)
{
	if (ensureMsgf(ParentHittestIndex < WidgetsCachedThisFrame->Num(), TEXT("Widget '%s' being drawn before its parent."), *ArrangedWidget.ToString()))
	{
		// Update the FGeometry to transform into desktop space.
		FArrangedWidget WindowAdjustedWidget(ArrangedWidget);
		WindowAdjustedWidget.Geometry.AppendTransform(FSlateLayoutTransform(InWindowOffset));

#if 0
		// Enable this code if you're trying to make sure a widget is only ever added to the hit test grid once.
		const FCachedWidget* ExistingCachedWidget =
			WidgetsCachedThisFrame->FindByPredicate([Widget](const FCachedWidget& CachedWidget) {
			return CachedWidget.WidgetPtr.Pin() == Widget.Widget;
		});

		ensure(ExistingCachedWidget == nullptr);
#endif

		const int32 ClippingStateIndex = ClippingManager.GetClippingIndex();

		// Remember this widget, its geometry, and its place in the logical hierarchy.
		const int32 WidgetIndex = WidgetsCachedThisFrame->Add(FCachedWidget(ParentHittestIndex, WindowAdjustedWidget, ClippingStateIndex, LayerId));
		check(WidgetIndex < WidgetsCachedThisFrame->Num());
		if (ParentHittestIndex != INDEX_NONE)
		{
			(*WidgetsCachedThisFrame)[ParentHittestIndex].AddChild(WidgetIndex);
		}

		if (Visibility.IsHitTestVisible())
		{
			// Mark any cell that is overlapped by this widget.

			// Compute the render space clipping rect, and compute it's aligned bounds so we can insert conservatively into the hit test grid.
			FSlateRect GridRelativeBoundingClipRect =
				WindowAdjustedWidget.Geometry.GetRenderBoundingRect()
				.OffsetBy(-GridOrigin);

			// Starting and ending cells covered by this widget.	
			const FIntPoint UpperLeftCell = FIntPoint(
				FMath::Max(0, FMath::FloorToInt(GridRelativeBoundingClipRect.Left / CellSize.X)),
				FMath::Max(0, FMath::FloorToInt(GridRelativeBoundingClipRect.Top / CellSize.Y)));

			const FIntPoint LowerRightCell = FIntPoint(
				FMath::Min(NumCells.X - 1, FMath::FloorToInt(GridRelativeBoundingClipRect.Right / CellSize.X)),
				FMath::Min(NumCells.Y - 1, FMath::FloorToInt(GridRelativeBoundingClipRect.Bottom / CellSize.Y)));

			for (int32 XIndex = UpperLeftCell.X; XIndex <= LowerRightCell.X; ++XIndex)
			{
				for (int32 YIndex = UpperLeftCell.Y; YIndex <= LowerRightCell.Y; ++YIndex)
				{
					CellAt(XIndex, YIndex).CachedWidgetIndexes.Add(WidgetIndex);
				}
			}
		}

		return WidgetIndex;
	}
	else
	{
		return INDEX_NONE;
	}
}

void FHittestGrid::InsertCustomHitTestPath( TSharedRef<ICustomHitTestPath> CustomHitTestPath, int32 WidgetIndex )
{
	if( WidgetsCachedThisFrame->IsValidIndex( WidgetIndex ) )
	{
		(*WidgetsCachedThisFrame)[WidgetIndex].CustomPath = CustomHitTestPath;
	}
}

bool FHittestGrid::IsDescendantOf(const TSharedRef<SWidget> Parent, const FCachedWidget& Child)
{
	TSharedPtr<SWidget> ChildWidget = Child.WidgetPtr.Pin();

	if (!ChildWidget.IsValid() || ChildWidget == Parent)
	{
		return false;
	}

	int32 CurWidgetIndex = Child.ParentIndex;
	while (CurWidgetIndex != INDEX_NONE)
	{
		const FCachedWidget& CurCachedWidget = (*WidgetsCachedThisFrame)[CurWidgetIndex];
		CurWidgetIndex = CurCachedWidget.ParentIndex;

		if (Parent == CurCachedWidget.WidgetPtr.Pin())
		{
			return true;
		}
	}

	return false;
}

template<typename TCompareFunc, typename TSourceSideFunc, typename TDestSideFunc>
TSharedPtr<SWidget> FHittestGrid::FindFocusableWidget(FSlateRect WidgetRect, const FSlateRect SweptRect, int32 AxisIndex, int32 Increment, const EUINavigation Direction, const FNavigationReply& NavigationReply, TCompareFunc CompareFunc, TSourceSideFunc SourceSideFunc, TDestSideFunc DestSideFunc)
{
	FIntPoint CurrentCellPoint = GetCellCoordinate(WidgetRect.GetCenter());

	int32 StartingIndex = CurrentCellPoint[AxisIndex];

	float CurrentSourceSide = SourceSideFunc(WidgetRect);

	int32 StrideAxis, StrideAxisMin, StrideAxisMax;

	// Ensure that the hit test grid is valid before proceeding
	if (NumCells.X < 1 || NumCells.Y < 1)
	{
		return TSharedPtr<SWidget>();
	}

	if (AxisIndex == 0)
	{
		StrideAxis = 1;
		StrideAxisMin = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Top / CellSize.Y), 0), NumCells.Y - 1);
		StrideAxisMax = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Bottom / CellSize.Y), 0), NumCells.Y - 1);
	}
	else
	{
		StrideAxis = 0;
		StrideAxisMin = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Left / CellSize.X), 0), NumCells.X - 1);
		StrideAxisMax = FMath::Min(FMath::Max(FMath::FloorToInt(SweptRect.Right / CellSize.X), 0), NumCells.X - 1);
	}

	bool bWrapped = false;
	while (CurrentCellPoint[AxisIndex] >= 0 && CurrentCellPoint[AxisIndex] < NumCells[AxisIndex])
	{
		FIntPoint StrideCellPoint = CurrentCellPoint;
		int32 CurrentCellProcessed = CurrentCellPoint[AxisIndex];

		// Increment before the search as a wrap case will change our current cell.
		CurrentCellPoint[AxisIndex] += Increment;

		FSlateRect BestWidgetRect;
		TSharedPtr<SWidget> BestWidget = TSharedPtr<SWidget>();

		for (StrideCellPoint[StrideAxis] = StrideAxisMin; StrideCellPoint[StrideAxis] <= StrideAxisMax; ++StrideCellPoint[StrideAxis])
		{
			FHittestGrid::FCell& Cell = FHittestGrid::CellAt(StrideCellPoint.X, StrideCellPoint.Y);
			const TArray<int32>& IndexesInCell = Cell.CachedWidgetIndexes;

			for (int32 i = IndexesInCell.Num() - 1; i >= 0; --i)
			{
				int32 CurrentIndex = IndexesInCell[i];
				check(CurrentIndex < WidgetsCachedThisFrame->Num());

				const FCachedWidget& TestCandidate = (*WidgetsCachedThisFrame)[CurrentIndex];
				FSlateRect TestCandidateRect = TestCandidate.CachedGeometry.GetRenderBoundingRect().OffsetBy(-GridOrigin);

				if (CompareFunc(DestSideFunc(TestCandidateRect), CurrentSourceSide) && FSlateRect::DoRectanglesIntersect(SweptRect, TestCandidateRect))
				{
					// If this found widget isn't closer then the previously found widget then keep looking.
					if (BestWidget.IsValid() && !CompareFunc(DestSideFunc(BestWidgetRect), DestSideFunc(TestCandidateRect)))
					{
						continue;
					}

					// If we have a non escape boundary condition and this widget isn't a descendant of our boundary condition widget then it's invalid so we keep looking.
					if (NavigationReply.GetBoundaryRule() != EUINavigationRule::Escape
						&& NavigationReply.GetHandler().IsValid()
						&& !IsDescendantOf(NavigationReply.GetHandler().ToSharedRef(), TestCandidate))
					{
						continue;
					}

					TSharedPtr<SWidget> Widget = TestCandidate.WidgetPtr.Pin();
					if (Widget.IsValid() && Widget->IsEnabled() && Widget->SupportsKeyboardFocus())
					{
						BestWidgetRect = TestCandidateRect;
						BestWidget = Widget;
					}
				}
			}
		}

		if (BestWidget.IsValid())
		{
			// Check for the need to apply our rule
			if (CompareFunc(DestSideFunc(BestWidgetRect), SourceSideFunc(SweptRect)))
			{
				switch (NavigationReply.GetBoundaryRule())
				{
				case EUINavigationRule::Explicit:
					return NavigationReply.GetFocusRecipient();
				case EUINavigationRule::Custom:
				{
					const FNavigationDelegate& FocusDelegate = NavigationReply.GetFocusDelegate();
					if (FocusDelegate.IsBound())
					{
						return FocusDelegate.Execute(Direction);
					}
					return TSharedPtr<SWidget>();
				}
				case EUINavigationRule::Stop:
					return TSharedPtr<SWidget>();
				case EUINavigationRule::Wrap:
					CurrentSourceSide = DestSideFunc(SweptRect);
					FVector2D SampleSpot = WidgetRect.GetCenter();
					SampleSpot[AxisIndex] = CurrentSourceSide;
					CurrentCellPoint = GetCellCoordinate(SampleSpot);
					bWrapped = true;
					break;
				}
			}

			return BestWidget;
		}

		// break if we have looped back to where we started.
		if (bWrapped && StartingIndex == CurrentCellProcessed) { break; }

		// If were going to fail our bounds check and our rule is to wrap then wrap our position
		if (!(CurrentCellPoint[AxisIndex] >= 0 && CurrentCellPoint[AxisIndex] < NumCells[AxisIndex]) && NavigationReply.GetBoundaryRule() == EUINavigationRule::Wrap)
		{
			CurrentSourceSide = DestSideFunc(SweptRect);
			FVector2D SampleSpot = WidgetRect.GetCenter();
			SampleSpot[AxisIndex] = CurrentSourceSide;
			CurrentCellPoint = GetCellCoordinate(SampleSpot);
			bWrapped = true;
		}
	}

	return TSharedPtr<SWidget>();
}

TSharedPtr<SWidget> FHittestGrid::FindNextFocusableWidget(const FArrangedWidget& StartingWidget, const EUINavigation Direction, const FNavigationReply& NavigationReply, const FArrangedWidget& RuleWidget)
{
	FSlateRect WidgetRect =
		TransformRect(
			Concatenate(
				Inverse(StartingWidget.Geometry.GetAccumulatedLayoutTransform()),
				StartingWidget.Geometry.GetAccumulatedRenderTransform()
			),
			FSlateRotatedRect(StartingWidget.Geometry.GetLayoutBoundingRect())
		)
		.ToBoundingRect()
		.OffsetBy(-GridOrigin);

	FSlateRect BoundingRuleRect =
		TransformRect(
			Concatenate(
				Inverse(RuleWidget.Geometry.GetAccumulatedLayoutTransform()),
				RuleWidget.Geometry.GetAccumulatedRenderTransform()
			),
			FSlateRotatedRect(RuleWidget.Geometry.GetLayoutBoundingRect())
		)
		.ToBoundingRect()
		.OffsetBy(-GridOrigin);

	FSlateRect SweptWidgetRect = WidgetRect;

	TSharedPtr<SWidget> Widget = TSharedPtr<SWidget>();

	switch (Direction)
	{
	case EUINavigation::Left:
		SweptWidgetRect.Left = BoundingRuleRect.Left;
		SweptWidgetRect.Right = BoundingRuleRect.Right;
		SweptWidgetRect.Top += 0.5f;
		SweptWidgetRect.Bottom -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 0, -1, Direction, NavigationReply,
			[](float A, float B) { return A - 0.1f < B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Left; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Right; }); // Dest side function
		break;
	case EUINavigation::Right:
		SweptWidgetRect.Left = BoundingRuleRect.Left;
		SweptWidgetRect.Right = BoundingRuleRect.Right;
		SweptWidgetRect.Top += 0.5f;
		SweptWidgetRect.Bottom -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 0, 1, Direction, NavigationReply,
			[](float A, float B) { return A + 0.1f > B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Right; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Left; }); // Dest side function
		break;
	case EUINavigation::Up:
		SweptWidgetRect.Top = BoundingRuleRect.Top;
		SweptWidgetRect.Bottom = BoundingRuleRect.Bottom;
		SweptWidgetRect.Left += 0.5f;
		SweptWidgetRect.Right -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 1, -1, Direction, NavigationReply,
			[](float A, float B) { return A - 0.1f < B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Top; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Bottom; }); // Dest side function
		break;
	case EUINavigation::Down:
		SweptWidgetRect.Top = BoundingRuleRect.Top;
		SweptWidgetRect.Bottom = BoundingRuleRect.Bottom;
		SweptWidgetRect.Left += 0.5f;
		SweptWidgetRect.Right -= 0.5f;
		Widget = FindFocusableWidget(WidgetRect, SweptWidgetRect, 1, 1, Direction, NavigationReply,
			[](float A, float B) { return A + 0.1f > B; }, // Compare function
			[](FSlateRect SourceRect) { return SourceRect.Bottom; }, // Source side function
			[](FSlateRect DestRect) { return DestRect.Top; }); // Dest side function
		break;

	default:
		break;
	}

	return Widget;
}

FIntPoint FHittestGrid::GetCellCoordinate(FVector2D Position)
{
	return FIntPoint(
		FMath::Min(FMath::Max(FMath::FloorToInt(Position.X / CellSize.X), 0), NumCells.X - 1),
		FMath::Min(FMath::Max(FMath::FloorToInt(Position.Y / CellSize.Y), 0), NumCells.Y - 1));
}

bool FHittestGrid::IsValidCellCoord(const FIntPoint& CellCoord) const
{
	return IsValidCellCoord(CellCoord.X, CellCoord.Y);
}

bool FHittestGrid::IsValidCellCoord(const int32 XCoord, const int32 YCoord) const
{
	const int32 Index = (YCoord * NumCells.X) + XCoord;
	return Cells.IsValidIndex(Index);
}

void FHittestGrid::LogGrid() const
{
	FString TempString;
	for (int y = 0; y < NumCells.Y; ++y)
	{
		for (int x = 0; x < NumCells.X; ++x)
		{
			TempString += "\t";
			TempString += "[";
			for (int i : CellAt(x, y).CachedWidgetIndexes)
			{
				TempString += FString::Printf(TEXT("%d,"), i);
			}
			TempString += "]";
		}
		TempString += "\n";
	}

	TempString += "\n";

	UE_LOG(LogHittestDebug, Warning, TEXT("\n%s"), *TempString);

	for (int i = 0; i < WidgetsCachedThisFrame->Num(); ++i)
	{
		if ((*WidgetsCachedThisFrame)[i].ParentIndex == INDEX_NONE)
		{
			LogChildren(i, 0, *WidgetsCachedThisFrame);
		}
	}
}

void FHittestGrid::LogChildren(int32 Index, int32 IndentLevel, const TArray<FCachedWidget>& WidgetsCachedThisFrame)
{
	FString IndentString;
	for (int i = 0; i < IndentLevel; ++i)
	{
		IndentString += "|\t";
	}

	const FCachedWidget& CachedWidget = WidgetsCachedThisFrame[Index];
	const TSharedPtr<SWidget> CachedWidgetPtr = CachedWidget.WidgetPtr.Pin();
	const FString WidgetString = CachedWidgetPtr.IsValid() ? CachedWidgetPtr->ToString() : TEXT("(null)");
	UE_LOG(LogHittestDebug, Warning, TEXT("%s[%d] => %s @ %s"), *IndentString, Index, *WidgetString, *CachedWidget.CachedGeometry.ToString());

	for (int i = 0; i < CachedWidget.Children.Num(); ++i)
	{
		LogChildren(CachedWidget.Children[i], IndentLevel + 1, WidgetsCachedThisFrame);
	}
}

FHittestGrid::FWidgetPathAndDist FHittestGrid::GetWidgetPathAndDist(const FGridTestingParams& Params, const bool bIgnoreEnabledStatus) const
{
	//Also grab the Hit Index, and the distance to the top hit
	const FIndexAndDistance HitIndex = GetHitIndexFromCellIndex(Params);

	if (HitIndex.WidgetIndex != INDEX_NONE)
	{
		// if we have a custom path, we want to do the testing for 3D Widgets
		const FCachedWidget& PhysicallyHitWidget = (*WidgetsCachedThisFrame)[HitIndex.WidgetIndex];
		if (PhysicallyHitWidget.CustomPath.IsValid())
		{
			const FVector2D DesktopSpaceCoordinate = Params.CursorPositionInGrid + GridOrigin;
			TArray<FWidgetAndPointer> LogicalBubblePath = GetBubblePathFromHitIndex(HitIndex.WidgetIndex, bIgnoreEnabledStatus);
			const TArray<FWidgetAndPointer> BubblePathExtension = PhysicallyHitWidget.CustomPath.Pin()->GetBubblePathAndVirtualCursors(PhysicallyHitWidget.CachedGeometry, DesktopSpaceCoordinate, bIgnoreEnabledStatus);
			LogicalBubblePath.Append(BubblePathExtension);
			return FWidgetPathAndDist(LogicalBubblePath, 0.0, PhysicallyHitWidget.LayerId);
		}
		else
		{
			//get the path from the hit index, check if anything came back
			const TArray<FWidgetAndPointer> BubblePath = GetBubblePathFromHitIndex(HitIndex.WidgetIndex, bIgnoreEnabledStatus);
			return FWidgetPathAndDist(BubblePath, BubblePath.Num() > 0 ? HitIndex.DistanceSqToWidget : -1.0f, PhysicallyHitWidget.LayerId);
		}
	}

	//return if we have a valid path or not
	return FWidgetPathAndDist();
}

FHittestGrid::FIndexAndDistance FHittestGrid::GetHitIndexFromCellIndex(const FGridTestingParams& Params) const
{
	//check if the cell coord 
	if (IsValidCellCoord(Params.CellCoord))
	{
		const TArray<int32>& IndexesInCell = CellAt(Params.CellCoord.X, Params.CellCoord.Y).CachedWidgetIndexes;
		const TArray< FSlateClippingState >& ClippingStates = ClippingManager.GetClippingStates();

		// Consider front-most widgets first for hittesting.
		for (int32 i = IndexesInCell.Num() - 1; i >= 0; --i)
		{
			const int32 WidgetIndex = IndexesInCell[i];

			check(WidgetsCachedThisFrame->IsValidIndex(WidgetIndex));

			const FHittestGrid::FCachedWidget& TestCandidate = (*WidgetsCachedThisFrame)[WidgetIndex];

			// When performing a point hittest, accept all hittestable widgets.
			// When performing a hittest with a radius, only grab interactive widgets.
			const bool bIsValidWidget = !Params.bTestWidgetIsInteractive || (TestCandidate.WidgetPtr.IsValid() && TestCandidate.WidgetPtr.Pin()->IsInteractable());
			if (bIsValidWidget)
			{
				const FVector2D DesktopSpaceCoordinate = Params.CursorPositionInGrid + GridOrigin;
				
				bool bPointInsideClipMasks = true;
				if (TestCandidate.ClippingStateIndex != INDEX_NONE)
				{
					const FSlateClippingState& ClippingState = ClippingStates[TestCandidate.ClippingStateIndex];

					// TODO Solve non-zero radius cursors?
					bPointInsideClipMasks = ClippingState.IsPointInside(DesktopSpaceCoordinate);
				}

				if (bPointInsideClipMasks)
				{
					// Compute the render space clipping rect (FGeometry exposes a layout space clipping rect).
					const FSlateRotatedRect DesktopOrientedClipRect = TransformRect(
						Concatenate(
							Inverse(TestCandidate.CachedGeometry.GetAccumulatedLayoutTransform()),
							TestCandidate.CachedGeometry.GetAccumulatedRenderTransform()),
						FSlateRotatedRect(TestCandidate.CachedGeometry.GetLayoutBoundingRect())
					);

					if (IsOverlappingSlateRotatedRect(DesktopSpaceCoordinate, Params.Radius, DesktopOrientedClipRect))
					{
						// We are within the search radius!
						const bool bNeedsDistanceSearch = Params.Radius > 0.0f;
						// For non-0 radii also record the distance to cursor's center so that we can pick the closest hit from the results.
						const float DistSq = (bNeedsDistanceSearch) ? DistanceSqToSlateRotatedRect(DesktopSpaceCoordinate, DesktopOrientedClipRect) : 0.0f;
						return FIndexAndDistance(WidgetIndex, DistSq);
					}
				}
			}
		}
	}

	return FIndexAndDistance();
}

TArray<FWidgetAndPointer> FHittestGrid::GetBubblePathFromHitIndex(const int32 HitIndex, const bool bIgnoreEnabledStatus) const
{
	TArray<FWidgetAndPointer> BubblePath;

	if (WidgetsCachedThisFrame->IsValidIndex(HitIndex))
	{
		int32 CurWidgetIndex = HitIndex;
		do
		{
			check(CurWidgetIndex < WidgetsCachedThisFrame->Num());
			const FHittestGrid::FCachedWidget& CurCachedWidget = (*WidgetsCachedThisFrame)[CurWidgetIndex];
			const TSharedPtr<SWidget> CachedWidgetPtr = CurCachedWidget.WidgetPtr.Pin();


			const bool bPathInterrupted = !CachedWidgetPtr.IsValid();
			if (bPathInterrupted)
			{
				// A widget in the path to the root has been removed, so anything
				// we thought we had hittest so far is no longer actually in the hierarchy.
				// Continue bubbling to the root of the hirarchy to find an unbroken chain to root.
				// The leafmost widget in that chain will get first shot at the events.
				BubblePath.Reset();
			}
			else
			{
				BubblePath.Insert(FWidgetAndPointer(FArrangedWidget(CachedWidgetPtr.ToSharedRef(), CurCachedWidget.CachedGeometry), TSharedPtr<FVirtualPointerPosition>()), 0);
			}
			CurWidgetIndex = CurCachedWidget.ParentIndex;
		} while (CurWidgetIndex != INDEX_NONE);


		if (SlateVerifyHitTestVisibility)
		{
			// Hit Test Invisible widgets effects all of the logical children.
			// Normally this isn't a problem, but in the case of low framerate
			// if multiple mouse events buffer up, and are consumed in one frame
			// it's possible that in one frame, the first mouse event might change
			// the hit-test ability of widgets.
			{
				const int32 HitTestInvisibleWidgetIndex = BubblePath.IndexOfByPredicate([](const FArrangedWidget& SomeWidget) { return !SomeWidget.Widget->GetVisibility().AreChildrenHitTestVisible(); });
				if (HitTestInvisibleWidgetIndex != INDEX_NONE)
				{
					BubblePath.RemoveAt(HitTestInvisibleWidgetIndex, BubblePath.Num() - HitTestInvisibleWidgetIndex);
				}
			}

			// Similar to the above check, this determines if a widget became hit test invisible
			// directly, because rather than an entire set of children becoming invisible
			// this addresses the problem of a specific widget changing visibility.
			{
				int32 FirstHitTestWidgetIndex = INDEX_NONE;
				for (int32 Index = BubblePath.Num() - 1; Index >= 0; --Index)
				{
					const EVisibility Visibilty = BubblePath[Index].Widget->GetVisibility();

					if (Visibilty.IsHitTestVisible())
					{
						FirstHitTestWidgetIndex = Index;
						break;
					}
				}

				if (FirstHitTestWidgetIndex != INDEX_NONE)
				{
					const int32 RemovalCount = (BubblePath.Num() - (FirstHitTestWidgetIndex + 1));
					if (RemovalCount > 0)
					{
						BubblePath.RemoveAt(FirstHitTestWidgetIndex + 1, RemovalCount);
					}
				}
				else
				{
					BubblePath.Reset();
				}
			}
		}

		// Disabling a widget disables all of its logical children
		// This effect is achieved by truncating the path to the
		// root-most enabled widget.
		if (!bIgnoreEnabledStatus)
		{
			const int32 DisabledWidgetIndex = BubblePath.IndexOfByPredicate([](const FArrangedWidget& SomeWidget) { return !SomeWidget.Widget->IsEnabled(); });
			if (DisabledWidgetIndex != INDEX_NONE)
			{
				BubblePath.RemoveAt(DisabledWidgetIndex, BubblePath.Num() - DisabledWidgetIndex);
			}
		}
	}

	return BubblePath;
}