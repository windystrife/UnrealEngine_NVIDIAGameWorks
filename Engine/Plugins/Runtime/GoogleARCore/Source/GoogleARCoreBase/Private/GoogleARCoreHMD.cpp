// Copyright 2017 Google Inc.

#include "GoogleARCoreHMD.h"
#include "Engine/Engine.h"
#include "RHIDefinitions.h"
#include "GameFramework/PlayerController.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreMotionManager.h"
#include "GoogleARCoreXRCamera.h"

FGoogleARCoreHMD::FGoogleARCoreHMD()
	: TangoDeviceInstance(nullptr)
	, bHMDEnabled(true)
	, bSceneViewExtensionRegistered(false)
	, bARCameraEnabled(false)
	, bColorCameraRenderingEnabled(false)
	, bHasValidPose(false)
	, bLateUpdateEnabled(false)
	, bNeedToFlipCameraImage(false)
	, CachedPosition(FVector::ZeroVector)
	, CachedOrientation(FQuat::Identity)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, TangoHMDEnableCommand(TEXT("ar.tango.HMD.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_HMDEnable",
			"Tango specific extension.\n"
			"Enable or disable Tango ARHMD.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::TangoHMDEnableCommandHandler))
	, ARCameraModeEnableCommand(TEXT("ar.tango.ARCameraMode.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_ARCameraEnable",
			"Tango specific extension.\n"
			"Enable or disable Tango AR Camera Mode.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::ARCameraModeEnableCommandHandler))
	, ColorCamRenderingEnableCommand(TEXT("ar.tango.ColorCamRendering.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_ColorCamRenderingEnable",
			"Tango specific extension.\n"
			"Enable or disable color camera rendering.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::ColorCamRenderingEnableCommandHandler))
	, LateUpdateEnableCommand(TEXT("ar.tango.LateUpdate.bEnable"),
		*NSLOCTEXT("Tango", "CCommandText_LateUpdateEnable",
			"Tango specific extension.\n"
			"Enable or disable late update in TangoARHMD.").ToString(),
		FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateRaw(this, &FGoogleARCoreHMD::LateUpdateEnableCommandHandler))
{
	UE_LOG(LogGoogleARCoreHMD, Log, TEXT("Creating Tango HMD"));
	TangoDeviceInstance = FGoogleARCoreDevice::GetInstance();
	check(TangoDeviceInstance);

	// Register our ability to hit-test in AR with Unreal
	IModularFeatures::Get().RegisterModularFeature(IARHitTestingSupport::GetModularFeatureName(), static_cast<IARHitTestingSupport*>(this));
	IModularFeatures::Get().RegisterModularFeature(IARTrackingQuality::GetModularFeatureName(), static_cast<IARTrackingQuality*>(this));
}


FGoogleARCoreHMD::~FGoogleARCoreHMD()
{
	// Unregister our ability to hit-test in AR with Unreal
	IModularFeatures::Get().UnregisterModularFeature(IARHitTestingSupport::GetModularFeatureName(), static_cast<IARHitTestingSupport*>(this));
	IModularFeatures::Get().UnregisterModularFeature(IARTrackingQuality::GetModularFeatureName(), static_cast<IARTrackingQuality*>(this));
}

///////////////////////////////////////////////////////////////
// Begin FGoogleARCoreHMD IHeadMountedDisplay Virtual Interface   //
///////////////////////////////////////////////////////////////
FName FGoogleARCoreHMD::GetSystemName() const
{
	static FName DefaultName(TEXT("FGoogleARCoreHMD"));
	return DefaultName;
}

bool FGoogleARCoreHMD::HasValidTrackingPosition()
{
	return bHasValidPose;
}

bool FGoogleARCoreHMD::IsHeadTrackingAllowed() const
{
#if PLATFORM_ANDROID
	return true;
#else
	return false;
#endif
}

bool FGoogleARCoreHMD::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId == IXRTrackingSystem::HMDDeviceId)
	{
		OutOrientation = CachedOrientation;
		OutPosition = CachedPosition;
		return true;
	}
	else
	{
		return false;
	}
}

FString FGoogleARCoreHMD::GetVersionString() const
{
	FString s = FString::Printf(TEXT("TangoARHMD - %s, built %s, %s"), *FEngineVersion::Current().ToString(),
		UTF8_TO_TCHAR(__DATE__), UTF8_TO_TCHAR(__TIME__));

	return s;
}

bool FGoogleARCoreHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		static const int32 DeviceId = IXRTrackingSystem::HMDDeviceId;
		OutDevices.Add(DeviceId);
		return true;
	}
	return false;
}

void FGoogleARCoreHMD::RefreshPoses()
{
	// @todo viewext we should move stuff from OnStartGameFrame into here?
}


bool FGoogleARCoreHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
	FGoogleARCorePose CurrentPose;
	if (TangoDeviceInstance->GetIsTangoRunning())
	{
		if (bARCameraEnabled)
		{
			if (bLateUpdateEnabled)
			{
				// When using late update, we simple query the latest camera pose, we will make sure camera is synced with the camera texture in late update.
				bHasValidPose = TangoDeviceInstance->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::CAMERA_COLOR, CurrentPose);
			}
			else
			{
				// Use blocking call to grab the camera pose at the current camera image timestamp.
				double CameraTimestamp = TangoDeviceInstance->TangoARCameraManager.GetColorCameraImageTimestamp();
				bHasValidPose = TangoDeviceInstance->TangoMotionManager.GetPoseAtTime(EGoogleARCoreReferenceFrame::CAMERA_COLOR, CameraTimestamp, CurrentPose);
			}
		}
		else
		{
			bHasValidPose = TangoDeviceInstance->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::DEVICE, CurrentPose);
		}

		if (bHasValidPose)
		{
			CachedOrientation = CurrentPose.Pose.GetRotation();
			CachedPosition = CurrentPose.Pose.GetTranslation();
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////
// Begin FGoogleARCoreHMD ISceneViewExtension Virtual Interface //
/////////////////////////////////////////////////////////////

void FGoogleARCoreHMD::ConfigTangoHMD(bool bEnableHMD, bool bEnableARCamera, bool bEnableLateUpdate)
{
	//	EnableHMD (bEnableHMD);
	bARCameraEnabled = bEnableARCamera;
	bColorCameraRenderingEnabled = bEnableARCamera;
	bLateUpdateEnabled = bEnableLateUpdate;
}


void FGoogleARCoreHMD::EnableColorCameraRendering(bool bEnableColorCameraRnedering)
{
	bColorCameraRenderingEnabled = bEnableColorCameraRnedering;
}

bool FGoogleARCoreHMD::GetColorCameraRenderingEnabled()
{
	return bColorCameraRenderingEnabled;
}

bool FGoogleARCoreHMD::GetTangoHMDARModeEnabled()
{
	return bARCameraEnabled;
}

bool FGoogleARCoreHMD::GetTangoHMDLateUpdateEnabled()
{
	return bLateUpdateEnabled;
}

float FGoogleARCoreHMD::GetWorldToMetersScale() const
{
	if (IsInGameThread() && GWorld != nullptr)
	{
		return GWorld->GetWorldSettings()->WorldToMeters;
	}

	// Default value, assume Unreal units are in centimeters
	return 100.0f;
}

//bool FGoogleARCoreHMD::ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults)
//{
//	return false;
//}

EARTrackingQuality FGoogleARCoreHMD::ARGetTrackingQuality() const
{
	
	if (!FGoogleARCoreDevice::GetInstance()->GetIsTangoRunning())
	{
		return EARTrackingQuality::NotAvailable;
	}

	// @todo arcore : HasValidTrackingPosition() is non-const. Why?
	if (!bHasValidPose)
	{
		return EARTrackingQuality::Limited;
	}

	return EARTrackingQuality::Normal;
}

TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> FGoogleARCoreHMD::GetXRCamera(int32 DeviceId /*= HMDDeviceId*/)
{
	check(DeviceId == HMDDeviceId);

	if (!XRCamera.IsValid())
	{
		XRCamera = FSceneViewExtensions::NewExtension<FGoogleARCoreXRCamera>(*this, DeviceId);
	}
	return XRCamera;
}

/** Console command Handles */
void FGoogleARCoreHMD::TangoHMDEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		//EnableHMD(bShouldEnable);
	}
}
void FGoogleARCoreHMD::ARCameraModeEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		bARCameraEnabled = bShouldEnable;
	}
}
void FGoogleARCoreHMD::ColorCamRenderingEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		bColorCameraRenderingEnabled = bShouldEnable;
	}
}
void FGoogleARCoreHMD::LateUpdateEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		bLateUpdateEnabled = bShouldEnable;
		TangoDeviceInstance->SetForceLateUpdateEnable(bShouldEnable);
	}
}

