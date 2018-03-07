// Copyright 2017 Google Inc.

#include "GoogleARCoreAndroidHelper.h"
#include "GoogleARCoreBaseLogCategory.h"
#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#endif

#include "GoogleARCoreDevice.h"

#if PLATFORM_ANDROID
extern "C"
{
	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_cacheJavaObjects(
			JNIEnv* env, jobject, jobject jTangoUpdateCallback) {
		UE_LOG(LogGoogleARCore, Log, TEXT("Caching Tango Java Object."));
		TangoService_CacheJavaObjects(env, jTangoUpdateCallback);
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onTangoServiceConnected(
		JNIEnv* env, jobject, jobject tango) {
		UE_LOG(LogGoogleARCore, Log, TEXT("On Tango Service Connected! Cache Tango Object!"));
		TangoService_CacheTangoObject(env, tango);
		FGoogleARCoreAndroidHelper::OnTangoServiceConnect();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onPoseAvailableNative(
		JNIEnv* env, jobject, jobject poseData) {
		TangoService_JavaCallback_OnPoseAvailable(env, poseData);
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onTextureAvailableNative(
			JNIEnv* env, jobject, jint cameraId) {
		TangoService_JavaCallback_OnTextureAvailable(static_cast<int>(cameraId));
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onImageAvailableNative(
			JNIEnv* env, jobject, jobject image, jobject metadata, jint cameraId) {
		TangoService_JavaCallback_OnImageAvailable(env, static_cast<int>(cameraId),
			image, metadata);
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onPointCloudAvailableNative(
		JNIEnv* env, jobject, jobject poitnCloudData) {
		TangoService_JavaCallback_OnPointCloudAvailable(env, poitnCloudData);
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onTangoEventNative(
			JNIEnv* env, jobject, jobject event) {
		TangoService_JavaCallback_OnTangoEvent(env, event);
	}

	// Functions that are called on Android lifecycle events.

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onApplicationCreated(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnApplicationCreated();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onApplicationDestroyed(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnApplicationDestroyed();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onApplicationPause(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnApplicationPause();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onApplicationResume(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnApplicationResume();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onApplicationStop(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnApplicationStop();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onApplicationStart(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnApplicationStart();
	}

	void Java_com_projecttango_unreal_TangoNativeEngineMethodWrapper_onDisplayOrientationChanged(JNIEnv*, jobject)
	{
		FGoogleARCoreAndroidHelper::OnDisplayOrientationChanged();
	}
}

// Redirect events to TangoLifecycle class:

void FGoogleARCoreAndroidHelper::OnApplicationCreated()
{
	FGoogleARCoreDevice::GetInstance()->OnApplicationCreated();
}

void FGoogleARCoreAndroidHelper::OnApplicationDestroyed()
{
	FGoogleARCoreDevice::GetInstance()->OnApplicationDestroyed();
}

void FGoogleARCoreAndroidHelper::OnApplicationPause()
{
	FGoogleARCoreDevice::GetInstance()->OnApplicationPause();
}

void FGoogleARCoreAndroidHelper::OnApplicationStart()
{
	FGoogleARCoreDevice::GetInstance()->OnApplicationStart();
}

void FGoogleARCoreAndroidHelper::OnApplicationStop()
{
	FGoogleARCoreDevice::GetInstance()->OnApplicationStop();
}

void FGoogleARCoreAndroidHelper::OnApplicationResume()
{
	FGoogleARCoreDevice::GetInstance()->OnApplicationResume();
}

void FGoogleARCoreAndroidHelper::OnDisplayOrientationChanged()
{
	FGoogleARCoreDevice::GetInstance()->OnDisplayOrientationChanged();
}

void FGoogleARCoreAndroidHelper::OnTangoServiceConnect()
{
	FGoogleARCoreDevice::GetInstance()->OnTangoServiceBound();
}

#endif
bool FGoogleARCoreAndroidHelper::HasAreaDescriptionPermission()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_TangoHasAreaDescriptionPermission", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
#endif
	return false;
}

void FGoogleARCoreAndroidHelper::RequestAreaDescriptionPermission()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_TangoRequestAreaDescriptionPermission", "()V", false);
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
#endif
}

int32 FGoogleARCoreAndroidHelper::CurrentDisplayRotation = 0;

void FGoogleARCoreAndroidHelper::UpdateDisplayRotation()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GetDisplayRotation", "()I", false);
		CurrentDisplayRotation = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
#endif
}

int32 FGoogleARCoreAndroidHelper::GetDisplayRotation()
{
	return CurrentDisplayRotation;
}

int32 FGoogleARCoreAndroidHelper::GetColorCameraRotation()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_GetColorCameraRotation", "()I", false);
		return FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
#endif
	return 0;
}

bool FGoogleARCoreAndroidHelper::IsTangoCorePresent()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_IsTangoCorePresent", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
	return false;
#else
	return true;
#endif
}

bool FGoogleARCoreAndroidHelper::IsTangoCoreUpToDate()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_IsTangoCoreUpToDate", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
	return false;
#else
	return true;
#endif
}

bool FGoogleARCoreAndroidHelper::IsARCoreSupported()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_IsARCoreSupported", "()Z", false);
		return FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
#endif
	return false;
}

void FGoogleARCoreAndroidHelper::CreateTangoObject()
{
#if PLATFORM_ANDROID
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		static jmethodID Method = FJavaWrapper::FindMethod(Env, FJavaWrapper::GameActivityClassID, "AndroidThunkJava_CreateTangoObject", "()V", false);
		return FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, Method);
	}
#endif
}
