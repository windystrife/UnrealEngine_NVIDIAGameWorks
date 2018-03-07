// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "SColorGradingWheel.h"
#include "CoreStyle.h"
#include "RenderingCommon.h"
#include "DrawElements.h"



/* SColorGradingWheel methods
 *****************************************************************************/

void SColorGradingWheel::Construct(const FArguments& InArgs)
{
	Image = FCoreStyle::Get().GetBrush("ColorGradingWheel.HueValueCircle");
	SelectorImage = FCoreStyle::Get().GetBrush("ColorGradingWheel.Selector");
	SelectedColor = InArgs._SelectedColor;
	DesiredWheelSize = InArgs._DesiredWheelSize;
	ExponentDisplacement = InArgs._ExponentDisplacement;
	OnMouseCaptureBegin = InArgs._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InArgs._OnMouseCaptureEnd;
	OnValueChanged = InArgs._OnValueChanged;
}


/* SWidget overrides
 *****************************************************************************/

FVector2D SColorGradingWheel::ComputeDesiredSize(float) const
{
	if (DesiredWheelSize.IsSet())
	{
		int32 CachedDesiredWheelSize = DesiredWheelSize.Get();
		return FVector2D(CachedDesiredWheelSize, CachedDesiredWheelSize);
	}
	return Image->ImageSize;
}


FReply SColorGradingWheel::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}


FReply SColorGradingWheel::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnMouseCaptureBegin.ExecuteIfBound(SelectedColor.Get());

		if (!ProcessMouseAction(MyGeometry, MouseEvent, false))
		{
			OnMouseCaptureEnd.ExecuteIfBound(SelectedColor.Get());
			return FReply::Unhandled();
		}

		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}


FReply SColorGradingWheel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
	{
		OnMouseCaptureEnd.ExecuteIfBound(SelectedColor.Get());

		return FReply::Handled().ReleaseMouseCapture();
	}

	return FReply::Unhandled();
}


FReply SColorGradingWheel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!HasMouseCapture())
	{
		return FReply::Unhandled();
	}

	ProcessMouseAction(MyGeometry, MouseEvent, true);

	return FReply::Handled();
}


int32 SColorGradingWheel::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const bool bIsEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bIsEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FVector2D& SelectorSize = SelectorImage->ImageSize;
	FVector2D CircleSize = AllottedGeometry.GetLocalSize() - SelectorSize;
	FVector2D AllottedGeometrySize = AllottedGeometry.GetLocalSize();
	if (DesiredWheelSize.IsSet())
	{
		int32 CachedDesiredWheelSize = DesiredWheelSize.Get();
		CircleSize.X = (float)CachedDesiredWheelSize - SelectorSize.X;
		CircleSize.Y = (float)CachedDesiredWheelSize - SelectorSize.Y;
		AllottedGeometrySize.X = CachedDesiredWheelSize;
		AllottedGeometrySize.Y = CachedDesiredWheelSize;
	}
	
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
		AllottedGeometry.ToPaintGeometry(0.5f * (AllottedGeometrySize + CalcRelativePositionFromCenter() * CircleSize - SelectorSize), SelectorSize),
		SelectorImage,
		DrawEffects,
		InWidgetStyle.GetColorAndOpacityTint() * SelectorImage->GetTint(InWidgetStyle)
	);

	return LayerId + 1;
}


/* SColorGradingWheel implementation
 *****************************************************************************/

FVector2D SColorGradingWheel::CalcRelativePositionFromCenter() const
{
	float Hue = SelectedColor.Get().R;
	float Saturation = SelectedColor.Get().G;
	if (ExponentDisplacement.IsSet() && ExponentDisplacement.Get() != 1.0f && !FMath::IsNearlyEqual(ExponentDisplacement.Get(), 0.0f, 0.00001f))
	{
		//Use log curve to set the distance G value
		Saturation = FMath::Pow(Saturation, 1.0f / ExponentDisplacement.Get());
	}
	float Angle = Hue / 180.0f * PI;
	float Radius = Saturation;

	return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
}


bool SColorGradingWheel::ProcessMouseAction(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, bool bProcessWhenOutsideColorWheel)
{
	FVector2D GeometrySize = MyGeometry.GetLocalSize();
	if (DesiredWheelSize.IsSet())
	{
		int32 CachedDesiredWheelSize = DesiredWheelSize.Get();
		GeometrySize.X = CachedDesiredWheelSize;
		GeometrySize.Y = CachedDesiredWheelSize;
	}

	const FVector2D LocalMouseCoordinate = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	const FVector2D RelativePositionFromCenter = (2.0f * LocalMouseCoordinate - GeometrySize) / (GeometrySize - SelectorImage->ImageSize);
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
			float LinearRadius = FMath::Min(RelativeRadius, 1.0f);
			if (ExponentDisplacement.IsSet() && ExponentDisplacement.Get() != 1.0f)
			{
				//Use log curve to set the distance G value
				float LogDistance = FMath::Pow(LinearRadius, ExponentDisplacement.Get());
				NewColor.G = LogDistance;
			}
			else
			{
				NewColor.G = LinearRadius;
			}
		}

		OnValueChanged.ExecuteIfBound(NewColor);
	}

	return (RelativeRadius <= 1.0f);
}
