// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/SlateTextLayout.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/ISlateRunRenderer.h"
#include "Framework/Text/ISlateLineHighlighter.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/SlatePasswordRun.h"

TSharedRef< FSlateTextLayout > FSlateTextLayout::Create(FTextBlockStyle InDefaultTextStyle)
{
	TSharedRef< FSlateTextLayout > Layout = MakeShareable( new FSlateTextLayout(MoveTemp(InDefaultTextStyle)) );
	Layout->AggregateChildren();

	return Layout;
}

FSlateTextLayout::FSlateTextLayout(FTextBlockStyle InDefaultTextStyle)
	: DefaultTextStyle(MoveTemp(InDefaultTextStyle))
	, Children()
	, bIsPassword(false)
	, LocalizedFallbackFontRevision(0)
{

}

FChildren* FSlateTextLayout::GetChildren()
{
	return &Children;
}

void FSlateTextLayout::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	for (int32 LineIndex = 0; LineIndex < LineViews.Num(); LineIndex++)
	{
		const FTextLayout::FLineView& LineView = LineViews[ LineIndex ];

		for (int32 BlockIndex = 0; BlockIndex < LineView.Blocks.Num(); BlockIndex++)
		{
			const TSharedRef< ILayoutBlock > Block = LineView.Blocks[ BlockIndex ];
			const TSharedRef< ISlateRun > Run = StaticCastSharedRef< ISlateRun >( Block->GetRun() );
			Run->ArrangeChildren( Block, AllottedGeometry, ArrangedChildren );
		}
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

int32 GShowTextDebugging = 0;
static FAutoConsoleVariableRef CVarSlateShowTextDebugging(TEXT("Slate.ShowTextDebugging"), GShowTextDebugging, TEXT("Show debugging painting for text rendering."), ECVF_Default);

#endif

int32 FSlateTextLayout::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FLinearColor BlockDebugHue( 0, 1.0f, 1.0f, 0.5 );
#endif

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	int32 HighestLayerId = LayerId;

	for (const FTextLayout::FLineView& LineView : LineViews)
	{
		// Is this line visible?  This checks if the culling rect, which represents the AABB around the last clipping rect, intersects the 
		// line of text, this requires that we get the text line into render space.
		// TODO perhaps save off this line view rect during text layout?
		const FVector2D LocalLineOffset = LineView.Offset * InverseScale;
		const FSlateRect LineViewRect(AllottedGeometry.GetRenderBoundingRect(FSlateRect(LocalLineOffset, LocalLineOffset + (LineView.Size * InverseScale))));
		if ( !FSlateRect::DoRectanglesIntersect(LineViewRect, MyCullingRect))
		{
			continue;
		}

		// Render any underlays for this line
		const int32 HighestUnderlayLayerId = OnPaintHighlights( Args, LineView, LineView.UnderlayHighlights, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

		const int32 BlockDebugLayer = HighestUnderlayLayerId;
		const int32 TextLayer = BlockDebugLayer + 1;
		int32 HighestBlockLayerId = TextLayer;

		// Render every block for this line
		for (const TSharedRef< ILayoutBlock >& Block : LineView.Blocks)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ( GShowTextDebugging )
			{
				BlockDebugHue.R += 50.0f;

				FSlateDrawElement::MakeBox(
					OutDrawElements, 
					BlockDebugLayer,
					AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, Block->GetLocationOffset()))),
					&DefaultTextStyle.HighlightShape,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint() * BlockDebugHue.HSVToLinearRGB()
					);
			}
#endif

			const TSharedRef< ISlateRun > Run = StaticCastSharedRef< ISlateRun >( Block->GetRun() );

			int32 HighestRunLayerId = TextLayer;
			const TSharedPtr< ISlateRunRenderer > RunRenderer = StaticCastSharedPtr< ISlateRunRenderer >( Block->GetRenderer() );
			if ( RunRenderer.IsValid() )
			{
				HighestRunLayerId = RunRenderer->OnPaint( Args, LineView, Run, Block, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, TextLayer, InWidgetStyle, bParentEnabled );
			}
			else
			{
				HighestRunLayerId = Run->OnPaint( Args, LineView, Block, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, TextLayer, InWidgetStyle, bParentEnabled );
			}

			HighestBlockLayerId = FMath::Max( HighestBlockLayerId, HighestRunLayerId );
		}

		// Render any overlays for this line
		//#jira UE - 49124 Cursor in virtual keyboard and UMG don't match
		if (FSlateApplication::Get().AllowMoveCursor())
		{
			const int32 HighestOverlayLayerId = OnPaintHighlights( Args, LineView, LineView.OverlayHighlights, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, HighestBlockLayerId, InWidgetStyle, bParentEnabled );
			HighestLayerId = FMath::Max( HighestLayerId, HighestOverlayLayerId );
		}
	}

	return HighestLayerId;
}

int32 FSlateTextLayout::OnPaintHighlights( const FPaintArgs& Args, const FTextLayout::FLineView& LineView, const TArray<FLineViewHighlight>& Highlights, const FTextBlockStyle& InDefaultTextStyle, const FGeometry& AllottedGeometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 CurrentLayerId = LayerId;

	for (const FLineViewHighlight& Highlight : Highlights)
	{
		const TSharedPtr< ISlateLineHighlighter > LineHighlighter = StaticCastSharedPtr< ISlateLineHighlighter >( Highlight.Highlighter );
		if (LineHighlighter.IsValid())
		{
			CurrentLayerId = LineHighlighter->OnPaint( Args, LineView, Highlight.OffsetX, Highlight.Width, InDefaultTextStyle, AllottedGeometry, CullingRect, OutDrawElements, CurrentLayerId, InWidgetStyle, bParentEnabled );
		}
	}

	return CurrentLayerId;
}

void FSlateTextLayout::EndLayout()
{
	FTextLayout::EndLayout();
	AggregateChildren();
}

void FSlateTextLayout::UpdateIfNeeded()
{
	const uint16 CurrentLocalizedFallbackFontRevision = FSlateApplication::Get().GetRenderer()->GetFontCache()->GetLocalizedFallbackFontRevision();
	if (CurrentLocalizedFallbackFontRevision != LocalizedFallbackFontRevision)
	{
		if (LocalizedFallbackFontRevision != 0)
		{
			// If the localized fallback font has changed, we need to purge the current layout data as things may need to be re-measured
			DirtyFlags |= ETextLayoutDirtyState::Layout;
			DirtyAllLineModels(ELineModelDirtyState::WrappingInformation | ELineModelDirtyState::ShapingCache);
		}

		LocalizedFallbackFontRevision = CurrentLocalizedFallbackFontRevision;
	}

	FTextLayout::UpdateIfNeeded();
}

void FSlateTextLayout::SetDefaultTextStyle(FTextBlockStyle InDefaultTextStyle)
{
	DefaultTextStyle = MoveTemp(InDefaultTextStyle);
}

const FTextBlockStyle& FSlateTextLayout::GetDefaultTextStyle() const
{
	return DefaultTextStyle;
}

void FSlateTextLayout::SetIsPassword(const TAttribute<bool>& InIsPassword)
{
	bIsPassword = InIsPassword;
}

void FSlateTextLayout::AggregateChildren()
{
	Children.Empty();
	const TArray< FLineModel >& LayoutLineModels = GetLineModels();
	for (int32 LineModelIndex = 0; LineModelIndex < LayoutLineModels.Num(); LineModelIndex++)
	{
		const FLineModel& LineModel = LayoutLineModels[ LineModelIndex ];
		for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
		{
			const FRunModel& LineRun = LineModel.Runs[ RunIndex ];
			const TSharedRef< ISlateRun > SlateRun = StaticCastSharedRef< ISlateRun >( LineRun.GetRun() );

			const TArray< TSharedRef<SWidget> >& RunChildren = SlateRun->GetChildren();
			for (int32 ChildIndex = 0; ChildIndex < RunChildren.Num(); ChildIndex++)
			{
				const TSharedRef< SWidget >& Child = RunChildren[ ChildIndex ];
				Children.Add( Child );
			}
		}
	}
}

TSharedRef<IRun> FSlateTextLayout::CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const
{
	if (bIsPassword.Get(false))
	{
		return FSlatePasswordRun::Create(FRunInfo(), NewText, DefaultTextStyle, NewRange);
	}
	return FSlateTextRun::Create(FRunInfo(), NewText, DefaultTextStyle, NewRange);
}
