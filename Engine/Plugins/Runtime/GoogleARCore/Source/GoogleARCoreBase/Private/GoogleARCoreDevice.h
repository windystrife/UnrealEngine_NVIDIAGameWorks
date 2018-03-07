// Copyright 2017 Google Inc.

#pragma once

#include "CoreDelegates.h"
#include "Engine/EngineBaseTypes.h"
#include "HAL/ThreadSafeBool.h"
#include "GoogleARCorePrimitives.h"
#include "GoogleARCoreMotionManager.h"
#include "GoogleARCoreCameraManager.h"
#include "GoogleARCorePointCloudManager.h"
#include "GoogleARCorePlaneManager.h"
#include "GoogleARCoreAnchorManager.h"

#if PLATFORM_ANDROID
#include "tango_client_api2.h"
#endif

#define ENABLE_ARCORE_DEBUG_LOG 1

DECLARE_MULTICAST_DELEGATE(FOnTangoServiceBound);
DECLARE_MULTICAST_DELEGATE(FOnTangoServiceUnbound);

class FGoogleARCoreDevice
{
public:
	static FGoogleARCoreDevice* GetInstance();

	FGoogleARCoreDevice();

	bool GetIsGoogleARCoreSupported();

	/**
	 * Whether a proper connection with the Tango Core system service is currently established.
	 * When this is is false much of Tango's functionality will be unavailable.
	 *
	 * Note that it is technically possible for Tango to stop at any time (for instance, if the Tango Core service
	 * is updated on the device), and thus does not guarantee that Tango will still be bound during
	 * subsequent calls to anything.
	 * @return True if a proper connection with the Tango Core system service is currently established.
	 */
	bool GetIsTangoBound();

	/**
	 * Get whether Tango is currently running.
	 *
	 * Note that it is technically possible for Tango to stop at any time (for instance, if the Tango Core service
	 * is updated on the device), and thus does not guarantee that Tango will still be bound and running during
	 * subsequent calls to anything.
	 * @return True if Tango is currently running.
	 */
	bool GetIsTangoRunning();

	/**
	 * Update Tango plugin to use a new configuration.
	 */
	void UpdateTangoConfiguration(const FGoogleARCoreSessionConfig& NewConfiguration);

	/**
	 * Reset Tango plugin to use the global project Tango configuration.
	 */
	void ResetTangoConfiguration();

	void GetCurrentSessionConfig(FGoogleARCoreSessionConfig& OutCurrentTangoConfig);

#if PLATFORM_ANDROID
	/**
	 * Get the current TangoConfig object that Tango is running with. Will return NULL if Tango is not running.
	 */
	TangoConfig GetCurrentLowLevelTangoConfig();
#endif

	/**
	 * Get the current base frame Tango is running on.
	 */
	EGoogleARCoreReferenceFrame GetCurrentBaseFrame();

	/**
	 * Get the base frame Tango from the given Tango configuration.
	 */
	EGoogleARCoreReferenceFrame GetBaseFrame(FGoogleARCoreSessionConfig TangoConfig);

	/**
	 * Get Unreal Units per meter, based off of the current map's VR World to Meters setting.
	 * @return Unreal Units per meter.
	 */
	float GetWorldToMetersScale();

	int32 GetDepthCameraFrameRate();
	bool SetDepthCameraFrameRate(int32 NewFrameRate);

public:
	FGoogleARCoreMotionManager TangoMotionManager;
	FGoogleARCoreCameraManager TangoARCameraManager;
	FGoogleARCorePointCloudManager TangoPointCloudManager;
	UGoogleARCoreAnchorManager* ARAnchorManager;
	UGoogleARCorePlaneManager* PlaneManager;

	FOnTangoServiceBound OnTangoServiceBoundDelegate;
	FOnTangoServiceUnbound OnTangoServiceUnboundDelegate;

	void StartTrackingSession();
	void StopTrackingSession();

	void RunOnGameThread(TFunction<void()> Func)
	{
		RunOnGameThreadQueue.Enqueue(Func);
	}

	void GetRequiredRuntimePermissionsForConfiguration(const FGoogleARCoreSessionConfig& Config, TArray<FString>& RuntimePermissions)
	{
		RuntimePermissions.Reset();
		// TODO: check for depth camera when it is supported here.
		RuntimePermissions.Add("android.permission.CAMERA");
	}
	void HandleRuntimePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& Granted);

	void SetForceLateUpdateEnable(bool bEnable)
	{
		bForceLateUpdateEnabled = bEnable;
	}

private:
	// Android lifecycle events.
	void OnApplicationCreated();
	void OnApplicationDestroyed();
	void OnApplicationPause();
	void OnApplicationResume();
	void OnApplicationStart();
	void OnApplicationStop();
	void OnDisplayOrientationChanged();
	void OnAreaDescriptionPermissionResult(bool bWasGranted);
	// Unreal plugin events.
	void OnModuleLoaded();
	void OnModuleUnloaded();

	void OnWorldTickStart(ELevelTick TickType, float DeltaTime);

	bool BindTangoServiceAndCheckPermission(const FGoogleARCoreSessionConfig& ConfigurationData);
	bool StartSession(const FGoogleARCoreSessionConfig& ConfigurationData);

#if PLATFORM_ANDROID
	// Tango Service bound event
	void OnTangoServiceBound();
#endif

	friend class FGoogleARCoreAndroidHelper;
	friend class FGoogleARCoreBaseModule;

private:
	bool bIsARCoreSupported;
	bool bNeedToCreateTangoObject;
	FThreadSafeBool bTangoIsBound; // Whether a proper connection with the Tango Core system service is currently established.
	FThreadSafeBool bTangoIsRunning; // Whether Tango is currently running.
	bool bForceLateUpdateEnabled; // A debug flag to force use late update.
	bool bTangoConfigChanged;
	bool bAreaDescriptionPermissionRequested;
	bool bAndroidRuntimePermissionsRequested;
	bool bAndroidRuntimePermissionsGranted;
	bool bStartTangoTrackingRequested; // User called StartSession
	bool bShouldTangoRestart; // Start tracking on activity start
	float WorldToMeterScale;
	class UTangoAndroidPermissionHandler* PermissionHandler;
	FThreadSafeBool bDisplayOrientationChanged;

	FGoogleARCoreSessionConfig ProjectTangoConfig; // Project Tango config set from Tango Plugin Setting
	FGoogleARCoreSessionConfig RequestTangoConfig; // The current request tango config, could be different than the project config
	FGoogleARCoreSessionConfig LastKnownConfig; // Record of the configuration Tango was last successfully started with

#if PLATFORM_ANDROID
	TangoConfig LowLevelTangoConfig; // Low level Tango Config object in Tango client api
#endif

	static void TangoEventRouter(void *Ptr, const struct TangoEvent* Event);
	void OnTangoEvent(const struct TangoEvent* Event);

	TQueue<TFunction<void()>> RunOnGameThreadQueue;
};
