// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Framework/Application/NavigationConfig.h"
#include "Types/SlateEnums.h"
#include "Input/Events.h"
#include "Misc/App.h"

FNavigationConfig::FNavigationConfig()
	: bTabNavigation(true)
	, bKeyNavigation(true)
	, bAnalogNavigation(true)
	, AnalogNavigationThreshold(0.40f)
{
	KeyEventRules.Emplace(EKeys::Left, EUINavigation::Left);
	KeyEventRules.Emplace(EKeys::Gamepad_DPad_Left, EUINavigation::Left);

	KeyEventRules.Emplace(EKeys::Right, EUINavigation::Right);
	KeyEventRules.Emplace(EKeys::Gamepad_DPad_Right, EUINavigation::Right);

	KeyEventRules.Emplace(EKeys::Up, EUINavigation::Up);
	KeyEventRules.Emplace(EKeys::Gamepad_DPad_Up, EUINavigation::Up);

	KeyEventRules.Emplace(EKeys::Down, EUINavigation::Down);
	KeyEventRules.Emplace(EKeys::Gamepad_DPad_Down, EUINavigation::Down);
}

FNavigationConfig::~FNavigationConfig()
{
}

EUINavigation FNavigationConfig::GetNavigationDirectionFromKey(const FKeyEvent& InKeyEvent) const
{
	if (const EUINavigation* Rule = KeyEventRules.Find(InKeyEvent.GetKey()))
	{
		if (bKeyNavigation)
		{
			return *Rule;
		}
	}
	else if (bTabNavigation && InKeyEvent.GetKey() == EKeys::Tab )
	{
		//@TODO: Really these uses of input should be at a lower priority, only occurring if nothing else handled them
		// For now this code prevents consuming them when some modifiers are held down, allowing some limited binding
		const bool bAllowEatingKeyEvents = !InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown() && !InKeyEvent.IsCommandDown();

		if ( bAllowEatingKeyEvents )
		{
			return ( InKeyEvent.IsShiftDown() ) ? EUINavigation::Previous : EUINavigation::Next;
		}
	}

	return EUINavigation::Invalid;
}

EUINavigation FNavigationConfig::GetNavigationDirectionFromAnalog(const FAnalogInputEvent& InAnalogEvent)
{
	if (bAnalogNavigation)
	{
		EUINavigation DesiredNavigation = GetNavigationDirectionFromAnalogInternal(InAnalogEvent);
		if (DesiredNavigation != EUINavigation::Invalid)
		{
			FAnalogNavigationState& State = AnalogNavigationState.FindOrAdd(DesiredNavigation);

			const float RepeatRate = GetRepeatRateForPressure( FMath::Abs(InAnalogEvent.GetAnalogValue()), FMath::Max(State.Repeats - 1, 0));
			if (FApp::GetCurrentTime() - State.LastNavigationTime > RepeatRate)
			{
				State.LastNavigationTime = FApp::GetCurrentTime();
				State.Repeats++;
				return DesiredNavigation;
			}
		}
	}

	return EUINavigation::Invalid;
}

EUINavigation FNavigationConfig::GetNavigationDirectionFromAnalogInternal(const FAnalogInputEvent& InAnalogEvent)
{
	if (bAnalogNavigation)
	{
		if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftX)
		{
			if (InAnalogEvent.GetAnalogValue() < -AnalogNavigationThreshold)
			{
				return EUINavigation::Left;
			}
			else if (InAnalogEvent.GetAnalogValue() > AnalogNavigationThreshold)
			{
				return EUINavigation::Right;
			}
			else
			{
				AnalogNavigationState.Add(EUINavigation::Left, FAnalogNavigationState());
				AnalogNavigationState.Add(EUINavigation::Right, FAnalogNavigationState());
			}
		}
		else if (InAnalogEvent.GetKey() == EKeys::Gamepad_LeftY)
		{
			if (InAnalogEvent.GetAnalogValue() > AnalogNavigationThreshold)
			{
				return EUINavigation::Up;
			}
			else if (InAnalogEvent.GetAnalogValue() < -AnalogNavigationThreshold)
			{
				return EUINavigation::Down;
			}
			else
			{
				AnalogNavigationState.Add(EUINavigation::Up, FAnalogNavigationState());
				AnalogNavigationState.Add(EUINavigation::Down, FAnalogNavigationState());
			}
		}
	}

	return EUINavigation::Invalid;
}

float FNavigationConfig::GetRepeatRateForPressure(float InPressure, int32 InRepeats) const
{
	float RepeatRate = (InRepeats == 0) ? 0.3f : 0.2f;
	if (InPressure > 0.90f)
	{
		return RepeatRate * 0.35f;
	}

	return RepeatRate;
}