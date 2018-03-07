// Copyright 2017 Google Inc.

#pragma once
#include "XRTrackingSystemBase.h"
#include "HeadMountedDisplay.h"
#include "IHeadMountedDisplay.h"
#include "SceneViewExtension.h"
#include "SceneViewport.h"
#include "SceneView.h"
#include "GoogleARCoreDevice.h"
#include "ARHitTestingSupport.h"
#include "ARTrackingQuality.h"

/**
* Tango Head Mounted Display used for Argument Reality
*/
class FGoogleARCoreHMD : public FXRTrackingSystemBase, public IARHitTestingSupport, public IARTrackingQuality, public TSharedFromThis<FGoogleARCoreHMD, ESPMode::ThreadSafe>
{
	friend class FGoogleARCoreXRCamera;

private:
	/** IXRTrackingSystem */
	virtual FName GetSystemName() const override;
	virtual bool GetCurrentPose(int32 DeviceId, FQuat& OutOrientation, FVector& OutPosition) override;
	virtual FString GetVersionString() const override;
	virtual bool EnumerateTrackedDevices(TArray<int32>& OutDevices, EXRTrackedDeviceType Type = EXRTrackedDeviceType::Any) override;
	virtual void RefreshPoses() override;
	virtual TSharedPtr<class IXRCamera, ESPMode::ThreadSafe> GetXRCamera(int32 DeviceId = HMDDeviceId) override;

public:
	/** IHeadMountedDisplay interface */

	// @todo : revisit this function; do we need them?
	virtual bool HasValidTrackingPosition() override;
	// @todo : can I get rid of this? At least rename to IsCameraTracking / IsTrackingAllowed()
	virtual bool IsHeadTrackingAllowed() const override;

	virtual bool OnStartGameFrame(FWorldContext& WorldContext) override;

	// TODO: Figure out if we need to allow developer set/reset base orientation and position.
	virtual void ResetOrientationAndPosition(float yaw = 0.f) override {}

	/** Console command Handles */
	void TangoHMDEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void ARCameraModeEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void ColorCamRenderingEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);
	void LateUpdateEnableCommandHandler(const TArray<FString>& Args, UWorld* World, FOutputDevice& Ar);

	// @todo move this to some interface
	virtual float GetWorldToMetersScale() const override;


	//~ IARHitTestingSupport
	//virtual bool ARLineTraceFromScreenPoint(const FVector2D ScreenPosition, TArray<FARHitTestResult>& OutHitResults) override;
	//~ IARHitTestingSupport

	//~ IARTrackingQuality
	virtual EARTrackingQuality ARGetTrackingQuality() const override;
	//~ IARTrackingQuality

public:
	FGoogleARCoreHMD();

	~FGoogleARCoreHMD();

	/** Config the TangoHMD.
	* When bEnableHMD is true, TangoHMD will update game camera position and orientation using Tango pose.
	* When bEnableARCamera is true, TangoHMD will sync the camera projection matrix with Tango color camera
	* and render the color camera video overlay
	*/
	void ConfigTangoHMD(bool bEnableHMD, bool bEnableARCamera, bool bUseLateUpdate);

	void EnableColorCameraRendering(bool bEnableColorCameraRnedering);

	bool GetColorCameraRenderingEnabled();

	bool GetTangoHMDARModeEnabled();

	bool GetTangoHMDLateUpdateEnabled();

private:
	FGoogleARCoreDevice* TangoDeviceInstance;

	bool bHMDEnabled;
	bool bSceneViewExtensionRegistered;
	bool bARCameraEnabled;
	bool bColorCameraRenderingEnabled;
	bool bHasValidPose;
	bool bLateUpdateEnabled;
	bool bNeedToFlipCameraImage;

	FVector CachedPosition;
	FQuat CachedOrientation;
	FRotator DeltaControlRotation;    // same as DeltaControlOrientation but as rotator
	FQuat DeltaControlOrientation; // same as DeltaControlRotation but as quat

	bool bLateUpdatePoseIsValid;
	FGoogleARCorePose LateUpdatePose;

	TSharedPtr<class ISceneViewExtension, ESPMode::ThreadSafe> ViewExtension;

	/** Console commands */
	FAutoConsoleCommand TangoHMDEnableCommand;
	FAutoConsoleCommand ARCameraModeEnableCommand;
	FAutoConsoleCommand ColorCamRenderingEnableCommand;
	FAutoConsoleCommand LateUpdateEnableCommand;
};

DEFINE_LOG_CATEGORY_STATIC(LogGoogleARCoreHMD, Log, All);
