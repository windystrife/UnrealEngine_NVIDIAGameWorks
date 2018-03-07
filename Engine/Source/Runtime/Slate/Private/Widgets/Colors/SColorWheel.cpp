// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Colors/SColorWheel.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"


/* SColorWheel methods
 *****************************************************************************/

void SColorWheel::Construct(const FArguments& InArgs)
{
	Image = FCoreStyle::Get().GetBrush("ColorWheel.HueValueCircle");
	SelectorImage = FCoreStyle::Get().GetBrush("ColorWheel.Selector");
	SelectedColor = InArgs._SelectedColor;

	OnMouseCaptureBegin = InArgs._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InArgs._OnMouseCaptureEnd;
	OnValueChanged = InArgs._OnValueChanged;
}


/* SWidget overrides
 *****************************************************************************/

FVector2D SColorWheel::ComputeDesiredSize(float) const
{
	return Image->ImageSize + SelectorImage->ImageSize;
}


FReply SColorWheel::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}


FReply SColorWheel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnMouseCaptureBegin.ExecuteIfBound();

		if (!ProcessMouseAction(MyGeometry, MouseEvent, false))
		{
			OnMouseCaptureEnd.ExecuteIfBound();
			return FReply::Unhandled();
		}

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}


FReply SColorWheel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
	{
		OnMouseCaptureEnd.ExecuteIfBound();

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}


FReply SColorWheel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	ProcessMouseAction(MyGeometry, MouseEvent, true);

	return FReply::Handled();
}


int32 SColorWheel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FVector2D& SelectorSize = SelectorImage->ImageSize;
	const FVector2D CircleSize = AllottedGeometry.GetLocalSize() - SelectorSize;
	
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(0.5 * SelectorSize, CircleSize),
		Image,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * Image->GetTint(InWidgetStyle)
	);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId + 1,
		AllottedGeometry.ToPaintGeometry(0.5f * (AllottedGeometry.GetLocalSize() + CalcRelativePositionFromCenter() * CircleSize - SelectorSize), SelectorSize),
		SelectorImage,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * SelectorImage->GetTint(InWidgetStyle)
	);

	return LayerId + 1;
}


/* SColorWheel implementation
 *****************************************************************************/

FVector2D SColorWheel::CalcRelativePositionFromCenter() const
{
	float Hue = SelectedColor.Get().R;
	float Saturation = SelectedColor.Get().G;
	float Angle = Hue / 180.0f * PI;
	float Radius = Saturation;

	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
}


bool SColorWheel::ProcessMouseAction(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bProcessWhenOutsideColorWheel)
{
	const FVector2D LocalMouseCoordinate = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D RelativePositionFromCenter = (2.0f * LocalMouseCoordinate - MyGeometry.GetLocalSize()) / (MyGeometry.GetLocalSize() - SelectorImage->ImageSize);
	const float RelativeRadius = RelativePositionFromCenter.Size();

	if (RelativeRadius <= 1.0f || bProcessWhenOutsideColorWheel)
	{
		float Angle = FMath::Atan2(RelativePositionFromCenter.Y, RelativePositionFromCenter.X);

		if (Angle < 0.0f)
		{
			Angle += 2.0f * PI;
		}

		FLinearColor NewColor = SelectedColor.Get();
		{
			NewColor.R = Angle * 180.0f * INV_PI;
			NewColor.G = FMath::Min(RelativeRadius, 1.0f);
		}

		OnValueChanged.ExecuteIfBound(NewColor);
	}

	return (RelativeRadius <= 1.0f);
}
