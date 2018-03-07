// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
//
#include "Classes/SteamVRFunctionLibrary.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

USteamVRFunctionLibrary::USteamVRFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

#if STEAMVR_SUPPORTED_PLATFORMS
FSteamVRHMD* GetSteamVRHMD()
{
	static FName SystemName(TEXT("SteamVR"));
	if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == SystemName))
	{
		return static_cast<FSteamVRHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

IMotionController* GetSteamMotionController()
{
	static FName DeviceTypeName(TEXT("SteamVRController"));
	TArray<IMotionController*> MotionControllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
	for (IMotionController* MotionController : MotionControllers)
	{
		if (MotionController->GetMotionControllerDeviceTypeName() == DeviceTypeName)
		{
			return MotionController;
		}
	}
	return nullptr;
}
#endif // STEAMVR_SUPPORTED_PLATFORMS

void USteamVRFunctionLibrary::GetValidTrackedDeviceIds(ESteamVRTrackedDeviceType DeviceType, TArray<int32>& OutTrackedDeviceIds)
{
#if STEAMVR_SUPPORTED_PLATFORMS
	OutTrackedDeviceIds.Empty();

	EXRTrackedDeviceType XRDeviceType = EXRTrackedDeviceType::Invalid;
	switch (DeviceType)
	{
	case ESteamVRTrackedDeviceType::Controller:
		XRDeviceType = EXRTrackedDeviceType::Controller;
		break;
	case ESteamVRTrackedDeviceType::TrackingReference:
		XRDeviceType = EXRTrackedDeviceType::TrackingReference;
		break;
	case ESteamVRTrackedDeviceType::Other:
		XRDeviceType = EXRTrackedDeviceType::Other;
		break;
	case ESteamVRTrackedDeviceType::Invalid:
		XRDeviceType = EXRTrackedDeviceType::Invalid;
		break;
	default:
		break;
	}


	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		SteamVRHMD->EnumerateTrackedDevices(OutTrackedDeviceIds, XRDeviceType);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS
}

bool USteamVRFunctionLibrary::GetTrackedDevicePositionAndOrientation(int32 DeviceId, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	FSteamVRHMD* SteamVRHMD = GetSteamVRHMD();
	if (SteamVRHMD)
	{
		FQuat DeviceOrientation = FQuat::Identity;
		RetVal = SteamVRHMD->GetCurrentPose(DeviceId, DeviceOrientation, OutPosition);
		OutOrientation = DeviceOrientation.Rotator();
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetVal;
}

bool USteamVRFunctionLibrary::GetHandPositionAndOrientation(int32 ControllerIndex, EControllerHand Hand, FVector& OutPosition, FRotator& OutOrientation)
{
	bool RetVal = false;

#if STEAMVR_SUPPORTED_PLATFORMS
	IMotionController* SteamMotionController = GetSteamMotionController();
	if (SteamMotionController)
	{
		// Note: the steam motion controller ignores the WorldToMeters scale argument.
		RetVal = SteamMotionController->GetControllerOrientationAndPosition(ControllerIndex, Hand, OutOrientation, OutPosition, -1.0f);
	}
#endif // STEAMVR_SUPPORTED_PLATFORMS

	return RetVal;
}
