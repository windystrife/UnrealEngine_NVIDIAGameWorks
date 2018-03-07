// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Text/SRichTextBlock.h"

#if WITH_FANCY_TEXT

#include "Widgets/Text/TextBlockLayout.h"
#include "Framework/Text/RichTextMarkupProcessing.h"
#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Types/ReflectionMetadata.h"

SRichTextBlock::SRichTextBlock()
{
}

SRichTextBlock::~SRichTextBlock()
{
	// Needed to avoid "deletion of pointer to incomplete type 'FTextBlockLayout'; no destructor called" error when using TUniquePtr
}

void SRichTextBlock::Construct( const FArguments& InArgs )
{
	BoundText = InArgs._Text;
	HighlightText = InArgs._HighlightText;

	TextStyle = *InArgs._TextStyle;
	WrapTextAt = InArgs._WrapTextAt;
	AutoWrapText = InArgs._AutoWrapText;
	WrappingPolicy = InArgs._WrappingPolicy;
	Margin = InArgs._Margin;
	LineHeightPercentage = InArgs._LineHeightPercentage;
	Justification = InArgs._Justification;
	MinDesiredWidth = InArgs._MinDesiredWidth;

	{
		TSharedPtr<IRichTextMarkupParser> Parser = InArgs._Parser;
		if ( !Parser.IsValid() )
		{
			Parser = FDefaultRichTextMarkupParser::Create();
		}

		TSharedPtr<FRichTextLayoutMarshaller> Marshaller = InArgs._Marshaller;
		if (!Marshaller.IsValid())
		{
			Marshaller = FRichTextLayoutMarshaller::Create(Parser, nullptr, InArgs._Decorators, InArgs._DecoratorStyleSet);
		}
		
		for (const TSharedRef< ITextDecorator >& Decorator : InArgs.InlineDecorators)
		{
			Marshaller->AppendInlineDecorator(Decorator);
		}

		TextLayoutCache = MakeUnique<FTextBlockLayout>(TextStyle, InArgs._TextShapingMethod, InArgs._TextFlowDirection, InArgs._CreateSlateTextLayout, Marshaller.ToSharedRef(), nullptr);
		TextLayoutCache->SetDebugSourceInfo(TAttribute<FString>::Create(TAttribute<FString>::FGetter::CreateLambda([this]{ return FReflectionMetaData::GetWidgetDebugInfo(this); })));
	}
}

int32 SRichTextBlock::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// OnPaint will also update the text layout cache if required
	LayerId = TextLayoutCache->OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled(bParentEnabled));

	return LayerId;
}

FVector2D SRichTextBlock::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	// ComputeDesiredSize will also update the text layout cache if required
	const FVector2D TextSize = TextLayoutCache->ComputeDesiredSize(
		FTextBlockLayout::FWidgetArgs(BoundText, HighlightText, WrapTextAt, AutoWrapText, WrappingPolicy, Margin, LineHeightPercentage, Justification),
		LayoutScaleMultiplier, TextStyle
		);

	return FVector2D(FMath::Max(TextSize.X, MinDesiredWidth.Get()), TextSize.Y);
}

FChildren* SRichTextBlock::GetChildren()
{
	return TextLayoutCache->GetChildren();
}

void SRichTextBlock::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	TextLayoutCache->ArrangeChildren( AllottedGeometry, ArrangedChildren );
}

void SRichTextBlock::SetText( const TAttribute<FText>& InTextAttr )
{
	BoundText = InTextAttr;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetHighlightText( const TAttribute<FText>& InHighlightText )
{
	HighlightText = InHighlightText;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetTextShapingMethod(const TOptional<ETextShapingMethod>& InTextShapingMethod)
{
	TextLayoutCache->SetTextShapingMethod(InTextShapingMethod);
	Invalidate(EInvalidateWidget::Layout);
}

void SRichTextBlock::SetTextFlowDirection(const TOptional<ETextFlowDirection>& InTextFlowDirection)
{
	TextLayoutCache->SetTextFlowDirection(InTextFlowDirection);
	Invalidate(EInvalidateWidget::Layout);
}

void SRichTextBlock::SetWrapTextAt(const TAttribute<float>& InWrapTextAt)
{
	WrapTextAt = InWrapTextAt;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetAutoWrapText(const TAttribute<bool>& InAutoWrapText)
{
	AutoWrapText = InAutoWrapText;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetWrappingPolicy(const TAttribute<ETextWrappingPolicy>& InWrappingPolicy)
{
	WrappingPolicy = InWrappingPolicy;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetLineHeightPercentage(const TAttribute<float>& InLineHeightPercentage)
{
	LineHeightPercentage = InLineHeightPercentage;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetMargin(const TAttribute<FMargin>& InMargin)
{
	Margin = InMargin;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetJustification(const TAttribute<ETextJustify::Type>& InJustification)
{
	Justification = InJustification;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::SetTextStyle(const FTextBlockStyle& InTextStyle)
{
	TextStyle = InTextStyle;
	Invalidate(EInvalidateWidget::Layout);
}

void SRichTextBlock::SetMinDesiredWidth(const TAttribute<float>& InMinDesiredWidth)
{
	MinDesiredWidth = InMinDesiredWidth;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SRichTextBlock::Refresh()
{
	TextLayoutCache->DirtyContent();
	Invalidate(EInvalidateWidget::Layout);
}

bool SRichTextBlock::ComputeVolatility() const
{
	return SWidget::ComputeVolatility() 
		|| BoundText.IsBound()
		|| HighlightText.IsBound()
		|| WrapTextAt.IsBound()
		|| AutoWrapText.IsBound()
		|| WrappingPolicy.IsBound()
		|| Margin.IsBound()
		|| Justification.IsBound()
		|| LineHeightPercentage.IsBound()
		|| MinDesiredWidth.IsBound();
}

#endif //WITH_FANCY_TEXT
