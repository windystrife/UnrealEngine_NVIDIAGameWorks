// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SSlider.h"
#include "Rendering/DrawElements.h"

void SSlider::Construct( const SSlider::FArguments& InDeclaration )
{
	check(InDeclaration._Style);

	Style = InDeclaration._Style;

	IndentHandle = InDeclaration._IndentHandle;
	LockedAttribute = InDeclaration._Locked;
	Orientation = InDeclaration._Orientation;
	StepSize = InDeclaration._StepSize;
	ValueAttribute = InDeclaration._Value;
	SliderBarColor = InDeclaration._SliderBarColor;
	SliderHandleColor = InDeclaration._SliderHandleColor;
	bIsFocusable = InDeclaration._IsFocusable;
	OnMouseCaptureBegin = InDeclaration._OnMouseCaptureBegin;
	OnMouseCaptureEnd = InDeclaration._OnMouseCaptureEnd;
	OnControllerCaptureBegin = InDeclaration._OnControllerCaptureBegin;
	OnControllerCaptureEnd = InDeclaration._OnControllerCaptureEnd;
	OnValueChanged = InDeclaration._OnValueChanged;

	bControllerInputCaptured = false;
}

int32 SSlider::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// we draw the slider like a horizontal slider regardless of the orientation, and apply a render transform to make it display correctly.
	// However, the AllottedGeometry is computed as it will be rendered, so we have to use the "horizontal orientation" when doing drawing computations.
	const float AllottedWidth = Orientation == Orient_Horizontal ? AllottedGeometry.GetLocalSize().X : AllottedGeometry.GetLocalSize().Y;
	const float AllottedHeight = Orientation == Orient_Horizontal ? AllottedGeometry.GetLocalSize().Y : AllottedGeometry.GetLocalSize().X;

	float HandleRotation;
	FVector2D HandleTopLeftPoint;
	FVector2D SliderStartPoint;
	FVector2D SliderEndPoint;

	// calculate slider geometry as if it's a horizontal slider (we'll rotate it later if it's vertical)
	const FVector2D HandleSize = Style->NormalThumbImage.ImageSize;
	const FVector2D HalfHandleSize = 0.5f * HandleSize;
	const float Indentation = IndentHandle.Get() ? HandleSize.X : 0.0f;

	const float SliderLength = AllottedWidth - (Indentation + HandleSize.X);
	const float SliderPercent = ValueAttribute.Get();
	const float SliderHandleOffset = SliderPercent * SliderLength;
	const float SliderY = 0.5f * AllottedHeight;

	HandleRotation = 0.0f;
	HandleTopLeftPoint = FVector2D(SliderHandleOffset + (0.5f * Indentation), SliderY - HalfHandleSize.Y);

	SliderStartPoint = FVector2D(HalfHandleSize.X, SliderY);
	SliderEndPoint = FVector2D(AllottedWidth - HalfHandleSize.X, SliderY);

	FGeometry SliderGeometry = AllottedGeometry;
	
	// rotate the slider 90deg if it's vertical. The 0 side goes on the bottom, the 1 side on the top.
	if (Orientation == Orient_Vertical)
	{
		// Do this by translating along -X by the width of the geometry, then rotating 90 degreess CCW (left-hand coords)
		FSlateRenderTransform SlateRenderTransform = TransformCast<FSlateRenderTransform>(Concatenate(Inverse(FVector2D(AllottedWidth, 0)), FQuat2D(FMath::DegreesToRadians(-90.0f))));
		// create a child geometry matching this one, but with the render transform.
		SliderGeometry = AllottedGeometry.MakeChild(
			FVector2D(AllottedWidth, AllottedHeight), 
			FSlateLayoutTransform(), 
			SlateRenderTransform, FVector2D::ZeroVector);
	}

	const bool bEnabled = ShouldBeEnabled(bParentEnabled);
	const ESlateDrawEffect DrawEffects = bEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	// draw slider bar
	auto BarTopLeft = FVector2D(SliderStartPoint.X, SliderStartPoint.Y - Style->BarThickness * 0.5f);
	auto BarSize = FVector2D(SliderEndPoint.X - SliderStartPoint.X, Style->BarThickness);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		SliderGeometry.ToPaintGeometry(BarTopLeft, BarSize),
		LockedAttribute.Get() ? &Style->DisabledBarImage : &Style->NormalBarImage,
		DrawEffects,
		SliderBarColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
		);

	++LayerId;

	// draw slider thumb
	FSlateDrawElement::MakeBox( 
		OutDrawElements,
		LayerId,
		SliderGeometry.ToPaintGeometry(HandleTopLeftPoint, Style->NormalThumbImage.ImageSize),
		LockedAttribute.Get() ? &Style->DisabledThumbImage : &Style->NormalThumbImage,
		DrawEffects,
		SliderHandleColor.Get().GetColor(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	return LayerId;
}

FVector2D SSlider::ComputeDesiredSize( float ) const
{
	static const FVector2D SSliderDesiredSize(16.0f, 16.0f);

	if ( Style == nullptr )
	{
		return SSliderDesiredSize;
	}

	const float Thickness = FMath::Max(Style->BarThickness, Style->NormalThumbImage.ImageSize.Y);

	if (Orientation == Orient_Vertical)
	{
		return FVector2D(Thickness, SSliderDesiredSize.Y);
	}

	return FVector2D(SSliderDesiredSize.X, Thickness);
}

bool SSlider::IsLocked() const
{
	return LockedAttribute.Get();
}

bool SSlider::IsInteractable() const
{
	return IsEnabled() && !IsLocked() && SupportsKeyboardFocus();
}

bool SSlider::SupportsKeyboardFocus() const
{
	return bIsFocusable;
}

void SSlider::ResetControllerState()
{
	if (bControllerInputCaptured)
	{
		OnControllerCaptureEnd.ExecuteIfBound();
		bControllerInputCaptured = false;
	}
}

FReply SSlider::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = FReply::Unhandled();
	const FKey KeyPressed = InKeyEvent.GetKey();

	if (IsInteractable())
	{
		// The controller's bottom face button must be pressed once to begin manipulating the slider's value.
		// Navigation away from the widget is prevented until the button has been pressed again or focus is lost.
		// The value can be manipulated by using the game pad's directional arrows ( relative to slider orientation ).
		if (KeyPressed == EKeys::Enter || KeyPressed == EKeys::SpaceBar || KeyPressed == EKeys::Virtual_Accept)
		{
			if (bControllerInputCaptured == false)
			{
				// Begin capturing controller input and allow user to modify the slider's value.
				bControllerInputCaptured = true;
				OnControllerCaptureBegin.ExecuteIfBound();
				Reply = FReply::Handled();
			}
			else
			{
				ResetControllerState();
				Reply = FReply::Handled();
			}
		}

		if (bControllerInputCaptured)
		{
			float NewValue = ValueAttribute.Get();
			if (Orientation == EOrientation::Orient_Horizontal)
			{
				if (KeyPressed == EKeys::Left || KeyPressed == EKeys::Gamepad_DPad_Left || KeyPressed == EKeys::Gamepad_LeftStick_Left)
				{
					NewValue -= StepSize.Get();
				}
				else if (KeyPressed == EKeys::Right || KeyPressed == EKeys::Gamepad_DPad_Right || KeyPressed == EKeys::Gamepad_LeftStick_Right)
				{
					NewValue += StepSize.Get();
				}
			}
			else
			{
				if (KeyPressed == EKeys::Down || KeyPressed == EKeys::Gamepad_DPad_Down || KeyPressed == EKeys::Gamepad_LeftStick_Down)
				{
					NewValue -= StepSize.Get();
				}
				else if (KeyPressed == EKeys::Up || KeyPressed == EKeys::Gamepad_DPad_Up || KeyPressed == EKeys::Gamepad_LeftStick_Up)
				{
					NewValue += StepSize.Get();
				}
			}

			CommitValue(FMath::Clamp(NewValue, 0.0f, 1.0f));
			Reply = FReply::Handled();
		}
		else
		{
			Reply = SLeafWidget::OnKeyDown(MyGeometry, InKeyEvent);
		}
	}
	else
	{
		Reply = SLeafWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

	return Reply;
}

FReply SSlider::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = FReply::Unhandled();
	if (bControllerInputCaptured)
	{
		Reply = FReply::Handled();
	}
	return Reply;
}

void SSlider::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	if (bControllerInputCaptured)
	{
		// Commit and reset state
		CommitValue(ValueAttribute.Get());
		ResetControllerState();
	}
}

FReply SSlider::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && !IsLocked())
	{
		OnMouseCaptureBegin.ExecuteIfBound();
		CommitValue(PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition()));
		
		// Release capture for controller/keyboard when switching to mouse.
		ResetControllerState();
		
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	return FReply::Unhandled();
}

FReply SSlider::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if ((MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) && HasMouseCapture())
	{
		SetCursor(EMouseCursor::Default);
		OnMouseCaptureEnd.ExecuteIfBound();
		
		// Release capture for controller/keyboard when switching to mouse.
		ResetControllerState();
		
		return FReply::Handled().ReleaseMouseCapture();	
	}

	return FReply::Unhandled();
}

FReply SSlider::OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if (this->HasMouseCapture() && !IsLocked())
	{
		SetCursor((Orientation == Orient_Horizontal) ? EMouseCursor::ResizeLeftRight : EMouseCursor::ResizeUpDown);
		CommitValue(PositionToValue(MyGeometry, MouseEvent.GetLastScreenSpacePosition()));
		
		// Release capture for controller/keyboard when switching to mouse
		ResetControllerState();

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SSlider::CommitValue(float NewValue)
{
	if (!ValueAttribute.IsBound())
	{
		ValueAttribute.Set(NewValue);
	}

	OnValueChanged.ExecuteIfBound(NewValue);
}

float SSlider::PositionToValue( const FGeometry& MyGeometry, const FVector2D& AbsolutePosition )
{
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(AbsolutePosition);

	float RelativeValue;
	float Denominator;
	// Only need X as we rotate the thumb image when rendering vertically
	const float Indentation = Style->NormalThumbImage.ImageSize.X * (IndentHandle.Get() ? 2.f : 1.f);
	const float HalfIndentation = 0.5f * Indentation;

	if (Orientation == Orient_Horizontal)
	{
		Denominator = MyGeometry.Size.X - Indentation;
		RelativeValue = (Denominator != 0.f) ? (LocalPosition.X - HalfIndentation) / Denominator : 0.f;
	}
	else
	{
		Denominator = MyGeometry.Size.Y - Indentation;
		// Inverse the calculation as top is 0 and bottom is 1
		RelativeValue = (Denominator != 0.f) ? ((MyGeometry.Size.Y - LocalPosition.Y) - HalfIndentation) / Denominator : 0.f;
	}
	
	return FMath::Clamp(RelativeValue, 0.0f, 1.0f);
}

float SSlider::GetValue() const
{
	return ValueAttribute.Get();
}

void SSlider::SetValue(const TAttribute<float>& InValueAttribute)
{
	ValueAttribute = InValueAttribute;
}

void SSlider::SetIndentHandle(const TAttribute<bool>& InIndentHandle)
{
	IndentHandle = InIndentHandle;
}

void SSlider::SetLocked(const TAttribute<bool>& InLocked)
{
	LockedAttribute = InLocked;
}

void SSlider::SetOrientation(EOrientation InOrientation)
{
	Orientation = InOrientation;
}

void SSlider::SetSliderBarColor(FSlateColor InSliderBarColor)
{
	SliderBarColor = InSliderBarColor;
}

void SSlider::SetSliderHandleColor(FSlateColor InSliderHandleColor)
{
	SliderHandleColor = InSliderHandleColor;
}

float SSlider::GetStepSize() const
{
	return StepSize.Get();
}

void SSlider::SetStepSize(const TAttribute<float>& InStepSize)
{
	StepSize = InStepSize;
}
