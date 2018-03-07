// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Images/SImage.h"
#include "Rendering/DrawElements.h"

void SImage::Construct( const FArguments& InArgs )
{
	Image = InArgs._Image;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	OnMouseButtonDownHandler = InArgs._OnMouseButtonDown;
}

int32 SImage::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateBrush* ImageBrush = Image.Get();

	if ((ImageBrush != nullptr) && (ImageBrush->DrawAs != ESlateBrushDrawType::NoDrawType))
	{
		const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
		const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

		const FLinearColor FinalColorAndOpacity( InWidgetStyle.GetColorAndOpacityTint() * ColorAndOpacity.Get().GetColor(InWidgetStyle) * ImageBrush->GetTint( InWidgetStyle ) );

		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), ImageBrush, DrawEffects, FinalColorAndOpacity );
	}
	return LayerId;
}

FReply SImage::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ( OnMouseButtonDownHandler.IsBound() )
	{
		// If a handler is assigned, call it.
		return OnMouseButtonDownHandler.Execute(MyGeometry, MouseEvent);
	}
	else
	{
		// otherwise the event is unhandled.
		return FReply::Unhandled();
	}	
}

FVector2D SImage::ComputeDesiredSize( float ) const
{
	const FSlateBrush* ImageBrush = Image.Get();
	if (ImageBrush != nullptr)
	{
		return ImageBrush->ImageSize;
	}
	return FVector2D::ZeroVector;
}

void SImage::SetColorAndOpacity( const TAttribute<FSlateColor>& InColorAndOpacity )
{
	if ( !ColorAndOpacity.IdenticalTo(InColorAndOpacity) )
	{
		ColorAndOpacity = InColorAndOpacity;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SImage::SetColorAndOpacity( FLinearColor InColorAndOpacity )
{
	if ( ColorAndOpacity.IsBound() || ColorAndOpacity.Get() != InColorAndOpacity )
	{
		ColorAndOpacity = InColorAndOpacity;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SImage::SetImage(TAttribute<const FSlateBrush*> InImage)
{
	if (Image.IsBound() || InImage.IsBound() || (Image.Get() != InImage.Get()))
	{
		Image = InImage;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SImage::SetOnMouseButtonDown(FPointerEventHandler EventHandler)
{
	OnMouseButtonDownHandler = EventHandler;
}
