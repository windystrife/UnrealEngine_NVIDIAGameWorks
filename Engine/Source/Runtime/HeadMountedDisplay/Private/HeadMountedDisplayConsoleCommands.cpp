// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
//
#include "HeadMountedDisplay.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/Package.h"
#include "HeadMountedDisplayTypes.h"
#include "ISpectatorScreenController.h"
#include "IXRTrackingSystem.h"

//#include "UObject/UObjectGlobals.h"

#if !UE_BUILD_SHIPPING
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#endif

#define LOCTEXT_NAMESPACE "HeadMountedDisplay"
DEFINE_LOG_CATEGORY_STATIC(LogHeadMountedDisplayCommand, Display, All);

/**
* HMD device console vars
*/
static TAutoConsoleVariable<int32> CVarHiddenAreaMask(
	TEXT("vr.HiddenAreaMask"),
	1,
	*LOCTEXT("CVarText_HiddenAreaMask", "Enable or disable hidden area mask\n0: disabled\n1: enabled").ToString(),
	ECVF_Scalability | ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarEnableDevOverrides(
	TEXT("vr.Debug.bEnableDevOverrides"),
	0,
	*LOCTEXT("CVarText_EnableDevOverrides", "Enables or disables console commands that modify various developer-only settings.").ToString());

static TAutoConsoleVariable<int32> CVarMixLayerPriorities(
	TEXT("vr.StereoLayers.bMixLayerPriorities"),
	0,
	*LOCTEXT("CVarText_MixLayerPriorities", "By default, Face-Locked Stereo Layers are always rendered on top of any other layer position types.\nSet this to a non-zero value to disable this behavior (not supported on all platforms.)").ToString(),
	ECVF_RenderThreadSafe);



#if !UE_BUILD_SHIPPING
static void DrawDebugTrackingSensorLocations(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!GEngine || !GEngine->XRSystem.IsValid())
	{
		return;
	}

	if (!PlayerController)
	{
		PlayerController = GWorld->GetFirstPlayerController();
		if (!PlayerController)
		{
			return;
		}
	}

	TArray<int32> SensorDeviceIDs;
	GEngine->XRSystem->EnumerateTrackedDevices(SensorDeviceIDs, EXRTrackedDeviceType::TrackingReference);
	if (SensorDeviceIDs.Num() == 0)
	{
		return;
	}

	const FColor FrustrumColor = (GEngine->XRSystem->HasValidTrackingPosition() ? FColor::Green : FColor::Red);
	const FColor CenterLineColor = FColor::Yellow;

	APawn* Pawn = PlayerController->GetPawn();
	AActor* ViewTarget = PlayerController->GetViewTarget();
	if (!Pawn || ! ViewTarget)
	{
		return;
	}
	FQuat DeltaControlOrientation = Pawn->GetViewRotation().Quaternion();
	FVector LocationOffset = ViewTarget->GetTransform().GetLocation();

	if (!ViewTarget->HasActiveCameraComponent())
	{
		FVector HeadPosition; /* unsused */
		FQuat HeadOrient;
		GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HeadOrient, HeadPosition);
		DeltaControlOrientation = DeltaControlOrientation * HeadOrient.Inverse();
	}

	for (int32 SensorID : SensorDeviceIDs)
	{
		FVector SensorOrigin;
		FQuat SensorOrient;
		FXRSensorProperties SensorProperties;
		GEngine->XRSystem->GetTrackingSensorProperties(SensorID, SensorOrient, SensorOrigin, SensorProperties);


		SensorOrient = DeltaControlOrientation * SensorOrient;
		SensorOrigin = DeltaControlOrientation.RotateVector(SensorOrigin);

		// Calculate the edge vectors of the pyramid from the FoV angles
		const float LeftTan = -FMath::Tan(FMath::DegreesToRadians(SensorProperties.LeftFOV));
		const float RightTan = FMath::Tan(FMath::DegreesToRadians(SensorProperties.RightFOV));
		const float TopTan = FMath::Tan(FMath::DegreesToRadians(SensorProperties.TopFOV));
		const float BottomTan = -FMath::Tan(FMath::DegreesToRadians(SensorProperties.BottomFOV));
		FVector EdgeTR(1, RightTan, TopTan);
		FVector EdgeTL(1, LeftTan, TopTan);
		FVector EdgeBL(1, LeftTan, BottomTan);
		FVector EdgeBR(1, RightTan, BottomTan);

		// Create a matrix to translate from sensor-relative coordinates to the view location
		FMatrix Matrix = SensorOrient * FMatrix::Identity;
		Matrix *= FTranslationMatrix(SensorOrigin);
		Matrix *= FTranslationMatrix(LocationOffset);

		// Calculate coordinates of the tip (location of the sensor) and the base of the pyramid (far plane)
		FVector Tip = Matrix.TransformPosition(FVector::ZeroVector);
		FVector BaseTR = Matrix.TransformPosition(EdgeTR * SensorProperties.FarPlane);
		FVector BaseTL = Matrix.TransformPosition(EdgeTL * SensorProperties.FarPlane);
		FVector BaseBL = Matrix.TransformPosition(EdgeBL * SensorProperties.FarPlane);
		FVector BaseBR = Matrix.TransformPosition(EdgeBR * SensorProperties.FarPlane);

		// Calculate coordinates of where the near plane intersects the pyramid
		FVector NearTR = Matrix.TransformPosition(EdgeTR * SensorProperties.NearPlane);
		FVector NearTL = Matrix.TransformPosition(EdgeTL * SensorProperties.NearPlane);
		FVector NearBL = Matrix.TransformPosition(EdgeBL * SensorProperties.NearPlane);
		FVector NearBR = Matrix.TransformPosition(EdgeBR * SensorProperties.NearPlane);

		// Draw a point at the sensor position
		DrawDebugPoint(GWorld, Tip, 5, FrustrumColor);

		// Draw the four edges of the pyramid
		DrawDebugLine(GWorld, Tip, BaseTR, FrustrumColor);
		DrawDebugLine(GWorld, Tip, BaseTL, FrustrumColor);
		DrawDebugLine(GWorld, Tip, BaseBL, FrustrumColor);
		DrawDebugLine(GWorld, Tip, BaseBR, FrustrumColor);

		// Draw the base (far plane)
		DrawDebugLine(GWorld, BaseTR, BaseTL, FrustrumColor);
		DrawDebugLine(GWorld, BaseTL, BaseBL, FrustrumColor);
		DrawDebugLine(GWorld, BaseBL, BaseBR, FrustrumColor);
		DrawDebugLine(GWorld, BaseBR, BaseTR, FrustrumColor);

		// Draw the near plane
		DrawDebugLine(GWorld, NearTR, NearTL, FrustrumColor);
		DrawDebugLine(GWorld, NearTL, NearBL, FrustrumColor);
		DrawDebugLine(GWorld, NearBL, NearBR, FrustrumColor);
		DrawDebugLine(GWorld, NearBR, NearTR, FrustrumColor);

		// Draw a center line from the sensor to the focal point
		FVector CenterLine = Matrix.TransformPosition(FVector(SensorProperties.CameraDistance, 0, 0));
		DrawDebugLine(GWorld, Tip, CenterLine, CenterLineColor);
		DrawDebugPoint(GWorld, CenterLine, 5, CenterLineColor);
	}
}

static void ShowTrackingSensors(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	static FDelegateHandle Handle;
	if (Args.Num())
	{
		bool bShouldEnable = FCString::ToBool(*Args[0]);
		if (Handle.IsValid() != bShouldEnable)
		{
			if (bShouldEnable)
			{
				Handle = UDebugDrawService::Register(TEXT("Game"), FDebugDrawDelegate::CreateStatic(DrawDebugTrackingSensorLocations));
			}
			else
			{
				UDebugDrawService::Unregister(Handle);
				Handle.Reset();
			}
		}
	}
	Ar.Logf(TEXT("Tracking sensor drawing is %s"), Handle.IsValid() ? TEXT("enabled") : TEXT("disabled"));
}

static FAutoConsoleCommand CShowTrackingSensorsCmd(
	TEXT("vr.Debug.VisualizeTrackingSensors"),
	*LOCTEXT("CVarText_ShowTrackingSensors", "Show or hide the location and coverage area of the tracking sensors\nUse 1, True, or Yes to enable, 0, False or No to disable.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(ShowTrackingSensors));
#endif

static void TrackingOrigin(const TArray<FString>& Args, UWorld* , FOutputDevice& Ar)
{
	const static UEnum* TrackingOriginEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EHMDTrackingOrigin"));
	int Origin = INDEX_NONE;
	if (Args.Num())
	{
		if (FCString::IsNumeric(*Args[0]))
		{
			Origin = FCString::Atoi(*Args[0]);
		}
		else
		{
			Origin = TrackingOriginEnum->GetIndexByName(*Args[0]);
		}
		if (Origin < 0 || Origin > 1)
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid tracking orgin, %s"), *Args[0]);
		}

		if (GEngine && GEngine->XRSystem.IsValid())
		{
			GEngine->XRSystem->SetTrackingOrigin(EHMDTrackingOrigin::Type(Origin));
		}
	}
	else
	{
		if (GEngine && GEngine->XRSystem.IsValid())
		{
			Origin = GEngine->XRSystem->GetTrackingOrigin();
		}
		Ar.Logf(TEXT("Tracking orgin is set to %s"), *TrackingOriginEnum->GetNameStringByIndex(Origin));
	}
}

static FAutoConsoleCommand CTrackingOriginCmd(
	TEXT("vr.TrackingOrigin"),
	*LOCTEXT("CCommandText_TrackingOrigin", "Floor or 0 - tracking origin is at the floor, Eye or 1 - tracking origin is at the eye level.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(TrackingOrigin));

namespace HMDConsoleCommandsHelpers
{
	ISpectatorScreenController* GetSpectatorScreenController()
	{
		if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetHMDDevice())
		{
			return GEngine->XRSystem->GetHMDDevice()->GetSpectatorScreenController();
		}
		return nullptr;
	}
}

static void SpectatorScreenMode(const TArray<FString>& Args, UWorld* , FOutputDevice& Ar)
{
	ISpectatorScreenController* const Controller = HMDConsoleCommandsHelpers::GetSpectatorScreenController();
	if (Controller == nullptr)
	{
		Ar.Logf(ELogVerbosity::Error, TEXT("SpectatorScreenMode is not controllable now, cannot change or get mode."), *Args[0]);
		return;
	}

	const static UEnum* ScreenModeEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ESpectatorScreenMode"));
	int ModeVal = INDEX_NONE;
	if (Args.Num())
	{
		if (FCString::IsNumeric(*Args[0]))
		{
			ModeVal = FCString::Atoi(*Args[0]);
		}
		else
		{
			ModeVal = ScreenModeEnum->GetIndexByName(*Args[0]);
		}
		if (ModeVal < ESpectatorScreenModeFirst || ModeVal > ESpectatorScreenModeLast)
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid spectator screen mode: %s"), *Args[0]);
		}

		Controller->SetSpectatorScreenMode(ESpectatorScreenMode(ModeVal));
	}
	else
	{
		if (GEngine && GEngine->XRSystem.IsValid())
		{
			ModeVal = (int)Controller->GetSpectatorScreenMode();
		}
		Ar.Logf(TEXT("Spectator screen mode is set to: %s"), *ScreenModeEnum->GetNameStringByIndex(ModeVal));
	}
}

static FAutoConsoleCommand CSpectatorModeCmd(
	TEXT("vr.SpectatorScreenMode"),
	*LOCTEXT("CVarText_SpectatorScreenMode",
		"Changes the look of the spectator if supported by the HMD plugin.\n 0: disable mirroring\n 1: single eye\n 2: stereo pair\nNumbers larger than 2 may be possible and specify HMD plugin-specific variations.\nNegative values are treated the same as 0.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(SpectatorScreenMode));

static void HMDResetPosition(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		GEngine->XRSystem->ResetPosition();
	}
}

static FAutoConsoleCommand CHMDResetPositionCmd(
	TEXT("vr.HeadTracking.ResetPosition"),
	*LOCTEXT("CVarText_HMDResetPosition", "Reset the position of the head mounted display.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(HMDResetPosition));


static void HMDResetOrientation(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		float Yaw = 0.f;
		if (Args.Num() > 0)
		{
			Yaw = FCString::Atof(*Args[0]);
		}
		GEngine->XRSystem->ResetOrientation(Yaw);
	}
}

static FAutoConsoleCommand CHMDResetOrientationCmd(
	TEXT("vr.HeadTracking.ResetOrientation"),
	*LOCTEXT("CVarText_HMDResetOrientation", "Reset the rotation of the head mounted display.\nPass in an optional yaw for the new rotation in degrees .").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(HMDResetOrientation));

static void HMDReset(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		float Yaw = 0.f;
		if (Args.Num() > 0)
		{
			Yaw = FCString::Atof(*Args[0]);
		}
		GEngine->XRSystem->ResetOrientationAndPosition(Yaw);
	}
}

static FAutoConsoleCommand CHMDResetCmd(
	TEXT("vr.HeadTracking.Reset"),
	*LOCTEXT("CVarText_HMDReset", "Reset the rotation and position of the head mounted display.\nPass in an optional yaw for the new rotation in degrees.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(HMDReset));


static void HMDStatus(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		auto HMD = GEngine->XRSystem;
		Ar.Logf(TEXT("Position tracking status: %s\nHead tracking allowed: %s\nNumber of tracking sensors: %d"), 
			HMD->DoesSupportPositionalTracking() ? (HMD->HasValidTrackingPosition() ? TEXT("active") : TEXT("lost")) : TEXT("not supported"),
			HMD->IsHeadTrackingAllowed() ? TEXT("yes") : TEXT("no"),
			HMD->CountTrackedDevices(EXRTrackedDeviceType::TrackingReference));
	}
}

static FAutoConsoleCommand CHMDStatusCmd(
	TEXT("vr.HeadTracking.Status"),
	*LOCTEXT("CVarText_HMDStatus", "Reports the current status of the head tracking.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(HMDStatus));

static void EnableHMD(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	bool bEnable = 0;
	IHeadMountedDisplay* HMD = GEngine && GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;
	if (!HMD)
	{
		return;
	}
	
	if (Args.Num())
	{
		bEnable = FCString::ToBool(*Args[0]);
		HMD->EnableHMD(bEnable);

		/* TODO: calling EnableStereo after calling EnableHMD replicates the function library behavior:
		if (GEngine->StereoRenderingDevice.IsValid())
		{
			GEngine->StereoRenderingDevice->EnableStereo(bEnable);
		}
		*/
	}
	else
	{
		bEnable = HMD->IsHMDEnabled();
		Ar.Logf(TEXT("HMD device is %s"), bEnable?TEXT("enabled"):TEXT("disabled"));
	}
}

static FAutoConsoleCommand CEnableHMDCmd(
	TEXT("vr.bEnableHMD"),
	*LOCTEXT("CCommandText_EnableHMD", "Enables or disables the HMD device. Use 1, True, or Yes to enable, 0, False or No to disable.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(EnableHMD));

static void EnableStereo(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	bool bEnable = 0;
	IHeadMountedDisplay* HMD = GEngine && GEngine->XRSystem.IsValid() ? GEngine->XRSystem->GetHMDDevice() : nullptr;

	if (Args.Num())
	{
		bEnable = FCString::ToBool(*Args[0]);

		if (GEngine && GEngine->StereoRenderingDevice.IsValid())
		{
			if (HMD && !HMD->IsHMDEnabled())
			{
				Ar.Logf(TEXT("HMD is disabled. Use 'vr.bEnableHMD True' to re-enable it."));
			}
			GEngine->StereoRenderingDevice->EnableStereo(bEnable);
		}
	}
	else
	{
		if (GEngine && GEngine->StereoRenderingDevice.IsValid())
		{
			bEnable = GEngine->StereoRenderingDevice->IsStereoEnabled();
		}
		Ar.Logf(TEXT("Stereo is %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
	}
}

static FAutoConsoleCommand CEnableStereoCmd(
	TEXT("vr.bEnableStereo"),
	*LOCTEXT("CCommandText_EnableStereo", "Enables or disables the stereo rendering. Use 1, True, or Yes to enable, 0, False or No to disable.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(EnableStereo));

static void HMDVersion(const TArray<FString>& Args, UWorld*, FOutputDevice& Ar)
{
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		Ar.Logf(*GEngine->XRSystem->GetVersionString());
	}
}

static FAutoConsoleCommand CHMDVersionCmd(
	TEXT("vr.HMDVersion"),
	*LOCTEXT("CCommandText_HMDVersion", "Prints version information for the current HMD device.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(HMDVersion));

static void WorldToMeters(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar)
{
	if (Args.Num())
	{
		float WorldToMeters = -1;
		if (FCString::IsNumeric(*Args[0]))
		{
			WorldToMeters = FCString::Atof(*Args[0]);
		}

		if (WorldToMeters <= 0)
		{
			Ar.Logf(ELogVerbosity::Error, TEXT("Invalid argument, %s. World to meters scale must be larger than 0."), *Args[0]);
		}
		else
		{
			World->GetWorldSettings()->WorldToMeters = WorldToMeters;
		}
	}
	else
	{
		Ar.Logf(TEXT("World to meters scale is %0.2f"), World->GetWorldSettings()->WorldToMeters);
	}
}

static FAutoConsoleCommand CWorldToMetersCmd(
	TEXT("vr.WorldToMetersScale"),
	*LOCTEXT("CCommandText_WorldToMeters", 
		"Get or set the current world to meters scale.").ToString(),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(WorldToMeters));

/**
* Exec handler that aliases old deprecated VR console commands to the new ones.
*
* @param InWorld World context
* @param Cmd 	the exec command being executed
* @param Ar 	the archive to log results to
*
* @return true if the handler consumed the input, false to continue searching handlers
*/
static bool CompatExec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	const TCHAR* OrigCmd = Cmd;
	FString AliasedCommand;
	if (FParse::Command(&Cmd, TEXT("vr.SetTrackingOrigin")))
	{
		AliasedCommand = FString::Printf(TEXT("vr.TrackingOrigin %s"), Cmd);
	}
	if (FParse::Command(&Cmd, TEXT("vr.MirrorMode")))
	{
		AliasedCommand = FString::Printf(TEXT("vr.SpectatorScreenMode %s"), Cmd);
	}
	else if (FParse::Command(&Cmd, TEXT("HMDPOS")))
	{
		FString CmdName = FParse::Token(Cmd, 0);
		if (CmdName.Equals(TEXT("EYE"), ESearchCase::IgnoreCase) || CmdName.Equals(TEXT("FLOOR"), ESearchCase::IgnoreCase))
		{
			AliasedCommand = FString::Printf(TEXT("vr.TrackingOrigin %s"), *CmdName);
		}
	}
	else if (FParse::Command(&Cmd, TEXT("HMD")))
	{
		if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			AliasedCommand = TEXT("vr.bEnableHMD True");
		}
		if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			AliasedCommand = TEXT("vr.bEnableHMD False");
		}
		if (FParse::Command(&Cmd, TEXT("SP")) || FParse::Command(&Cmd, TEXT("SCREENPERCENTAGE")))
		{
			AliasedCommand = FString::Printf(TEXT("r.ScreenPercentage %s"), Cmd);
		}
	}
	else if (FParse::Command(&Cmd, TEXT("STEREO")))
	{
		FString Value;
		if (FParse::Command(&Cmd, TEXT("ON")) || FParse::Command(&Cmd, TEXT("ENABLE")))
		{
			AliasedCommand = TEXT("vr.bEnableStereo True");
		}
		else if (FParse::Command(&Cmd, TEXT("OFF")) || FParse::Command(&Cmd, TEXT("DISABLE")))
		{
			AliasedCommand = TEXT("vr.bEnableStereo False");
		}
		else if (FParse::Value(Cmd, TEXT("W2M="), Value))
		{
			AliasedCommand = FString::Printf(TEXT("vr.WorldToMetersScale %s"), *Value);
		}
	}
	else if (FParse::Command(&Cmd, TEXT("HMDVERSION")))
	{
		AliasedCommand = TEXT("vr.HMDVersion");
	}

	if (!AliasedCommand.IsEmpty())
	{
		Ar.Logf(ELogVerbosity::Warning, TEXT("%s is deprecated. Use %s instead"), OrigCmd, *AliasedCommand);

		return IConsoleManager::Get().ProcessUserConsoleInput(*AliasedCommand, Ar, InWorld);
	}
	else
	{
		return false;
	}
}

static FStaticSelfRegisteringExec CompatExecRegistration(CompatExec);

#undef LOCTEXT_NAMESPACE

