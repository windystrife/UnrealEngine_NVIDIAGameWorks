// Copyright 2017 Google Inc.

#pragma once

#include "CoreMinimal.h"
#include "GoogleARCoreCameraManager.h"
#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#endif

/** Wrappers for accessing Tango stuff that lives in Java. */
class FGoogleARCoreAndroidHelper
{
public:

	/**
	 * Update the Android display orientation as per the android.view.Display class' getRotation() method.
	 */
	static void UpdateDisplayRotation();

	/**
	 * Get Andriod display orientation.
	 */
	static int32 GetDisplayRotation();

	/**
	 * Get Android camera orientation as per the android.hardware.Camera.CameraInfo class' orientation field.
	 */
	static int32 GetColorCameraRotation();

	static bool HasAreaDescriptionPermission();
	static void RequestAreaDescriptionPermission();

#if PLATFORM_ANDROID
	// Helpers for redirecting Android events.
	static void OnApplicationCreated();
	static void OnApplicationDestroyed();
	static void OnApplicationPause();
	static void OnApplicationResume();
	static void OnApplicationStop();
	static void OnApplicationStart();
	static void OnDisplayOrientationChanged();

	static void OnTangoServiceConnect();
#endif

	static bool IsTangoCorePresent();
	static bool IsTangoCoreUpToDate();
	static bool IsARCoreSupported();
	static void CreateTangoObject();

private:
	static int32 CurrentDisplayRotation;
};
