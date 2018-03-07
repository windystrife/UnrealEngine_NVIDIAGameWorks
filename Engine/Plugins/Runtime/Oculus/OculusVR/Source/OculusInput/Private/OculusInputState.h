// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusInputModule.h"

#if OCULUS_INPUT_SUPPORTED_PLATFORMS
#include "IMotionController.h"
#include "InputCoreTypes.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"

namespace OculusInput
{


//-------------------------------------------------------------------------------------------------
// Button names
//-------------------------------------------------------------------------------------------------

enum class EOculusTouchControllerButton
{
	// NOTE: The Trigger and Grip digital buttons are synthetic.  Oculus hardware doesn't support a digital press for these
	Trigger,
	Grip,

	XA,
	YB,
	Thumbstick,

	Thumbstick_Up,
	Thumbstick_Down,
	Thumbstick_Left,
	Thumbstick_Right,

	Menu,

	/** Total number of controller buttons */
	TotalButtonCount
};

enum class EOculusRemoteControllerButton
{
	DPad_Up,
	DPad_Down,
	DPad_Left,
	DPad_Right,
	
	Enter,
	Back,

	VolumeUp,
	VolumeDown,
	Home,

	/** Total number of controller buttons */
	TotalButtonCount
};

enum class EOculusTouchCapacitiveAxes
{
	Thumbstick,
	Trigger,
	XA,
	YB,
	IndexPointing,
	ThumbUp,
	
	/** Total number of capacitive axes */
	TotalAxisCount
};


//-------------------------------------------------------------------------------------------------
// FOculusKey
//-------------------------------------------------------------------------------------------------

struct FOculusKey
{
	static const FKey OculusTouch_Left_Thumbstick;
	static const FKey OculusTouch_Left_Trigger;
	static const FKey OculusTouch_Left_FaceButton1; // X or A
	static const FKey OculusTouch_Left_FaceButton2; // Y or B
	static const FKey OculusTouch_Left_IndexPointing;
	static const FKey OculusTouch_Left_ThumbUp;

	static const FKey OculusTouch_Right_Thumbstick;
	static const FKey OculusTouch_Right_Trigger;
	static const FKey OculusTouch_Right_FaceButton1; // X or A
	static const FKey OculusTouch_Right_FaceButton2; // Y or B
	static const FKey OculusTouch_Right_IndexPointing;
	static const FKey OculusTouch_Right_ThumbUp;

	static const FKey OculusRemote_DPad_Up;
	static const FKey OculusRemote_DPad_Down;
	static const FKey OculusRemote_DPad_Left;
	static const FKey OculusRemote_DPad_Right;

	static const FKey OculusRemote_Enter;
	static const FKey OculusRemote_Back;

	static const FKey OculusRemote_VolumeUp;
	static const FKey OculusRemote_VolumeDown;
	static const FKey OculusRemote_Home;
};


//-------------------------------------------------------------------------------------------------
// FOculusKeyNames
//-------------------------------------------------------------------------------------------------

struct FOculusKeyNames
{
	typedef FName Type;

	static const FName OculusTouch_Left_Thumbstick;
	static const FName OculusTouch_Left_Trigger;
	static const FName OculusTouch_Left_FaceButton1; // X or A
	static const FName OculusTouch_Left_FaceButton2; // Y or B
	static const FName OculusTouch_Left_IndexPointing;
	static const FName OculusTouch_Left_ThumbUp;

	static const FName OculusTouch_Right_Thumbstick;
	static const FName OculusTouch_Right_Trigger;
	static const FName OculusTouch_Right_FaceButton1; // X or A
	static const FName OculusTouch_Right_FaceButton2; // Y or B
	static const FName OculusTouch_Right_IndexPointing;
	static const FName OculusTouch_Right_ThumbUp;

	static const FName OculusRemote_DPad_Up;
	static const FName OculusRemote_DPad_Down;
	static const FName OculusRemote_DPad_Left;
	static const FName OculusRemote_DPad_Right;

	static const FName OculusRemote_Enter;
	static const FName OculusRemote_Back;

	static const FName OculusRemote_VolumeUp;
	static const FName OculusRemote_VolumeDown;
	static const FName OculusRemote_Home;
};


//-------------------------------------------------------------------------------------------------
// FOculusButtonState -  Digital button state
//-------------------------------------------------------------------------------------------------

struct FOculusButtonState
{
	/** The Unreal button this maps to.  Different depending on whether this is the Left or Right hand controller */
	FName Key;

	/** Whether we're pressed or not.  While pressed, we will generate repeat presses on a timer */
	bool bIsPressed;

	/** Next time a repeat event should be generated for each button */
	double NextRepeatTime;


	/** Default constructor that just sets sensible defaults */
	FOculusButtonState()
		: Key( NAME_None ),
		  bIsPressed( false ),
		  NextRepeatTime( 0.0 )
	{
	}
};


//-------------------------------------------------------------------------------------------------
// FOculusTouchCapacitiveState - Capacitive Axis State
//-------------------------------------------------------------------------------------------------

struct FOculusTouchCapacitiveState
{
	/** The axis that this button state maps to */
	FName Axis;

	/** How close the finger is to this button, from 0.f to 1.f */
	float State;

	FOculusTouchCapacitiveState()
		: Axis(NAME_None)
		, State(0.f)
	{
	}
};


//-------------------------------------------------------------------------------------------------
// FOculusTouchControllerState - Input state for an Oculus motion controller
//-------------------------------------------------------------------------------------------------

struct FOculusTouchControllerState
{
	/** True if the device is connected, otherwise false */
	bool bIsConnected;

	/** True if position is being tracked, otherwise false */
	bool bIsPositionTracked;

	/** True if orientation is being tracked, otherwise false */
	bool bIsOrientationTracked;

	/** Analog trigger */
	float TriggerAxis;

	/** Grip trigger */
	float GripAxis;

	/** Thumbstick */
	FVector2D ThumbstickAxes;

	/** Button states */
	FOculusButtonState Buttons[ (int32)EOculusTouchControllerButton::TotalButtonCount ];

	/** Capacitive Touch axes */
	FOculusTouchCapacitiveState CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::TotalAxisCount];

	/** Whether or not we're playing a haptic effect.  If true, force feedback calls will be early-outed in favor of the haptic effect */
	bool bPlayingHapticEffect;

	/** Haptic frequency (zero to disable) */
	float HapticFrequency;

	/** Haptic amplitude (zero to disable) */
	float HapticAmplitude;

	/** Number of times that controller was recentered (for mobile controllers) */
	int RecenterCount;


	/** Explicit constructor sets up sensible defaults */
	FOculusTouchControllerState( const EControllerHand Hand )
		: bIsConnected( false ),
		  bIsPositionTracked( false ),
		  bIsOrientationTracked( false ),
		  TriggerAxis( 0.0f ),
		  GripAxis( 0.0f ),
		  ThumbstickAxes( FVector2D::ZeroVector ),
		  bPlayingHapticEffect( false ),
		  HapticFrequency( 0.0f ),
		  HapticAmplitude( 0.0f ),
		  RecenterCount(0)
	{
		for( FOculusButtonState& Button : Buttons )
		{
			Button.bIsPressed = false;
			Button.NextRepeatTime = 0.0;
		}

		Buttons[ (int32)EOculusTouchControllerButton::Trigger ].Key = ( Hand == EControllerHand::Left ) ? FGamepadKeyNames::MotionController_Left_Trigger : FGamepadKeyNames::MotionController_Right_Trigger;
		Buttons[ (int32)EOculusTouchControllerButton::Grip ].Key = ( Hand == EControllerHand::Left ) ? FGamepadKeyNames::MotionController_Left_Grip1 : FGamepadKeyNames::MotionController_Right_Grip1;
		Buttons[ (int32)EOculusTouchControllerButton::Thumbstick ].Key = ( Hand == EControllerHand::Left ) ? FGamepadKeyNames::MotionController_Left_Thumbstick : FGamepadKeyNames::MotionController_Right_Thumbstick;
		Buttons[ (int32)EOculusTouchControllerButton::XA ].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_FaceButton1 : FGamepadKeyNames::MotionController_Right_FaceButton1;
		Buttons[ (int32)EOculusTouchControllerButton::YB ].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_FaceButton2 : FGamepadKeyNames::MotionController_Right_FaceButton2;
		Buttons[ (int32)EOculusTouchControllerButton::Thumbstick_Up].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_Up : FGamepadKeyNames::MotionController_Right_Thumbstick_Up;
		Buttons[ (int32)EOculusTouchControllerButton::Thumbstick_Down].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_Down : FGamepadKeyNames::MotionController_Right_Thumbstick_Down;
		Buttons[ (int32)EOculusTouchControllerButton::Thumbstick_Left].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_Left : FGamepadKeyNames::MotionController_Right_Thumbstick_Left;
		Buttons[ (int32)EOculusTouchControllerButton::Thumbstick_Right].Key = (Hand == EControllerHand::Left) ? FGamepadKeyNames::MotionController_Left_Thumbstick_Right : FGamepadKeyNames::MotionController_Right_Thumbstick_Right;

		Buttons[ (int32)EOculusTouchControllerButton::Menu ].Key = FGamepadKeyNames::SpecialRight;

		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::Thumbstick].Axis = (Hand == EControllerHand::Left) ? FOculusKeyNames::OculusTouch_Left_Thumbstick : FOculusKeyNames::OculusTouch_Right_Thumbstick;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::Trigger].Axis = (Hand == EControllerHand::Left) ? FOculusKeyNames::OculusTouch_Left_Trigger : FOculusKeyNames::OculusTouch_Right_Trigger;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::XA].Axis = (Hand == EControllerHand::Left) ? FOculusKeyNames::OculusTouch_Left_FaceButton1 : FOculusKeyNames::OculusTouch_Right_FaceButton1;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::YB].Axis = (Hand == EControllerHand::Left) ? FOculusKeyNames::OculusTouch_Left_FaceButton2 : FOculusKeyNames::OculusTouch_Right_FaceButton2;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::IndexPointing].Axis = (Hand == EControllerHand::Left) ? FOculusKeyNames::OculusTouch_Left_IndexPointing : FOculusKeyNames::OculusTouch_Right_IndexPointing;
		CapacitiveAxes[(int32)EOculusTouchCapacitiveAxes::ThumbUp].Axis = (Hand == EControllerHand::Left) ? FOculusKeyNames::OculusTouch_Left_ThumbUp : FOculusKeyNames::OculusTouch_Right_ThumbUp;
	}

	/** Default constructor does nothing.  Don't use it.  This only exists because we cannot initialize an array of objects with no default constructor on non-C++ 11 compliant compilers (VS 2013) */
	FOculusTouchControllerState()
	{
	}
};


//-------------------------------------------------------------------------------------------------
// FOculusRemoteControllerState
//-------------------------------------------------------------------------------------------------

struct FOculusRemoteControllerState
{
	/** Button states */
	FOculusButtonState Buttons[(int32)EOculusRemoteControllerButton::TotalButtonCount];

	FOculusRemoteControllerState()
	{
		for (FOculusButtonState& Button : Buttons)
		{
			Button.bIsPressed = false;
			Button.NextRepeatTime = 0.0;
		}

		Buttons[(int32)EOculusRemoteControllerButton::DPad_Up].Key = FOculusKeyNames::OculusRemote_DPad_Up;
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Down].Key = FOculusKeyNames::OculusRemote_DPad_Down;
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Left].Key = FOculusKeyNames::OculusRemote_DPad_Left;
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Right].Key = FOculusKeyNames::OculusRemote_DPad_Right;
		Buttons[(int32)EOculusRemoteControllerButton::Enter].Key = FOculusKeyNames::OculusRemote_Enter;
		Buttons[(int32)EOculusRemoteControllerButton::Back].Key = FOculusKeyNames::OculusRemote_Back;

		Buttons[(int32)EOculusRemoteControllerButton::VolumeUp].Key = FOculusKeyNames::OculusRemote_VolumeUp;
		Buttons[(int32)EOculusRemoteControllerButton::VolumeDown].Key = FOculusKeyNames::OculusRemote_VolumeDown;
		Buttons[(int32)EOculusRemoteControllerButton::Home].Key = FOculusKeyNames::OculusRemote_Home;
	}

	void ReinitButtonsForGamepadCompat()
	{
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Up].Key = FGamepadKeyNames::DPadUp;
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Down].Key = FGamepadKeyNames::DPadDown;
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Left].Key = FGamepadKeyNames::DPadLeft;
		Buttons[(int32)EOculusRemoteControllerButton::DPad_Right].Key = FGamepadKeyNames::DPadRight;
		Buttons[(int32)EOculusRemoteControllerButton::Enter].Key = FGamepadKeyNames::SpecialRight;
		Buttons[(int32)EOculusRemoteControllerButton::Back].Key = FGamepadKeyNames::SpecialLeft;
	}
};


//-------------------------------------------------------------------------------------------------
// FOculusTouchControllerPair - A pair of wireless motion controllers, one for either hand
//-------------------------------------------------------------------------------------------------

struct FOculusTouchControllerPair
{
	/** The Unreal controller index assigned to this pair */
	int32 UnrealControllerIndex;

	/** Current device state for either hand */
	FOculusTouchControllerState ControllerStates[ 2 ];


	/** Default constructor that sets up sensible defaults */
	FOculusTouchControllerPair()
		: UnrealControllerIndex( INDEX_NONE ),
		ControllerStates()
	{
		ControllerStates[ (int32)EControllerHand::Left ] = FOculusTouchControllerState( EControllerHand::Left );
		ControllerStates[ (int32)EControllerHand::Right ] = FOculusTouchControllerState( EControllerHand::Right );	
	}
};


} // namespace OculusInput

#endif	// OCULUS_INPUT_SUPPORTED_PLATFORMS

