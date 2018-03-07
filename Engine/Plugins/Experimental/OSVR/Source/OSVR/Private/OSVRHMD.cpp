//
// Copyright 2014, 2015 Razer Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//


#include "OSVRHMD.h"
#include "OSVRPrivate.h"
#include "OSVRTypes.h"
#include "SharedPointer.h"
#include "SceneViewport.h"
#include "OSVREntryPoint.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameEngine.h"
#include "UnrealEngine.h"

#include "Scalability.h"
#include "Runtime/Core/Public/Misc/DateTime.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Classes/Editor/EditorEngine.h"
#endif

#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include <osvr/Util/ReturnCodesC.h>
#include <osvr/RenderKit/RenderManagerD3D11C.h>
#include "HideWindowsPlatformTypes.h"
#else
#include <osvr/RenderKit/RenderManagerOpenGLC.h>
#endif

#include <osvr/Util/MatrixConventionsC.h>
#include <osvr/ClientKit/ParametersC.h>

DEFINE_LOG_CATEGORY(OSVRHMDLog);

//---------------------------------------------------
// IHeadMountedDisplay Implementation
//---------------------------------------------------

void FOSVRHMD::OnBeginPlay(FWorldContext& InWorldContext)
{
    bPlaying = true;
    StartCustomPresent();
}

void FOSVRHMD::OnEndPlay(FWorldContext& InWorldContext)
{
    bPlaying = false;
    StopCustomPresent();
}

void FOSVRHMD::StartCustomPresent()
{
#if PLATFORM_WINDOWS
    if (!mCustomPresent && IsPCPlatform(GMaxRHIShaderPlatform) && !IsOpenGLPlatform(GMaxRHIShaderPlatform))
    {
        // currently, FCustomPresent creates its own client context, so no need to
        // synchronize with the one from FOSVREntryPoint.
        mCustomPresent = new FCurrentCustomPresent(nullptr/*osvrClientContext*/);
    }
#endif
}

void FOSVRHMD::StopCustomPresent()
{
    mCustomPresent = nullptr;
}

bool FOSVRHMD::IsHMDConnected()
{
    return bHmdConnected;
}

bool FOSVRHMD::IsHMDEnabled() const
{
    return bHmdConnected && bHmdEnabled;
}

void FOSVRHMD::EnableHMD(bool bEnable)
{
    // Make EnableHMD idempotent so that it and EnableStereo can call each other
    if (bHmdEnabled == bEnable)
    {
        return;
    }
    bHmdEnabled = bEnable;
    EnableStereo(bHmdEnabled);
}

EHMDDeviceType::Type FOSVRHMD::GetHMDDeviceType() const
{
    return EHMDDeviceType::DT_ES2GenericStereoMesh;
}

/**
* This is more of a temporary workaround to an issue with getting the render target
* size from the RenderManager. On the game thread, we can't get the render target sizes
* unless we have already initialized the render manager, which we can only do on the render
* thread. In the future, we'll move those RenderManager APIs to OSVR-Core so we can call
* them from any thread with access to the client context.
*/
void FOSVRHMD::GetRenderTargetSize_GameThread(float windowWidth, float windowHeight, float &width, float &height)
{
    auto clientContext = mOSVREntryPoint->GetClientContext();
    size_t length;
    osvrClientGetStringParameterLength(clientContext, "/renderManagerConfig", &length);
    if (length > 0)
    {
        char* renderManagerConfigStr = new char[length];
        osvrClientGetStringParameter(clientContext, "/renderManagerConfig", renderManagerConfigStr, length);

        auto reader = TJsonReaderFactory<>::Create(renderManagerConfigStr);
        TSharedPtr<FJsonObject> jsonObject;
        if (FJsonSerializer::Deserialize(reader, jsonObject))
        {
            auto subObj = jsonObject->GetObjectField("renderManagerConfig");
            double renderOverfillFactor = 1.0f;
            double renderOversampleFactor = 1.0f;

            if (subObj->HasTypedField<EJson::Number>("renderOverfillFactor"))
            {
                renderOverfillFactor = subObj->GetNumberField("renderOverfillFactor");
            }
            if (subObj->HasTypedField<EJson::Number>("renderOversampleFactor"))
            {
                renderOversampleFactor = subObj->GetNumberField("renderOversampleFactor");
            }

            width = windowWidth * renderOverfillFactor * renderOversampleFactor;
            height = windowHeight * renderOverfillFactor * renderOversampleFactor;
        }
        delete[] renderManagerConfigStr;
    }
    else
    {
        width = windowWidth;
        height = windowHeight;
    }
}

bool FOSVRHMD::GetHMDMonitorInfo(MonitorInfo& MonitorDesc)
{
    FScopeLock lock(mOSVREntryPoint->GetClientContextMutex());
    if (IsInitialized()
        && osvrClientCheckDisplayStartup(DisplayConfig) == OSVR_RETURN_SUCCESS)
    {
        auto leftEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::LEFT_EYE);
        auto rightEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::RIGHT_EYE);
        OSVR_ViewportDimension width = (OSVR_ViewportDimension)leftEye.X + (OSVR_ViewportDimension)rightEye.X;
        OSVR_ViewportDimension height = (OSVR_ViewportDimension)leftEye.Y;

        float fWidth, fHeight;
        GetRenderTargetSize_GameThread(width, height, fWidth, fHeight);
        MonitorDesc.MonitorName = "OSVR-Display"; //@TODO
        MonitorDesc.MonitorId = 0;				  //@TODO
        MonitorDesc.DesktopX = 0;
        MonitorDesc.DesktopY = 0;
        MonitorDesc.ResolutionX = fWidth;
        MonitorDesc.ResolutionY = fHeight;
        return true;
    }
    else
    {
        MonitorDesc.MonitorName = "";
        MonitorDesc.MonitorId = 0;
        MonitorDesc.DesktopX = MonitorDesc.DesktopY = MonitorDesc.ResolutionX = MonitorDesc.ResolutionY = 0;
    }

    return false;
}

void FOSVRHMD::RefreshPoses()
{
    OSVR_Pose3 pose;
    OSVR_ReturnCode returnCode;
    FScopeLock lock(mOSVREntryPoint->GetClientContextMutex());
    auto clientContext = mOSVREntryPoint->GetClientContext();

    returnCode = osvrClientUpdate(clientContext);
    check(returnCode == OSVR_RETURN_SUCCESS);

    returnCode = osvrClientGetViewerPose(DisplayConfig, 0, &pose);
    if (returnCode == OSVR_RETURN_SUCCESS)
    {
        LastHmdOrientation = CurHmdOrientation;
        LastHmdPosition = CurHmdPosition;
        CurHmdPosition = BaseOrientation.Inverse().RotateVector(OSVR2FVector(pose.translation, WorldToMetersScale) - BasePosition);
        CurHmdOrientation = BaseOrientation.Inverse() * OSVR2FQuat(pose.rotation);
		bHasValidPose = true;
    }
	else
	{
		bHasValidPose = false;
	}
}

bool FOSVRHMD::DoesSupportPositionalTracking() const
{
    return true;
}

bool FOSVRHMD::HasValidTrackingPosition()
{
    return bHaveVisionTracking;
}

bool FOSVRHMD::OnStartGameFrame(FWorldContext& WorldContext)
{
    check(IsInGameThread());
    static auto sFinishCurrentFrame = IConsoleManager::Get().FindConsoleVariable(TEXT("r.FinishCurrentFrame"));
    if (!bHmdOverridesApplied)
    {
        sFinishCurrentFrame->Set(0);
        bHmdOverridesApplied = true;
    }
	if (GWorld != nullptr)
	{
		WorldToMetersScale = GWorld->GetWorldSettings()->WorldToMeters;
	}
	RefreshPoses();
    return true;
}

float FOSVRHMD::GetInterpupillaryDistance() const
{
    return HMDDescription.GetInterpupillaryDistance();
}

void FOSVRHMD::SetInterpupillaryDistance(float NewInterpupillaryDistance)
{
    // intentionally left blank
}

void FOSVRHMD::GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const
{
    OutHFOVInDegrees = 0.0f;
    OutVFOVInDegrees = 0.0f;
}

bool FOSVRHMD::EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type /*= EXRTrackedDeviceType::Any*/)
{
	if (Type == EXRTrackedDeviceType::Any || Type == EXRTrackedDeviceType::HeadMountedDisplay)
	{
		OutDevices.Add(IXRTrackingSystem::HMDDeviceId);
		return true;
	}
	return false;
}

bool FOSVRHMD::GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition)
{
	if (DeviceId != HMDDeviceId)
	{
		return false;
	}

	OutOrientation = CurHmdOrientation;
	OutPosition = CurHmdPosition;
	return bHasValidPose;
}

void FOSVRHMD::RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const
{
    // @TODO ???
}
bool FOSVRHMD::IsChromaAbCorrectionEnabled() const
{
    // @TODO - why does Unreal need to know this? We're doing distortion/chroma correction
    // in render manager.
    return false;
}

//---------------------------------------------------
// IStereoRendering Implementation
//---------------------------------------------------

bool FOSVRHMD::IsStereoEnabled() const
{
    return bStereoEnabled && bHmdEnabled;
}

bool FOSVRHMD::EnableStereo(bool bStereo)
{
    bool bNewSteroEnabled = IsHMDConnected() ? bStereo : false;
    if (bNewSteroEnabled == bStereoEnabled)
    {
        return bStereoEnabled;
    }
    bStereoEnabled = bNewSteroEnabled;

    if (bStereoEnabled)
    {
        StartCustomPresent();
    }
    else
    {
        StopCustomPresent();
    }

    if (bHmdEnabled != bStereoEnabled)
    {
        EnableHMD(bStereoEnabled);
    }

    auto leftEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::LEFT_EYE);
    auto rightEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::RIGHT_EYE);
    auto width = leftEye.X + rightEye.X;
    auto height = leftEye.Y;

    GetRenderTargetSize_GameThread(width, height, width, height);

    // On Android, we currently use the resolution Unreal sets for us, bypassing OSVR
    // We may revisit once display plugins are added to OSVR-Core.
#if !PLATFORM_ANDROID
    FSystemResolution::RequestResolutionChange(width, height, EWindowMode::Windowed);
#endif


    FSceneViewport* sceneViewport;
    if (!GIsEditor)
    {
        //UE_LOG(OSVRHMDLog, Warning, TEXT("OSVR getting UGameEngine::SceneViewport viewport"));
        UGameEngine* gameEngine = Cast<UGameEngine>(GEngine);
        sceneViewport = gameEngine->SceneViewport.Get();
    }
#if WITH_EDITOR
    else
    {
        //UE_LOG(OSVRHMDLog, Warning, TEXT("OSVR getting editor viewport"));
        UEditorEngine* editorEngine = CastChecked<UEditorEngine>(GEngine);
        sceneViewport = (FSceneViewport*)editorEngine->GetPIEViewport();
        if (sceneViewport == nullptr || !sceneViewport->IsStereoRenderingAllowed())
        {
            sceneViewport = (FSceneViewport*)editorEngine->GetActiveViewport();
            if (sceneViewport != nullptr && !sceneViewport->IsStereoRenderingAllowed())
            {
                sceneViewport = nullptr;
            }
        }
    }
#endif

    if (!sceneViewport)
    {
        UE_LOG(OSVRHMDLog, Warning, TEXT("OSVR scene viewport does not exist"));
        return false;
    }
    else
    {
        //UE_LOG(OSVRHMDLog, Warning, TEXT("OSVR scene viewport exists"));
#if !WITH_EDITOR
        auto window = sceneViewport->FindWindow();
#endif
        if (bStereo)
        {
            //UE_LOG(OSVRHMDLog, Warning, TEXT("OSVR bStereo was true"));
            // the render targets may be larger or smaller than the display resolution
            // due to renderOverfillFactor and renderOversampleFactor settings
            // The viewports should match the render target size not the display size
            //if (mCustomPresent)
            //{
            //uint32 iWidth, iHeight;
            //mCustomPresent->CalculateRenderTargetSize(iWidth, iHeight);
            //float screenScale = GetScreenScale();
            //width = float(iWidth) * (1.0f / screenScale);
            //height = float(iHeight) * (1.0f / screenScale);
            //}
            //else
            //{
            // temporary workaround. The above code doesn't work because when the game
            // is packaged, mCustomPresent is not initialized before this call. In the editor, it is.
            // calling CalculateRenderTargetSize when mCustomPresent isn't initialized
            // results in Initialize being called, which has to be done on the render thread.
            // The proper fix is to move the render target size API from render manager to OSVR-Core
            // so we don't need a graphics context to calculate them. In the meantime, we'll
            // implement this temporary workaround (parse the renderManagerConfig manually and
            // calculate the render target sizes ourselves).

            //}
            //UE_LOG(OSVRHMDLog, Warning, TEXT("OSVR Actually set viewport size"));
            sceneViewport->SetViewportSize(width, height);
#if !WITH_EDITOR
            if (window.IsValid())
            {
                window->SetViewportSizeDrivenByWindow(false);
            }
#endif
        }
#if !WITH_EDITOR
        else
        {
            if (window.IsValid())
            {
                auto size = sceneViewport->FindWindow()->GetSizeInScreen();
                sceneViewport->SetViewportSize(size.X, size.Y);
                window->SetViewportSizeDrivenByWindow(true);
            }
        }
#endif
    }

    GEngine->bForceDisableFrameRateSmoothing = bStereo;

    return bStereoEnabled;
}

float FOSVRHMD::GetScreenScale() const
{
    static IConsoleVariable* CVScreenPercentage = IConsoleManager::Get().FindConsoleVariable(TEXT("r.screenpercentage"));
    float screenScale = 1.0f;
    if (CVScreenPercentage)
    {
        screenScale = float(CVScreenPercentage->GetInt()) / 100.0f;
    }
    return screenScale;
}

void FOSVRHMD::AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const
{
    float screenScale = GetScreenScale();

    if (mCustomPresent && mCustomPresent->IsInitialized())
    {
        mCustomPresent->CalculateRenderTargetSize(SizeX, SizeY, screenScale);
        // FCustomPresent is expected to account for screenScale,
        // so we need to back it out here
        SizeX = int(float(SizeX) * (1.0f / screenScale));
        SizeY = int(float(SizeY) * (1.0f / screenScale));
    }
    else
    {
        auto leftEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::LEFT_EYE);
        auto rightEye = HMDDescription.GetDisplaySize(OSVRHMDDescription::RIGHT_EYE);
        SizeX = leftEye.X + rightEye.X;
        SizeY = leftEye.Y;
    }
    SizeX = SizeX / 2;
    if (StereoPass == eSSP_RIGHT_EYE)
    {
        X += SizeX;
    }
}

void FOSVRHMD::ResetOrientationAndPosition(float yaw)
{
    ResetOrientation(yaw);
    ResetPosition();
}

void FOSVRHMD::ResetOrientation(float yaw)
{
    FRotator ViewRotation;
    ViewRotation = FRotator(CurHmdOrientation);
    ViewRotation.Pitch = 0;
    ViewRotation.Roll = 0;
    ViewRotation.Yaw += BaseOrientation.Rotator().Yaw;

    if (yaw != 0.f)
    {
        // apply optional yaw offset
        ViewRotation.Yaw -= yaw;
        ViewRotation.Normalize();
    }

    BaseOrientation = ViewRotation.Quaternion();
}

void FOSVRHMD::ResetPosition()
{
    // Reset position
    BasePosition = CurHmdPosition;
}

void FOSVRHMD::SetBaseRotation(const FRotator& BaseRot)
{
}

FRotator FOSVRHMD::GetBaseRotation() const
{
    return FRotator::ZeroRotator;
}

void FOSVRHMD::SetBaseOrientation(const FQuat& BaseOrient)
{
    BaseOrientation = BaseOrient;
}

FQuat FOSVRHMD::GetBaseOrientation() const
{
    return BaseOrientation;
}

namespace
{
    static OSVR_MatrixConventions gMatrixFlags = OSVR_MATRIX_ROWMAJOR | OSVR_MATRIX_RHINPUT;
}

FMatrix FOSVRHMD::GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const
{
    auto mutex = mOSVREntryPoint->GetClientContextMutex();
    FScopeLock lock(mutex);

    FMatrix ret;
    float nearClip = GNearClippingPlane;
    float farClip = TNumericLimits< float >::Max();
    if (mCustomPresent)
    {
        float left, right, bottom, top;
        mCustomPresent->GetProjectionMatrix(
            StereoPassType == eSSP_LEFT_EYE ? 0 : 1,
            left, right, bottom, top, nearClip, farClip);
        ret = HMDDescription.GetProjectionMatrix(left, right, bottom, top, nearClip, farClip);
    }
    else
    {
        ret = HMDDescription.GetProjectionMatrix(
            StereoPassType == eSSP_LEFT_EYE ? OSVRHMDDescription::LEFT_EYE : OSVRHMDDescription::RIGHT_EYE,
            DisplayConfig, nearClip, farClip);
    }

    return ret;
}

//---------------------------------------------------
// ISceneViewExtension Implementation
//---------------------------------------------------


bool FOSVRHMD::GetHMDDistortionEnabled() const
{
	return false;
}

FOSVRHMD::FOSVRHMD(TSharedPtr<class OSVREntryPoint, ESPMode::ThreadSafe> entryPoint) :
    mOSVREntryPoint(entryPoint)
{
    static const FName RendererModuleName("Renderer");
    RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName);
    FScopeLock lock(mOSVREntryPoint->GetClientContextMutex());
    auto osvrClientContext = mOSVREntryPoint->GetClientContext();

    // Prevents debugger hangs that sometimes occur with only one monitor.
#if OSVR_UNREAL_DEBUG_FORCED_WINDOWMODE
    FSystemResolution::RequestResolutionChange(1280, 720, EWindowMode::Windowed); // bStereo ? WindowedMirror : Windowed
#endif

    StartCustomPresent();

    // enable vsync
    IConsoleVariable* CVSyncVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.VSync"));
    if (CVSyncVar)
    {
        CVSyncVar->Set(false);
    }

    // check if the client context is ok.
    bool bClientContextOK = entryPoint->IsOSVRConnected();

    // get the display context
    bool bDisplayConfigOK = false;
    if (bClientContextOK)
    {
        bool bFailure = false;

        auto rc = osvrClientGetDisplay(osvrClientContext, &DisplayConfig);
        if (rc == OSVR_RETURN_FAILURE)
        {
            UE_LOG(OSVRHMDLog, Warning, TEXT("Could not create DisplayConfig. Treating this as if the HMD is not connected."));
        }
        else
        {
            auto begin = FDateTime::Now().GetSecond();
            auto end = begin + 3;
            while (!bDisplayConfigOK && FDateTime::Now().GetSecond() < end)
            {
                bDisplayConfigOK = osvrClientCheckDisplayStartup(DisplayConfig) == OSVR_RETURN_SUCCESS;
                if (!bDisplayConfigOK)
                {
                    bFailure = osvrClientUpdate(osvrClientContext) == OSVR_RETURN_FAILURE;
                    if (bFailure)
                    {
                        UE_LOG(OSVRHMDLog, Warning, TEXT("osvrClientUpdate failed during startup. Treating this as \"HMD not connected\""));
                        break;
                    }
                }
                FPlatformProcess::Sleep(0.2f);
            }
            bDisplayConfigOK = bDisplayConfigOK && !bFailure;
            if (!bDisplayConfigOK)
            {
                UE_LOG(OSVRHMDLog, Warning, TEXT("DisplayConfig failed to startup. This could mean that there is nothing mapped to /me/head. Treating this as if the HMD is not connected."));
            }
        }
    }

    bool bDisplayConfigMatchesUnrealExpectations = false;
    if (bDisplayConfigOK)
    {
        bool bSuccess = HMDDescription.Init(osvrClientContext, DisplayConfig);
        if (bSuccess)
        {
            bDisplayConfigMatchesUnrealExpectations = HMDDescription.OSVRViewerFitsUnrealModel(DisplayConfig);
            if (!bDisplayConfigMatchesUnrealExpectations)
            {
                UE_LOG(OSVRHMDLog, Warning, TEXT("The OSVR display config does not match the expectations of Unreal. Possibly incompatible HMD configuration."));
            }
        }
        else
        {
            UE_LOG(OSVRHMDLog, Warning, TEXT("Unable to initialize the HMDDescription. Possible failures during initialization."));
        }
    }

    // our version of connected is that the client context is ok (server is running)
    // and the display config is ok (/me/head exists and received a pose)
    bHmdConnected = bClientContextOK && bDisplayConfigOK && bDisplayConfigMatchesUnrealExpectations;
}

FOSVRHMD::~FOSVRHMD()
{
    FScopeLock lock(mOSVREntryPoint->GetClientContextMutex());
}

bool FOSVRHMD::IsInitialized() const
{
    // @TODO
    return true;
}
