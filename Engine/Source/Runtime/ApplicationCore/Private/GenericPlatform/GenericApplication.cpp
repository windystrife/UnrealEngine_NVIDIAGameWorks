// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericApplication.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"

const FGamepadKeyNames::Type FGamepadKeyNames::Invalid(NAME_None);

// Ensure that the GamepadKeyNames match those in InputCoreTypes.cpp
const FGamepadKeyNames::Type FGamepadKeyNames::LeftAnalogX("Gamepad_LeftX");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftAnalogY("Gamepad_LeftY");
const FGamepadKeyNames::Type FGamepadKeyNames::RightAnalogX("Gamepad_RightX");
const FGamepadKeyNames::Type FGamepadKeyNames::RightAnalogY("Gamepad_RightY");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftTriggerAnalog("Gamepad_LeftTriggerAxis");
const FGamepadKeyNames::Type FGamepadKeyNames::RightTriggerAnalog("Gamepad_RightTriggerAxis");

const FGamepadKeyNames::Type FGamepadKeyNames::LeftThumb("Gamepad_LeftThumbstick");
const FGamepadKeyNames::Type FGamepadKeyNames::RightThumb("Gamepad_RightThumbstick");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialLeft("Gamepad_Special_Left");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialLeft_X("Gamepad_Special_Left_X");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialLeft_Y("Gamepad_Special_Left_Y");
const FGamepadKeyNames::Type FGamepadKeyNames::SpecialRight("Gamepad_Special_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonBottom("Gamepad_FaceButton_Bottom");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonRight("Gamepad_FaceButton_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonLeft("Gamepad_FaceButton_Left");
const FGamepadKeyNames::Type FGamepadKeyNames::FaceButtonTop("Gamepad_FaceButton_Top");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftShoulder("Gamepad_LeftShoulder");
const FGamepadKeyNames::Type FGamepadKeyNames::RightShoulder("Gamepad_RightShoulder");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftTriggerThreshold("Gamepad_LeftTrigger");
const FGamepadKeyNames::Type FGamepadKeyNames::RightTriggerThreshold("Gamepad_RightTrigger");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadUp("Gamepad_DPad_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadDown("Gamepad_DPad_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadRight("Gamepad_DPad_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::DPadLeft("Gamepad_DPad_Left");

const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickUp("Gamepad_LeftStick_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickDown("Gamepad_LeftStick_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickRight("Gamepad_LeftStick_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::LeftStickLeft("Gamepad_LeftStick_Left");

const FGamepadKeyNames::Type FGamepadKeyNames::RightStickUp("Gamepad_RightStick_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::RightStickDown("Gamepad_RightStick_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::RightStickRight("Gamepad_RightStick_Right");
const FGamepadKeyNames::Type FGamepadKeyNames::RightStickLeft("Gamepad_RightStick_Left");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton1("MotionController_Left_FaceButton1");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton2("MotionController_Left_FaceButton2");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton3("MotionController_Left_FaceButton3");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton4("MotionController_Left_FaceButton4");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton5("MotionController_Left_FaceButton5");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton6("MotionController_Left_FaceButton6");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton7("MotionController_Left_FaceButton7");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_FaceButton8("MotionController_Left_FaceButton8");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Shoulder("MotionController_Left_Shoulder");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Trigger("MotionController_Left_Trigger");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Grip1("MotionController_Left_Grip1");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Grip2("MotionController_Left_Grip2");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick("MotionController_Left_Thumbstick");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick_Up("MotionController_Left_Thumbstick_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick_Down("MotionController_Left_Thumbstick_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick_Left("MotionController_Left_Thumbstick_Left");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick_Right("MotionController_Left_Thumbstick_Right");

	//		Right Controller
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton1("MotionController_Right_FaceButton1");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton2("MotionController_Right_FaceButton2");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton3("MotionController_Right_FaceButton3");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton4("MotionController_Right_FaceButton4");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton5("MotionController_Right_FaceButton5");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton6("MotionController_Right_FaceButton6");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton7("MotionController_Right_FaceButton7");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_FaceButton8("MotionController_Right_FaceButton8");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Shoulder("MotionController_Right_Shoulder");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Trigger("MotionController_Right_Trigger");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Grip1("MotionController_Right_Grip1");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Grip2("MotionController_Right_Grip2");

const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick("MotionController_Right_Thumbstick");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick_Up("MotionController_Right_Thumbstick_Up");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick_Down("MotionController_Right_Thumbstick_Down");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick_Left("MotionController_Right_Thumbstick_Left");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick_Right("MotionController_Right_Thumbstick_Right");

	//   Motion Controller Axes
	//		Left Controller
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick_X("MotionController_Left_Thumbstick_X");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Thumbstick_Y("MotionController_Left_Thumbstick_Y");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_TriggerAxis("MotionController_Left_TriggerAxis");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Grip1Axis( "MotionController_Left_Grip1Axis" );
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Left_Grip2Axis( "MotionController_Left_Grip2Axis" );

	//		Right Controller
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick_X("MotionController_Right_Thumbstick_X");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Thumbstick_Y("MotionController_Right_Thumbstick_Y");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_TriggerAxis("MotionController_Right_TriggerAxis");
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Grip1Axis( "MotionController_Right_Grip1Axis" );
const FGamepadKeyNames::Type FGamepadKeyNames::MotionController_Right_Grip2Axis( "MotionController_Right_Grip2Axis" );

float GDebugSafeZoneRatio = 1.0f;
float GDebugActionZoneRatio = 1.0f;

struct FSafeZoneConsoleVariables
{
	FAutoConsoleVariableRef DebugSafeZoneRatioCVar;
	FAutoConsoleVariableRef DebugActionZoneRatioCVar;

	FSafeZoneConsoleVariables()
		: DebugSafeZoneRatioCVar(
			TEXT("r.DebugSafeZone.TitleRatio"),
			GDebugSafeZoneRatio,
			TEXT("The safe zone ratio that will be returned by FDisplayMetrics::GetDisplayMetrics on platforms that don't have a defined safe zone (0..1)\n")
			TEXT(" default: 1.0"))
		, DebugActionZoneRatioCVar(
			TEXT("r.DebugActionZone.ActionRatio"),
			GDebugActionZoneRatio,
			TEXT("The action zone ratio that will be returned by FDisplayMetrics::GetDisplayMetrics on platforms that don't have a defined safe zone (0..1)\n")
			TEXT(" default: 1.0"))
	{
		DebugSafeZoneRatioCVar->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FSafeZoneConsoleVariables::OnDebugSafeZoneChanged));
		DebugActionZoneRatioCVar->SetOnChangedCallback(FConsoleVariableDelegate::CreateRaw(this, &FSafeZoneConsoleVariables::OnDebugSafeZoneChanged));
	}

	void OnDebugSafeZoneChanged(IConsoleVariable* Var)
	{
		FCoreDelegates::OnSafeFrameChangedEvent.Broadcast();
	}
};

FSafeZoneConsoleVariables GSafeZoneConsoleVariables;

FPlatformRect FDisplayMetrics::GetMonitorWorkAreaFromPoint(const FVector2D& Point) const
{
	for (const FMonitorInfo& Info : MonitorInfo)
	{
		// The point may not actually be inside the work area (for example on Windows taskbar or Mac menu bar), so we use DisplayRect to find the monitor
		if (Point.X >= Info.DisplayRect.Left && Point.X <= Info.DisplayRect.Right && Point.Y >= Info.DisplayRect.Top && Point.Y <= Info.DisplayRect.Bottom)
		{
			return Info.WorkArea;
		}
	}

	return FPlatformRect(0, 0, 0, 0);
}

float FDisplayMetrics::GetDebugTitleSafeZoneRatio()
{
	return GDebugSafeZoneRatio;
}

float FDisplayMetrics::GetDebugActionSafeZoneRatio()
{
	return GDebugActionZoneRatio;
}

void FDisplayMetrics::ApplyDefaultSafeZones()
{
	const float SafeZoneRatio = GetDebugTitleSafeZoneRatio();
	if (SafeZoneRatio < 1.0f)
	{
		const float HalfUnsafeRatio = (1.0f - SafeZoneRatio) * 0.5f;
		TitleSafePaddingSize = FVector2D(PrimaryDisplayWidth * HalfUnsafeRatio, PrimaryDisplayHeight * HalfUnsafeRatio);
	}

	const float ActionSafeZoneRatio = GetDebugActionSafeZoneRatio();
	if (ActionSafeZoneRatio < 1.0f)
	{
		const float HalfUnsafeRatio = (1.0f - ActionSafeZoneRatio) * 0.5f;
		ActionSafePaddingSize = FVector2D(PrimaryDisplayWidth * HalfUnsafeRatio, PrimaryDisplayHeight * HalfUnsafeRatio);
	}
}

void FDisplayMetrics::PrintToLog() const
{
	UE_LOG(LogInit, Log, TEXT("Display metrics:"));
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayWidth: %d"), PrimaryDisplayWidth);
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayHeight: %d"), PrimaryDisplayHeight);
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayWorkAreaRect:"));
	UE_LOG(LogInit, Log, TEXT("    Left=%d, Top=%d, Right=%d, Bottom=%d"), 
		PrimaryDisplayWorkAreaRect.Left, PrimaryDisplayWorkAreaRect.Top, 
		PrimaryDisplayWorkAreaRect.Right, PrimaryDisplayWorkAreaRect.Bottom);

	UE_LOG(LogInit, Log, TEXT("  VirtualDisplayRect:"));
	UE_LOG(LogInit, Log, TEXT("    Left=%d, Top=%d, Right=%d, Bottom=%d"), 
		VirtualDisplayRect.Left, VirtualDisplayRect.Top, 
		VirtualDisplayRect.Right, VirtualDisplayRect.Bottom);

	UE_LOG(LogInit, Log, TEXT("  TitleSafePaddingSize: %s"), *TitleSafePaddingSize.ToString());
	UE_LOG(LogInit, Log, TEXT("  ActionSafePaddingSize: %s"), *ActionSafePaddingSize.ToString());

	const int NumMonitors = MonitorInfo.Num();
	UE_LOG(LogInit, Log, TEXT("  Number of monitors: %d"), NumMonitors);

	for (int MonitorIdx = 0; MonitorIdx < NumMonitors; ++MonitorIdx)
	{
		const FMonitorInfo & Info = MonitorInfo[MonitorIdx];
		UE_LOG(LogInit, Log, TEXT("    Monitor %d"), MonitorIdx);
		UE_LOG(LogInit, Log, TEXT("      Name: %s"), *Info.Name);
		UE_LOG(LogInit, Log, TEXT("      ID: %s"), *Info.ID);
		UE_LOG(LogInit, Log, TEXT("      NativeWidth: %d"), Info.NativeWidth);
		UE_LOG(LogInit, Log, TEXT("      NativeHeight: %d"), Info.NativeHeight);
		UE_LOG(LogInit, Log, TEXT("      bIsPrimary: %s"), Info.bIsPrimary ? TEXT("true") : TEXT("false"));
	}
}
