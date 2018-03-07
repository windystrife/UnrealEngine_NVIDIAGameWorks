// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SProfilerMiniView.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateColorBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "ProfilerSession.h"


SProfilerMiniView::SProfilerMiniView()
	: bIsActiveTimerRegistered( false )
{
	Reset();
}

SProfilerMiniView::~SProfilerMiniView()
{

}

void SProfilerMiniView::Construct( const FArguments& InArgs )
{
	BindCommands();
}

void SProfilerMiniView::Reset()
{
	MaxFrameTime = 0.0f;
	AllFrames.Empty();
	RecentlyAddedFrames.Empty();
	StatMetadata = nullptr;

	FMemory::Memzero( PaintStateMemory );
	PaintState = nullptr;

	MousePositionOnButtonDown = FVector2D::ZeroVector;

	SelectionBoxFrameStart = 0;
	SelectionBoxFrameEnd = 0;

	HoveredFrameIndex = 0;
	DistanceDragged = 0.0f;
	NumPixelsPerSample = 0;
	NumPixelsPerFrame = 0.0f;

	bIsLeftMousePressed = false;
	bIsRightMousePressed = false;
	bCanBeStartDragged = false;
	bCanBeEndDragged = false;
	bAllowSelectionBoxZooming = false;

	CursorType = EMiniviewCursor::Default;
}

EActiveTimerReturnType SProfilerMiniView::EnsureDataUpdateDuringPreview(double InCurrentTime, float InDeltaTime)
{
	if (RecentlyAddedFrames.Num() > 0)
	{
		bUpdateData = true;
		return EActiveTimerReturnType::Continue;
	}
	
	bIsActiveTimerRegistered = false;
	return EActiveTimerReturnType::Stop;
}

void SProfilerMiniView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if( ThisGeometry != AllottedGeometry )
	{
		// Refresh.
		MaxFrameTime = 0.0f;
		bUpdateData = true;
	}

	ThisGeometry = AllottedGeometry;

	if( IsReady() )
	{
		NumPixelsPerFrame = (float)AllottedGeometry.Size.X / (float)AllFrames.Num();
	}

	if( ShouldUpdateData() )
	{
		ProcessData();
		bUpdateData = false;
	}
}

int32 SProfilerMiniView::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
//	SCOPE_LOG_TIME_FUNC();

	// Rendering info.
	const bool bEnabled = ShouldBeEnabled( bParentEnabled );
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateBrush* MiniViewArea = FEditorStyle::GetBrush( "Profiler.LineGraphArea" );
	const FSlateBrush* WhiteBrush = FEditorStyle::GetBrush( "WhiteTexture" );

	PaintState = new((void*)PaintStateMemory) FSlateOnPaintState( AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, DrawEffects );

	const float MiniViewSizeX = AllottedGeometry.Size.X;
	const float MiniViewSizeY = AllottedGeometry.Size.Y;

	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	FSlateFontInfo SummaryFont( FPaths::EngineContentDir() / TEXT( "Slate/Fonts/Roboto-Regular.ttf" ), 8 );
	const float MaxFontCharHeight = FontMeasureService->Measure( TEXT( "!" ), SummaryFont ).Y;

	// Draw background.
	FSlateDrawElement::MakeBox
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry( FVector2D( 0, 0 ), FVector2D( MiniViewSizeX, AllottedGeometry.Size.Y ) ),
		MiniViewArea,
		DrawEffects,
		MiniViewArea->GetTint( InWidgetStyle ) * InWidgetStyle.GetColorAndOpacityTint()
	);
	LayerId++;

	// Draw all samples.
	if( IsReady() )
	{
		static const FSlateColorBrush SolidWhiteBrush = FSlateColorBrush( FColorList::White );
		// #Profiler 2014-04-24 move to the global scope.
		const FColor GameThreadColor = FColorList::Red;
		const FColor RenderThreadColor = FColorList::Blue;
		const FColor OtherThreadsColor = FColorList::Grey;

		const float SampleScaleY = MiniViewSizeY / MaxFrameTime;

		float CurrentSamplePosX = 0;
		float NextSamplePosX = GetNumPixelsPerSample();
		for( const FMiniViewSample& MiniViewSample : MiniViewSamples )
		{
			const float AllSizeY = FMath::TruncToFloat( MiniViewSample.TotalThreadTime*SampleScaleY );
			const float GTSizeY = FMath::TruncToFloat( MiniViewSample.GameThreadTime*SampleScaleY );
			const float RTSizeY = FMath::TruncToFloat( MiniViewSample.RenderThreadTime*SampleScaleY );
			const float OtherSizeY = AllSizeY - GTSizeY - RTSizeY;

			const float DestSamplePosX0 = FMath::TruncToFloat( CurrentSamplePosX );
			const float DestSamplePosX1 = FMath::TruncToFloat( NextSamplePosX );
			const float DestSampleSizeX = DestSamplePosX1 - DestSamplePosX0;

			// The game thread on the bottom.
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( DestSamplePosX0, MiniViewSizeY - GTSizeY ), FVector2D( DestSampleSizeX, GTSizeY ) ),
				&SolidWhiteBrush,
				DrawEffects,
				GameThreadColor
			);

			// Next the render thread.
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( DestSamplePosX0, MiniViewSizeY - GTSizeY - RTSizeY ), FVector2D( DestSampleSizeX, RTSizeY ) ),
				&SolidWhiteBrush,
				DrawEffects,
				RenderThreadColor
			);

			// Disabled for now.
#if	0
			// Finally the others threads as one sample.
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( DestSamplePosX, MiniViewSizeY - AllSizeY ), FVector2D( DestSampleSizeX, OtherSizeY ) ),
				&SolidWhiteBrush,
				DrawEffects,
				OtherThreadsColor
			);
#endif // 0

			CurrentSamplePosX = NextSamplePosX;
			NextSamplePosX += GetNumPixelsPerSample();
		}

		// Draw the selection box
		LayerId++;
		
		const int32 MaxFrameIndex = AllFrames.Num() - 1;
		
		const int32 SelectionBoxX0 = FMath::TruncToInt( FrameIndexToPosition( SelectionBoxFrameStart ) );
		const int32 SelectionBoxX1 = FMath::TruncToInt(FrameIndexToPosition(SelectionBoxFrameEnd + 1));

		if( SelectionBoxFrameStart > 0 )
		{
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( 0.0f, 0.0f ), FVector2D( (float)SelectionBoxX0, AllottedGeometry.Size.Y ) ),
				&SolidWhiteBrush,
				DrawEffects,
				FColorList::Grey.WithAlpha( 192 )
			);
		}
	
		if( SelectionBoxFrameEnd < MaxFrameIndex )
		{
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( SelectionBoxX1, 0.0f ), FVector2D( MiniViewSizeX - SelectionBoxX1, AllottedGeometry.Size.Y ) ),
				&SolidWhiteBrush,
				DrawEffects,
				FColorList::Grey.WithAlpha( 192 )
			);
		}

		// Draw the filler, to hide the difference between window's width and mini-view samples' width.
		const float FillerSizeX = MiniViewSizeX - CurrentSamplePosX;
		if( FillerSizeX > 0.0f )
		{
			const float FillerPosX0 = MiniViewSizeX - FillerSizeX;
			FSlateDrawElement::MakeBox
			(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry( FVector2D( FillerPosX0, 0.0f ), FVector2D( FillerSizeX + 1.0f, AllottedGeometry.Size.Y ) ),
				&SolidWhiteBrush,
				DrawEffects,
				// #Profiler: 2014-04-09 How to get this color from Slate?
				FColor(96,96,96)
			);
		}

		// Border of the selection box.
		LayerId++;
		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry( FVector2D( SelectionBoxX0, 0.0f ), FVector2D( SelectionBoxX1 - SelectionBoxX0, AllottedGeometry.Size.Y ) ),
			FEditorStyle::GetBrush( "PlainBorder" ),
			DrawEffects,
			FColorList::Green
		);

		// Draw the current mouse position with the highlighted sample.
		LayerId++;

		// Draw the basic information about threads and data range.
		const float MarkerPosX = 4.0f;
		const float MarkerPosY = 4.0f;

		// Upper left
		DrawText( TEXT( "Rendering thread" ), SummaryFont, FVector2D( MarkerPosX, MarkerPosY ), RenderThreadColor, FColor::Black, FVector2D( 1.0f, 1.0f ) );
		// Down left
		DrawText( TEXT( "Game thread" ), SummaryFont, FVector2D( MarkerPosX, MiniViewSizeY - MarkerPosY - MaxFontCharHeight ), GameThreadColor, FColor::Black, FVector2D( 1.0f, 1.0f ) );

		// Upper right
		const FString ThreadTimeMax = FString::Printf( TEXT( "%5.2f MS" ), MaxFrameTime );
		const float ThreadTimeMaxSizeX = FontMeasureService->Measure( ThreadTimeMax, SummaryFont ).X;
		DrawText( ThreadTimeMax, SummaryFont, FVector2D( MiniViewSizeX - ThreadTimeMaxSizeX - MarkerPosX, MarkerPosY ), FColor::White, FColor::Black, FVector2D( 1.0f, 1.0f ) );

		// Down right
		const FString ThreadTimeMin = TEXT( "0.0 MS" );
		const float ThreadTimeMinSizeX = FontMeasureService->Measure( ThreadTimeMin, SummaryFont ).X;
		DrawText( ThreadTimeMin, SummaryFont, FVector2D( MiniViewSizeX - ThreadTimeMinSizeX - MarkerPosX, MiniViewSizeY - MarkerPosY - MaxFontCharHeight ), FColor::White, FColor::Black, FVector2D( 1.0f, 1.0f ) );
	
	}

#if 0/*DEBUG_PROFILER_PERFORMANCE*/
	
	LayerId++;
	// Draw debug information.
	float GraphDescPosY = 0;

	// Debug text.
	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D( 16.0f, GraphDescPosY ) ),
		FString::Printf( TEXT( "SelectionBox FrameStart=%4i, FrameEnd=%4i (%3i) Hovered=%4i, NumPixelsPerSample=%2.1f" ), SelectionBoxFrameStart, SelectionBoxFrameEnd, SelectionBoxFrameEnd - SelectionBoxFrameStart, HoveredFrameIndex, GetNumPixelsPerSample() ),
		SummaryFont,
		MyCullingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY += MaxFontCharHeight + 1.0f;

	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D( 16.0f, GraphDescPosY ) ),
		FString::Printf( TEXT( "AllFrames=%4i, MiniViewSamples=%3i, MaxFrameTime=%2.0f DistanceDragged=%2.0f" ), AllFrames.Num(), MiniViewSamples.Num(), MaxFrameTime, DistanceDragged ),
		SummaryFont,
		MyCullingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY += MaxFontCharHeight + 1.0f;

	FSlateDrawElement::MakeText
	(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToOffsetPaintGeometry( FVector2D( 16.0f, GraphDescPosY ) ),
		FString::Printf( TEXT( "bCanBeStartD=%1i, bCanBeEndD=%1i" ), (int32)bCanBeStartDragged, (int32)bCanBeEndDragged ),
		SummaryFont,
		MyCullingRect,
		DrawEffects,
		FLinearColor::White
	);
	GraphDescPosY += MaxFontCharHeight + 1.0f;

#endif // DEBUG_PROFILER_PERFORMANCE

	return SCompoundWidget::OnPaint( Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled && IsEnabled() );
}


void SProfilerMiniView::DrawText( const FString& Text, const FSlateFontInfo& FontInfo, FVector2D Position, const FColor& TextColor, const FColor& ShadowColor, FVector2D ShadowOffset ) const
{
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


FReply SProfilerMiniView::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();

	if( IsReady() )
	{
		MousePositionOnButtonDown = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );

		if( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
		{
			bIsLeftMousePressed = true;

			if( bCanBeStartDragged )
			{
				DistanceDragged = FrameIndexToPosition( SelectionBoxFrameStart );
			}
			else if( bCanBeEndDragged )
			{
				DistanceDragged = FrameIndexToPosition( SelectionBoxFrameEnd );
			}
			else
			{
				// Clicked outside the selection box, so move the selection box to that position.
				DistanceDragged = MousePositionOnButtonDown.X;
			}

			if( bCanBeStartDragged || bCanBeEndDragged )
			{
				// Capture mouse, so we can move outside this widget.
				Reply = FReply::Handled().CaptureMouse( SharedThis( this ) );
			}
		}
		else if( MouseEvent.GetEffectingButton() == EKeys::RightMouseButton )
		{
			bIsRightMousePressed = true;
		}
	}

	return Reply;
}

FReply SProfilerMiniView::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
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
				if( !bCanBeStartDragged && !bCanBeEndDragged )
				{
					MoveSelectionBox( PositionToFrameIndex( DistanceDragged ) );
				}
				else if( bCanBeStartDragged || bCanBeEndDragged )
				{
					// Release the mouse, we are no longer dragging.
					Reply = FReply::Handled().ReleaseMouseCapture();
				}		
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

FReply SProfilerMiniView::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
//	SCOPE_LOG_TIME_FUNC();

	FReply Reply = FReply::Unhandled();

	if( IsReady() )
	{
		const FVector2D MousePosition = MyGeometry.AbsoluteToLocal( MouseEvent.GetScreenSpacePosition() );
		HoveredFrameIndex = PositionToFrameIndex( MousePosition.X );

		const float CursorPosXDelta = MouseEvent.GetCursorDelta().X;

		if( MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ) )
		{
			if( HasMouseCapture() && !MouseEvent.GetCursorDelta().IsZero() )
			{
				DistanceDragged += CursorPosXDelta;

				const int32 MouseFrameIndex = PositionToFrameIndex( DistanceDragged );

				if( !bAllowSelectionBoxZooming )
				{
					const int32 SelectionBoxSize = SelectionBoxFrameEnd - SelectionBoxFrameStart;
					if( bCanBeStartDragged )
					{				
						SelectionBoxFrameStart = MouseFrameIndex;
						SelectionBoxFrameStart = FMath::Clamp( SelectionBoxFrameStart, 0, FMath::Max( AllFrames.Num() - SelectionBoxSize - 1, 0 ) );
						SelectionBoxFrameEnd = FMath::Min( SelectionBoxFrameStart + SelectionBoxSize, AllFrames.Num() - 1 );
					}
					else if( bCanBeEndDragged )
					{
						SelectionBoxFrameEnd = MouseFrameIndex;
						SelectionBoxFrameEnd = FMath::Clamp( SelectionBoxFrameEnd, SelectionBoxSize - 1, AllFrames.Num() - -1 );
						SelectionBoxFrameStart = FMath::Max( SelectionBoxFrameEnd - SelectionBoxSize, 0 );
					}
				}
				else
				{
					
				}

				// Inform other widgets that we have moved the selection box.
				SelectionBoxChangedEvent.Broadcast( SelectionBoxFrameStart, SelectionBoxFrameEnd );

				Reply = FReply::Handled();
			}
		}
		else
		{
			const float StartEdgeDistance = FrameIndexToPosition(SelectionBoxFrameStart) - MousePosition.X;
			const float EndEdgeDistance = MousePosition.X - FrameIndexToPosition(SelectionBoxFrameEnd);

			bCanBeStartDragged = StartEdgeDistance < MOUSE_SNAP_DISTANCE && StartEdgeDistance > 0.0f;
			bCanBeEndDragged = EndEdgeDistance < MOUSE_SNAP_DISTANCE && EndEdgeDistance > 0.0f;

			if( StartEdgeDistance <= 0.0f && EndEdgeDistance <= 0.0f )
			{
				// Drag the whole selection box.
				bCanBeStartDragged = true;
				bCanBeEndDragged = true;
				CursorType = EMiniviewCursor::Hand;
			}
			else if( bCanBeStartDragged != bCanBeEndDragged )
			{
				CursorType = EMiniviewCursor::Arrow;
			}
			else
			{
				CursorType = EMiniviewCursor::Default;
			}
		}
	}

	return Reply;
}

void SProfilerMiniView::OnMouseEnter( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{

}

void SProfilerMiniView::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	if( !HasMouseCapture() )
	{
		bIsLeftMousePressed = false;
		bIsRightMousePressed = false;

		bCanBeStartDragged = false;
		bCanBeEndDragged = false;

		CursorType = EMiniviewCursor::Default;
	}
}

FReply SProfilerMiniView::OnMouseWheel( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	return Reply;
}

FReply SProfilerMiniView::OnMouseButtonDoubleClick( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	FReply Reply = FReply::Unhandled();
	return Reply;
}


FCursorReply SProfilerMiniView::OnCursorQuery( const FGeometry& MyGeometry, const FPointerEvent& CursorEvent ) const
{
	FCursorReply CursorReply = FCursorReply::Unhandled();
	if( CursorType == EMiniviewCursor::Arrow )
	{
		CursorReply = FCursorReply::Cursor( EMouseCursor::ResizeLeftRight );
	}
	else if( CursorType == EMiniviewCursor::Hand )
	{
		CursorReply = FCursorReply::Cursor( EMouseCursor::GrabHand );
	}
	return CursorReply;
}

void SProfilerMiniView::ShowContextMenu( const FVector2D& ScreenSpacePosition )
{

}

void SProfilerMiniView::BindCommands()
{

}



void SProfilerMiniView::AddThreadTime( int32 InFrameIndex, const TMap<uint32, float>& InThreadMS, const TSharedRef<FProfilerStatMetaData>& InStatMetaData )
{
	FFrameThreadTimes FrameThreadTimes;
	FrameThreadTimes.FrameNumber = InFrameIndex;
	FrameThreadTimes.ThreadTimes = InThreadMS;
	RecentlyAddedFrames.Add( FrameThreadTimes );

	if (!bIsActiveTimerRegistered)
	{
		bIsActiveTimerRegistered = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SProfilerMiniView::EnsureDataUpdateDuringPreview));
	}

	StatMetadata = InStatMetaData;
}

void SProfilerMiniView::MoveWithoutZoomSelectionBox( int32 FrameStart, int32 FrameEnd )
{
	const int32 MaxFrameIndex = FMath::Max( AllFrames.Num() - 1, 0 );
	SelectionBoxFrameStart = FMath::Clamp( FrameStart, 0, MaxFrameIndex );
	SelectionBoxFrameEnd = FMath::Clamp( FrameEnd, 0, MaxFrameIndex );
	bAllowSelectionBoxZooming = false;
}


void SProfilerMiniView::MoveAndZoomSelectionBox( int32 FrameStart, int32 FrameEnd )
{
	const int32 MaxFrameIndex = FMath::Max( AllFrames.Num() - 1, 0 );
	SelectionBoxFrameStart = FMath::Clamp( FrameStart, 0, MaxFrameIndex );
	SelectionBoxFrameEnd = FMath::Clamp( FrameEnd, 0, MaxFrameIndex );
	//bAllowSelectionBoxZooming = true;
}

void SProfilerMiniView::MoveSelectionBox( int32 FrameIndex )
{
	const int32 SelectionBoxSize = SelectionBoxFrameEnd - SelectionBoxFrameStart;
	const int32 SelectionBoxHalfSize = SelectionBoxSize / 2;
	const int32 CenterFrameIndex = FMath::Clamp( FrameIndex - SelectionBoxHalfSize, 0, AllFrames.Num() - 1 - SelectionBoxSize );

	// Inform other widgets that we have moved the selection box.
	SelectionBoxFrameStart = CenterFrameIndex;
	SelectionBoxFrameEnd = CenterFrameIndex + SelectionBoxSize;
	SelectionBoxChangedEvent.Broadcast( SelectionBoxFrameStart, SelectionBoxFrameEnd );
}

const int32 SProfilerMiniView::PositionToFrameIndex( const float InPositionX ) const
{
	int32 FrameIndex = 0;

	if( IsReady() )
	{
		const int32 NumAllFrames = AllFrames.Num();
		const int32 NumMiniViewSamples = MiniViewSamples.Num();

		const float ScaleRatio = (float)NumAllFrames / (float)NumMiniViewSamples;
		const float MouseSampleIndex = InPositionX / GetNumPixelsPerSample();

		FrameIndex = FMath::TruncToInt(MouseSampleIndex * ScaleRatio);
		FrameIndex = FMath::Clamp( FrameIndex, 0, NumAllFrames - 1 );
	}

	return FrameIndex;
}


void SProfilerMiniView::ProcessData()
{
	static FTotalTimeAndCount TimeAndCount( 0.0f, 0 );
	//SCOPE_LOG_TIME_FUNC_WITH_GLOBAL( &TimeAndCount );

	AllFrames.Append( RecentlyAddedFrames );
	RecentlyAddedFrames.Reset();

	if( !IsReady() )
	{
		return;
	}

	UpdateNumPixelsPerSample();

	const int32 NumMiniViewSamples = FMath::TruncToInt(ThisGeometry.Size.X / GetNumPixelsPerSample());
	MiniViewSamples.Reset( NumMiniViewSamples );

	MiniViewSamples.AddZeroed( NumMiniViewSamples );
	const int32 NumAllFrames = AllFrames.Num();

	const uint32 GameThreadID = StatMetadata->GetGameThreadID();
	const TArray<uint32>& RenderThreadIDs = StatMetadata->GetRenderThreadID();

	// Prepare data to fit into the window.
	{
		// Aggregate.
		const float ScaleRatio = (float)NumMiniViewSamples / (float)NumAllFrames;

		for( int32 FrameIndex = 0; FrameIndex < NumAllFrames; ++FrameIndex )
		{
			const FFrameThreadTimes& FrameThreadTimes = AllFrames[FrameIndex];
			const int32 SampleIndex = FMath::TruncToInt( ScaleRatio*FrameIndex );

			FMiniViewSample& Dest = MiniViewSamples[SampleIndex];
			Dest.AddFrameAndFindMax( FrameThreadTimes );
		}

		for( FMiniViewSample& It : MiniViewSamples )
		{
			It.CalculateMaxThreadTime( GameThreadID, RenderThreadIDs );
		}
	}
#if	0
	else
	{
		// Replicate.
		const float ScaleRatio = (float)NumAllFrames / (float)WindowNumSamples;

		float CurrentFrameIndex = 0.0f;
		for( int32 SampleIndex = 0; SampleIndex < WindowNumSamples; ++SampleIndex )
		{
			const int FrameIndex = FMath::TruncToInt( CurrentFrameIndex );
			const FFrameThreadTimes& FrameThreadTimes = AllFrames[FrameIndex];

			FMiniViewThreadTimes& Dest = WindowFrames[SampleIndex];
			Dest.Aggregate( AllFrames[FrameIndex] );
			Dest.CalculateMaxThreadTime( GameThreadID, RenderThreadIDs );

			CurrentFrameIndex += ScaleRatio;
		}
	}
#endif // 0

	// Calculate max thread time, used to scaling the displayed samples.
	for( const FMiniViewSample& MiniViewSample : MiniViewSamples )
	{
		MaxFrameTime = FMath::Max( MaxFrameTime, MiniViewSample.TotalThreadTime );
	}

	MaxFrameTime = FMath::Clamp( MaxFrameTime, 0.0f, (float)MAX_VISIBLE_THREADTIME );
}
