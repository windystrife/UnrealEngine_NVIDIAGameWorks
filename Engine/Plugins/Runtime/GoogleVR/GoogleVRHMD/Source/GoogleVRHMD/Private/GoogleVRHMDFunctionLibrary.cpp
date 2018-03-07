// Copyright 2017 Google Inc.

#include "Classes/GoogleVRHMDFunctionLibrary.h"
#include "GoogleVRHMD.h"

UGoogleVRHMDFunctionLibrary::UGoogleVRHMDFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

static FGoogleVRHMD* GetHMD()
{
	if (GEngine->XRSystem.IsValid() && GEngine->XRSystem->GetSystemName() == FName("FGoogleVRHMD"))
	{
		return static_cast<FGoogleVRHMD*>(GEngine->XRSystem.Get());
	}

	return nullptr;
}

bool UGoogleVRHMDFunctionLibrary::IsGoogleVRHMDEnabled()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsHMDEnabled();
	}
	return false;
}

bool UGoogleVRHMDFunctionLibrary::IsGoogleVRStereoRenderingEnabled()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsStereoEnabled();
	}
	return false;
}

void UGoogleVRHMDFunctionLibrary::SetSustainedPerformanceModeEnabled(bool bEnable)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetSPMEnable(bEnable);
	}
}

void UGoogleVRHMDFunctionLibrary::SetDistortionCorrectionEnabled(bool bEnable)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		HMD->SetDistortionCorrectionEnabled(bEnable);
	}
}

bool UGoogleVRHMDFunctionLibrary::SetDefaultViewerProfile(const FString& ViewerProfileURL)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetDefaultViewerProfile(ViewerProfileURL);
	}

	return false;
}

void UGoogleVRHMDFunctionLibrary::SetDistortionMeshSize(EDistortionMeshSizeEnum MeshSize)
{
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD)
	{
		HMD->SetDistortionMeshSize(MeshSize);
	}
}

bool UGoogleVRHMDFunctionLibrary::GetDistortionCorrectionEnabled()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetDistortionCorrectionEnabled();
	}

	return false;
}

FString UGoogleVRHMDFunctionLibrary::GetViewerModel()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetViewerModel();
	}

	return TEXT("");
}

FString UGoogleVRHMDFunctionLibrary::GetViewerVendor()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetViewerVendor();
	}

	return TEXT("");
}

bool UGoogleVRHMDFunctionLibrary::IsVrLaunch()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsVrLaunch();
	}

	return false;
}

bool UGoogleVRHMDFunctionLibrary::IsInDaydreamMode()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->IsInDaydreamMode();
	}

	return false;
}

FIntPoint UGoogleVRHMDFunctionLibrary::GetGVRHMDRenderTargetSize()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->GetGVRHMDRenderTargetSize();
	}

	return FIntPoint::ZeroValue;
}

FIntPoint UGoogleVRHMDFunctionLibrary::SetRenderTargetSizeToDefault()
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetRenderTargetSizeToDefault();
	}

	return FIntPoint::ZeroValue;
}

bool UGoogleVRHMDFunctionLibrary::SetGVRHMDRenderTargetScale(float ScaleFactor, FIntPoint& OutRenderTargetSize)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetGVRHMDRenderTargetSize(ScaleFactor, OutRenderTargetSize);
	}

	return false;
}

bool UGoogleVRHMDFunctionLibrary::SetGVRHMDRenderTargetSize(int DesiredWidth, int DesiredHeight, FIntPoint& OutRenderTargetSize)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetGVRHMDRenderTargetSize(DesiredWidth, DesiredHeight, OutRenderTargetSize);
	}

	return false;
}

void UGoogleVRHMDFunctionLibrary::SetNeckModelScale(float ScaleFactor)
{
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD)
	{
		return HMD->SetNeckModelScale(ScaleFactor);
	}
}

float UGoogleVRHMDFunctionLibrary::GetNeckModelScale()
{
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD)
	{
		return HMD->GetNeckModelScale();
	}

	return 0.0f;
}

FString UGoogleVRHMDFunctionLibrary::GetIntentData()
{
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD)
	{
		return HMD->GetIntentData();
	}

	return TEXT("");
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenEnable(bool bEnable)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->bEnableSplashScreen = bEnable;
	}
#endif
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenTexture(UTexture2D* Texture, FVector2D UVOffset, FVector2D UVSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid() && Texture)
	{
		HMD->GVRSplash->SplashTexture = Texture;
		HMD->GVRSplash->SplashTexturePath = "";
		HMD->GVRSplash->SplashTextureUVOffset = UVOffset;
		HMD->GVRSplash->SplashTextureUVSize = UVSize;
	}
#endif
}

void UGoogleVRHMDFunctionLibrary::ClearDaydreamLoadingSplashScreenTexture()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->SplashTexture = nullptr;
		HMD->GVRSplash->SplashTexturePath = "";
	}
#endif
}

float UGoogleVRHMDFunctionLibrary::GetDaydreamLoadingSplashScreenDistance()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		return HMD->GVRSplash->RenderDistanceInMeter;
	}
#endif
	return 0.0f;
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenDistance(float NewDistanceInMeter)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->RenderDistanceInMeter = NewDistanceInMeter;
	}
#endif
}

float UGoogleVRHMDFunctionLibrary::GetDaydreamLoadingSplashScreenScale()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		return HMD->GVRSplash->RenderScale;
	}
#endif
	return 0.0f;
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenScale(float NewSize)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->RenderScale = NewSize;
	}
#endif
}

float UGoogleVRHMDFunctionLibrary::GetDaydreamLoadingSplashScreenViewAngle()
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		return HMD->GVRSplash->ViewAngleInDegree;
	}
#endif
	return 0.0f;
}

void UGoogleVRHMDFunctionLibrary::SetDaydreamLoadingSplashScreenViewAngle(float NewViewAngle)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if(HMD && HMD->GVRSplash.IsValid())
	{
		HMD->GVRSplash->ViewAngleInDegree = NewViewAngle;
	}
#endif
}

bool UGoogleVRHMDFunctionLibrary::GetFloorHeight(float& FloorHeight)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD && HMD->GetFloorHeight(&FloorHeight))
	{
		return true;
	}
#endif
	return false;
}

bool UGoogleVRHMDFunctionLibrary::GetSafetyCylinderInnerRadius(float& InnerRadius)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD && HMD->GetSafetyCylinderInnerRadius(&InnerRadius))
	{
		return true;
	}
#endif
	return false;
}

bool UGoogleVRHMDFunctionLibrary::GetSafetyCylinderOuterRadius(float& OuterRadius)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD && HMD->GetSafetyCylinderOuterRadius(&OuterRadius))
	{
		return true;
	}
#endif
	return false;
}

bool UGoogleVRHMDFunctionLibrary::GetSafetyRegion(ESafetyRegionType& RegionType)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD && HMD->GetSafetyRegion(&RegionType))
	{
		return true;
	}
#endif
	return false;
}

bool UGoogleVRHMDFunctionLibrary::GetRecenterTransform(FQuat& RecenterOrientation, FVector& RecenterPosition)
{
#if GOOGLEVRHMD_SUPPORTED_PLATFORMS
	FGoogleVRHMD* HMD = GetHMD();
	if (HMD && HMD->GetRecenterTransform(RecenterOrientation, RecenterPosition))
	{
		return true;
	}
#endif
	return false;
}