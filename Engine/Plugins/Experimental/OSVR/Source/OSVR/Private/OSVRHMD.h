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

#pragma once

#include "OSVRPrivate.h"
#include "IOSVR.h"
#include "OSVRHMDDescription.h"
#include "HeadMountedDisplay.h"
#include "HeadMountedDisplayBase.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "RendererInterface.h"
#include "XRRenderTargetManager.h"

#include <osvr/ClientKit/DisplayC.h>

#if PLATFORM_WINDOWS
#include "OSVRCustomPresentD3D11.h"
#else
#include "OSVRCustomPresentOpenGL.h"
#endif

DECLARE_LOG_CATEGORY_EXTERN(OSVRHMDLog, Log, All);

/**
* OSVR Head Mounted Display
*/
class FOSVRHMD : public FHeadMountedDisplayBase, public FXRRenderTargetManager, public TSharedFromThis< FOSVRHMD, ESPMode::ThreadSafe >
{
public:

	/** IXRTrackingSystem interface */
	virtual FName GetSystemName() const override
	{
		static FName DefaultName(TEXT("OSVR"));
		return DefaultName;
	}
    virtual void OnBeginPlay(FWorldContext& InWorldContext) override;
    virtual void OnEndPlay(FWorldContext& InWorldContext) override;
    virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;
    
	virtual bool DoesSupportPositionalTracking() const override;
    virtual bool HasValidTrackingPosition() override;

	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual void RefreshPoses() override;

	virtual void SetBaseRotation(const FRotator& BaseRot) override;
    virtual FRotator GetBaseRotation() const override;
    virtual void SetBaseOrientation(const FQuat& BaseOrient) override;
    virtual FQuat GetBaseOrientation() const override;

	/** Resets orientation by setting roll and pitch to 0,
	assuming that current yaw is forward direction and assuming
	current position as 0 point. */
	virtual void ResetOrientation(float yaw) override;
	virtual void ResetPosition() override;
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override;

	/**
    * Rebase the input position and orientation to that of the HMD's base
    */
    virtual void RebaseObjectOrientationAndPosition(FVector& Position, FQuat& Orientation) const override;

	virtual class IHeadMountedDisplay* GetHMDDevice() override { return this; }
	virtual class TSharedPtr< class IStereoRendering, ESPMode::ThreadSafe > GetStereoRenderingDevice() override
	{
		return SharedThis(this);
	}

	virtual float GetWorldToMetersScale() const override
	{
		return WorldToMetersScale;
	}

	virtual void BeginRendering_RenderThread(const FTransform& NewRelativeTransform, FRHICommandListImmediate& RHICmdList, FSceneViewFamily& ViewFamily) override;

	/** IHeadMountedDisplay interface */
	virtual bool IsHMDConnected() override;
    virtual bool IsHMDEnabled() const override;
    virtual void EnableHMD(bool bEnable = true) override;
    virtual EHMDDeviceType::Type GetHMDDeviceType() const override;

	virtual bool GetHMDMonitorInfo(MonitorInfo&) override;
    virtual void GetFieldOfView(float& OutHFOVInDegrees, float& OutVFOVInDegrees) const override;


    virtual void SetInterpupillaryDistance(float NewInterpupillaryDistance) override;
    virtual float GetInterpupillaryDistance() const override;

	virtual bool GetHMDDistortionEnabled() const override;
    virtual bool IsChromaAbCorrectionEnabled() const override;

    virtual void DrawDistortionMesh_RenderThread(struct FRenderingCompositePassContext& Context, const FIntPoint& TextureSize) override;

    /** IStereoRendering interface */
	virtual FRHICustomPresent* GetCustomPresent() override
	{
		return mCustomPresent;
	}
	virtual bool IsStereoEnabled() const override;
    virtual bool EnableStereo(bool bStereo = true) override;
    virtual void AdjustViewRect(EStereoscopicPass StereoPass, int32& X, int32& Y, uint32& SizeX, uint32& SizeY) const override;
    virtual FMatrix GetStereoProjectionMatrix(const enum EStereoscopicPass StereoPassType) const override;
	virtual void RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture, FVector2D WindowSize) const override;
    virtual void GetEyeRenderParams_RenderThread(const struct FRenderingCompositePassContext& Context, FVector2D& EyeToSrcUVScaleValue, FVector2D& EyeToSrcUVOffsetValue) const override;
	virtual IStereoRenderTargetManager* GetRenderTargetManager() override { return this; }

	/** FXRRenderTargetManager interface */
    virtual void CalculateRenderTargetSize(const FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) override;
    virtual void UpdateViewportRHIBridge(bool bUseSeparateRenderTarget, const class FViewport& Viewport, FRHIViewport* const ViewportRHI) override;
    virtual bool AllocateRenderTargetTexture(uint32 index, uint32 sizeX, uint32 sizeY, uint8 format, uint32 numMips, uint32 flags, uint32 targetableTextureFlags, FTexture2DRHIRef& outTargetableTexture, FTexture2DRHIRef& outShaderResourceTexture, uint32 numSamples = 1) override;

    virtual bool ShouldUseSeparateRenderTarget() const override
    {
        check(IsInGameThread());
        return IsStereoEnabled();
    }

public:
    /** Constructor */
    FOSVRHMD(TSharedPtr<class OSVREntryPoint, ESPMode::ThreadSafe> entryPoint);

    /** Destructor */
    virtual ~FOSVRHMD();

    /** @return	True if the HMD was initialized OK */
    bool IsInitialized() const;

private:
	void StartCustomPresent();
    void StopCustomPresent();
    void GetRenderTargetSize_GameThread(float windowWidth, float windowHeight, float &width, float &height);
    float GetScreenScale() const;

    TSharedPtr<class OSVREntryPoint, ESPMode::ThreadSafe> mOSVREntryPoint;
    IRendererModule* RendererModule = nullptr;

    /** Player's orientation tracking */
    mutable FQuat CurHmdOrientation = FQuat::Identity;
    mutable FVector CurHmdPosition = FVector::ZeroVector;

    /** Player's orientation tracking (on render thread) */
    mutable FQuat CurHmdOrientationRT = FQuat::Identity;

    FRotator DeltaControlRotation = FRotator::ZeroRotator; // same as DeltaControlOrientation but as rotator
    FQuat DeltaControlOrientation = FQuat::Identity; // same as DeltaControlRotation but as quat


    mutable FQuat LastHmdOrientation = FQuat::Identity; // contains last APPLIED ON GT HMD orientation
    FVector LastHmdPosition = FVector::ZeroVector;		  // contains last APPLIED ON GT HMD position

                                                          /** HMD base values, specify forward orientation and zero pos offset */
    FQuat BaseOrientation = FQuat::Identity; // base orientation
    FVector BasePosition = FVector::ZeroVector;

    /** World units (UU) to Meters scale.  Read from the level, and used to transform positional tracking data */
    float WorldToMetersScale = 100.0f; // @todo: isn't this meters to world units scale?

    bool bHaveVisionTracking = false;
	bool bHasValidPose;

    bool bStereoEnabled = false;
    bool bHmdEnabled = false;
    bool bHmdConnected = false;
    bool bHmdOverridesApplied = false;
    bool bWaitedForClientStatus = false;
    bool bPlaying = false;

    OSVRHMDDescription HMDDescription;
    OSVR_DisplayConfig DisplayConfig = nullptr;
    TRefCountPtr<FCurrentCustomPresent> mCustomPresent;
};
