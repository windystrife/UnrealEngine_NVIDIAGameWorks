// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerThreadView.h"
#include "Brushes/SlateColorBrush.h"
#include "EditorStyleSet.h"


SProfilerThreadView::SProfilerThreadView()
{
	Reset();
}

SProfilerThreadView::~SProfilerThreadView()
{

}

void SProfilerThreadView::Construct( const FArguments& InArgs )
{
	BindCommands();
}

void SProfilerThreadView::Reset()
{
	ProfilerStream = nullptr;

	FMemory::Memzero( PaintStateMemory );
	PaintState = nullptr;

	MousePosition = FVector2D::ZeroVector;
	LastMousePosition = FVector2D::ZeroVector;
	MousePositionOnButtonDown = FVector2D::ZeroVector;

	PositionXMS = 0.0f;
	PositionY = 0.0f;
	RangeXMS = 0.0f;
	RangeY = 0.0f;
	TotalRangeXMS = 0.0f;
	TotalRangeY = 0.0f;
	ZoomFactorX = 1.0f;

	NumMillisecondsPerWindow = NUM_MILLISECONDS_PER_WINDOW;
	NumPixelsPerMillisecond = 0.0f;
	NumMillisecondsPerSample = 0.0f;

	HoveredFrameIndex = 0;
	HoveredThreadID = 0;
	HoveredPositionX = 0.0f;
	HoveredPositionY = 0.0f;

	DistanceDragged = 0.0f;

	bIsLeftMousePressed = false;
	bIsRightMousePressed = false;
	bUpdateData = false;
	CursorType = EThreadViewCursor::Default;
}

void SProfilerThreadView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if( AllottedGeometry.Size.X > 0.0f )
	{
		if( ThisGeometry.Size.X != AllottedGeometry.Size.X )
		{
			// Refresh.
			ThisGeometry = AllottedGeometry;
			bUpdateData = true;
		}

		if( ShouldUpdateData() && IsReady() )
		{
			UpdateInternalConstants();
			ProcessData();
			bUpdateData = false;
		}
	}
}

int32 SProfilerThreadView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	//	SCOPE_LOG_TIME_FUNC();

	// Rendering info.
	const bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* BackgroundBrush = FEditorStyle::GetBrush( "Profiler.LineGraphArea" );
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush( "WhiteTexture" );

	// Paint state for this call to OnPaint, valid only in this scope.
	PaintState = new((void*)PaintStateMemory) FSlateOnPaintState( AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, DrawEffects );

	// Draw background.
	FSlateDrawElement::MakeBox
	(
		PaintState->OutDrawElements,
		PaintState->LayerId,
		PaintState->AllottedGeometry.ToPaintGeometry( FVector2D( 0, 0 ), PaintState->Size2D() ),
		BackgroundBrush,
		PaintState->DrawEffects,
		BackgroundBrush->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
	);
	LayerId++;

	// Draw all cycle counters for each thread nodes.
	if( IsReady() )
	{
		DrawFramesBackgroundAndTimelines();
		DrawUIStackNodes();
		DrawFrameMarkers();
	}

#if 0/*DEBUG_PROFILER_PERFORMANCE*/

	LayerId++;
	// Draw debug information.
	float GraphDescPosY = PaintState->Size2D().Y - 4.0f * PaintState->SummaryFont8Height;

	// Debug text.
	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D( 16.0f, GraphDescPosY ) ),
		FString::Printf( TEXT( "Pos X=%f,Y=%f R X=%f,Y=%f TR X=%f,Y=%f ZF X=%f" ), PositionXMS, PositionY, RangeXMS, RangeY, TotalRangeXMS, TotalRangeY, ZoomFactorX ),
		PaintState->SummaryFont8,
		MyCullingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY -= PaintState->SummaryFont8Height + 1.0f;

	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D( 16.0f, GraphDescPosY ) ),
		FString::Printf( TEXT( "NumMSPerWin=%f H Fr=%i,TID=%i,PX=%f,PY=%f" ), NumMillisecondsPerWindow, HoveredFrameIndex, HoveredThreadID, HoveredPositionX, HoveredPositionY ),
		PaintState->SummaryFont8,
		MyCullingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY -= PaintState->SummaryFont8Height + 1.0f;

	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D( 16.0f, GraphDescPosY ) ),
		FString::Printf( TEXT( "DistD=%.2f FI=%3i,%3i" ), DistanceDragged, FramesIndices.X, FramesIndices.Y ),
		PaintState->SummaryFont8,
		MyCullingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY -= PaintState->SummaryFont8Height + 1.0f;

#endif // DEBUG_PROFILER_PERFORMANCE

	// Reset paint state.
	PaintState = nullptr;

	return SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled() );
}


void SProfilerThreadView::DrawFramesBackgroundAndTimelines() const
{
	static const FSlateColorBrush SolidWhiteBrush = FSlateColorBrush( FColorList::White );

	check( PaintState );
	const double ThreadViewOffsetPx = PositionXMS*NumPixelsPerMillisecond;
	PaintState->LayerId++;
	TArray<FVector2D> LinePoints;

	// Draw frames background for easier reading.
	for( const auto& ThreadNode : ProfilerUIStream.ThreadNodes )
	{
		if( ThreadNode.StatName == NAME_GameThread )
		{
			const FVector2D PositionPx = ThreadNode.GetLocalPosition( ThreadViewOffsetPx, -1.0f );
			const FVector2D SizePx = FVector2D( ThreadNode.WidthPx, PaintState->Size2D().Y );

			const FSlateRect ClippedFrameBackgroundRect = PaintState->LocalClippingRect.IntersectionWith( FSlateRect( PositionPx, PositionPx + SizePx ) );

			FSlateDrawElement::MakeBox
			(
				PaintState->OutDrawElements,
				PaintState->LayerId,
				PaintState->AllottedGeometry.ToPaintGeometry( ClippedFrameBackgroundRect.GetTopLeft(), ClippedFrameBackgroundRect.GetSize() ),
				&SolidWhiteBrush,
				PaintState->DrawEffects,
				(ThreadNode.FrameIndex % 2) ? FColorList::White.WithAlpha( 64 ) : FColorList::White.WithAlpha( 128 )
			);

			// Check if this frame time marker is inside the visible area.
			const float LocalPositionXPx = PositionPx.X + SizePx.X;
			if( LocalPositionXPx < 0.0f || LocalPositionXPx > PaintState->Size2D().X )
			{
				continue;
			}

			LinePoints.Reset( 2 );
			LinePoints.Add( FVector2D( LocalPositionXPx, 0.0f ) );
			LinePoints.Add( FVector2D( LocalPositionXPx, PaintState->Size2D().Y ) );

			// Draw frame time marker.
			FSlateDrawElement::MakeLines
			(
				PaintState->OutDrawElements,
				PaintState->LayerId,
				PaintState->AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				PaintState->DrawEffects,
				PaintState->WidgetStyle.GetColorAndOpacityTint() * FColorList::SkyBlue,
				false
			);
		}
	}

	PaintState->LayerId++;

	const double PositionXStartPx = FMath::TruncToFloat( PositionXMS*NumPixelsPerMillisecond / (double)NUM_PIXELS_BETWEEN_TIMELINE )*(double)NUM_PIXELS_BETWEEN_TIMELINE;
	const double PositionXEndPx = PositionXStartPx + RangeXMS*NumPixelsPerMillisecond;

	for( double TimelinePosXPx = PositionXStartPx; TimelinePosXPx < PositionXEndPx; TimelinePosXPx += (double)NUM_PIXELS_BETWEEN_TIMELINE )
	{
		LinePoints.Reset( 2 );
		LinePoints.Add( FVector2D( TimelinePosXPx - ThreadViewOffsetPx, 0.0f ) );
		LinePoints.Add( FVector2D( TimelinePosXPx - ThreadViewOffsetPx, PaintState->Size2D().Y ) );

		// Draw time line.
		FSlateDrawElement::MakeLines
		(
			PaintState->OutDrawElements,
			PaintState->LayerId,
			PaintState->AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			PaintState->DrawEffects,
			PaintState->WidgetStyle.GetColorAndOpacityTint() * FColorList::LimeGreen,
			false
		);
	}
}


void SProfilerThreadView::DrawUIStackNodes() const
{
//	SCOPE_LOG_TIME_FUNC();
	check( PaintState );
	const double ThreadViewOffsetPx = PositionXMS*NumPixelsPerMillisecond;
	PaintState->LayerId++;

	static const FSlateBrush* BorderBrush = FEditorStyle::GetBrush( "Profiler.ThreadView.SampleBorder" );
	const FColor GameThreadColor = FColorList::Red;
	const FColor RenderThreadColor = FColorList::Blue;
	const FColor ThreadColors[2] = {GameThreadColor, RenderThreadColor};

	// Draw nodes.
	for( const auto& RowOfNodes : ProfilerUIStream.LinearRowsOfNodes )
	{
		int32 NodeIndex = 0;
		for( const auto& UIStackNode : RowOfNodes )
		{
			NodeIndex++;
			// Check if the node is visible.
			//if( UIStackNode->IsVisible() )
			{
				const FVector2D PositionPx = UIStackNode->GetLocalPosition( ThreadViewOffsetPx, PositionY ) * FVector2D( 1.0f, NUM_PIXELS_PER_ROW );
				const FVector2D SizePx = FVector2D( FMath::Max( UIStackNode->WidthPx - 1.0, 0.0 ), NUM_PIXELS_PER_ROW );
				const FSlateRect ClippedNodeRect = PaintState->LocalClippingRect.IntersectionWith( FSlateRect( PositionPx, PositionPx + SizePx ) );

				// Check if this node is inside the visible area.
				if( ClippedNodeRect.IsEmpty() )
				{
					continue;
				}

				FColor NodeColor = UIStackNode->bIsCombined ? ThreadColors[UIStackNode->ThreadIndex].WithAlpha( 64 ) : ThreadColors[UIStackNode->ThreadIndex].WithAlpha( 192 );
				NodeColor.G += (NodeIndex % 2) ? 0 : 64;

				// Draw a cycle counter for this profiler UI stack node.
				FSlateDrawElement::MakeBox
				(
					PaintState->OutDrawElements,
					PaintState->LayerId,
					PaintState->AllottedGeometry.ToPaintGeometry( ClippedNodeRect.GetTopLeft(), ClippedNodeRect.GetSize() ),
					BorderBrush,
					PaintState->DrawEffects,
					NodeColor
				);
			}
		}
	}

	// #Profiler: 2014-04-29 Separate layer for makebox, makeshadowtext, maketext.
	PaintState->LayerId++;

	const float MarkerPosYOffsetPx = ((float)NUM_PIXELS_PER_ROW - PaintState->SummaryFont8Height)*0.5f;
	
	// Draw nodes' descriptions.
	for( const auto& RowOfNodes : ProfilerUIStream.LinearRowsOfNodes )
	{
		for( const auto& UIStackNode : RowOfNodes )
		{
			const FVector2D PositionPx = UIStackNode->GetLocalPosition( ThreadViewOffsetPx, PositionY ) * FVector2D( 1.0f, NUM_PIXELS_PER_ROW );
			const FVector2D SizePx = FVector2D( UIStackNode->WidthPx, NUM_PIXELS_PER_ROW );
			const FSlateRect ClippedNodeRect = PaintState->LocalClippingRect.IntersectionWith( FSlateRect( PositionPx, PositionPx + SizePx ) );

			// Check if this node is inside the visible area.
			if( ClippedNodeRect.IsEmpty() )
			{
				continue;
			}

			FString StringStatName = UIStackNode->StatName.GetPlainNameString();
			FString StringStatNameWithTime = StringStatName + FString::Printf( TEXT( " (%.4f MS)" ), UIStackNode->GetDurationMS() );
			if( UIStackNode->bIsCulled )
			{
				StringStatName += TEXT( " [C]" );
				StringStatNameWithTime += TEXT( " [C]" );
			}

			// Update position of the text to be always visible and try to center it.
			const float StatNameWidthPx = PaintState->FontMeasureService->Measure( StringStatName, PaintState->SummaryFont8 ).X;
			const float StatNameWithTimeWidthPx = PaintState->FontMeasureService->Measure( StringStatNameWithTime, PaintState->SummaryFont8 ).X;
			const float TextAreaWidthPx = ClippedNodeRect.GetSize().X;

			bool bUseShortVersion = true;
			FVector2D AdjustedPositionPx;
			// Center the stat name with timing if we can.
			if( TextAreaWidthPx > StatNameWithTimeWidthPx )
			{
				AdjustedPositionPx = FVector2D( ClippedNodeRect.Left + (TextAreaWidthPx - StatNameWithTimeWidthPx)*0.5f, PositionPx.Y + MarkerPosYOffsetPx );
				bUseShortVersion = false;
			}
			// Center the stat name.
			else if( TextAreaWidthPx > StatNameWidthPx )
			{
				AdjustedPositionPx = FVector2D( ClippedNodeRect.Left + (TextAreaWidthPx - StatNameWidthPx)*0.5f, PositionPx.Y + MarkerPosYOffsetPx );
			}
			// Move to the edge.
			else
			{
				AdjustedPositionPx = FVector2D( ClippedNodeRect.Left, PositionPx.Y + MarkerPosYOffsetPx );
			}

			const FVector2D AbsolutePositionPx = PaintState->AllottedGeometry.LocalToAbsolute( ClippedNodeRect.GetTopLeft() );
			const FSlateRect AbsoluteClippingRect = FSlateRect( AbsolutePositionPx, AbsolutePositionPx + ClippedNodeRect.GetSize() );
			
			DrawText( bUseShortVersion ? StringStatName : StringStatNameWithTime, PaintState->SummaryFont8, AdjustedPositionPx, FColorList::White, FColorList::Black, FVector2D( 1.0f, 1.0f ), &AbsoluteClippingRect );
		}
	}
}


void SProfilerThreadView::DrawFrameMarkers() const
{
	check( PaintState );
	const double ThreadViewOffsetPx = PositionXMS*NumPixelsPerMillisecond;
	PaintState->LayerId++;

	for( const auto& ThreadNode : ProfilerUIStream.ThreadNodes )
	{
		if( ThreadNode.StatName == NAME_GameThread )
		{
			const float MarkerPosXPx = ThreadNode.GetLocalPosition( ThreadViewOffsetPx, 0.0f ).X + ThreadNode.WidthPx;

			// Check if this frame time marker is inside the visible area.
			if( MarkerPosXPx < 0.0f || MarkerPosXPx > PaintState->Size2D().X )
			{
				continue;
			}

			// Draw text.
			const FString FrameIndexStr = FString::Printf( TEXT( "%i" ), ThreadNode.FrameIndex );
			const FString FrameTimesStr = FString::Printf( TEXT( "%.4f [%.4f] MS" ), ThreadNode.CycleCountersEndTimeMS, ThreadNode.GetDurationMS() );

			float MarkerPosYPx = PaintState->Size2D().Y - 2 * PaintState->SummaryFont8Height;
			DrawText( FrameIndexStr, PaintState->SummaryFont8, FVector2D( MarkerPosXPx, MarkerPosYPx ), FColorList::SkyBlue, FColorList::Black, FVector2D( 1.0f, 1.0f ) );

			MarkerPosYPx += PaintState->SummaryFont8Height;
			DrawText( FrameTimesStr, PaintState->SummaryFont8, FVector2D( MarkerPosXPx, MarkerPosYPx ), FColorList::SkyBlue, FColorList::Black, FVector2D( 1.0f, 1.0f ) );
		}
	}

	PaintState->LayerId++;

	const double PositionXStartPx = FMath::TruncToFloat( PositionXMS*NumPixelsPerMillisecond / (double)NUM_PIXELS_BETWEEN_TIMELINE )*(double)NUM_PIXELS_BETWEEN_TIMELINE;
	const double PositionXEndPx = PositionXStartPx + RangeXMS*NumPixelsPerMillisecond;

	for( double TimelinePosXPx = PositionXStartPx; TimelinePosXPx < PositionXEndPx; TimelinePosXPx += (double)NUM_PIXELS_BETWEEN_TIMELINE )
	{
		const FString TimelineStr = FString::Printf( TEXT( "%.4f MS" ), TimelinePosXPx / NumPixelsPerMillisecond );

		// Draw time line text.
		float MarkerPosYPx = PaintState->Size2D().Y - 3 * PaintState->SummaryFont8Height;
		DrawText( TimelineStr, PaintState->SummaryFont8, FVector2D( TimelinePosXPx - ThreadViewOffsetPx, MarkerPosYPx ), FColorList::LimeGreen, FColorList::Black, FVector2D( 1.0f, 1.0f ) );
	}

#if	0
	for( int32 FrameIndex = FramesIndices.X; FrameIndex < FramesIndices.Y; ++FrameIndex )
	{
		const double FrameTimeMS = ProfilerStream->GetFrameTimeMS( FrameIndex );
		const double ElapsedTimeMS = ProfilerStream->GetElapsedFrameTimeMS( FrameIndex );
		const double FrameEndMarkerLocalPosX = ElapsedTimeMS * NumPixelsPerMillisecond - ThreadViewOffsetPx;

		// Draw text.
		const FVector2D TextLocalPosTop = FVector2D( FrameEndMarkerLocalPosX, PaintState->SummaryFont8Height );
		const FString FrameTimeStr = FString::Printf( TEXT( "%.4f [%.4f] MS" ), ElapsedTimeMS, FrameTimeMS );
		DrawText( FrameTimeStr, PaintState->SummaryFont8, TextLocalPosTop, FColorList::SkyBlue, FColorList::Black, FVector2D( 1.0f, 1.0f ) );

		const FVector2D TextLocalPosBottom = FVector2D( FrameEndMarkerLocalPosX, PaintState->Size2D().Y - 2 * PaintState->SummaryFont8Height );
		DrawText( FrameTimeStr, PaintState->SummaryFont8, TextLocalPosBottom, FColorList::SkyBlue, FColorList::Black, FVector2D( 1.0f, 1.0f ) );
	}
#endif // 0
}


void SProfilerThreadView::DrawUIStackNodes_Recursively( const FProfilerUIStackNode& UIStackNode ) const
{
	// OBSOLETE
	check( PaintState );
	// Don't render thread nodes.
	if( UIStackNode.ThreadIndex != FProfilerUIStackNode::THREAD_NODE_INDEX )
	{
		static const FSlateColorBrush SolidWhiteBrush = FSlateColorBrush( FColorList::White );
		const FColor GameThreadColor = FColorList::Red;
	
		const FVector2D Position = FVector2D( UIStackNode.PositionXPx /*- PositionXMS*/, UIStackNode.PositionY*NUM_PIXELS_PER_ROW );
		const FVector2D Size = FVector2D( UIStackNode.WidthPx, NUM_PIXELS_PER_ROW );

		// Draw a cycle counter for this profiler UI stack node.
		FSlateDrawElement::MakeBox
		(
			PaintState->OutDrawElements,
			PaintState->LayerId,
			PaintState->AllottedGeometry.ToPaintGeometry( Position, Size ),
			&SolidWhiteBrush,
			PaintState->DrawEffects,
			GameThreadColor
		);

		const FString StringStatName = UIStackNode.StatName.GetPlainNameString();
		DrawText( UIStackNode.StatName.GetPlainNameString(), PaintState->SummaryFont8, Position, FColorList::White, FColorList::Black, FVector2D( 1.0f, 1.0f ) );
	}

	for( const auto& UIStackNodeChild : UIStackNode.Children )
	{
		DrawUIStackNodes_Recursively( UIStackNodeChild );
	}
}


void SProfilerThreadView::DrawText( const FString& Text, const FSlateFontInfo& FontInfo, FVector2D Position, const FColor& TextColor, const FColor& ShadowColor, FVector2D ShadowOffset, const FSlateRect* ClippingRect /*= nullptr*/ ) const
{
	check( PaintState );

	if( ShadowOffset.SizeSquared() > 0.0f )
	{
		FSlateDrawElement::MakeText
		(
			PaintState->OutDrawElements,
			PaintState->LayerId,
			PaintState->AllottedGeometry.ToOffsetPaintGeometry( Position + ShadowOffset ),
			Text,
			FontInfo,
			PaintState->DrawEffects,
			ShadowColor
		);
	}

	FSlateDrawElement::MakeText
	(
		PaintState->OutDrawElements,
		++PaintState->LayerId,
		PaintState->AllottedGeometry.ToOffsetPaintGeometry( Position ),
		Text,
		FontInfo,
		PaintState->DrawEffects,
		TextColor
	);
}


FReply SProfilerThreadView::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();

	if( IsReady() )
	{
		MousePositionOnButtonDown = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			bIsLeftMousePressed = true;
			DistanceDragged = PositionXMS;

			// Capture mouse, so we can move outside this widget.
			Reply = FReply::Handled().CaptureMouse( SharedThis( this ) );
		}
		else if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
		{
			bIsRightMousePressed = true;
		}
	}

	return Reply;
}

FReply SProfilerThreadView::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();

	if( IsReady() )
	{
		const FVector2D MousePositionOnButtonUp = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		const bool bIsValidForMouseClick = MousePositionOnButtonUp.Equals( MousePositionOnButtonDown, MOUSE_SNAP_DISTANCE );

		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			if( bIsLeftMousePressed )
			{
				// Release the mouse, we are no longer dragging.
				Reply = FReply::Handled().ReleaseMouseCapture();
			}

			bIsLeftMousePressed = false;
		}
		else if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
		{
			if( bIsRightMousePressed )
			{
				if( bIsValidForMouseClick )
				{
					ShowContextMenu( MouseEvent.GetScreenSpacePosition() );
					Reply = FReply::Handled();
				}
			}
			bIsRightMousePressed = false;
		}
	}

	return Reply;
}

FReply SProfilerThreadView::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	//	SCOPE_LOG_TIME_FUNC();

	FReply Reply = FReply::Unhandled();

	if( IsReady() )
	{
		const FVector2D LocalMousePosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		
		HoveredPositionX = 0.0;//PositionToFrameIndex( LocalMousePosition.X );
		HoveredPositionY = 0.0;

		const float CursorPosXDelta = -MouseEvent.GetCursorDelta().X;
		const float ScrollSpeed = 1.0f / ZoomFactorX;

		if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
		{
			if( HasMouseCapture() && !MouseEvent.GetCursorDelta().IsZero() )
			{
				DistanceDragged += CursorPosXDelta*ScrollSpeed*0.1;

				// Inform other widgets that we have scrolled the thread-view.
				SetPositionX( FMath::Clamp( DistanceDragged, 0.0, TotalRangeXMS - RangeXMS ) );
				CursorType = EThreadViewCursor::Hand;
				Reply = FReply::Handled();
			}
		}
		else
		{
			CursorType = EThreadViewCursor::Default;
		}
	}

	return Reply;
}

void SProfilerThreadView::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{

}

void SProfilerThreadView::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if( !HasMouseCapture() )
	{
		bIsLeftMousePressed = false;
		bIsRightMousePressed = false;

		CursorType = EThreadViewCursor::Default;
	}
}

FReply SProfilerThreadView::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	
	const bool bZoomIn = MouseEvent.GetWheelDelta() < 0.0f;
	const double Center = PositionXMS + RangeXMS*0.5f;

	const double MinVisibleRangeMS = 1.0f / INV_MIN_VISIBLE_RANGE_X;
	const double NewUnclampedRange = bZoomIn ? RangeXMS*1.25f : RangeXMS / 1.25f;
	const double NewRange = FMath::Clamp( NewUnclampedRange, MinVisibleRangeMS, FMath::Min( TotalRangeXMS, (double)MAX_VISIBLE_RANGE_X ) );

	const double NewPositionX = FMath::Clamp( Center, NewRange*0.5f, TotalRangeXMS - NewRange*0.5f ) - NewRange*0.5f;
	SetTimeRange( NewPositionX, NewPositionX + NewRange );

	return Reply;
}

FReply SProfilerThreadView::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	return Reply;
}


FCursorReply SProfilerThreadView::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	FCursorReply CursorReply = FCursorReply::Unhandled();
	if( CursorType == EThreadViewCursor::Arrow )
	{
		CursorReply = FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
	}
	else if( CursorType == EThreadViewCursor::Hand )
	{
		CursorReply = FCursorReply::Cursor( EMouseCursor::GrabHand );
	}
	return CursorReply;
}

void SProfilerThreadView::ShowContextMenu( const FVector2D& ScreenSpacePosition )
{

}

void SProfilerThreadView::BindCommands()
{

}


/* SProfilerThreadView interface
 *****************************************************************************/

void SProfilerThreadView::SetPositionXToByScrollBar( double ScrollOffset ) 
{
	SetPositionX( ScrollOffset*TotalRangeXMS );
}


void SProfilerThreadView::SetPositionX( double NewPositionXMS )
{
	const double ClampedPositionXMS = FMath::Clamp( NewPositionXMS, 0.0, TotalRangeXMS - RangeXMS );
	SetTimeRange( ClampedPositionXMS, ClampedPositionXMS + RangeXMS, true );
}


void SProfilerThreadView::SetPositonYTo( double ScrollOffset )
{

}


void SProfilerThreadView::SetTimeRange( double StartTimeMS, double EndTimeMS, bool bBroadcast )
{
	check( EndTimeMS > StartTimeMS );

	PositionXMS = StartTimeMS;
	RangeXMS = EndTimeMS - StartTimeMS;
	FramesIndices = ProfilerStream->GetFramesIndicesForTimeRange( StartTimeMS, EndTimeMS );

	bUpdateData = true;
		
	//UE_LOG( LogTemp, Log, TEXT( "StartTimeMS=%f, EndTimeMS=%f, bBroadcast=%1i FramesIndices=%3i,%3i" ), StartTimeMS, EndTimeMS, (int)bBroadcast, FramesIndices.X, FramesIndices.Y );
	if( bBroadcast )
	{
		ViewPositionXChangedEvent.Broadcast( StartTimeMS, EndTimeMS, TotalRangeXMS, FramesIndices.X, FramesIndices.Y );
	}
}


void SProfilerThreadView::SetFrameRange(int32 FrameStart, int32 FrameEnd)
{
	const double EndTimeMS = ProfilerStream->GetElapsedFrameTimeMS(FrameEnd);
	const double StartTimeMS = ProfilerStream->GetElapsedFrameTimeMS(FrameStart) - ProfilerStream->GetFrameTimeMS(FrameStart);
	SetTimeRange(StartTimeMS, EndTimeMS, true);
}


void SProfilerThreadView::AttachProfilerStream(const FProfilerStream& InProfilerStream)
{
	ProfilerStream = &InProfilerStream;

	TotalRangeXMS = ProfilerStream->GetElapsedTime();
	TotalRangeY = ProfilerStream->GetNumThreads()*FProfilerUIStream::DEFAULT_VISIBLE_THREAD_DEPTH;

	// Display the first frame.
	const FProfilerFrame* ProfilerFrame = ProfilerStream->GetProfilerFrame(0);
	SetTimeRange(ProfilerFrame->Root->CycleCounterStartTimeMS, ProfilerFrame->Root->CycleCounterEndTimeMS);
}


/* SProfilerThreadView implementation
 *****************************************************************************/

void SProfilerThreadView::ProcessData()
{
	//	SCOPE_LOG_TIME_FUNC();

	ProfilerUIStream.GenerateUIStream( *ProfilerStream, PositionXMS, PositionXMS + RangeXMS, ZoomFactorX, NumMillisecondsPerWindow, NumPixelsPerMillisecond, NumMillisecondsPerSample );
}


bool SProfilerThreadView::IsReady() const
{
	return ProfilerStream && ProfilerStream->GetNumFrames() > 0;
}


bool SProfilerThreadView::ShouldUpdateData()
{
	return bUpdateData;
}


void SProfilerThreadView::UpdateInternalConstants()
{
	ZoomFactorX = (double)NUM_MILLISECONDS_PER_WINDOW / RangeXMS;
	RangeY = FMath::RoundToFloat(ThisGeometry.GetLocalSize().Y / (double)NUM_PIXELS_PER_ROW);

	const double Aspect = ThisGeometry.GetLocalSize().X / NUM_MILLISECONDS_PER_WINDOW * ZoomFactorX;
	NumMillisecondsPerWindow = (double)ThisGeometry.GetLocalSize().X / Aspect;
	NumPixelsPerMillisecond = (double)ThisGeometry.GetLocalSize().X / NumMillisecondsPerWindow;
	NumMillisecondsPerSample = NumMillisecondsPerWindow / (double)ThisGeometry.GetLocalSize().X * (double)MIN_NUM_PIXELS_PER_SAMPLE;
}
