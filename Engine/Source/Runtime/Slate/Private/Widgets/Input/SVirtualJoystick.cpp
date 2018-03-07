// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Input/SVirtualJoystick.h"
#include "Rendering/DrawElements.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"


const float OPACITY_LERP_RATE = 3.f;


static FORCEINLINE float GetScaleFactor(const FGeometry& Geometry)
{
	const float DesiredWidth = 1024.0f;

	float UndoDPIScaling = 1.0f / Geometry.Scale;
	return (Geometry.GetDrawSize().GetMax() / DesiredWidth) * UndoDPIScaling;
}

FORCEINLINE float SVirtualJoystick::GetBaseOpacity()
{
	return (State == State_Active || State == State_CountingDownToInactive) ? ActiveOpacity : InactiveOpacity;
}

void SVirtualJoystick::FControlInfo::Reset()
{
	// snap the visual center back to normal (for controls that have a center on touch)
	VisualCenter = CorrectedCenter;
}

void SVirtualJoystick::Construct( const FArguments& InArgs )
{
	State = State_Inactive;
	bVisible = true;
	bPreventReCenter = false;
	
	// just set some defaults
	ActiveOpacity = 1.0f;
	InactiveOpacity = 0.1f;
	TimeUntilDeactive = 0.5f;
	TimeUntilReset = 2.0f;
	ActivationDelay = 0.f;
	CurrentOpacity = InactiveOpacity;
	StartupDelay = 0.f;

	// listen for displaymetrics changes to reposition controls
	FSlateApplication::Get().GetPlatformApplication()->OnDisplayMetricsChanged().AddSP(this, &SVirtualJoystick::HandleDisplayMetricsChanged);
}

void SVirtualJoystick::HandleDisplayMetricsChanged(const FDisplayMetrics& NewDisplayMetric)
{
	// Mark all controls to be repositioned on next tick
	for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++)
	{
		Controls[ControlIndex].bHasBeenPositioned = false;
	}
}

void SVirtualJoystick::SetGlobalParameters(float InActiveOpacity, float InInactiveOpacity, float InTimeUntilDeactive, float InTimeUntilReset, float InActivationDelay, bool InbPreventReCenter, float InStartupDelay)
{
	ActiveOpacity = InActiveOpacity;
	InactiveOpacity = InInactiveOpacity;
	TimeUntilDeactive = InTimeUntilDeactive;
	TimeUntilReset = InTimeUntilReset;
	ActivationDelay = InActivationDelay;
	StartupDelay = InStartupDelay;

	bPreventReCenter = InbPreventReCenter;

	if (StartupDelay > 0.f)
	{
		State = State_WaitForStart;
	}
}

bool SVirtualJoystick::ShouldDisplayTouchInterface()
{
	bool bAlwaysShowTouchInterface = false;
	GConfig->GetBool(TEXT("/Script/Engine.InputSettings"), TEXT("bAlwaysShowTouchInterface"), bAlwaysShowTouchInterface, GInputIni);

	// do we want to show virtual joysticks?
	return FPlatformMisc::GetUseVirtualJoysticks() || bAlwaysShowTouchInterface || FSlateApplication::Get().IsFakingTouchEvents();
}

static int32 ResolveRelativePosition(float Position, float RelativeTo, float ScaleFactor)
{
	// absolute from edge
	if (Position < -1.0f)
	{
		return RelativeTo + Position * ScaleFactor;
	}
	// relative from edge
	else if (Position < 0.0f)
	{
		return RelativeTo + Position * RelativeTo;
	}
	// relative from 0
	else if (Position <= 1.0f)
	{
		return Position * RelativeTo;
	}
	// absolute from 0
	else
	{
		return Position * ScaleFactor;
	}

}

static bool PositionIsInside(const FVector2D& Center, const FVector2D& Position, const FVector2D& BoxSize)
{
	return
		Position.X >= Center.X - BoxSize.X * 0.5f &&
		Position.X <= Center.X + BoxSize.X * 0.5f &&
		Position.Y >= Center.Y - BoxSize.Y * 0.5f &&
		Position.Y <= Center.Y + BoxSize.Y * 0.5f;
}

int32 SVirtualJoystick::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 RetLayerId = LayerId;

	if (bVisible)
	{
		FLinearColor ColorAndOpacitySRGB = InWidgetStyle.GetColorAndOpacityTint();
		ColorAndOpacitySRGB.A = CurrentOpacity;

		for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++)
		{
			const FControlInfo& Control = Controls[ControlIndex];

			if (Control.Image2.IsValid())
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
					Control.VisualCenter - FVector2D(Control.CorrectedVisualSize.X * 0.5f, Control.CorrectedVisualSize.Y * 0.5f),
					Control.CorrectedVisualSize),
					Control.Image2.Get(),
					ESlateDrawEffect::None,
					ColorAndOpacitySRGB
					);
			}

			if (Control.Image1.IsValid())
			{
				FSlateDrawElement::MakeBox(
					OutDrawElements,
					RetLayerId++,
					AllottedGeometry.ToPaintGeometry(
					Control.VisualCenter + Control.ThumbPosition - FVector2D(Control.CorrectedThumbSize.X * 0.5f, Control.CorrectedThumbSize.Y * 0.5f),
					Control.CorrectedThumbSize),
					Control.Image1.Get(),
					ESlateDrawEffect::None,
					ColorAndOpacitySRGB
					);
			}
		}
	}
	
	return RetLayerId;
}

FVector2D SVirtualJoystick::ComputeDesiredSize( float ) const
{
	return FVector2D(100, 100);
}

bool SVirtualJoystick::SupportsKeyboardFocus() const
{
	return false;
}

FReply SVirtualJoystick::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
//	UE_LOG(LogTemp, Log, TEXT("Pointer index: %d"), Event.GetPointerIndex());

	FVector2D LocalCoord = MyGeometry.AbsoluteToLocal( Event.GetScreenSpacePosition() );

	for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++)
	{
		FControlInfo& Control = Controls[ControlIndex];

		// skip controls already in use
		if (Control.CapturedPointerIndex == -1)
		{
			if (PositionIsInside(Control.CorrectedCenter, LocalCoord, Control.CorrectedInteractionSize))
			{
				// Align Joystick inside of Screen
				AlignBoxIntoScreen(LocalCoord, Control.CorrectedVisualSize, MyGeometry.GetLocalSize());

				Control.CapturedPointerIndex = Event.GetPointerIndex();

				if (ActivationDelay == 0.f)
				{
					CurrentOpacity = ActiveOpacity;

					if (!bPreventReCenter)
					{
						Control.VisualCenter = LocalCoord;
					}

					if (HandleTouch(ControlIndex, LocalCoord, MyGeometry.GetLocalSize())) // Never fail!
					{
						return FReply::Handled().CaptureMouse(SharedThis(this));
					}
				}
				else
				{
					Control.bNeedUpdatedCenter = true;
					Control.ElapsedTime = 0.f;
					Control.NextCenter = LocalCoord;

					return FReply::Unhandled();
				}
			}
		}
	}

//	CapturedPointerIndex[CapturedJoystick] = -1;

	return FReply::Unhandled();
}

FReply SVirtualJoystick::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
	FVector2D LocalCoord = MyGeometry.AbsoluteToLocal( Event.GetScreenSpacePosition() );

	for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++)
	{
		FControlInfo& Control = Controls[ControlIndex];

		// is this control the one captured to this pointer?
		if (Control.CapturedPointerIndex == Event.GetPointerIndex())
		{
			if (Control.bNeedUpdatedCenter)
			{
				return FReply::Unhandled();
			}
			else if (HandleTouch(ControlIndex, LocalCoord, MyGeometry.GetLocalSize()))
			{
				return FReply::Handled();
			}
		}
	}

	return FReply::Unhandled();
}

FReply SVirtualJoystick::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& Event)
{
	for ( int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++ )
	{
		FControlInfo& Control = Controls[ControlIndex];

		// is this control the one captured to this pointer?
		if ( Control.CapturedPointerIndex == Event.GetPointerIndex() )
		{
			// release and center the joystick
			Control.ThumbPosition = FVector2D(0, 0);
			Control.CapturedPointerIndex = -1;

			// send one more joystick update for the centering
			Control.bSendOneMoreEvent = true;

			// Pass event as unhandled if time is too short
			if ( Control.bNeedUpdatedCenter )
			{
				Control.bNeedUpdatedCenter = false;
				return FReply::Unhandled();
			}

			return FReply::Handled().ReleaseMouseCapture();
		}
	}

	return FReply::Unhandled();
}

bool SVirtualJoystick::HandleTouch(int32 ControlIndex, const FVector2D& LocalCoord, const FVector2D& ScreenSize)
{
	FControlInfo& Control = Controls[ControlIndex];

	// figure out position around center
	FVector2D Offset = LocalCoord - Controls[ControlIndex].VisualCenter;
	// only do work if we aren't at the center
	if (Offset == FVector2D(0, 0))
	{
		Control.ThumbPosition = Offset;
	}
	else
	{
		// clamp to the ellipse of the stick (snaps to the visual size, so, the art should go all the way to the edge of the texture)
		float DistanceToTouchSqr = Offset.SizeSquared();
		float Angle = FMath::Atan2(Offset.Y, Offset.X);

		// length along line to ellipse: L = 1.0 / sqrt(((sin(T)/Rx)^2 + (cos(T)/Ry)^2))
		float CosAngle = FMath::Cos(Angle);
		float SinAngle = FMath::Sin(Angle);
		float XTerm = CosAngle / (Control.CorrectedVisualSize.X * 0.5f);
		float YTerm = SinAngle / (Control.CorrectedVisualSize.Y * 0.5f);
		float DistanceToEdge = FMath::InvSqrt(XTerm * XTerm + YTerm * YTerm);

		// only clamp 
		if (DistanceToTouchSqr > FMath::Square(DistanceToEdge))
		{
			Control.ThumbPosition = FVector2D(DistanceToEdge * CosAngle,  DistanceToEdge * SinAngle);
		}
		else
		{
			Control.ThumbPosition = Offset;
		}
	}

	FVector2D AbsoluteThumbPos = Control.ThumbPosition + Controls[ControlIndex].VisualCenter;
	AlignBoxIntoScreen(AbsoluteThumbPos, Control.CorrectedThumbSize, ScreenSize);
	Control.ThumbPosition = AbsoluteThumbPos - Controls[ControlIndex].VisualCenter;

	return true;
}

void SVirtualJoystick::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (State == State_WaitForStart || State == State_CountingDownToStart)
	{
		CurrentOpacity = 0.f;
	}
	else
	{
		// lerp to the desired opacity based on whether the user is interacting with the joystick
		CurrentOpacity = FMath::Lerp(CurrentOpacity, GetBaseOpacity(), OPACITY_LERP_RATE * InDeltaTime);
	}

	// count how many controls are active
	int32 NumActiveControls = 0;

	for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++)
	{
		FControlInfo& Control = Controls[ControlIndex];

		if (Control.bNeedUpdatedCenter)
		{
			Control.ElapsedTime += InDeltaTime;
			if (Control.ElapsedTime > ActivationDelay)
			{
				Control.bNeedUpdatedCenter = false;
				CurrentOpacity = ActiveOpacity;

				if (!bPreventReCenter)
				{
					Control.VisualCenter = Control.NextCenter;
				}

				HandleTouch(ControlIndex, Control.NextCenter, AllottedGeometry.GetLocalSize());
			}
		}

		// calculate absolute positions based on geometry
		// @todo: Need to manage geometry changing!
		if (!Control.bHasBeenPositioned)
		{
			// figure out how much to scale the control sizes
			float ScaleFactor = GetScaleFactor(AllottedGeometry);

			// update all the sizes
			Control.CorrectedCenter = FVector2D(ResolveRelativePosition(Control.Center.X, AllottedGeometry.GetLocalSize().X, ScaleFactor), ResolveRelativePosition(Control.Center.Y, AllottedGeometry.GetLocalSize().Y, ScaleFactor));
			Control.VisualCenter = Control.CorrectedCenter;
			Control.CorrectedVisualSize = FVector2D(ResolveRelativePosition(Control.VisualSize.X, AllottedGeometry.GetLocalSize().X, ScaleFactor), ResolveRelativePosition(Control.VisualSize.Y, AllottedGeometry.GetLocalSize().Y, ScaleFactor));
			Control.CorrectedInteractionSize = FVector2D(ResolveRelativePosition(Control.InteractionSize.X, AllottedGeometry.GetLocalSize().X, ScaleFactor), ResolveRelativePosition(Control.InteractionSize.Y, AllottedGeometry.GetLocalSize().Y, ScaleFactor));
			Control.CorrectedThumbSize = FVector2D(ResolveRelativePosition(Control.ThumbSize.X, AllottedGeometry.GetLocalSize().X, ScaleFactor), ResolveRelativePosition(Control.ThumbSize.Y, AllottedGeometry.GetLocalSize().Y, ScaleFactor));
			Control.CorrectedInputScale = Control.InputScale; // *ScaleFactor;
			Control.bHasBeenPositioned = true;
		}

		if (Control.CapturedPointerIndex >= 0 || Control.bSendOneMoreEvent)
		{
			Control.bSendOneMoreEvent = false;

			// Get the corrected thumb offset scale (now allows ellipse instead of assuming square)
			FVector2D ThumbScaledOffset = FVector2D(Control.ThumbPosition.X * 2.0f / Control.CorrectedVisualSize.X, Control.ThumbPosition.Y * 2.0f / Control.CorrectedVisualSize.Y);
			float ThumbSquareSum = ThumbScaledOffset.X * ThumbScaledOffset.X + ThumbScaledOffset.Y * ThumbScaledOffset.Y;
			float ThumbMagnitude = FMath::Sqrt(ThumbSquareSum);
			FVector2D ThumbNormalized = FVector2D(0.f, 0.f);
			if (ThumbSquareSum > SMALL_NUMBER)
			{
				const float Scale = 1.0f / ThumbMagnitude;
				ThumbNormalized = FVector2D(ThumbScaledOffset.X * Scale, ThumbScaledOffset.Y * Scale);
			}

			// Find the scale to apply to ThumbNormalized vector to project onto unit square
			float ToSquareScale = fabs(ThumbNormalized.Y) > fabs(ThumbNormalized.X) ? FMath::Sqrt((ThumbNormalized.X * ThumbNormalized.X) / (ThumbNormalized.Y * ThumbNormalized.Y) + 1.0f)
				: ThumbNormalized.X == 0.0f ? 1.0f : FMath::Sqrt((ThumbNormalized.Y * ThumbNormalized.Y) / (ThumbNormalized.X * ThumbNormalized.X) + 1.0f);

			// Apply proportional offset corrected for projection to unit square
			FVector2D NormalizedOffset = ThumbNormalized * Control.CorrectedInputScale * ThumbMagnitude * ToSquareScale;

			// now pass the fake joystick events to the game
			const FGamepadKeyNames::Type XAxis = (Control.MainInputKey.IsValid() ? Control.MainInputKey.GetFName() : (ControlIndex == 0 ? FGamepadKeyNames::LeftAnalogX : FGamepadKeyNames::RightAnalogX));
			const FGamepadKeyNames::Type YAxis = (Control.AltInputKey.IsValid() ? Control.AltInputKey.GetFName() : (ControlIndex == 0 ? FGamepadKeyNames::LeftAnalogY : FGamepadKeyNames::RightAnalogY));

	//		UE_LOG(LogTemp, Log, TEXT("Joysticking %f,%f"), NormalizedOffset.X, -NormalizedOffset.Y);
			FSlateApplication::Get().SetAllUserFocusToGameViewport();
			FSlateApplication::Get().OnControllerAnalog(XAxis, 0, NormalizedOffset.X);
			FSlateApplication::Get().OnControllerAnalog(YAxis, 0, -NormalizedOffset.Y);
		}

		// is this active?
		if (Control.CapturedPointerIndex != -1)
		{
			NumActiveControls++;
		}
	}


	// STATE MACHINE!
	if (NumActiveControls > 0 || bPreventReCenter)
	{
		// any active control snaps the state to active immediately
		State = State_Active;
	}
	else
	{
		switch (State)
		{
			case State_WaitForStart:
				{
					State = State_CountingDownToStart;
					Countdown = StartupDelay;
				}
				break;
			case State_CountingDownToStart:
				// update the countdown
				Countdown -= InDeltaTime;
				if (Countdown <= 0.0f)
				{
					State = State_Inactive;
				}
				break;
			case State_Active:
				if (NumActiveControls == 0)
				{
					// start going to inactive
					State = State_CountingDownToInactive;
					Countdown = TimeUntilDeactive;
				}
				break;

			case State_CountingDownToInactive:
				// update the countdown
				Countdown -= InDeltaTime;
				if (Countdown <= 0.0f)
				{
					// should we start counting down to a control reset?
					if (TimeUntilReset > 0.0f)
					{
						State = State_CountingDownToReset;
						Countdown = TimeUntilReset;
					}
					else
					{
						// if not, then just go inactive
						State = State_Inactive;
					}
				}
				break;

			case State_CountingDownToReset:
				Countdown -= InDeltaTime;
				if (Countdown <= 0.0f)
				{
					// reset all the controls
					for (int32 ControlIndex = 0; ControlIndex < Controls.Num(); ControlIndex++)
					{
						Controls[ControlIndex].Reset();
					}

					// finally, go inactive
					State = State_Inactive;
				}
				break;
		}
	}
}

void SVirtualJoystick::SetJoystickVisibility(const bool bInVisible, const bool bInFade)
{
	// if we aren't fading, then just set the current opacity to desired
	if (!bInFade)
	{
		if (bInVisible)
		{
			CurrentOpacity = GetBaseOpacity();
		}
		else
		{
			CurrentOpacity = 0.f;
		}
	}

	bVisible = bInVisible;
}

void SVirtualJoystick::AlignBoxIntoScreen(FVector2D& Position, const FVector2D& Size, const FVector2D& ScreenSize)
{
	if ( Size.X > ScreenSize.X || Size.Y > ScreenSize.Y )
	{
		return;
	}

	// Align box to fit into screen
	if ( Position.X - Size.X * 0.5f < 0.f )
	{
		Position.X = Size.X * 0.5f;
	}

	if ( Position.X + Size.X * 0.5f > ScreenSize.X )
	{
		Position.X = ScreenSize.X - Size.X * 0.5f;
	}

	if ( Position.Y - Size.Y * 0.5f < 0.f )
	{
		Position.Y = Size.Y * 0.5f;
	}

	if ( Position.Y + Size.Y * 0.5f > ScreenSize.Y )
	{
		Position.Y = ScreenSize.Y - Size.Y * 0.5f;
	}
}
