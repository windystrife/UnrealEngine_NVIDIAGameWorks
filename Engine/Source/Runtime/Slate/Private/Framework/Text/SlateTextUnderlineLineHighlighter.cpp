// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/SlateTextUnderlineLineHighlighter.h"
#include "Rendering/SlateLayoutTransform.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"

FSlateTextUnderlineLineHighlighter::FSlateTextUnderlineLineHighlighter(const FSlateBrush& InUnderlineBrush, const FSlateFontInfo& InFontInfo, const FSlateColor InColorAndOpacity, const FVector2D InShadowOffset, const FLinearColor InShadowColorAndOpacity)
	: UnderlineBrush(InUnderlineBrush)
	, FontInfo(InFontInfo)
	, ColorAndOpacity(InColorAndOpacity)
	, ShadowOffset(InShadowOffset)
	, ShadowColorAndOpacity(InShadowColorAndOpacity)
{
}

int32 FSlateTextUnderlineLineHighlighter::OnPaint(const FPaintArgs& Args, const FTextLayout::FLineView& Line, const float OffsetX, const float Width, const FTextBlockStyle& DefaultStyle, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

	const uint16 MaxHeight = FontCache->GetMaxCharacterHeight(FontInfo, AllottedGeometry.Scale);
	const int16 Baseline = FontCache->GetBaseline(FontInfo, AllottedGeometry.Scale);

	int16 UnderlinePos, UnderlineThickness;
	FontCache->GetUnderlineMetrics(FontInfo, AllottedGeometry.Scale, UnderlinePos, UnderlineThickness);

	const FVector2D Location(Line.Offset.X + OffsetX, Line.Offset.Y + MaxHeight + Baseline - (UnderlinePos * 0.5f));
	const FVector2D Size(Width, FMath::Max<int16>(1, UnderlineThickness));

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	if (Size.X)
	{
		const FLinearColor LineColorAndOpacity = ColorAndOpacity.GetColor(InWidgetStyle);

		const bool ShouldDropShadow = ShadowColorAndOpacity.A > 0.f && ShadowOffset.SizeSquared() > 0.f;

		// A negative shadow offset should be applied as a positive offset to the underline to avoid clipping issues
		const FVector2D DrawShadowOffset(
			(ShadowOffset.X > 0.0f) ? ShadowOffset.X * AllottedGeometry.Scale : 0.0f,
			(ShadowOffset.Y > 0.0f) ? ShadowOffset.Y * AllottedGeometry.Scale : 0.0f
			);
		const FVector2D DrawUnderlineOffset(
			(ShadowOffset.X < 0.0f) ? -ShadowOffset.X * AllottedGeometry.Scale : 0.0f,
			(ShadowOffset.Y < 0.0f) ? -ShadowOffset.Y * AllottedGeometry.Scale : 0.0f
			);

		// Draw the optional shadow
		if (ShouldDropShadow)
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location + DrawShadowOffset))),
				&UnderlineBrush,
				bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
				ShadowColorAndOpacity * InWidgetStyle.GetColorAndOpacityTint()
				);
		}

		// Draw underline
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Size), FSlateLayoutTransform(TransformPoint(InverseScale, Location + DrawUnderlineOffset))),
			&UnderlineBrush,
			bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect,
			LineColorAndOpacity * InWidgetStyle.GetColorAndOpacityTint()
			);
	}

	return LayerId;
}

TSharedRef<FSlateTextUnderlineLineHighlighter> FSlateTextUnderlineLineHighlighter::Create(const FSlateBrush& InUnderlineBrush, const FSlateFontInfo& InFontInfo, const FSlateColor InColorAndOpacity, const FVector2D InShadowOffset, const FLinearColor InShadowColorAndOpacity)
{
	return MakeShareable(new FSlateTextUnderlineLineHighlighter(InUnderlineBrush, InFontInfo, InColorAndOpacity, InShadowOffset, InShadowColorAndOpacity));
}
