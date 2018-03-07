// Copyright 2017 Google Inc.

#include "AndroidPermission.h"
#include "AndroidPermissionFunctionLibrary.h"
#include "AndroidPermissionCallbackProxy.h"
#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "AndroidApplication.h"
#endif

#define LOCTEXT_NAMESPACE "FAndroidPermissionModule"

void FAndroidPermissionModule::StartupModule()
{
	// prepare java class and method cache
	UAndroidPermissionFunctionLibrary::Initialize();
	// prepare the singleton proxy object
	UAndroidPermissionCallbackProxy::GetInstance();
}

void FAndroidPermissionModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAndroidPermissionModule, AndroidPermission)