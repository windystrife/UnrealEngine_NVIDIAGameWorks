// Copyright 2017 Google Inc.

#include "Classes/GoogleVRTransition2DCallbackProxy.h"
#include "CoreMinimal.h"
#include "LogMacros.h"
#include "GoogleVRTransition2D.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "AndroidApplication.h"
#endif

static UGoogleVRTransition2DCallbackProxy *pProxy = NULL;

UGoogleVRTransition2DCallbackProxy *UGoogleVRTransition2DCallbackProxy::GetInstance()
{
	if (!pProxy) {
		pProxy = NewObject<UGoogleVRTransition2DCallbackProxy>();
		pProxy->AddToRoot();

	}
	UE_LOG(LogGoogleVRTransition2D, Log, TEXT("UGoogleVRTransition2DCallbackProxy::GetInstance"));
	return pProxy;
}


#if PLATFORM_ANDROID
JNI_METHOD void Java_com_google_vr_sdk_samples_transition_GVRTransitionHelper_onTransitionTo2D(JNIEnv *env, jclass clazz, jobject thiz)
{
	if (!pProxy) return;

	UE_LOG(LogGoogleVRTransition2D, Log, TEXT("GVRTransitionHelper_onTransitionTo2D, Broadcasting..."));
	pProxy->OnTransitionTo2D.Broadcast();
}
#endif
