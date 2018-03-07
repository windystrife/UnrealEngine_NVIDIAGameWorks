// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidInputInterface.h"
//#include "AndroidInputDeviceMappings.h"
#include "IInputDevice.h"
#include "GenericApplicationMessageHandler.h"
#include "HAL/ThreadingBase.h"
#include <android/input.h>
#include "Misc/CallbackDevice.h"
#include "HAL/PlatformTime.h"
#include "IConsoleManager.h"


TArray<TouchInput> FAndroidInputInterface::TouchInputStack = TArray<TouchInput>();
FCriticalSection FAndroidInputInterface::TouchInputCriticalSection;

FAndroidGamepadDeviceMapping FAndroidInputInterface::DeviceMapping[MAX_NUM_CONTROLLERS];

bool FAndroidInputInterface::VibeIsOn;
FForceFeedbackValues FAndroidInputInterface::VibeValues;

FAndroidControllerData FAndroidInputInterface::OldControllerData[MAX_NUM_CONTROLLERS];
FAndroidControllerData FAndroidInputInterface::NewControllerData[MAX_NUM_CONTROLLERS];

FGamepadKeyNames::Type FAndroidInputInterface::ButtonMapping[MAX_NUM_CONTROLLER_BUTTONS];
float FAndroidInputInterface::InitialButtonRepeatDelay;
float FAndroidInputInterface::ButtonRepeatDelay;

FDeferredAndroidMessage FAndroidInputInterface::DeferredMessages[MAX_DEFERRED_MESSAGE_QUEUE_SIZE];
int32 FAndroidInputInterface::DeferredMessageQueueLastEntryIndex = 0;
int32 FAndroidInputInterface::DeferredMessageQueueDroppedCount   = 0;

TArray<FAndroidInputInterface::MotionData> FAndroidInputInterface::MotionDataStack
	= TArray<FAndroidInputInterface::MotionData>();

int32 GAndroidOldXBoxWirelessFirmware = 0;
static FAutoConsoleVariableRef CVarAndroidOldXBoxWirelessFirmware(
	TEXT("Android.OldXBoxWirelessFirmware"),
	GAndroidOldXBoxWirelessFirmware,
	TEXT("Determines how XBox Wireless controller mapping is handled. 0 assumes new firmware, 1 will use old firmware mapping (Default: 0)"),
	ECVF_Default);

TSharedRef< FAndroidInputInterface > FAndroidInputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new FAndroidInputInterface( InMessageHandler ) );
}

FAndroidInputInterface::~FAndroidInputInterface()
{
}

namespace AndroidKeyNames
{
	const FGamepadKeyNames::Type Android_Back("Android_Back");
	const FGamepadKeyNames::Type Android_Menu("Android_Menu");
}

FAndroidInputInterface::FAndroidInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
{
	ButtonMapping[0] = FGamepadKeyNames::FaceButtonBottom;
	ButtonMapping[1] = FGamepadKeyNames::FaceButtonRight;
	ButtonMapping[2] = FGamepadKeyNames::FaceButtonLeft;
	ButtonMapping[3] = FGamepadKeyNames::FaceButtonTop;
	ButtonMapping[4] = FGamepadKeyNames::LeftShoulder;
	ButtonMapping[5] = FGamepadKeyNames::RightShoulder;
	ButtonMapping[6] = FGamepadKeyNames::SpecialRight;
	ButtonMapping[7] = FGamepadKeyNames::SpecialLeft;
	ButtonMapping[8] = FGamepadKeyNames::LeftThumb;
	ButtonMapping[9] = FGamepadKeyNames::RightThumb;
	ButtonMapping[10] = FGamepadKeyNames::LeftTriggerThreshold;
	ButtonMapping[11] = FGamepadKeyNames::RightTriggerThreshold;
	ButtonMapping[12] = FGamepadKeyNames::DPadUp;
	ButtonMapping[13] = FGamepadKeyNames::DPadDown;
	ButtonMapping[14] = FGamepadKeyNames::DPadLeft;
	ButtonMapping[15] = FGamepadKeyNames::DPadRight;
	ButtonMapping[16] = AndroidKeyNames::Android_Back;  // Technically just an alias for SpecialLeft
	ButtonMapping[17] = AndroidKeyNames::Android_Menu;  // Technically just an alias for SpecialRight

	InitialButtonRepeatDelay = 0.2f;
	ButtonRepeatDelay = 0.1f;

	VibeIsOn = false;

	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		DeviceMapping[DeviceIndex].DeviceInfo.DeviceId = 0;
		DeviceMapping[DeviceIndex].DeviceState = MappingState::Unassigned;
	}
}

void FAndroidInputInterface::ResetGamepadAssignments()
{
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		if (DeviceMapping[DeviceIndex].DeviceState == MappingState::Valid)
		{
			FCoreDelegates::OnControllerConnectionChange.Broadcast(false, -1, DeviceIndex);
		}

		DeviceMapping[DeviceIndex].DeviceInfo.DeviceId = 0;
		DeviceMapping[DeviceIndex].DeviceState = MappingState::Unassigned;
	}
}

void FAndroidInputInterface::ResetGamepadAssignmentToController(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
		return;

	if (DeviceMapping[ControllerId].DeviceState == MappingState::Valid)
	{
		FCoreDelegates::OnControllerConnectionChange.Broadcast(false, -1, ControllerId);
	}

	DeviceMapping[ControllerId].DeviceInfo.DeviceId = 0;
	DeviceMapping[ControllerId].DeviceState = MappingState::Unassigned;
}

bool FAndroidInputInterface::IsControllerAssignedToGamepad(int32 ControllerId)
{
	if (ControllerId < 0 || ControllerId >= MAX_NUM_CONTROLLERS)
		return false;

	return (DeviceMapping[ControllerId].DeviceState == MappingState::Valid);
}

void FAndroidInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
}

void FAndroidInputInterface::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}

extern bool AndroidThunkCpp_GetInputDeviceInfo(int32 deviceId, FAndroidInputDeviceInfo &results);

void FAndroidInputInterface::Tick(float DeltaTime)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(DeltaTime);
	}
}

void FAndroidInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetChannelValue(ControllerId, ChannelType, Value);
	}

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	// Save a copy of the value for future comparison
	switch (ChannelType)
	{
		case FForceFeedbackChannelType::LEFT_LARGE:
			VibeValues.LeftLarge = Value;
			break;

		case FForceFeedbackChannelType::LEFT_SMALL:
			VibeValues.LeftSmall = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_LARGE:
			VibeValues.RightLarge = Value;
			break;

		case FForceFeedbackChannelType::RIGHT_SMALL:
			VibeValues.RightSmall = Value;
			break;

		default:
			// Unknown channel, so ignore it
			break;
	}

	// Update with the latest values (wait for SendControllerEvents later?)
	UpdateVibeMotors();
}

void FAndroidInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetChannelValues(ControllerId, Values);
	}

	// Note: only one motor on Android at the moment, but remember all the settings
	// update will look at combination of all values to pick state

	VibeValues = Values;

	// Update with the latest values (wait for SendControllerEvents later?)
	UpdateVibeMotors();
}

extern bool AndroidThunkCpp_IsGamepadAttached();

bool FAndroidInputInterface::IsGamepadAttached() const
{
	// Check for gamepads that have already been validated
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		FAndroidGamepadDeviceMapping& CurrentDevice = DeviceMapping[DeviceIndex];

		if (CurrentDevice.DeviceState == MappingState::Valid)
		{
			return true;
		}
	}

	for (auto DeviceIt = ExternalInputDevices.CreateConstIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			return true;
		}
	}

	//if all of this fails, do a check on the Java side to see if the gamepad is attached
	return AndroidThunkCpp_IsGamepadAttached();
}

extern void AndroidThunkCpp_Vibrate(int32 Duration);

void FAndroidInputInterface::UpdateVibeMotors()
{
	// Use largest vibration state as value
	float MaxLeft = VibeValues.LeftLarge > VibeValues.LeftSmall ? VibeValues.LeftLarge : VibeValues.LeftSmall;
	float MaxRight = VibeValues.RightLarge > VibeValues.RightSmall ? VibeValues.RightLarge : VibeValues.RightSmall;
	float Value = MaxLeft > MaxRight ? MaxLeft : MaxRight;

	if (VibeIsOn)
	{
		// Turn it off if below threshold
		if (Value < 0.3f)
		{
			AndroidThunkCpp_Vibrate(0);
			VibeIsOn = false;
		}
	}
	else {
		if (Value >= 0.3f)
		{
			// Turn it on for 10 seconds (or until below threshold)
			AndroidThunkCpp_Vibrate(10000);
			VibeIsOn = true;
		}
	}
}

static uint32 CharMap[] =
{
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    L'0',
    L'1',
    L'2',
    L'3',
    L'4',
    L'5',
    L'6',
    L'7',
    L'8',
    L'9',
    L'*',
	L'#',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    L'a',
    L'b',
    L'c',
    L'd',
    L'e',
    L'f',
    L'g',
    L'h',
    L'i',
    L'j',
    L'k',
    L'l',
    L'm',
    L'n',
    L'o',
    L'p',
    L'q',
    L'r',
    L's',
    L't',
    L'u',
    L'v',
    L'w',
    L'x',
    L'y',
    L'z',
    L',',
    L'.',
    0,
    0,
    0,
    0,
    L'\t',
    L' ',
    0,
    0,
    0,
    L'\n',
    L'\b',
    L'`',
    L'-',
    L'=',
    L'[',
    L']',
    L'\\',
    L';',
    L'\'',
    L'/',
    L'@',
    0,
    0,
    0,   // *Camera* focus
    L'+',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    L'0',
    L'1',
    L'2',
    L'3',
    L'4',
    L'5',
    L'6',
    L'7',
    L'8',
    L'9',
    L'/',
    L'*',
    L'-',
    L'+',
    L'.',
    L',',
    L'\n',
    L'=',
    L'(',
    L')',
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};

static uint32 CharMapShift[] =
{
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L')',
	L'!',
	L'@',
	L'#',
	L'$',
	L'%',
	L'^',
	L'&',
	L'*',
	L'(',
	L'*',
	L'#',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'A',
	L'B',
	L'C',
	L'D',
	L'E',
	L'F',
	L'G',
	L'H',
	L'I',
	L'J',
	L'K',
	L'L',
	L'M',
	L'N',
	L'O',
	L'P',
	L'Q',
	L'R',
	L'S',
	L'T',
	L'U',
	L'V',
	L'W',
	L'X',
	L'Y',
	L'Z',
	L'<',
	L'>',
	0,
	0,
	0,
	0,
	L'\t',
	L' ',
	0,
	0,
	0,
	L'\n',
	L'\b',
	L'~',
	L'_',
	L'+',
	L'{',
	L'}',
	L'|',
	L':',
	L'\"',
	L'?',
	L'@',
	0,
	0,
	0,   // *Camera* focus
	L'+',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	L'0',
	L'1',
	L'2',
	L'3',
	L'4',
	L'5',
	L'6',
	L'7',
	L'8',
	L'9',
	L'/',
	L'*',
	L'-',
	L'+',
	L'.',
	L',',
	L'\n',
	L'=',
	L'(',
	L')',
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

void FAndroidInputInterface::SendControllerEvents()
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// Check for gamepads needing validation
	for (int32 DeviceIndex = 0; DeviceIndex < MAX_NUM_CONTROLLERS; DeviceIndex++)
	{
		FAndroidGamepadDeviceMapping& CurrentDevice = DeviceMapping[DeviceIndex];

		if (CurrentDevice.DeviceState == MappingState::ToValidate)
		{
			// Query for the device type from Java side
			if (AndroidThunkCpp_GetInputDeviceInfo(CurrentDevice.DeviceInfo.DeviceId, CurrentDevice.DeviceInfo))
			{
				// It is possible this is actually a previously assigned controller if it disconnected and reconnected (device ID can change)
				int32 FoundMatch = -1;
				for (int32 DeviceScan = 0; DeviceScan < MAX_NUM_CONTROLLERS; DeviceScan++)
				{
					if (DeviceMapping[DeviceScan].DeviceState != MappingState::Valid)
						continue;

					if (DeviceMapping[DeviceScan].DeviceInfo.Descriptor.Equals(CurrentDevice.DeviceInfo.Descriptor))
					{
						FoundMatch = DeviceScan;
						break;
					}
				}

				// Deal with new controller
				if (FoundMatch == -1)
				{
					CurrentDevice.DeviceState = MappingState::Valid;

					// Generic mappings
					CurrentDevice.ButtonRemapping = ButtonRemapType::Normal;
					CurrentDevice.LTAnalogRangeMinimum = 0.0f;
					CurrentDevice.RTAnalogRangeMinimum = 0.0f;
					CurrentDevice.bSupportsHat = false;
					CurrentDevice.bMapL1R1ToTriggers = false;
					CurrentDevice.bMapZRZToTriggers = false;
					CurrentDevice.bRightStickZRZ = true;
					CurrentDevice.bRightStickRXRY = false;

					// Use device name to decide on mapping scheme
					if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Amazon")))
					{
						if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Amazon Fire Game Controller")))
						{
							CurrentDevice.bSupportsHat = true;
						}
						else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Amazon Fire TV Remote")))
						{
						}
						else
						{
						}
					}
					else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("NVIDIA Corporation NVIDIA Controller")))
					{
						CurrentDevice.bSupportsHat = true;
					}
					else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Samsung Game Pad EI-GP20")))
					{
						CurrentDevice.bSupportsHat = true;
						CurrentDevice.bMapL1R1ToTriggers = true;
						CurrentDevice.bRightStickZRZ = false;
						CurrentDevice.bRightStickRXRY = true;
					}
					else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Mad Catz C.T.R.L.R")))
					{
						CurrentDevice.bSupportsHat = true;
					}
					else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("Xbox Wireless Controller")))
					{
						CurrentDevice.bSupportsHat = true;

						if (GAndroidOldXBoxWirelessFirmware == 1)
						{
							// Apply mappings for older firmware before 3.1.1221.0
							CurrentDevice.ButtonRemapping = ButtonRemapType::XBoxWireless;
							CurrentDevice.bMapL1R1ToTriggers = false;
							CurrentDevice.bMapZRZToTriggers = true;
							CurrentDevice.bRightStickZRZ = false;
							CurrentDevice.bRightStickRXRY = true;
						}
					}
					else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("SteelSeries Stratus XL")))
					{
						CurrentDevice.bSupportsHat = true;

						// For some reason the left trigger is at 0.5 when at rest so we have to adjust for that.
						CurrentDevice.LTAnalogRangeMinimum = 0.5f;
					}

					// The PS4 controller name is just "Wireless Controller" which is hardly unique so we can't trust a name
					// comparison. Instead we check the product and vendor IDs to ensure it's the correct one.
					else if (CurrentDevice.DeviceInfo.Name.StartsWith(TEXT("PS4 Wireless Controller")))
					{
						CurrentDevice.ButtonRemapping = ButtonRemapType::PS4;
						CurrentDevice.bSupportsHat = true;
						CurrentDevice.bRightStickZRZ = true;
					}

					FCoreDelegates::OnControllerConnectionChange.Broadcast(true, -1, DeviceIndex);

					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Assigned new gamepad controller %d: DeviceId=%d, ControllerId=%d, DeviceName=%s, Descriptor=%s"),
						DeviceIndex, CurrentDevice.DeviceInfo.DeviceId, CurrentDevice.DeviceInfo.ControllerId, *CurrentDevice.DeviceInfo.Name, *CurrentDevice.DeviceInfo.Descriptor);
					continue;
				}
				else
				{
					// Already assigned this controller so reconnect it
					DeviceMapping[FoundMatch].DeviceInfo.DeviceId = CurrentDevice.DeviceInfo.DeviceId;
					CurrentDevice.DeviceInfo.DeviceId = 0;
					CurrentDevice.DeviceState = MappingState::Unassigned;

					// Transfer state back to this controller
					NewControllerData[FoundMatch] = NewControllerData[DeviceIndex];
					NewControllerData[FoundMatch].DeviceId = FoundMatch;
					OldControllerData[FoundMatch].DeviceId = FoundMatch;

					//@TODO: uncomment this line in the future when disconnects are detected
					//FCoreDelegates::OnControllerConnectionChange.Broadcast(true, -1, FoundMatch);

					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Reconnected gamepad controller %d: DeviceId=%d, ControllerId=%d, DeviceName=%s, Descriptor=%s"),
						FoundMatch, DeviceMapping[FoundMatch].DeviceInfo.DeviceId, CurrentDevice.DeviceInfo.ControllerId, *CurrentDevice.DeviceInfo.Name, *CurrentDevice.DeviceInfo.Descriptor);
				}
			}
			else
			{
				// Did not find match so clear the assignment
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Failed to assign gamepad controller %d: DeviceId=%d"), DeviceIndex, CurrentDevice.DeviceInfo.DeviceId);
			}

		}
	}

	for(int i = 0; i < FAndroidInputInterface::TouchInputStack.Num(); ++i)
	{
		TouchInput Touch = FAndroidInputInterface::TouchInputStack[i];
		int32 ControllerId = FindExistingDevice(Touch.DeviceId);
		ControllerId = (ControllerId == -1) ? 0 : ControllerId;

		// send input to handler
		switch ( Touch.Type )
		{
		case TouchBegan:
			MessageHandler->OnTouchStarted(nullptr, Touch.Position, Touch.Handle, ControllerId);
			break;
		case TouchEnded:
			MessageHandler->OnTouchEnded(Touch.Position, Touch.Handle, ControllerId);
			break;
		case TouchMoved:
			MessageHandler->OnTouchMoved(Touch.Position, Touch.Handle, ControllerId);
			break;
		}
	}

	// Extract differences in new and old states and send messages
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		// Skip unassigned or invalid controllers (treat first one as special case)
		if (ControllerIndex > 0 && (DeviceMapping[ControllerIndex].DeviceState !=  MappingState::Valid))
		{
			continue;
		}

		FAndroidControllerData& OldControllerState = OldControllerData[ControllerIndex];
		FAndroidControllerData& NewControllerState = NewControllerData[ControllerIndex];

		if (NewControllerState.LXAnalog != OldControllerState.LXAnalog)
		{
			MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogX, NewControllerState.DeviceId, NewControllerState.LXAnalog);
		}
		if (NewControllerState.LYAnalog != OldControllerState.LYAnalog)
		{
			//LOGD("    Sending updated LeftAnalogY value of %f", NewControllerState.LYAnalog);
			MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftAnalogY, NewControllerState.DeviceId, NewControllerState.LYAnalog);
		}
		if (NewControllerState.RXAnalog != OldControllerState.RXAnalog)
		{
			//LOGD("    Sending updated RightAnalogX value of %f", NewControllerState.RXAnalog);
			MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogX, NewControllerState.DeviceId, NewControllerState.RXAnalog);
		}
		if (NewControllerState.RYAnalog != OldControllerState.RYAnalog)
		{
			//LOGD("    Sending updated RightAnalogY value of %f", NewControllerState.RYAnalog);
			MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightAnalogY, NewControllerState.DeviceId, NewControllerState.RYAnalog);
		}
		if (NewControllerState.LTAnalog != OldControllerState.LTAnalog)
		{
			//LOGD("    Sending updated LeftTriggerAnalog value of %f", NewControllerState.LTAnalog);
			MessageHandler->OnControllerAnalog(FGamepadKeyNames::LeftTriggerAnalog, NewControllerState.DeviceId, NewControllerState.LTAnalog);

			// Handle the trigger theshold "virtual" button state
			//check(ButtonMapping[10] == FGamepadKeyNames::LeftTriggerThreshold);
			NewControllerState.ButtonStates[10] = NewControllerState.LTAnalog >= 0.1f;
		}
		if (NewControllerState.RTAnalog != OldControllerState.RTAnalog)
		{
			//LOGD("    Sending updated RightTriggerAnalog value of %f", NewControllerState.RTAnalog);
			MessageHandler->OnControllerAnalog(FGamepadKeyNames::RightTriggerAnalog, NewControllerState.DeviceId, NewControllerState.RTAnalog);

			// Handle the trigger theshold "virtual" button state
			//check(ButtonMapping[11] == FGamepadKeyNames::RightTriggerThreshold);
			NewControllerState.ButtonStates[11] = NewControllerState.RTAnalog >= 0.1f;
		}

		const double CurrentTime = FPlatformTime::Seconds();

		// For each button check against the previous state and send the correct message if any
		for (int32 ButtonIndex = 0; ButtonIndex < MAX_NUM_CONTROLLER_BUTTONS; ButtonIndex++)
		{
			if (NewControllerState.ButtonStates[ButtonIndex] != OldControllerState.ButtonStates[ButtonIndex])
			{
				if (NewControllerState.ButtonStates[ButtonIndex])
				{
					//LOGD("    Sending joystick button down %d (first)", ButtonMapping[ButtonIndex]);
					MessageHandler->OnControllerButtonPressed(ButtonMapping[ButtonIndex], NewControllerState.DeviceId, false);
				}
				else
				{
					//LOGD("    Sending joystick button up %d", ButtonMapping[ButtonIndex]);
					MessageHandler->OnControllerButtonReleased(ButtonMapping[ButtonIndex], NewControllerState.DeviceId, false);
				}

				if (NewControllerState.ButtonStates[ButtonIndex])
				{
					// This button was pressed - set the button's NextRepeatTime to the InitialButtonRepeatDelay
					NewControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + InitialButtonRepeatDelay;
				}
			}
			else if (NewControllerState.ButtonStates[ButtonIndex] && NewControllerState.NextRepeatTime[ButtonIndex] <= CurrentTime)
			{
				// Send button repeat events
				MessageHandler->OnControllerButtonPressed(ButtonMapping[ButtonIndex], NewControllerState.DeviceId, true);

				// Set the button's NextRepeatTime to the ButtonRepeatDelay
				NewControllerState.NextRepeatTime[ButtonIndex] = CurrentTime + ButtonRepeatDelay;
			}
		}

		// Update the state for next time
		OldControllerState = NewControllerState;
	}

	for (int i = 0; i < FAndroidInputInterface::MotionDataStack.Num(); ++i)
	{
		MotionData motion_data = FAndroidInputInterface::MotionDataStack[i];

		MessageHandler->OnMotionDetected(
			motion_data.Tilt, motion_data.RotationRate,
			motion_data.Gravity, motion_data.Acceleration,
			0);
	}

	for (int32 MessageIndex = 0; MessageIndex < FMath::Min(DeferredMessageQueueLastEntryIndex, MAX_DEFERRED_MESSAGE_QUEUE_SIZE); ++MessageIndex)
	{
		const FDeferredAndroidMessage& DeferredMessage = DeferredMessages[MessageIndex];
		const int32 Char = DeferredMessage.KeyEventData.modifier & AMETA_SHIFT_ON ? CharMapShift[DeferredMessage.KeyEventData.keyId] : CharMap[DeferredMessage.KeyEventData.keyId];
		
		switch (DeferredMessage.messageType)
		{

			case MessageType_KeyDown:

				MessageHandler->OnKeyDown(DeferredMessage.KeyEventData.keyId, Char, DeferredMessage.KeyEventData.isRepeat);
				MessageHandler->OnKeyChar(Char,  DeferredMessage.KeyEventData.isRepeat);
				break;

			case MessageType_KeyUp:

				MessageHandler->OnKeyUp(DeferredMessage.KeyEventData.keyId, Char, false);
				break;
		} 
	}

	if (DeferredMessageQueueDroppedCount)
	{
		//should warn that messages got dropped, which message queue?
		DeferredMessageQueueDroppedCount = 0;
	}

	DeferredMessageQueueLastEntryIndex = 0;

	FAndroidInputInterface::TouchInputStack.Empty(0);

	FAndroidInputInterface::MotionDataStack.Empty();

	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SendControllerEvents();
	}
}

void FAndroidInputInterface::QueueTouchInput(const TArray<TouchInput>& InTouchEvents)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::TouchInputStack.Append(InTouchEvents);
}

int32 FAndroidInputInterface::FindExistingDevice(int32 deviceId)
{
	// Treat non-positive devices ids special
	if (deviceId < 1)
		return -1;

	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		if (DeviceMapping[ControllerIndex].DeviceInfo.DeviceId == deviceId && DeviceMapping[ControllerIndex].DeviceState == MappingState::Valid)
		{
			return ControllerIndex;
		}
	}

	// Did not find it
	return -1;
}

int32 FAndroidInputInterface::GetControllerIndex(int32 deviceId)
{
	// Treat non-positive device ids special (always controller 0)
	if (deviceId < 1)
		return 0;

	// Look for this deviceId in controllers discovered
	int32 UnassignedIndex = -1;
	for (int32 ControllerIndex = 0; ControllerIndex < MAX_NUM_CONTROLLERS; ControllerIndex++)
	{
		if (DeviceMapping[ControllerIndex].DeviceState == MappingState::Unassigned)
		{
			if (UnassignedIndex == -1)
				UnassignedIndex = ControllerIndex;
			continue;
		}

		if (DeviceMapping[ControllerIndex].DeviceInfo.DeviceId == deviceId)
		{
			return ControllerIndex;
		}
	}

	// Haven't seen this one before, make sure there is room for a new one
	if (UnassignedIndex == -1)
		return -1;

	// Register it
	DeviceMapping[UnassignedIndex].DeviceInfo.DeviceId = deviceId;
	OldControllerData[UnassignedIndex].DeviceId = UnassignedIndex;
	NewControllerData[UnassignedIndex].DeviceId = UnassignedIndex;

	// Mark it for validation later
	DeviceMapping[UnassignedIndex].DeviceState = MappingState::ToValidate;

	return UnassignedIndex;
}

void FAndroidInputInterface::JoystickAxisEvent(int32 deviceId, int32 axisId, float axisValue)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// Get the controller index matching deviceId (if there is one)
	deviceId = GetControllerIndex(deviceId);
	if (deviceId == -1)
		return;

	// Left trigger may need range correction
	if (axisId == AMOTION_EVENT_AXIS_LTRIGGER && DeviceMapping[deviceId].LTAnalogRangeMinimum != 0.0f)
	{
		const float AdjustMin = DeviceMapping[deviceId].LTAnalogRangeMinimum;
		const float AdjustMax = 1.0f - AdjustMin;
		NewControllerData[deviceId].LTAnalog = FMath::Clamp(axisValue - AdjustMin, 0.0f, AdjustMax) / AdjustMax;
		return;
	}

	// Right trigger may need range correction
	if (axisId == AMOTION_EVENT_AXIS_RTRIGGER && DeviceMapping[deviceId].RTAnalogRangeMinimum != 0.0f)
	{
		const float AdjustMin = DeviceMapping[deviceId].RTAnalogRangeMinimum;
		const float AdjustMax = 1.0f - AdjustMin;
		NewControllerData[deviceId].RTAnalog = FMath::Clamp(axisValue - AdjustMin, 0.0f, AdjustMax) / AdjustMax;
		return;
	}

	// Deal with left stick and triggers (generic)
	switch (axisId)
	{
		case AMOTION_EVENT_AXIS_X:			NewControllerData[deviceId].LXAnalog =  axisValue; return;
		case AMOTION_EVENT_AXIS_Y:			NewControllerData[deviceId].LYAnalog = -axisValue; return;
		case AMOTION_EVENT_AXIS_LTRIGGER:	NewControllerData[deviceId].LTAnalog =  axisValue; return;
		case AMOTION_EVENT_AXIS_RTRIGGER:	NewControllerData[deviceId].RTAnalog =  axisValue; return;
	}

	// Deal with right stick Z/RZ events
	if (DeviceMapping[deviceId].bRightStickZRZ)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_Z:		NewControllerData[deviceId].RXAnalog =  axisValue; return;
			case AMOTION_EVENT_AXIS_RZ:		NewControllerData[deviceId].RYAnalog = -axisValue; return;
		}
	}

	// Deal with right stick RX/RY events
	if (DeviceMapping[deviceId].bRightStickRXRY)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_RX:		NewControllerData[deviceId].RXAnalog =  axisValue; return;
			case AMOTION_EVENT_AXIS_RY:		NewControllerData[deviceId].RYAnalog = -axisValue; return;
		}
	}

	// Deal with Z/RZ mapping to triggers
	if (DeviceMapping[deviceId].bMapZRZToTriggers)
	{
		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_Z:		NewControllerData[deviceId].LTAnalog =  axisValue; return;
			case AMOTION_EVENT_AXIS_RZ:		NewControllerData[deviceId].RTAnalog =  axisValue; return;
		}
	}

	// Deal with hat (convert to DPAD buttons)
	if (DeviceMapping[deviceId].bSupportsHat)
	{
		// Apply a small dead zone to hats
		const float deadZone = 0.2f;

		switch (axisId)
		{
			case AMOTION_EVENT_AXIS_HAT_X:
				// AMOTION_EVENT_AXIS_HAT_X translates to KEYCODE_DPAD_LEFT and AKEYCODE_DPAD_RIGHT
				if (axisValue > deadZone)
				{
					NewControllerData[deviceId].ButtonStates[14] = false;	// DPAD_LEFT released
					NewControllerData[deviceId].ButtonStates[15] = true;	// DPAD_RIGHT pressed
				}
				else if (axisValue < -deadZone)
				{
					NewControllerData[deviceId].ButtonStates[14] = true;	// DPAD_LEFT pressed
					NewControllerData[deviceId].ButtonStates[15] = false;	// DPAD_RIGHT released
				}
				else
				{
					NewControllerData[deviceId].ButtonStates[14] = false;	// DPAD_LEFT released
					NewControllerData[deviceId].ButtonStates[15] = false;	// DPAD_RIGHT released
				}
				return;
			case AMOTION_EVENT_AXIS_HAT_Y:
				// AMOTION_EVENT_AXIS_HAT_Y translates to KEYCODE_DPAD_UP and AKEYCODE_DPAD_DOWN
				if (axisValue > deadZone)
				{
					NewControllerData[deviceId].ButtonStates[12] = false;	// DPAD_UP released
					NewControllerData[deviceId].ButtonStates[13] = true;	// DPAD_DOWN pressed
				}
				else if (axisValue < -deadZone)
				{
					NewControllerData[deviceId].ButtonStates[12] = true;	// DPAD_UP pressed
					NewControllerData[deviceId].ButtonStates[13] = false;	// DPAD_DOWN released
				}
				else
				{
					NewControllerData[deviceId].ButtonStates[12] = false;	// DPAD_UP released
					NewControllerData[deviceId].ButtonStates[13] = false;	// DPAD_DOWN released
				}
				return;
		}
	}
}

void FAndroidInputInterface::JoystickButtonEvent(int32 deviceId, int32 buttonId, bool buttonDown)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	// Get the controller index matching deviceId (if there is one)
	deviceId = GetControllerIndex(deviceId);
	if (deviceId == -1)
		return;

	// Deal with button remapping
	switch (DeviceMapping[deviceId].ButtonRemapping)
	{
		case ButtonRemapType::Normal:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_A:
				case AKEYCODE_DPAD_CENTER:   NewControllerData[deviceId].ButtonStates[0] = buttonDown; break;
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break;
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break;
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break;
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[4] = buttonDown;
											 if (DeviceMapping[deviceId].bMapL1R1ToTriggers)
											 {
												 NewControllerData[deviceId].ButtonStates[10] = buttonDown;
											 }
											 break;
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[5] = buttonDown;
											 if (DeviceMapping[deviceId].bMapL1R1ToTriggers)
											 {
												 NewControllerData[deviceId].ButtonStates[11] = buttonDown;
											 }
											 break;
				case AKEYCODE_BUTTON_START:
				case AKEYCODE_MENU:          NewControllerData[deviceId].ButtonStates[6] = buttonDown; NewControllerData[deviceId].ButtonStates[17] = buttonDown;  break;
				case AKEYCODE_BUTTON_SELECT:
				case AKEYCODE_BACK:          NewControllerData[deviceId].ButtonStates[7] = buttonDown; NewControllerData[deviceId].ButtonStates[16] = buttonDown;  break;
				case AKEYCODE_BUTTON_THUMBL: NewControllerData[deviceId].ButtonStates[8] = buttonDown; break;
				case AKEYCODE_BUTTON_THUMBR: NewControllerData[deviceId].ButtonStates[9] = buttonDown; break;
				case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[10] = buttonDown; break;
				case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[11] = buttonDown; break;
				case AKEYCODE_DPAD_UP:       NewControllerData[deviceId].ButtonStates[12] = buttonDown; break;
				case AKEYCODE_DPAD_DOWN:     NewControllerData[deviceId].ButtonStates[13] = buttonDown; break;
				case AKEYCODE_DPAD_LEFT:     NewControllerData[deviceId].ButtonStates[14] = buttonDown; break;
				case AKEYCODE_DPAD_RIGHT:    NewControllerData[deviceId].ButtonStates[15] = buttonDown; break;
			}
			break;

		case ButtonRemapType::XBoxWireless:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_A:      NewControllerData[deviceId].ButtonStates[0] = buttonDown; break; // A
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break; // B
				case AKEYCODE_BUTTON_C:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break; // X
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break; // Y
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[4] = buttonDown; break; // L1
				case AKEYCODE_BUTTON_Z:      NewControllerData[deviceId].ButtonStates[5] = buttonDown; break; // R1
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[6] = buttonDown; NewControllerData[deviceId].ButtonStates[17] = buttonDown;  break; // Menu
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[7] = buttonDown; NewControllerData[deviceId].ButtonStates[16] = buttonDown;  break; // View
				case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[8] = buttonDown; break; // ThumbL
				case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[9] = buttonDown; break; // ThumbR
			}
			break;

		case ButtonRemapType::PS4:
			switch (buttonId)
			{
				case AKEYCODE_BUTTON_B:      NewControllerData[deviceId].ButtonStates[0] = buttonDown; break; // Cross
				case AKEYCODE_BUTTON_C:      NewControllerData[deviceId].ButtonStates[1] = buttonDown; break; // Circle
				case AKEYCODE_BUTTON_A:      NewControllerData[deviceId].ButtonStates[2] = buttonDown; break; // Square
				case AKEYCODE_BUTTON_X:      NewControllerData[deviceId].ButtonStates[3] = buttonDown; break; // Triangle
				case AKEYCODE_BUTTON_Y:      NewControllerData[deviceId].ButtonStates[4] = buttonDown; break; // L1
				case AKEYCODE_BUTTON_Z:      NewControllerData[deviceId].ButtonStates[5] = buttonDown; break; // R1
				case AKEYCODE_BUTTON_L2:     NewControllerData[deviceId].ButtonStates[6] = buttonDown; NewControllerData[deviceId].ButtonStates[17] = buttonDown;  break; // Options
				case AKEYCODE_BUTTON_R2:     NewControllerData[deviceId].ButtonStates[7] = buttonDown; NewControllerData[deviceId].ButtonStates[16] = buttonDown;  break; // Share
				case AKEYCODE_BUTTON_SELECT: NewControllerData[deviceId].ButtonStates[8] = buttonDown; break; // ThumbL
				case AKEYCODE_BUTTON_START:  NewControllerData[deviceId].ButtonStates[9] = buttonDown; break; // ThumbR
				case AKEYCODE_BUTTON_L1:     NewControllerData[deviceId].ButtonStates[10] = buttonDown; break; // L2
				case AKEYCODE_BUTTON_R1:     NewControllerData[deviceId].ButtonStates[11] = buttonDown; break; // R2
			}
			break;
	}
}

void FAndroidInputInterface::DeferMessage(const FDeferredAndroidMessage& DeferredMessage)
{
	FScopeLock Lock(&TouchInputCriticalSection);
	// Get the index we should be writing to
	int32 Index = DeferredMessageQueueLastEntryIndex++;

	if (Index >= MAX_DEFERRED_MESSAGE_QUEUE_SIZE)
	{
		// Actually, if the queue is full, drop the message and increment a counter of drops
		DeferredMessageQueueDroppedCount++;
		return;
	}
	DeferredMessages[Index] = DeferredMessage;
}

void FAndroidInputInterface::QueueMotionData(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity, const FVector& Acceleration)
{
	FScopeLock Lock(&TouchInputCriticalSection);

	FAndroidInputInterface::MotionDataStack.Push(
		MotionData { Tilt, RotationRate, Gravity, Acceleration });
}
