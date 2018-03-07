// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	 GestureRecognizer - handles detecting when gestures happen
=============================================================================*/

#include "GestureRecognizer.h"
#include "GameFramework/PlayerInput.h"

void FGestureRecognizer::DetectGestures(const FVector (&Touches)[EKeys::NUM_TOUCH_KEYS], UPlayerInput* PlayerInput, float DeltaTime)
{
	// how many fingers currently held down?
	int32 TouchCount = 0;
	for (int32 TouchIndex = 0; TouchIndex < EKeys::NUM_TOUCH_KEYS; TouchIndex++)
	{
		if (Touches[TouchIndex].Z != 0)
		{
			TouchCount++;
		}
	}

	// Further processing is only needed if there were or are active touches.
	if (PreviousTouchCount != 0 || TouchCount != 0)
	{
		// place new anchor points
		for (int32 Index = 0; Index < ARRAY_COUNT(AnchorPoints); Index++)
		{
			if (PreviousTouchCount < Index + 1 && TouchCount >= Index + 1)
			{
				AnchorPoints[Index] = FVector2D(Touches[Index]);
			}
		}

		// handle different types of two finger gestures
		if (TouchCount >= 2)
		{
			float* CurrentAlpha = CurrentGestureValues.Find(EKeys::Gesture_Pinch);
			if (CurrentAlpha == nullptr)
			{
				// remember the starting distance
				SetAnchorDistanceSquared(AnchorPoints[0], AnchorPoints[1]);

				// alpha of 1 is initial pinch anchor distance
				CurrentGestureValues.Add(EKeys::Gesture_Pinch, 1.0f);
				HandleGesture(PlayerInput, EKeys::Gesture_Pinch, true, false);
			}
			else
			{
				// calculate current alpha
				float NewDistanceSq = (FVector2D(Touches[0]) - FVector2D(Touches[1])).SizeSquared();
				*CurrentAlpha = NewDistanceSq / AnchorDistanceSq;

				// Gestures are only processed for IE_Pressed events, so treat this like another "start"
				HandleGesture(PlayerInput, EKeys::Gesture_Pinch, true, false);
			}

			// calculate the angle of the vector between the touch points
			float NewAngle = FMath::Atan2(Touches[0].Y - Touches[1].Y, Touches[0].X - Touches[1].X);
			NewAngle = FRotator::ClampAxis(FMath::RadiansToDegrees(NewAngle));

			float* CurrentAngle = CurrentGestureValues.Find(EKeys::Gesture_Rotate);
			if (CurrentAngle == nullptr)
			{
				// save the starting angle
				StartAngle = NewAngle;

				// use 0 as the initial angle value; subsequent angles will be relative
				CurrentGestureValues.Add(EKeys::Gesture_Rotate, 0.0f);
				HandleGesture(PlayerInput, EKeys::Gesture_Rotate, true, false);
			}
			else
			{
				*CurrentAngle = NewAngle - StartAngle;

				// Gestures are only processed for IE_Pressed events, so treat this like another "start"
				HandleGesture(PlayerInput, EKeys::Gesture_Rotate, true, false);
			}
		}

		// Handle pinch and rotate release.
		if (PreviousTouchCount >= 2 && TouchCount < 2)
		{
			HandleGesture(PlayerInput, EKeys::Gesture_Pinch, false, true);
			HandleGesture(PlayerInput, EKeys::Gesture_Rotate, false, true);
		}

		if (PreviousTouchCount == 0 && TouchCount == 1)
		{
			// initialize the flick
			FlickTime = 0.0f;
		}
		else if (PreviousTouchCount == 1 && TouchCount == 1)
		{
			// remember the position for when we let go
			FlickCurrent = FVector2D(Touches[0].X, Touches[0].Y);
			FlickTime += DeltaTime;
		}
		else if (PreviousTouchCount >= 1 && TouchCount == 0)
		{
			// must be a fast flick
			if (FlickTime < 0.25f && (FlickCurrent - AnchorPoints[0]).SizeSquared() > 10000.f)
			{
				// this is the angle from +X in screen space, meaning right is 0, up is 90, left is 180, down is 270
				float Angle = FMath::Atan2(-(FlickCurrent.Y - AnchorPoints[0].Y), FlickCurrent.X - AnchorPoints[0].X);
				Angle = FRotator::ClampAxis(FMath::RadiansToDegrees(Angle));

				// flicks are one-shot, so we start and end in the same frame
				CurrentGestureValues.Add(EKeys::Gesture_Flick, Angle);
				HandleGesture(PlayerInput, EKeys::Gesture_Flick, true, true);
			}
		}
	}	

	// remember for next frame
	PreviousTouchCount = TouchCount;
}

void FGestureRecognizer::SetAnchorDistanceSquared(const FVector2D FirstPoint, const FVector2D SecontPoint)
{
	AnchorDistanceSq = (FirstPoint - SecontPoint).SizeSquared();
}

void FGestureRecognizer::HandleGesture(UPlayerInput* PlayerInput, FKey Gesture, bool bStarted, bool bEnded)
{
	float* Value = CurrentGestureValues.Find(Gesture);
	if (Value)
	{
		const EInputEvent GestureEvent = (bStarted ? IE_Pressed : bEnded ? IE_Released : IE_Repeat);
		PlayerInput->InputGesture(Gesture, GestureEvent, *Value);

		// remove it if the gesture is complete
		if (bEnded)
		{
			CurrentGestureValues.Remove(Gesture);
		}
	}
}

