// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HeadMountedDisplayBase.h"

#include "DefaultStereoLayers.h"
#include "EngineAnalytics.h"
#include "IAnalyticsProvider.h"
#include "AnalyticsEventAttribute.h"
#include "Misc/CoreDelegates.h"
#include "RenderingThread.h"
#include "Engine/Texture.h"
#include "DefaultSpectatorScreenController.h"
#include "DefaultXRCamera.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h" // for UEditorEngine::IsHMDTrackingAllowed()
#endif

// including interface headers without their own implementation file, so that 
// functions (default ctors, etc.) get compiled into this module
#include "IXRDeviceAssets.h"

void FHeadMountedDisplayBase::RecordAnalytics()
{
	TArray<FAnalyticsEventAttribute> EventAttributes;
	if (FEngineAnalytics::IsAvailable() && PopulateAnalyticsAttributes(EventAttributes))
	{
		// send analytics data
		FString OutStr(TEXT("Editor.VR.DeviceInitialised"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

bool FHeadMountedDisplayBase::PopulateAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& EventAttributes)
{
	IHeadMountedDisplay::MonitorInfo MonitorInfo;
	GetHMDMonitorInfo(MonitorInfo);

	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DeviceName"), GetSystemName().ToString()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayDeviceName"), *MonitorInfo.MonitorName));
#if PLATFORM_WINDOWS
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), MonitorInfo.MonitorId));
#else // Other platforms need some help in formatting size_t as text
	FString DisplayId(FString::Printf(TEXT("%llu"), (uint64)MonitorInfo.MonitorId));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("DisplayId"), DisplayId));
#endif
	FString MonResolution(FString::Printf(TEXT("(%d, %d)"), MonitorInfo.ResolutionX, MonitorInfo.ResolutionY));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Resolution"), MonResolution));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("InterpupillaryDistance"), GetInterpupillaryDistance()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("ChromaAbCorrectionEnabled"), IsChromaAbCorrectionEnabled()));
	EventAttributes.Add(FAnalyticsEventAttribute(TEXT("MirrorToWindow"), IsSpectatorScreenActive()));

	return true;
}

bool FHeadMountedDisplayBase::IsHeadTrackingAllowed() const
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		// @todo vreditor: We need to do a pass over VREditor code and make sure we are handling the VR modes correctly.  HeadTracking can be enabled without Stereo3D, for example
		UEditorEngine* EdEngine = Cast<UEditorEngine>(GEngine);
		return (!EdEngine || EdEngine->IsHMDTrackingAllowed()) && IsStereoEnabled();
	}
#endif // WITH_EDITOR
	return IsStereoEnabled();
}

IStereoLayers* FHeadMountedDisplayBase::GetStereoLayers()
{
	if (!DefaultStereoLayers.IsValid())
	{
		DefaultStereoLayers = FSceneViewExtensions::NewExtension<FDefaultStereoLayers>(this);
	}
	return DefaultStereoLayers.Get();
}

bool FHeadMountedDisplayBase::GetHMDDistortionEnabled() const
{
	return true;
}

FVector2D FHeadMountedDisplayBase::GetEyeCenterPoint_RenderThread(EStereoscopicPass Eye) const
{
	check(IsInRenderingThread());

	if (!IsStereoEnabled())
	{
		return FVector2D(0.5f, 0.5f);
	}

	const FMatrix StereoProjectionMatrix = GetStereoProjectionMatrix(Eye);
	//0,0,1 is the straight ahead point, wherever it maps to is the center of the projection plane in -1..1 coordinates.  -1,-1 is bottom left.
	const FVector4 ScreenCenter = StereoProjectionMatrix.TransformPosition(FVector(0.0f, 0.0f, 1.0f));
	//transform into 0-1 screen coordinates 0,0 is top left.  
	const FVector2D CenterPoint(0.5f + (ScreenCenter.X / 2.0f), 0.5f - (ScreenCenter.Y / 2.0f));
	return CenterPoint;
}


void FHeadMountedDisplayBase::BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& /* RHICmdList */, FSceneViewFamily& /* ViewFamily */)
{
	if (DefaultStereoLayers.IsValid())
	{
		DefaultStereoLayers->UpdateHmdTransform(NewRelativeTransform);
	}
}

void FHeadMountedDisplayBase::BeginRendering_GameThread()
{
}

void FHeadMountedDisplayBase::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	GetXRCamera()->CalculateStereoCameraOffset(StereoPassType, ViewRotation, ViewLocation);
}

void FHeadMountedDisplayBase::InitCanvasFromView(FSceneView* InView, UCanvas* Canvas)
{
}

bool FHeadMountedDisplayBase::IsSpectatorScreenActive() const
{
	ISpectatorScreenController const * Controller = GetSpectatorScreenController();
	return (Controller && Controller->GetSpectatorScreenMode() != ESpectatorScreenMode::Disabled);
}

ISpectatorScreenController* FHeadMountedDisplayBase::GetSpectatorScreenController()
{
	return SpectatorScreenController.Get();
}

class ISpectatorScreenController const * FHeadMountedDisplayBase::GetSpectatorScreenController() const
{
	return SpectatorScreenController.Get();
}
