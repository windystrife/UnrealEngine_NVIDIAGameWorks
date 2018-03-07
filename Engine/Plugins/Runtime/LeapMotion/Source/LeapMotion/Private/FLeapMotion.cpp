// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "FLeapMotion.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "LeapForwardDeclaration.h"
#include "LeapInterfaceUtility.h"

#define LOCTEXT_NAMESPACE "LeapPlugin"
#define PLUGIN_VERSION "2.0.0"

void FLeapMotion::StartupModule()
{
#if PLATFORM_64BITS
	FString WinDir = TEXT("Win64/");
#else
	FString WinDir = TEXT("Win32/");
#endif

	FString RootLeapMotionPath = FPaths::EngineDir() / FString::Printf(TEXT("Binaries/ThirdParty/LeapMotion/")) / WinDir;
	
	FPlatformProcess::PushDllDirectory(*RootLeapMotionPath);
	LeapMotionDLLHandle = FPlatformProcess::GetDllHandle(*(RootLeapMotionPath + "Leap.dll"));
	FPlatformProcess::PopDllDirectory(*RootLeapMotionPath);

	if (!LeapMotionDLLHandle)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to load LeapMotion library."));
	}

	//Expose all of our input mapping keys to the engine
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapLeftPinch, LOCTEXT("LeapLeftPinch", "Leap Left Pinch"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapLeftGrab, LOCTEXT("LeapLeftGrab", "Leap Left Grab"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapLeftPalmPitch, LOCTEXT("LeapLeftPalmPitch", "Leap Left Palm Pitch"), FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapLeftPalmYaw, LOCTEXT("LeapLeftPalmYaw", "Leap Left Palm Yaw"), FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapLeftPalmRoll, LOCTEXT("LeapLeftPalmRoll", "Leap Left Palm Roll"), FKeyDetails::FloatAxis));

	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapRightPinch, LOCTEXT("LeapRightPinch", "Leap Right Pinch"), FKeyDetails::GamepadKey));
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapRightGrab, LOCTEXT("LeapRightGrab", "Leap Right Grab"), FKeyDetails::GamepadKey));

	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapRightPalmPitch, LOCTEXT("LeapRightPalmPitch", "Leap Right Palm Pitch"), FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapRightPalmYaw, LOCTEXT("LeapRightPalmYaw", "Leap Right Palm Yaw"), FKeyDetails::FloatAxis));
	EKeys::AddKey(FKeyDetails(FKeysLeap::LeapRightPalmRoll, LOCTEXT("LeapRightPalmRoll", "Leap Right Palm Roll"), FKeyDetails::FloatAxis));

	UE_LOG(LeapPluginLog, Log, TEXT("Using LeapPlugin version %s"), TEXT(PLUGIN_VERSION));

	LeapController = new Leap::Controller();
}

void FLeapMotion::ShutdownModule()
{
	if (LeapController)
	{
		// Temp: The Leap::Controller destructor currently crashes
		//delete LeapController;
		LeapController = nullptr;
	}

	if (LeapMotionDLLHandle)
	{
		FPlatformProcess::FreeDllHandle(LeapMotionDLLHandle);
		LeapMotionDLLHandle = nullptr;
	}
}

Leap::Controller* FLeapMotion::Controller()
{
	return LeapController;
}

IMPLEMENT_MODULE(FLeapMotion, LeapMotion)

#undef LOCTEXT_NAMESPACE
