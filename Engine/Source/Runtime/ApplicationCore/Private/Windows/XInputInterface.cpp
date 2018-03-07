// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XInputInterface.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "Windows/WindowsApplication.h"
#include "GenericPlatform/GenericApplication.h"

#pragma pack (push,8)
#include "Windows/AllowWindowsPlatformTypes.h"
#include <XInput.h>
#include "Windows/HideWindowsPlatformTypes.h"
#pragma pack (pop)


TSharedRef< XInputInterface > XInputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new XInputInterface( InMessageHandler ) );
}


XInputInterface::XInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
{
	for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];
		FMemory::Memzero( &ControllerState, sizeof(FControllerState) );

		ControllerState.ControllerId = ControllerIndex;
	}

	bIsGamepadAttached = false;
	bNeedsControllerStateUpdate = true;
	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	// In the engine, all controllers map to xbox controllers for consistency 
	X360ToXboxControllerMapping[0] = 0;		// A
	X360ToXboxControllerMapping[1] = 1;		// B
	X360ToXboxControllerMapping[2] = 2;		// X
	X360ToXboxControllerMapping[3] = 3;		// Y
	X360ToXboxControllerMapping[4] = 4;		// L1
	X360ToXboxControllerMapping[5] = 5;		// R1
	X360ToXboxControllerMapping[6] = 7;		// Back 
	X360ToXboxControllerMapping[7] = 6;		// Start
	X360ToXboxControllerMapping[8] = 8;		// Left thumbstick
	X360ToXboxControllerMapping[9] = 9;		// Right thumbstick
	X360ToXboxControllerMapping[10] = 10;	// L2
	X360ToXboxControllerMapping[11] = 11;	// R2
	X360ToXboxControllerMapping[12] = 12;	// Dpad up
	X360ToXboxControllerMapping[13] = 13;	// Dpad down
	X360ToXboxControllerMapping[14] = 14;	// Dpad left
	X360ToXboxControllerMapping[15] = 15;	// Dpad right
	X360ToXboxControllerMapping[16] = 16;	// Left stick up
	X360ToXboxControllerMapping[17] = 17;	// Left stick down
	X360ToXboxControllerMapping[18] = 18;	// Left stick left
	X360ToXboxControllerMapping[19] = 19;	// Left stick right
	X360ToXboxControllerMapping[20] = 20;	// Right stick up
	X360ToXboxControllerMapping[21] = 21;	// Right stick down
	X360ToXboxControllerMapping[22] = 22;	// Right stick left
	X360ToXboxControllerMapping[23] = 23;	// Right stick right

	Buttons[0] = FGamepadKeyNames::FaceButtonBottom;
	Buttons[1] = FGamepadKeyNames::FaceButtonRight;
	Buttons[2] = FGamepadKeyNames::FaceButtonLeft;
	Buttons[3] = FGamepadKeyNames::FaceButtonTop;
	Buttons[4] = FGamepadKeyNames::LeftShoulder;
	Buttons[5] = FGamepadKeyNames::RightShoulder;
	Buttons[6] = FGamepadKeyNames::SpecialRight;
	Buttons[7] = FGamepadKeyNames::SpecialLeft;
	Buttons[8] = FGamepadKeyNames::LeftThumb;
	Buttons[9] = FGamepadKeyNames::RightThumb;
	Buttons[10] = FGamepadKeyNames::LeftTriggerThreshold;
	Buttons[11] = FGamepadKeyNames::RightTriggerThreshold;
	Buttons[12] = FGamepadKeyNames::DPadUp;
	Buttons[13] = FGamepadKeyNames::DPadDown;
	Buttons[14] = FGamepadKeyNames::DPadLeft;
	Buttons[15] = FGamepadKeyNames::DPadRight;
	Buttons[16] = FGamepadKeyNames::LeftStickUp;
	Buttons[17] = FGamepadKeyNames::LeftStickDown;
	Buttons[18] = FGamepadKeyNames::LeftStickLeft;
	Buttons[19] = FGamepadKeyNames::LeftStickRight;
	Buttons[20] = FGamepadKeyNames::RightStickUp;
	Buttons[21] = FGamepadKeyNames::RightStickDown;
	Buttons[22] = FGamepadKeyNames::RightStickLeft;
	Buttons[23] = FGamepadKeyNames::RightStickRight;
}

float ShortToNormalizedFloat(int16 AxisVal)
{
	// normalize [-32768..32767] -> [-1..1]
	const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
	return float(AxisVal) / Norm;
}

void XInputInterface::SendControllerEvents()
{
	bool bWereConnected[MAX_NUM_XINPUT_CONTROLLERS];
	XINPUT_STATE XInputStates[MAX_NUM_XINPUT_CONTROLLERS];

	bIsGamepadAttached = false;
	for ( int32 ControllerIndex=0; ControllerIndex < MAX_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];

		bWereConnected[ControllerIndex] = ControllerState.bIsConnected;

		if( ControllerState.bIsConnected || bNeedsControllerStateUpdate )
		{
			XINPUT_STATE& XInputState = XInputStates[ControllerIndex];
			FMemory::Memzero( &XInputState, sizeof(XINPUT_STATE) );

			ControllerState.bIsConnected = ( XInputGetState( ControllerIndex, &XInputState ) == ERROR_SUCCESS ) ? true : false;

			if (ControllerState.bIsConnected)
			{
				bIsGamepadAttached = true;
			}
		}
	}
		
	for ( int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_XINPUT_CONTROLLERS; ++ControllerIndex )
	{
		FControllerState& ControllerState = ControllerStates[ControllerIndex];

		const bool bWasConnected = bWereConnected[ControllerIndex];

		// If the controller is connected send events or if the controller was connected send a final event with default states so that 
		// the game doesn't think that controller buttons are still held down
		if( ControllerState.bIsConnected || bWasConnected )
		{
			const XINPUT_STATE& XInputState = XInputStates[ControllerIndex];

			// If the controller is connected now but was not before, refresh the information
			if (!bWasConnected && ControllerState.bIsConnected)
			{
				FCoreDelegates::OnControllerConnectionChange.Broadcast(true, -1, ControllerState.ControllerId);
			}
			else if (bWasConnected && !ControllerState.bIsConnected)
			{
				FCoreDelegates::OnControllerConnectionChange.Broadcast(false, -1, ControllerState.ControllerId);
			}
			
			bool CurrentStates[MAX_NUM_CONTROLLER_BUTTONS] = {0};
		
			// Get the current state of all buttons
			CurrentStates[X360ToXboxControllerMapping[0]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_A);
			CurrentStates[X360ToXboxControllerMapping[1]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_B);
			CurrentStates[X360ToXboxControllerMapping[2]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_X);
			CurrentStates[X360ToXboxControllerMapping[3]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_Y);
			CurrentStates[X360ToXboxControllerMapping[4]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
			CurrentStates[X360ToXboxControllerMapping[5]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
			CurrentStates[X360ToXboxControllerMapping[6]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK);
			CurrentStates[X360ToXboxControllerMapping[7]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_START);
			CurrentStates[X360ToXboxControllerMapping[8]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
			CurrentStates[X360ToXboxControllerMapping[9]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
			CurrentStates[X360ToXboxControllerMapping[10]] = !!(XInputState.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
			CurrentStates[X360ToXboxControllerMapping[11]] = !!(XInputState.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
			CurrentStates[X360ToXboxControllerMapping[12]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP);
			CurrentStates[X360ToXboxControllerMapping[13]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
			CurrentStates[X360ToXboxControllerMapping[14]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
			CurrentStates[X360ToXboxControllerMapping[15]] = !!(XInputState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
			CurrentStates[X360ToXboxControllerMapping[16]] = !!(XInputState.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[17]] = !!(XInputState.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[18]] = !!(XInputState.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[19]] = !!(XInputState.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[20]] = !!(XInputState.Gamepad.sThumbRY > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[21]] = !!(XInputState.Gamepad.sThumbRY < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[22]] = !!(XInputState.Gamepad.sThumbRX < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			CurrentStates[X360ToXboxControllerMapping[23]] = !!(XInputState.Gamepad.sThumbRX > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

			// Send new analog data if it's different or outside the platform deadzone.
			auto OnControllerAnalog = [this, &ControllerState](const FName& GamePadKey, const auto NewAxisValue, const float NewAxisValueNormalized, auto& OldAxisValue, const auto DeadZone) {
				if (OldAxisValue != NewAxisValue || FMath::Abs((int32)NewAxisValue) > DeadZone)
				{
					MessageHandler->OnControllerAnalog(GamePadKey, ControllerState.ControllerId, NewAxisValueNormalized);
				}
				OldAxisValue = NewAxisValue;
			};

			const auto& Gamepad = XInputState.Gamepad;

			OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, Gamepad.sThumbLX, ShortToNormalizedFloat(Gamepad.sThumbLX), ControllerState.LeftXAnalog, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
			OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, Gamepad.sThumbLY, ShortToNormalizedFloat(Gamepad.sThumbLY), ControllerState.LeftYAnalog, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

			OnControllerAnalog(FGamepadKeyNames::RightAnalogX, Gamepad.sThumbRX, ShortToNormalizedFloat(Gamepad.sThumbRX), ControllerState.RightXAnalog, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
			OnControllerAnalog(FGamepadKeyNames::RightAnalogY, Gamepad.sThumbRY, ShortToNormalizedFloat(Gamepad.sThumbRY), ControllerState.RightYAnalog, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

			OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, Gamepad.bLeftTrigger, Gamepad.bLeftTrigger / 255.f, ControllerState.LeftTriggerAnalog, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
			OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, Gamepad.bRightTrigger, Gamepad.bRightTrigger / 255.f, ControllerState.RightTriggerAnalog, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

			const double CurrentTime = FPlatformTime::Seconds();

			// For each button check against the previous state and send the correct message if any
			for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ++ButtonIndex)
			{
				if (CurrentStates[ButtonIndex] != ControllerState.ButtonStates[ButtonIndex])
				{
					if( CurrentStates[ButtonIndex] )
					{
						MessageHandler->OnControllerButtonPressed( Buttons[ButtonIndex], ControllerState.ControllerId, false );
					}
					else
					{
						MessageHandler->OnControllerButtonReleased( Buttons[ButtonIndex], ControllerState.ControllerId, false );
					}

					if ( CurrentStates[ButtonIndex] != 0 )
					{
						// this button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
						ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
					}
				}
				else if ( CurrentStates[ButtonIndex] != 0 && ControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime )
				{
					MessageHandler->OnControllerButtonPressed( Buttons[ButtonIndex], ControllerState.ControllerId, true );

					// set the button's NextRepeatTime to the ButtonRepeatDelay
					ControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
				}

				// Update the state for next time
				ControllerState.ButtonStates[ButtonIndex] = CurrentStates[ButtonIndex];
			}	

			// apply force feedback
			XINPUT_VIBRATION VibrationState;
 
			const float LargeValue = (ControllerState.ForceFeedback.LeftLarge > ControllerState.ForceFeedback.RightLarge ? ControllerState.ForceFeedback.LeftLarge : ControllerState.ForceFeedback.RightLarge);
			const float SmallValue = (ControllerState.ForceFeedback.LeftSmall > ControllerState.ForceFeedback.RightSmall ? ControllerState.ForceFeedback.LeftSmall : ControllerState.ForceFeedback.RightSmall);

			VibrationState.wLeftMotorSpeed = ( ::WORD ) ( LargeValue * 65535.0f );
			VibrationState.wRightMotorSpeed = ( ::WORD ) ( SmallValue * 65535.0f );
 
			XInputSetState( ( ::DWORD ) ControllerState.ControllerId, &VibrationState );			
		}
	}

	bNeedsControllerStateUpdate = false;
}


void XInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void XInputInterface::SetChannelValue( const int32 ControllerId, const FForceFeedbackChannelType ChannelType, const float Value )
{
	if (ControllerId >= 0 && ControllerId < MAX_NUM_XINPUT_CONTROLLERS)
	{
		FControllerState& ControllerState = ControllerStates[ ControllerId ];

		if( ControllerState.bIsConnected )
		{
			switch( ChannelType )
			{
				case FForceFeedbackChannelType::LEFT_LARGE:
					ControllerState.ForceFeedback.LeftLarge = Value;
					break;

				case FForceFeedbackChannelType::LEFT_SMALL:
					ControllerState.ForceFeedback.LeftSmall = Value;
					break;

				case FForceFeedbackChannelType::RIGHT_LARGE:
					ControllerState.ForceFeedback.RightLarge = Value;
					break;

				case FForceFeedbackChannelType::RIGHT_SMALL:
					ControllerState.ForceFeedback.RightSmall = Value;
					break;
			}
		}
	}
}

void XInputInterface::SetChannelValues( const int32 ControllerId, const FForceFeedbackValues &Values )
{
	if (ControllerId >= 0 && ControllerId < MAX_NUM_XINPUT_CONTROLLERS)
	{
		FControllerState& ControllerState = ControllerStates[ ControllerId ];

		if( ControllerState.bIsConnected )
		{
			ControllerState.ForceFeedback = Values;
		}
	}
}
