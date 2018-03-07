// Copyright 2017 Google Inc.

#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermission.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "AndroidApplication.h"
#endif

static UAndroidPermissionCallbackProxy *pProxy = NULL;

UAndroidPermissionCallbackProxy *UAndroidPermissionCallbackProxy::GetInstance()
{
	if (!pProxy) {
		pProxy = NewObject<UAndroidPermissionCallbackProxy>();
		pProxy->AddToRoot();

	}
	UE_LOG(LogAndroidPermission, Log, TEXT("UAndroidPermissionCallbackProxy::GetInstance"));
	return pProxy;
}

#if PLATFORM_ANDROID
JNI_METHOD void Java_com_google_vr_sdk_samples_permission_PermissionHelper_onAcquirePermissions(JNIEnv *env, jclass clazz, jobjectArray permissions, jintArray grantResults) 
{
	if (!pProxy) return;

	TArray<FString> arrPermissions;
	TArray<bool> arrGranted;
	int num = env->GetArrayLength(permissions);
	jint* jarrGranted = env->GetIntArrayElements(grantResults, 0);
	for (int i = 0; i < num; i++) {
		jstring str = (jstring)env->GetObjectArrayElement(permissions, i);
		const char* charStr = env->GetStringUTFChars(str, 0);
		arrPermissions.Add(FString(UTF8_TO_TCHAR(charStr)));
		env->ReleaseStringUTFChars(str, charStr);

		arrGranted.Add(jarrGranted[i] == 0 ? true : false); // 0: permission granted, -1: permission denied
	}
	env->ReleaseIntArrayElements(grantResults, jarrGranted, 0);

	UE_LOG(LogAndroidPermission, Log, TEXT("PermissionHelper_onAcquirePermissions %s %d (%d), Broadcasting..."),
		*(arrPermissions[0]), arrGranted[0], num);

	pProxy->OnPermissionsGrantedDelegate.ExecuteIfBound(arrPermissions, arrGranted);
	pProxy->OnPermissionsGrantedDynamicDelegate.Broadcast(arrPermissions, arrGranted);
}
#endif
