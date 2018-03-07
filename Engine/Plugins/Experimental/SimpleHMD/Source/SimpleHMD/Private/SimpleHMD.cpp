// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SimpleHMD.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "ISimpleHMDPlugin.h"
#include "SceneRendering.h"
#include "PostProcess/PostProcessHMD.h"

//---------------------------------------------------
// SimpleHMD Plugin Implementation
//---------------------------------------------------

class FSimpleHMDPlugin : public ISimpleHMDPlugin
{
	/** IHeadMountedDisplayModule implementation */
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;

	FString GetModuleKeyName() const override
	{
		return FString(TEXT("SimpleHMD"));
	}
};

IMPLEMENT_MODULE( FSimpleHMDPlugin, SimpleHMD )

TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > FSimpleHMDPlugin::CreateTrackingSystem()
{
	auto SimpleHMD = FSceneViewExtensions::NewExtension<FSimpleHMD>();
	if( SimpleHMD->IsInitialized() )
	{
		return SimpleHMD;
	}
	return nullptr;
}


float FSimpleHMD::GetWorldToMetersScale() const
{
	return GWorld ? GWorld->GetWorldSettings()->WorldToMeters : 100.0f;
}

//---------------------------------------------------
// SimpleHMD IHeadMountedDisplay Implementation
//---------------------------------------------------

bool FSimpleHMD::IsHMDEnabled() const
{
	return true;
}

void FSimpleHMD::EnableHMD(bool enable)
{
}

EHMDDeviceType::Type FSimpleHMD::GetHMDDeviceType() const
{
	return EHMDDeviceType::DT_ES2GenericStereoMesh;
}

bool FSimpleHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
	MonitorDesc.MonitorName = "";
	MonitorDesc.MonitorId = 0;
	MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
	return false;
}

void FSimpleHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
	OutHFOVInDegrees = 0.0f;
	OutVFOVInDegrees = 0.0f;
}

void FSimpleHMD::RefreshPoses()
{
}

bool FSimpleHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
		return true;
	}
	return false;
}

void FSimpleHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
}

float FSimpleHMD::GetInterpupillaryDistance() const
{
	return 0.064f;
}

void FSimpleHMD::GetHMDOrientation(FQuat& CurrentOrientation)
{
	// very basic.  no head model, no prediction, using debuglocalplayer
	ULocalPlayer* Player = GEngine->GetDebugLocalPlayer();

	if (Player != NULL && Player->PlayerController != NULL)
	{
		FVector RotationRate = Player->PlayerController->GetInputVectorKeyState(EKeys::RotationRate);

		double CurrentTime = FApp::GetCurrentTime();
		double DeltaTime = 0.0;

		if (LastSensorTime >= 0.0)
		{
			DeltaTime = CurrentTime - LastSensorTime;
		}

		LastSensorTime = CurrentTime;

		// mostly incorrect, but we just want some sensor input for testing
		RotationRate *= DeltaTime;
		CurrentOrientation *= FQuat(FRotator(FMath::RadiansToDegrees(-RotationRate.X), FMath::RadiansToDegrees(-RotationRate.Y), FMath::RadiansToDegrees(-RotationRate.Z)));
	}
	else
	{
		CurrentOrientation = FQuat(FRotator(0.0f, 0.0f, 0.0f));
	}
}

bool FSimpleHMD::GetCurrentPose(int32 DeviceId, FQuat& CurrentOrientation, FVector& CurrentPosition)
{
	if (DeviceId != IXRTrackingSystem::HMDDeviceId)
	{
		return false;
	}
	CurrentOrientation = CurHmdOrientation;
	CurrentPosition = FVector(0.0f, 0.0f, 0.0f);
	GetHMDOrientation(CurrentOrientation);
	CurHmdOrientation = LastHmdOrientation = CurrentOrientation;
	return true;
}

bool FSimpleHMD::IsChromaAbCorrectionEnabled() const
{
	return false;
}

void FSimpleHMD::ResetOrientationAndPosition(float yaw)
{
	ResetOrientation(yaw);
	ResetPosition();
}

void FSimpleHMD::ResetOrientation(float Yaw)
{
}

void FSimpleHMD::ResetPosition()
{
}

void FSimpleHMD::SetBaseRotation(const FRotator& BaseRot)
{
}

FRotator FSimpleHMD::GetBaseRotation() const
{
	return FRotator::ZeroRotator;
}

void FSimpleHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
}

FQuat FSimpleHMD::GetBaseOrientation() const
{
	return FQuat::Identity;
}

void FSimpleHMD::DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize)
{
	float ClipSpaceQuadZ = 0.0f;
	FMatrix QuadTexTransform = FMatrix::Identity;
	FMatrix QuadPosTransform = FMatrix::Identity;
	const FSceneView& View = Context.View;
	const FIntRect SrcRect = View.ViewRect;

	FRHICommandListImmediate& RHICmdList = Context.RHICmdList;
	const FSceneViewFamily& ViewFamily = *(View.Family);
	FIntPoint ViewportSize = ViewFamily.RenderTarget->GetSizeXY();
	RHICmdList.SetViewport(0, 0, 0.0f, ViewportSize.X, ViewportSize.Y, 1.0f);

	static const uint32 NumVerts = 8;
	static const uint32 NumTris = 4;

	static const FDistortionVertex Verts[8] =
	{
		// left eye
		{ FVector2D(-0.9f, -0.9f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(0.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(-0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(-0.9f, 0.9f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 0.0f), 1.0f, 0.0f },
		// right eye
		{ FVector2D(0.1f, -0.9f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), FVector2D(0.5f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, -0.9f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), FVector2D(1.0f, 1.0f), 1.0f, 0.0f },
		{ FVector2D(0.9f, 0.9f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), FVector2D(1.0f, 0.0f), 1.0f, 0.0f },
		{ FVector2D(0.1f, 0.9f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 1.0f, 0.0f },
	};

	static const uint16 Indices[12] = { /*Left*/ 0, 1, 2, 0, 2, 3, /*Right*/ 4, 5, 6, 4, 6, 7 };

	DrawIndexedPrimitiveUP(Context.RHICmdList, PT_TriangleList, 0, NumVerts, NumTris, &Indices,
		sizeof(Indices[0]), &Verts, sizeof(Verts[0]));
}

bool FSimpleHMD::IsStereoEnabled() const
{
	return true;
}

bool FSimpleHMD::EnableStereo(bool stereo)
{
	return true;
}

void FSimpleHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
	SizeX = SizeX / 2;
	if( StereoPass == eSSP_RIGHT_EYE )
	{
		X += SizeX;
	}
}

void FSimpleHMD::CalculateStereoViewOffset(const enum EStereoscopicPass StereoPassType, FRotator& ViewRotation, const float WorldToMeters, FVector& ViewLocation)
{
	if( StereoPassType != eSSP_FULL)
	{
		float EyeOffset = 3.20000005f;
		const float PassOffset = (StereoPassType == eSSP_LEFT_EYE) ? EyeOffset : -EyeOffset;
		ViewLocation += ViewRotation.Quaternion().RotateVector(FVector(0,PassOffset,0));
	}
}

FMatrix FSimpleHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
	const float ProjectionCenterOffset = 0.151976421f;
	const float PassProjectionOffset = (StereoPassType == eSSP_LEFT_EYE) ? ProjectionCenterOffset : -ProjectionCenterOffset;

	const float HalfFov = 2.19686294f / 2.f;
	const float InWidth = 640.f;
	const float InHeight = 480.f;
	const float XS = 1.0f / tan(HalfFov);
	const float YS = InWidth / tan(HalfFov) / InHeight;

	const float InNearZ = GNearClippingPlane;
	return FMatrix(
		FPlane(XS,                      0.0f,								    0.0f,							0.0f),
		FPlane(0.0f,					YS,	                                    0.0f,							0.0f),
		FPlane(0.0f,	                0.0f,								    0.0f,							1.0f),
		FPlane(0.0f,					0.0f,								    InNearZ,						0.0f))

		* FTranslationMatrix(FVector(PassProjectionOffset,0,0));
}

void FSimpleHMD::GetEyeRenderParams_RenderThread(const FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const
{
	EyeToSrcUVOffsetValue = FVector2D::ZeroVector;
	EyeToSrcUVScaleValue = FVector2D(1.0f, 1.0f);
}


void FSimpleHMD::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	InViewFamily.EngineShowFlags.MotionBlur = 0;
	InViewFamily.EngineShowFlags.HMDDistortion = true;
	InViewFamily.EngineShowFlags.SetScreenPercentage(true);
	InViewFamily.EngineShowFlags.StereoRendering = IsStereoEnabled();
}

void FSimpleHMD::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	InView.BaseHmdOrientation = FQuat(FRotator(0.0f,0.0f,0.0f));
	InView.BaseHmdLocation = FVector(0.f);
	InViewFamily.bUseSeparateRenderTarget = false;
}

void FSimpleHMD::PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView)
{
	check(IsInRenderingThread());
}

void FSimpleHMD::PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily)
{
	check(IsInRenderingThread());
}

bool FSimpleHMD::IsActiveThisFrame(class FViewport* InViewport) const
{
	return GEngine && GEngine->IsStereoscopic3D(InViewport);
}

FSimpleHMD::FSimpleHMD(const FAutoRegister& AutoRegister) :
	FSceneViewExtensionBase(AutoRegister),
	CurHmdOrientation(FQuat::Identity),
	LastHmdOrientation(FQuat::Identity),
	DeltaControlRotation(FRotator::ZeroRotator),
	DeltaControlOrientation(FQuat::Identity),
	LastSensorTime(-1.0)
{
}

FSimpleHMD::~FSimpleHMD()
{
}

bool FSimpleHMD::IsInitialized() const
{
	return true;
}
