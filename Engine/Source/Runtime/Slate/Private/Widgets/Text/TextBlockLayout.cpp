// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Text/TextBlockLayout.h"
#include "Fonts/FontCache.h"
#include "Framework/Text/SlateTextHighlightRunRenderer.h"
#include "Stats/SlateStats.h"

SLATE_DECLARE_CYCLE_COUNTER(GSlateTextBlockLayoutComputeDesiredSize, "FTextBlockLayout ComputeDesiredSize");

FTextBlockLayout::FTextBlockLayout(FTextBlockStyle InDefaultTextStyle, const TOptional<ETextShapingMethod> InTextShapingMethod, const TOptional<ETextFlowDirection> InTextFlowDirection, const FCreateSlateTextLayout& InCreateSlateTextLayout, TSharedRef<ITextLayoutMarshaller> InMarshaller, TSharedPtr<IBreakIterator> InLineBreakPolicy)
	: TextLayout((InCreateSlateTextLayout.IsBound()) ? InCreateSlateTextLayout.Execute(MoveTemp(InDefaultTextStyle)) : FSlateTextLayout::Create(MoveTemp(InDefaultTextStyle)))
	, Marshaller(MoveTemp(InMarshaller))
	, TextHighlighter(FSlateTextHighlightRunRenderer::Create())
	, CachedSize(ForceInitToZero)
{
	if (InTextShapingMethod.IsSet())
	{
		TextLayout->SetTextShapingMethod(InTextShapingMethod.GetValue());
	}

	if (InTextFlowDirection.IsSet())
	{
		TextLayout->SetTextFlowDirection(InTextFlowDirection.GetValue());
	}

	TextLayout->SetLineBreakIterator(MoveTemp(InLineBreakPolicy));
}

FVector2D FTextBlockLayout::ComputeDesiredSize(const FWidgetArgs& InWidgetArgs, const float InScale, const FTextBlockStyle& InTextStyle)
{
	SLATE_CYCLE_COUNTER_SCOPE_DETAILED(SLATE_STATS_DETAIL_LEVEL_HI, GSlateTextBlockLayoutComputeDesiredSize);
	TextLayout->SetScale(InScale);
	TextLayout->SetWrappingWidth(CalculateWrappingWidth(InWidgetArgs));
	TextLayout->SetWrappingPolicy(InWidgetArgs.WrappingPolicy.Get());
	TextLayout->SetMargin(InWidgetArgs.Margin.Get());
	TextLayout->SetJustification(InWidgetArgs.Justification.Get());
	TextLayout->SetLineHeightPercentage(InWidgetArgs.LineHeightPercentage.Get());

	// Has the style used for this text block changed?
	if(!IsStyleUpToDate(InTextStyle))
	{
		TextLayout->SetDefaultTextStyle(InTextStyle);
		Marshaller->MakeDirty(); // will regenerate the text using the new default style
	}

	{
		bool bRequiresTextUpdate = false;
		const FText& TextToSet = InWidgetArgs.Text.Get(FText::GetEmpty());
		if(!TextLastUpdate.IdenticalTo(TextToSet))
		{
			// The pointer used by the bound text has changed, however the text may still be the same - check that now
			if(!TextLastUpdate.IsDisplayStringEqualTo(TextToSet))
			{
				// The source text has changed, so update the internal text
				bRequiresTextUpdate = true;
			}

			// Update this even if the text is lexically identical, as it will update the pointer compared by IdenticalTo for the next Tick
			TextLastUpdate = FTextSnapshot(TextToSet);
		}

		if(bRequiresTextUpdate || Marshaller->IsDirty())
		{
			UpdateTextLayout(TextToSet);
		}
	}

	{
		const FText& HighlightTextToSet = InWidgetArgs.HighlightText.Get(FText::GetEmpty());
		if(!HighlightTextLastUpdate.IdenticalTo(HighlightTextToSet))
		{
			// The pointer used by the bound text has changed, however the text may still be the same - check that now
			if(!HighlightTextLastUpdate.IsDisplayStringEqualTo(HighlightTextToSet))
			{
				UpdateTextHighlights(HighlightTextToSet);
			}

			// Update this even if the text is lexically identical, as it will update the pointer compared by IdenticalTo for the next Tick
			HighlightTextLastUpdate = FTextSnapshot(HighlightTextToSet);
		}
	}

	// We need to update our size if the text layout has become dirty
	TextLayout->UpdateIfNeeded();

	return TextLayout->GetSize();
}

int32 FTextBlockLayout::OnPaint(const FPaintArgs& InPaintArgs, const FGeometry& InAllottedGeometry, const FSlateRect& InClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled)
{
	CachedSize = InAllottedGeometry.GetLocalSize();

	// Text blocks don't have scroll bars, so when the visible region is smaller than the desired size, 
	// we attempt to auto-scroll to keep the view of the text aligned with the current justification method
	FVector2D AutoScrollValue = FVector2D::ZeroVector; // Scroll to the left
	if(TextLayout->GetJustification() != ETextJustify::Left)
	{
		const float ActualWidth = TextLayout->GetSize().X;
		const float VisibleWidth = InAllottedGeometry.GetLocalSize().X;
		if(VisibleWidth < ActualWidth)
		{
			switch(TextLayout->GetJustification())
			{
			case ETextJustify::Center:
				AutoScrollValue.X = (ActualWidth - VisibleWidth) * 0.5f; // Scroll to the center
				break;

			case ETextJustify::Right:
				AutoScrollValue.X = (ActualWidth - VisibleWidth); // Scroll to the right
				break;

			default:
				break;
			}
		}
	}

	TextLayout->SetVisibleRegion(InAllottedGeometry.GetLocalSize(), AutoScrollValue);

	TextLayout->UpdateIfNeeded();

	return TextLayout->OnPaint(InPaintArgs, InAllottedGeometry, InClippingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void FTextBlockLayout::DirtyLayout()
{
	TextLayout->DirtyLayout();
}

void FTextBlockLayout::DirtyContent()
{
	DirtyLayout();
	Marshaller->MakeDirty();
}

void FTextBlockLayout::OverrideTextStyle(const FTextBlockStyle& InTextStyle)
{
	// Has the style used for this text block changed?
	if(!IsStyleUpToDate(InTextStyle))
	{
		TextLayout->SetDefaultTextStyle(InTextStyle);
		
		FString CurrentText;
		Marshaller->GetText(CurrentText, *TextLayout);
		UpdateTextLayout(CurrentText);
	}
}

void FTextBlockLayout::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	TextLayout->SetTextShapingMethod((InTextShapingMethod.IsSet()) ? InTextShapingMethod.GetValue() : GetDefaultTextShapingMethod());
}

void FTextBlockLayout::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	TextLayout->SetTextFlowDirection((InTextFlowDirection.IsSet()) ? InTextFlowDirection.GetValue() : GetDefaultTextFlowDirection());
}

void FTextBlockLayout::SetDebugSourceInfo(const TAttribute<FString>& InDebugSourceInfo)
{
	TextLayout->SetDebugSourceInfo(InDebugSourceInfo);
}

FChildren* FTextBlockLayout::GetChildren()
{
	return TextLayout->GetChildren();
}

void FTextBlockLayout::ArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	TextLayout->ArrangeChildren(AllottedGeometry, ArrangedChildren);
}


void FTextBlockLayout::UpdateTextLayout(const FText& InText)
{
	UpdateTextLayout(InText.ToString());
}

void FTextBlockLayout::UpdateTextLayout(const FString& InText)
{
	Marshaller->ClearDirty();
	TextLayout->ClearLines();

	TextLayout->ClearLineHighlights();
	TextLayout->ClearRunRenderers();

	Marshaller->SetText(InText, *TextLayout);

	HighlightTextLastUpdate = FTextSnapshot();
}

void FTextBlockLayout::UpdateTextHighlights(const FText& InHighlightText)
{
	const FString& HighlightTextString = InHighlightText.ToString();
	const int32 HighlightTextLength = HighlightTextString.Len();

	const TArray< FTextLayout::FLineModel >& LineModels = TextLayout->GetLineModels();

	TArray<FTextRunRenderer> TextHighlights;
	for(int32 LineIndex = 0; LineIndex < LineModels.Num(); ++LineIndex)
	{
		const FTextLayout::FLineModel& LineModel = LineModels[LineIndex];

		int32 FindBegin = 0;
		int32 CurrentHighlightBegin;
		const int32 TextLength = LineModel.Text->Len();
		while(FindBegin < TextLength && (CurrentHighlightBegin = LineModel.Text->Find(HighlightTextString, ESearchCase::IgnoreCase, ESearchDir::FromStart, FindBegin)) != INDEX_NONE)
		{
			FindBegin = CurrentHighlightBegin + HighlightTextLength;

			if(TextHighlights.Num() > 0 && TextHighlights.Last().LineIndex == LineIndex && TextHighlights.Last().Range.EndIndex == CurrentHighlightBegin)
			{
				TextHighlights[TextHighlights.Num() - 1] = FTextRunRenderer(LineIndex, FTextRange(TextHighlights.Last().Range.BeginIndex, FindBegin), TextHighlighter.ToSharedRef());
			}
			else
			{
				TextHighlights.Add(FTextRunRenderer(LineIndex, FTextRange(CurrentHighlightBegin, FindBegin), TextHighlighter.ToSharedRef()));
			}
		}
	}

	TextLayout->SetRunRenderers(TextHighlights);
}

bool FTextBlockLayout::IsStyleUpToDate(const FTextBlockStyle& NewStyle) const
{
	const FTextBlockStyle& CurrentStyle = TextLayout->GetDefaultTextStyle();

	return (CurrentStyle.Font == NewStyle.Font)
		&& (CurrentStyle.ColorAndOpacity == NewStyle.ColorAndOpacity)
		&& (CurrentStyle.ShadowOffset == NewStyle.ShadowOffset)
		&& (CurrentStyle.ShadowColorAndOpacity == NewStyle.ShadowColorAndOpacity)
		&& (CurrentStyle.SelectedBackgroundColor == NewStyle.SelectedBackgroundColor)
		&& (CurrentStyle.HighlightColor == NewStyle.HighlightColor)
		&& (CurrentStyle.HighlightShape == NewStyle.HighlightShape);
}

float FTextBlockLayout::CalculateWrappingWidth(const FWidgetArgs& InWidgetArgs) const
{
	const float WrapTextAt = InWidgetArgs.WrapTextAt.Get(0.0f);
	const bool bAutoWrapText = InWidgetArgs.AutoWrapText.Get(false);

	// Text wrapping can either be used defined (WrapTextAt), automatic (bAutoWrapText and CachedSize), 
	// or a mixture of both. Take whichever has the smallest value (>1)
	float WrappingWidth = WrapTextAt;
	if(bAutoWrapText && CachedSize.X >= 1.0f)
	{
		WrappingWidth = (WrappingWidth >= 1.0f) ? FMath::Min(WrappingWidth, CachedSize.X) : CachedSize.X;
	}

	return FMath::Max(0.0f, WrappingWidth);
}
