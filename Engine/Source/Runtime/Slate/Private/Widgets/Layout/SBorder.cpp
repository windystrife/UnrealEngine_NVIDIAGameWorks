// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SBorder.h"
#include "Rendering/DrawElements.h"

static FName SBorderTypeName("SBorder");

SBorder::SBorder()
	: BorderImage( FCoreStyle::Get().GetBrush( "Border" ) )
	, BorderBackgroundColor( FLinearColor::White )
	, DesiredSizeScale(FVector2D(1,1))
{
}

void SBorder::Construct( const SBorder::FArguments& InArgs )
{
	// Only do this if we're exactly an SBorder
	if ( GetType() == SBorderTypeName )
	{
		bCanTick = false;
		bCanSupportFocus = false;
	}

	ContentScale = InArgs._ContentScale;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	DesiredSizeScale = InArgs._DesiredSizeScale;

	ShowDisabledEffect = InArgs._ShowEffectWhenDisabled;

	BorderImage = InArgs._BorderImage;
	BorderBackgroundColor = InArgs._BorderBackgroundColor;
	ForegroundColor = InArgs._ForegroundColor;
	SetOnMouseButtonDown(InArgs._OnMouseButtonDown);
	SetOnMouseButtonUp(InArgs._OnMouseButtonUp);
	SetOnMouseMove(InArgs._OnMouseMove);
	SetOnMouseDoubleClick(InArgs._OnMouseDoubleClick);

	ChildSlot
		.HAlign(InArgs._HAlign)
		.VAlign(InArgs._VAlign)
		.Padding(InArgs._Padding)
	[
		InArgs._Content.Widget
	];
}

void SBorder::SetContent( TSharedRef< SWidget > InContent )
{
	ChildSlot
	[
		InContent
	];
}

const TSharedRef< SWidget >& SBorder::GetContent() const
{
	return ChildSlot.GetWidget();
}

void SBorder::ClearContent()
{
	ChildSlot.DetachWidget();
}

int32 SBorder::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateBrush* BrushResource = BorderImage.Get();
		
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const bool bShowDisabledEffect = ShowDisabledEffect.Get();
	ESlateDrawEffect DrawEffects = bShowDisabledEffect && !bEnabled ? ESlateDrawEffect::DisabledEffect : ESlateDrawEffect::None;

	if ( BrushResource && BrushResource->DrawAs != ESlateBrushDrawType::NoDrawType )
	{
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			BrushResource,
			DrawEffects,
			BrushResource->GetTint( InWidgetStyle ) *InWidgetStyle.GetColorAndOpacityTint() * BorderBackgroundColor.Get().GetColor( InWidgetStyle )
		);
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );
}

FVector2D SBorder::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DesiredSizeScale.Get() * SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
}

void SBorder::SetBorderBackgroundColor(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	BorderBackgroundColor = InColorAndOpacity;
}

void SBorder::SetDesiredSizeScale(const TAttribute<FVector2D>& InDesiredSizeScale)
{
	DesiredSizeScale = InDesiredSizeScale;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SBorder::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.HAlignment = HAlign;
}

void SBorder::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.VAlignment = VAlign;
}

void SBorder::SetPadding(const TAttribute<FMargin>& InPadding)
{
	ChildSlot.SlotPadding = InPadding;
}

void SBorder::SetShowEffectWhenDisabled(const TAttribute<bool>& InShowEffectWhenDisabled)
{
	ShowDisabledEffect = InShowEffectWhenDisabled;
}

void SBorder::SetBorderImage(const TAttribute<const FSlateBrush*>& InBorderImage)
{
	BorderImage = InBorderImage;
}
