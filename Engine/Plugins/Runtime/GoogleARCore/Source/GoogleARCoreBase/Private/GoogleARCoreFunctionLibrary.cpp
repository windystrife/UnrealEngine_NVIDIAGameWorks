// Copyright 2017 Google Inc.

#include "GoogleARCoreFunctionLibrary.h"
#include "UnrealEngine.h"
#include "GoogleARCoreBaseLogCategory.h"
#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreDevice.h"
#include "GoogleARCoreHMD.h"
#include "Engine/Engine.h"
#include "LatentActions.h"
#include "GoogleARCorePointCloudManager.h"

#if PLATFORM_ANDROID
#include "tango_client_api.h"
#endif

namespace
{
	FGoogleARCoreHMD* GetTangoHMD()
	{
		if (GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == FName("FGoogleARCoreHMD")))
		{
			return static_cast<FGoogleARCoreHMD*>(GEngine->XRSystem.Get());
		}

		return nullptr;
	}

	const float DefaultLineTraceDistance = 100000; // 1000 meter
}

/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | Lifecycle                     */
/************************************************************************/
EGoogleARCoreSupportEnum UGoogleARCoreSessionFunctionLibrary::IsGoogleARCoreSupported()
{
	if (FGoogleARCoreDevice::GetInstance()->GetIsGoogleARCoreSupported())
	{
		return EGoogleARCoreSupportEnum::Supported;
	}

	return EGoogleARCoreSupportEnum::NotSupported;
}

void UGoogleARCoreSessionFunctionLibrary::GetCurrentSessionConfig(FGoogleARCoreSessionConfig& OutCurrentTangoConfig)
{
	FGoogleARCoreDevice::GetInstance()->GetCurrentSessionConfig(OutCurrentTangoConfig);
}

void UGoogleARCoreSessionFunctionLibrary::GetSessionRequiredRuntimPermissionsWithConfig(
	const FGoogleARCoreSessionConfig& Config,
	TArray<FString>& RuntimePermissions)
{
	FGoogleARCoreDevice::GetInstance()->GetRequiredRuntimePermissionsForConfiguration(Config, RuntimePermissions);
}

EGoogleARCoreSessionStatus UGoogleARCoreSessionFunctionLibrary::GetSessionStatus()
{
	if (!FGoogleARCoreDevice::GetInstance()->GetIsTangoRunning())
	{
		return EGoogleARCoreSessionStatus::NotStarted;
	}

	FGoogleARCorePose TempPose;
	if (!FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetCurrentPose(EGoogleARCoreReferenceFrame::DEVICE, TempPose))
	{
		return EGoogleARCoreSessionStatus::NotTracking;
	}

	return EGoogleARCoreSessionStatus::Tracking;
}

void UGoogleARCoreSessionFunctionLibrary::StartSession()
{
	FGoogleARCoreDevice::GetInstance()->StartTrackingSession();
}

void UGoogleARCoreSessionFunctionLibrary::StartSessionWithConfig(const FGoogleARCoreSessionConfig& Configuration)
{
	FGoogleARCoreDevice::GetInstance()->UpdateTangoConfiguration(Configuration);
	FGoogleARCoreDevice::GetInstance()->StartTrackingSession();
}

void UGoogleARCoreSessionFunctionLibrary::StopSession()
{
	FGoogleARCoreDevice::GetInstance()->StopTrackingSession();
}

/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | PassthroughCamera             */
/************************************************************************/
bool UGoogleARCoreSessionFunctionLibrary::IsPassthroughCameraRenderingEnabled()
{
	FGoogleARCoreHMD* TangoHMD = GetTangoHMD();
	if (TangoHMD)
	{
		return TangoHMD->GetColorCameraRenderingEnabled();
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to config TangoHMD: TangoHMD is not available."));
	}
	return false;
}

void UGoogleARCoreSessionFunctionLibrary::SetPassthroughCameraRenderingEnabled(bool bEnable)
{
	FGoogleARCoreHMD* TangoHMD = GetTangoHMD();
	if (TangoHMD)
	{
		TangoHMD->EnableColorCameraRendering(bEnable);
	}
	else
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to config TangoHMD: TangoHMD is not available."));
	}
}

void UGoogleARCoreSessionFunctionLibrary::GetPassthroughCameraImageUV(TArray<FVector2D>& CameraImageUV)
{
	FGoogleARCoreDevice::GetInstance()->TangoARCameraManager.GetCameraImageUV(CameraImageUV);
}

/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | ARAnchor                      */
/************************************************************************/
bool UGoogleARCoreSessionFunctionLibrary::SpawnARAnchorActor(UObject* WorldContextObject, UClass* ARAnchorActorClass, const FTransform& ARAnchorWorldTransform, AGoogleARCoreAnchorActor*& OutARAnchorActor)
{
	UGoogleARCoreAnchor* NewARAnchorObject = nullptr;

	if (!ARAnchorActorClass->IsChildOf(AGoogleARCoreAnchorActor::StaticClass()))
	{
		UE_LOG(LogGoogleARCore, Error, TEXT("Failed to spawn GoogleARAnchorActor. The requested ARAnchorActorClass is not a child of AGoogleARCoreAnchorActor."));
		return false;
	}

	if (!UGoogleARCoreSessionFunctionLibrary::CreateARAnchorObject(ARAnchorWorldTransform, NewARAnchorObject))
	{
		return false;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return false;
	}

	OutARAnchorActor = World->SpawnActor<AGoogleARCoreAnchorActor>(ARAnchorActorClass, NewARAnchorObject->GetLatestPose().Pose);
	if (OutARAnchorActor)
	{
		OutARAnchorActor->SetARAnchor(NewARAnchorObject);
	}

	return OutARAnchorActor != nullptr;
}

bool UGoogleARCoreSessionFunctionLibrary::CreateARAnchorObject(const FTransform& ARAnchorWorldTransform, UGoogleARCoreAnchor*& OutARAnchorObject)
{
	if (!FGoogleARCoreDevice::GetInstance()->GetIsTangoRunning())
	{
		return false;
	}

	OutARAnchorObject = FGoogleARCoreDevice::GetInstance()->ARAnchorManager->AddARAnchor(ARAnchorWorldTransform);

	return OutARAnchorObject != nullptr;
}

void UGoogleARCoreSessionFunctionLibrary::RemoveGoogleARAnchorObject(UGoogleARCoreAnchorBase* ARAnchorObject)
{
	if (FGoogleARCoreDevice::GetInstance()->ARAnchorManager)
	{
		FGoogleARCoreDevice::GetInstance()->ARAnchorManager->RemoveARAnchor(ARAnchorObject);
	}
}
/************************************************************************/
/*  UGoogleARCoreSessionFunctionLibrary | HitTest                       */
/************************************************************************/
bool UGoogleARCoreSessionFunctionLibrary::LineTraceSingleOnFeaturePoints(UObject* WorldContextObject, const FVector& Start, const FVector& End, FVector& ImpactPoint)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		FVector ImpactNormal;
		return FGoogleARCoreDevice::GetInstance()->TangoPointCloudManager.PerformLineTraceOnFeaturePoint(Start, End, ImpactPoint, ImpactNormal);
	}
	return false;
}

bool UGoogleARCoreSessionFunctionLibrary::LineTraceSingleOnPlanes(UObject* WorldContextObject, const FVector& Start, const FVector& End, FVector& ImpactPoint, FVector& ImpactNormal, UGoogleARCorePlane*& PlaneObject, bool bCheckBoundingBoxOnly)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
	{
		if (FGoogleARCoreDevice::GetInstance()->PlaneManager)
		{
			return FGoogleARCoreDevice::GetInstance()->PlaneManager->PerformLineTraceOnPlanes(Start, End, ImpactPoint, ImpactNormal, PlaneObject, bCheckBoundingBoxOnly);
		}
	}
	return false;
}

/************************************************************************/
/*  UGoogleARCoreFrameFunctionLibrary                                   */
/************************************************************************/
bool UGoogleARCoreFrameFunctionLibrary::GetLatestPose(EGoogleARCorePoseType RefFrame, FGoogleARCorePose& OutTangoPose)
{
	return FGoogleARCoreDevice::GetInstance()->TangoMotionManager.GetCurrentPose(static_cast<EGoogleARCoreReferenceFrame>(RefFrame), OutTangoPose);
}

void UGoogleARCoreFrameFunctionLibrary::GetAllPlanes(TArray<UGoogleARCorePlane*>& ARCorePlaneList)
{
	if (FGoogleARCoreDevice::GetInstance()->PlaneManager)
	{
		FGoogleARCoreDevice::GetInstance()->PlaneManager->GetAllPlanes(ARCorePlaneList);
	}
}

void UGoogleARCoreFrameFunctionLibrary::GetLatestLightEstimation(float& PixelIntensity)
{
	FGoogleARCoreDevice::GetInstance()->TangoARCameraManager.GetLatestLightEstimation(PixelIntensity);
}

FGoogleARCorePointCloud UGoogleARCoreFrameFunctionLibrary::GetLatestPointCloud()
{
	return FGoogleARCoreDevice::GetInstance()->TangoPointCloudManager.GetLatestPointCloud();
}

