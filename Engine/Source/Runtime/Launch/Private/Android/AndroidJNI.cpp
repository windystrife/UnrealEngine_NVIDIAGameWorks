// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidJNI.h"
#include "ExceptionHandling.h"
#include "AndroidPlatformCrashContext.h"
#include "Runtime/Core/Public/Misc/DateTime.h"
#include "HAL/PlatformStackWalk.h"
#include "AndroidApplication.h"
#include "AndroidInputInterface.h"
#include "Widgets/Input/IVirtualKeyboardEntry.h"
#include "UnrealEngine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Math/Vector.h"

THIRD_PARTY_INCLUDES_START
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
THIRD_PARTY_INCLUDES_END

#define JNI_CURRENT_VERSION JNI_VERSION_1_6

JavaVM* GJavaVM;

// Pointer to target widget for virtual keyboard contents
static IVirtualKeyboardEntry *VirtualKeyboardWidget = NULL;

//virtualKeyboard shown
static volatile bool GVirtualKeyboardShown = false;

extern FString GFilePathBase;
extern FString GExternalFilePath;
extern FString GFontPathBase;
extern bool GOBBinAPK;
extern FString GOBBFilePathBase;
extern FString GAPKFilename;

FOnActivityResult FJavaWrapper::OnActivityResultDelegate;

//////////////////////////////////////////////////////////////////////////

#if UE_BUILD_SHIPPING
// always clear any exceptions in Shipping
#define CHECK_JNI_RESULT(Id) if (Id == 0) { Env->ExceptionClear(); }
#else
#define CHECK_JNI_RESULT(Id) \
if (Id == 0) \
{ \
	if (bIsOptional) { Env->ExceptionClear(); } \
	else { Env->ExceptionDescribe(); checkf(Id != 0, TEXT("Failed to find " #Id)); } \
}
#endif

#define CHECK_JNI_METHOD(Id) checkf(Id != nullptr, TEXT("Failed to find " #Id));

void FJavaWrapper::FindClassesAndMethods(JNIEnv* Env)
{
	bool bIsOptional = false;
	jclass localGameActivityClass = FindClass(Env, "com/epicgames/ue4/GameActivity", bIsOptional);
	GameActivityClassID = (jclass)Env->NewGlobalRef(localGameActivityClass);
	Env->DeleteLocalRef(localGameActivityClass);
	AndroidThunkJava_ShowConsoleWindow = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ShowConsoleWindow", "(Ljava/lang/String;)V", bIsOptional);
    AndroidThunkJava_ShowVirtualKeyboardInputDialog = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ShowVirtualKeyboardInputDialog", "(ILjava/lang/String;Ljava/lang/String;)V", bIsOptional);
    AndroidThunkJava_HideVirtualKeyboardInputDialog = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_HideVirtualKeyboardInputDialog", "()V", bIsOptional);
	AndroidThunkJava_ShowVirtualKeyboardInput = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ShowVirtualKeyboardInput", "(ILjava/lang/String;Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_HideVirtualKeyboardInput = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_HideVirtualKeyboardInput", "()V", bIsOptional);
	AndroidThunkJava_LaunchURL = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_LaunchURL", "(Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_GetAssetManager = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetAssetManager", "()Landroid/content/res/AssetManager;", bIsOptional);
	AndroidThunkJava_Minimize = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_Minimize", "()V", bIsOptional);
	AndroidThunkJava_ForceQuit = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ForceQuit", "()V", bIsOptional);
	AndroidThunkJava_GetFontDirectory = FindStaticMethod(Env, GameActivityClassID, "AndroidThunkJava_GetFontDirectory", "()Ljava/lang/String;", bIsOptional);
	AndroidThunkJava_Vibrate = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_Vibrate", "(I)V", bIsOptional);
	AndroidThunkJava_IsMusicActive = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_IsMusicActive", "()Z", bIsOptional);
	AndroidThunkJava_KeepScreenOn = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_KeepScreenOn", "(Z)V", bIsOptional);
	AndroidThunkJava_InitHMDs = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_InitHMDs", "()V", bIsOptional);
	AndroidThunkJava_DismissSplashScreen = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_DismissSplashScreen", "()V", bIsOptional);
	AndroidThunkJava_GetInputDeviceInfo = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetInputDeviceInfo", "(I)Lcom/epicgames/ue4/GameActivity$InputDeviceInfo;", bIsOptional);
	AndroidThunkJava_IsGamepadAttached = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_IsGamepadAttached", "()Z", bIsOptional);
	AndroidThunkJava_HasMetaDataKey = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_HasMetaDataKey", "(Ljava/lang/String;)Z", bIsOptional);
	AndroidThunkJava_GetMetaDataBoolean = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetMetaDataBoolean", "(Ljava/lang/String;)Z", bIsOptional);
	AndroidThunkJava_GetMetaDataInt = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetMetaDataInt", "(Ljava/lang/String;)I", bIsOptional);
	AndroidThunkJava_GetMetaDataString = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetMetaDataString", "(Ljava/lang/String;)Ljava/lang/String;", bIsOptional);
	AndroidThunkJava_SetSustainedPerformanceMode = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_SetSustainedPerformanceMode", "(Z)V", bIsOptional);
	AndroidThunkJava_ShowHiddenAlertDialog = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_ShowHiddenAlertDialog", "()V", bIsOptional);
	AndroidThunkJava_LocalNotificationScheduleAtTime = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_LocalNotificationScheduleAtTime", "(Ljava/lang/String;ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_LocalNotificationClearAll = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_LocalNotificationClearAll", "()V", bIsOptional);
	AndroidThunkJava_LocalNotificationGetLaunchNotification = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_LocalNotificationGetLaunchNotification", "()Lcom/epicgames/ue4/GameActivity$LaunchNotification;", bIsOptional);
	//AndroidThunkJava_LocalNotificationDestroyIfExists = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_LocalNotificationDestroyIfExists", "(I)Z", bIsOptional);
	AndroidThunkJava_HasActiveWiFiConnection = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_HasActiveWiFiConnection", "()Z", bIsOptional);
	AndroidThunkJava_GetAndroidId = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_GetAndroidId", "()Ljava/lang/String;", bIsOptional);

	// this is optional - only inserted if GearVR plugin enabled
	AndroidThunkJava_IsGearVRApplication = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_IsGearVRApplication", "()Z", true);

	// this is optional - only inserted if GCM plugin enabled
	AndroidThunkJava_RegisterForRemoteNotifications = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_RegisterForRemoteNotifications", "()V", true);
	AndroidThunkJava_UnregisterForRemoteNotifications = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_UnregisterForRemoteNotifications", "()V", true);

	// get field IDs for InputDeviceInfo class members
	jclass localInputDeviceInfoClass = FindClass(Env, "com/epicgames/ue4/GameActivity$InputDeviceInfo", bIsOptional);
	InputDeviceInfoClass = (jclass)Env->NewGlobalRef(localInputDeviceInfoClass);
	Env->DeleteLocalRef(localInputDeviceInfoClass);
	InputDeviceInfo_VendorId = FJavaWrapper::FindField(Env, InputDeviceInfoClass, "vendorId", "I", bIsOptional);
	InputDeviceInfo_ProductId = FJavaWrapper::FindField(Env, InputDeviceInfoClass, "productId", "I", bIsOptional);
	InputDeviceInfo_ControllerId = FJavaWrapper::FindField(Env, InputDeviceInfoClass, "controllerId", "I", bIsOptional);
	InputDeviceInfo_Name = FJavaWrapper::FindField(Env, InputDeviceInfoClass, "name", "Ljava/lang/String;", bIsOptional);
	InputDeviceInfo_Descriptor = FJavaWrapper::FindField(Env, InputDeviceInfoClass, "descriptor", "Ljava/lang/String;", bIsOptional);

	/** GooglePlay services */
	FindGooglePlayMethods(Env);
	/** GooglePlay billing services */
	FindGooglePlayBillingMethods(Env);

	// get field IDs for LaunchNotificationClass class members
	jclass localLaunchNotificationClass = FindClass(Env, "com/epicgames/ue4/GameActivity$LaunchNotification", bIsOptional);
	LaunchNotificationClass = (jclass)Env->NewGlobalRef(localLaunchNotificationClass);
	Env->DeleteLocalRef(localLaunchNotificationClass);
	LaunchNotificationUsed = FJavaWrapper::FindField(Env, LaunchNotificationClass, "used", "Z", bIsOptional);
	LaunchNotificationEvent = FJavaWrapper::FindField(Env, LaunchNotificationClass, "event", "Ljava/lang/String;", bIsOptional);
	LaunchNotificationFireDate = FJavaWrapper::FindField(Env, LaunchNotificationClass, "fireDate", "I", bIsOptional);

	jclass localThreadClass = FindClass(Env, "java/lang/Thread", bIsOptional);
	ThreadClass = (jclass)Env->NewGlobalRef(localThreadClass);
	Env->DeleteLocalRef(localThreadClass);
	CurrentThreadMethod = FindStaticMethod(Env, ThreadClass, "currentThread", "()Ljava/lang/Thread;", bIsOptional);
	SetNameMethod = FindMethod(Env, ThreadClass, "setName", "(Ljava/lang/String;)V", bIsOptional);

	// the rest are optional
	bIsOptional = true;

	// SurfaceView functionality for view scaling on some devices
	AndroidThunkJava_UseSurfaceViewWorkaround = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_UseSurfaceViewWorkaround", "()V", bIsOptional);
	AndroidThunkJava_SetDesiredViewSize = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_SetDesiredViewSize", "(II)V", bIsOptional);

	AndroidThunkJava_VirtualInputIgnoreClick = FindMethod(Env, GameActivityClassID, "AndroidThunkJava_VirtualInputIgnoreClick", "(II)Z", bIsOptional);
}

void FJavaWrapper::FindGooglePlayMethods(JNIEnv* Env)
{
	bool bIsOptional = true;

	// @todo split GooglePlay
	//	GoogleServicesClassID = FindClass(Env, "com/epicgames/ue4/GoogleServices", bIsOptional);
	GoogleServicesClassID = GameActivityClassID;
	AndroidThunkJava_ResetAchievements = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_ResetAchievements", "()V", bIsOptional);
	AndroidThunkJava_ShowAdBanner = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_ShowAdBanner", "(Ljava/lang/String;Z)V", bIsOptional);
	AndroidThunkJava_HideAdBanner = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_HideAdBanner", "()V", bIsOptional);
	AndroidThunkJava_CloseAdBanner = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_CloseAdBanner", "()V", bIsOptional);
	AndroidThunkJava_LoadInterstitialAd = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_LoadInterstitialAd", "(Ljava/lang/String;)V", bIsOptional);
	AndroidThunkJava_IsInterstitialAdAvailable = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IsInterstitialAdAvailable", "()Z", bIsOptional);
	AndroidThunkJava_IsInterstitialAdRequested = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IsInterstitialAdRequested", "()Z", bIsOptional);
	AndroidThunkJava_ShowInterstitialAd = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_ShowInterstitialAd", "()V", bIsOptional);
	AndroidThunkJava_GetAdvertisingId = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_GetAdvertisingId", "()Ljava/lang/String;", bIsOptional);
	AndroidThunkJava_GoogleClientConnect = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_GoogleClientConnect", "()V", bIsOptional);
	AndroidThunkJava_GoogleClientDisconnect = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_GoogleClientDisconnect", "()V", bIsOptional);
}
void FJavaWrapper::FindGooglePlayBillingMethods(JNIEnv* Env)
{
	// In app purchase functionality
	bool bSupportsInAppPurchasing = false;
	if (!GConfig->GetBool(TEXT("OnlineSubsystemGooglePlay.Store"), TEXT("bSupportsInAppPurchasing"), bSupportsInAppPurchasing, GEngineIni))
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("[JNI] - Failed to determine if app purchasing is enabled!"));
	}
	bool bIsStoreOptional = !bSupportsInAppPurchasing;

	jclass localStringClass = Env->FindClass("java/lang/String");
	JavaStringClass = (jclass)Env->NewGlobalRef(localStringClass);
	Env->DeleteLocalRef(localStringClass);
	AndroidThunkJava_IapSetupService = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapSetupService", "(Ljava/lang/String;)V", bIsStoreOptional);
	AndroidThunkJava_IapQueryInAppPurchases = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapQueryInAppPurchases", "([Ljava/lang/String;)Z", bIsStoreOptional);
	AndroidThunkJava_IapBeginPurchase = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapBeginPurchase", "(Ljava/lang/String;)Z", bIsStoreOptional);
	AndroidThunkJava_IapIsAllowedToMakePurchases = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapIsAllowedToMakePurchases", "()Z", bIsStoreOptional);
	AndroidThunkJava_IapRestorePurchases = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapRestorePurchases", "([Ljava/lang/String;[Z)Z", bIsStoreOptional);
	AndroidThunkJava_IapConsumePurchase = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapConsumePurchase", "(Ljava/lang/String;)Z", bIsStoreOptional);
	AndroidThunkJava_IapQueryExistingPurchases = FindMethod(Env, GoogleServicesClassID, "AndroidThunkJava_IapQueryExistingPurchases", "()Z", bIsStoreOptional);
}

jclass FJavaWrapper::FindClass(JNIEnv* Env, const ANSICHAR* ClassName, bool bIsOptional)
{
	jclass Class = Env->FindClass(ClassName);
	CHECK_JNI_RESULT(Class);
	return Class;
}

jmethodID FJavaWrapper::FindMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional)
{
	jmethodID Method = Class == NULL ? NULL : Env->GetMethodID(Class, MethodName, MethodSignature);
	CHECK_JNI_RESULT(Method);
	return Method;
}

jmethodID FJavaWrapper::FindStaticMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional)
{
	jmethodID Method = Class == NULL ? NULL : Env->GetStaticMethodID(Class, MethodName, MethodSignature);
	CHECK_JNI_RESULT(Method);
	return Method;
}

jfieldID FJavaWrapper::FindField(JNIEnv* Env, jclass Class, const ANSICHAR* FieldName, const ANSICHAR* FieldType, bool bIsOptional)
{
	jfieldID Field = Class == NULL ? NULL : Env->GetFieldID(Class, FieldName, FieldType);
	CHECK_JNI_RESULT(Field);
	return Field;
}

void FJavaWrapper::CallVoidMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	// make sure the function exists
	if (Method == NULL || Object == NULL)
	{
		return;
	}

	va_list Args;
	va_start(Args, Method);
	Env->CallVoidMethodV(Object, Method, Args);
	va_end(Args);
}

jobject FJavaWrapper::CallObjectMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return NULL;
	}

	va_list Args;
	va_start(Args, Method);
	jobject Return = Env->CallObjectMethodV(Object, Method, Args);
	va_end(Args);

	return Return;
}

int32 FJavaWrapper::CallIntMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return false;
	}

	va_list Args;
	va_start(Args, Method);
	jint Return = Env->CallIntMethodV(Object, Method, Args);
	va_end(Args);

	return (int32)Return;
}

bool FJavaWrapper::CallBooleanMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...)
{
	if (Method == NULL || Object == NULL)
	{
		return false;
	}

	va_list Args;
	va_start(Args, Method);
	jboolean Return = Env->CallBooleanMethodV(Object, Method, Args);
	va_end(Args);

	return (bool)Return;
}

//Declare all the static members of the class defs 
jclass FJavaWrapper::GameActivityClassID;
jobject FJavaWrapper::GameActivityThis;
jmethodID FJavaWrapper::AndroidThunkJava_ShowConsoleWindow;
jmethodID FJavaWrapper::AndroidThunkJava_ShowVirtualKeyboardInputDialog;
jmethodID FJavaWrapper::AndroidThunkJava_HideVirtualKeyboardInputDialog;
jmethodID FJavaWrapper::AndroidThunkJava_ShowVirtualKeyboardInput;
jmethodID FJavaWrapper::AndroidThunkJava_HideVirtualKeyboardInput;
jmethodID FJavaWrapper::AndroidThunkJava_LaunchURL;
jmethodID FJavaWrapper::AndroidThunkJava_GetAssetManager;
jmethodID FJavaWrapper::AndroidThunkJava_Minimize;
jmethodID FJavaWrapper::AndroidThunkJava_ForceQuit;
jmethodID FJavaWrapper::AndroidThunkJava_GetFontDirectory;
jmethodID FJavaWrapper::AndroidThunkJava_Vibrate;
jmethodID FJavaWrapper::AndroidThunkJava_IsMusicActive;
jmethodID FJavaWrapper::AndroidThunkJava_KeepScreenOn;
jmethodID FJavaWrapper::AndroidThunkJava_InitHMDs;
jmethodID FJavaWrapper::AndroidThunkJava_DismissSplashScreen;
jmethodID FJavaWrapper::AndroidThunkJava_GetInputDeviceInfo;
jmethodID FJavaWrapper::AndroidThunkJava_IsGamepadAttached;
jmethodID FJavaWrapper::AndroidThunkJava_HasMetaDataKey;
jmethodID FJavaWrapper::AndroidThunkJava_GetMetaDataBoolean;
jmethodID FJavaWrapper::AndroidThunkJava_GetMetaDataInt;
jmethodID FJavaWrapper::AndroidThunkJava_GetMetaDataString;
jmethodID FJavaWrapper::AndroidThunkJava_IsGearVRApplication;
jmethodID FJavaWrapper::AndroidThunkJava_RegisterForRemoteNotifications;
jmethodID FJavaWrapper::AndroidThunkJava_UnregisterForRemoteNotifications;
jmethodID FJavaWrapper::AndroidThunkJava_ShowHiddenAlertDialog;
jmethodID FJavaWrapper::AndroidThunkJava_LocalNotificationScheduleAtTime;
jmethodID FJavaWrapper::AndroidThunkJava_LocalNotificationClearAll;
jmethodID FJavaWrapper::AndroidThunkJava_LocalNotificationGetLaunchNotification;
//jmethodID FJavaWrapper::AndroidThunkJava_LocalNotificationDestroyIfExists;
jmethodID FJavaWrapper::AndroidThunkJava_HasActiveWiFiConnection;
jmethodID FJavaWrapper::AndroidThunkJava_GetAndroidId;
jmethodID FJavaWrapper::AndroidThunkJava_SetSustainedPerformanceMode;

jclass FJavaWrapper::InputDeviceInfoClass;
jfieldID FJavaWrapper::InputDeviceInfo_VendorId;
jfieldID FJavaWrapper::InputDeviceInfo_ProductId;
jfieldID FJavaWrapper::InputDeviceInfo_ControllerId;
jfieldID FJavaWrapper::InputDeviceInfo_Name;
jfieldID FJavaWrapper::InputDeviceInfo_Descriptor;

jclass FJavaWrapper::GoogleServicesClassID;
jobject FJavaWrapper::GoogleServicesThis;
jmethodID FJavaWrapper::AndroidThunkJava_ResetAchievements;
jmethodID FJavaWrapper::AndroidThunkJava_ShowAdBanner;
jmethodID FJavaWrapper::AndroidThunkJava_HideAdBanner;
jmethodID FJavaWrapper::AndroidThunkJava_CloseAdBanner;
jmethodID FJavaWrapper::AndroidThunkJava_LoadInterstitialAd;
jmethodID FJavaWrapper::AndroidThunkJava_IsInterstitialAdAvailable;
jmethodID FJavaWrapper::AndroidThunkJava_IsInterstitialAdRequested;
jmethodID FJavaWrapper::AndroidThunkJava_ShowInterstitialAd;
jmethodID FJavaWrapper::AndroidThunkJava_GetAdvertisingId;
jmethodID FJavaWrapper::AndroidThunkJava_GoogleClientConnect;
jmethodID FJavaWrapper::AndroidThunkJava_GoogleClientDisconnect;

jclass FJavaWrapper::JavaStringClass;
jmethodID FJavaWrapper::AndroidThunkJava_IapSetupService;
jmethodID FJavaWrapper::AndroidThunkJava_IapQueryInAppPurchases;
jmethodID FJavaWrapper::AndroidThunkJava_IapBeginPurchase;
jmethodID FJavaWrapper::AndroidThunkJava_IapIsAllowedToMakePurchases;
jmethodID FJavaWrapper::AndroidThunkJava_IapRestorePurchases;
jmethodID FJavaWrapper::AndroidThunkJava_IapQueryExistingPurchases;
jmethodID FJavaWrapper::AndroidThunkJava_IapConsumePurchase;

jmethodID FJavaWrapper::AndroidThunkJava_UseSurfaceViewWorkaround;
jmethodID FJavaWrapper::AndroidThunkJava_SetDesiredViewSize;

jmethodID FJavaWrapper::AndroidThunkJava_VirtualInputIgnoreClick;

jclass FJavaWrapper::LaunchNotificationClass;
jfieldID FJavaWrapper::LaunchNotificationUsed;
jfieldID FJavaWrapper::LaunchNotificationEvent;
jfieldID FJavaWrapper::LaunchNotificationFireDate;

jclass FJavaWrapper::ThreadClass;
jmethodID FJavaWrapper::CurrentThreadMethod;
jmethodID FJavaWrapper::SetNameMethod;

//Game-specific crash reporter
void EngineCrashHandler(const FGenericCrashContext& GenericContext)
{
	const FAndroidCrashContext& Context = static_cast<const FAndroidCrashContext&>(GenericContext);

	static int32 bHasEntered = 0;
	if (FPlatformAtomics::InterlockedCompareExchange(&bHasEntered, 1, 0) == 0)
	{
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR StackTrace[StackTraceSize];
		StackTrace[0] = 0;

		// Walk the stack and dump it to the allocated memory.
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, 0, Context.Context);
		UE_LOG(LogEngine, Error, TEXT("\n%s\n"), ANSI_TO_TCHAR(StackTrace));

		if (GLog)
		{
			GLog->SetCurrentThreadAsMasterThread();
			GLog->Flush();
		}
		
		if (GWarn)
		{
			GWarn->Flush();
		}
	}
}

void AndroidThunkCpp_KeepScreenOn(bool Enable)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// call the java side
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_KeepScreenOn, Enable);
	}
}

void AndroidThunkCpp_Vibrate(int32 Duration)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// call the java side
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_Vibrate, Duration);
	}
}

// Call the Java side code for initializing VR HMD modules
void AndroidThunkCpp_InitHMDs()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_InitHMDs);
	}
}

void AndroidThunkCpp_DismissSplashScreen()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_DismissSplashScreen);
	}
}

bool AndroidThunkCpp_GetInputDeviceInfo(int32 deviceId, FAndroidInputDeviceInfo &results)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jobject deviceInfo = (jobject)Env->CallObjectMethod(FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetInputDeviceInfo, deviceId);
		bool bIsOptional = false;
		results.DeviceId = deviceId;
		results.VendorId = (int32)Env->GetIntField(deviceInfo, FJavaWrapper::InputDeviceInfo_VendorId);
		results.ProductId = (int32)Env->GetIntField(deviceInfo, FJavaWrapper::InputDeviceInfo_ProductId);
		results.ControllerId = (int32)Env->GetIntField(deviceInfo, FJavaWrapper::InputDeviceInfo_ControllerId);

		jstring jsName = (jstring)Env->GetObjectField(deviceInfo, FJavaWrapper::InputDeviceInfo_Name);
		CHECK_JNI_RESULT(jsName);
		const char * nativeName = Env->GetStringUTFChars(jsName, 0);
		results.Name = FString(nativeName);
		Env->ReleaseStringUTFChars(jsName, nativeName);
		Env->DeleteLocalRef(jsName);

		jstring jsDescriptor = (jstring)Env->GetObjectField(deviceInfo, FJavaWrapper::InputDeviceInfo_Descriptor);
		CHECK_JNI_RESULT(jsDescriptor);
		const char * nativeDescriptor = Env->GetStringUTFChars(jsDescriptor, 0);
		results.Descriptor = FString(nativeDescriptor);
		Env->ReleaseStringUTFChars(jsDescriptor, nativeDescriptor);
		Env->DeleteLocalRef(jsDescriptor);
		
		// release references
		Env->DeleteLocalRef(deviceInfo);

		return true;
	}

	// failed
	results.DeviceId = deviceId;
	results.VendorId = 0;
	results.ProductId = 0;
	results.ControllerId = -1;
	results.Name = FString("Unknown");
	results.Descriptor = FString("Unknown");
	return false;
}

bool AndroidThunkCpp_VirtualInputIgnoreClick(int32 x, int32 y)
{
	bool Result = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		Result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_VirtualInputIgnoreClick, x, y);
	}
	return Result;
}

//Set GVirtualKeyboardShown.This function is declared in the Java-defined class, GameActivity.java: "public native void nativeVirtualKeyboardVisible(boolean bShown)"
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeVirtualKeyboardVisible(JNIEnv* jenv, jobject thiz, jboolean bShown)
{
	GVirtualKeyboardShown = bShown;

	//remove reference so the object can be clicked again to show the virtual keyboard
	if (!bShown)
	{
		VirtualKeyboardWidget = NULL;
	}
}

bool AndroidThunkCpp_IsVirtuaKeyboardShown()
{
	return GVirtualKeyboardShown;
}

bool AndroidThunkCpp_IsGamepadAttached()
{
	bool Result = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		Result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_IsGamepadAttached);
	}
	return Result;
}

bool AndroidThunkCpp_HasMetaDataKey(const FString& Key)
{
	bool Result = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring Argument = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		Result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_HasMetaDataKey, Argument);
		Env->DeleteLocalRef(Argument);
	}
	return Result;
}

bool AndroidThunkCpp_GetMetaDataBoolean(const FString& Key)
{
	bool Result = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring Argument = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		Result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetMetaDataBoolean, Argument);
		Env->DeleteLocalRef(Argument);
	}
	return Result;
}

int32 AndroidThunkCpp_GetMetaDataInt(const FString& Key)
{
	int32 Result = 0;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring Argument = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		Result = FJavaWrapper::CallIntMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetMetaDataInt, Argument);
		Env->DeleteLocalRef(Argument);
	}
	return Result;
}

FString AndroidThunkCpp_GetMetaDataString(const FString& Key)
{
	FString Result = FString("");
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring Argument = Env->NewStringUTF(TCHAR_TO_UTF8(*Key));
		jstring JavaString = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetMetaDataString, Argument);
		Env->DeleteLocalRef(Argument);
		if (JavaString != NULL)
		{
			const char* JavaChars = Env->GetStringUTFChars(JavaString, 0);
			Result = FString(UTF8_TO_TCHAR(JavaChars));
			Env->ReleaseStringUTFChars(JavaString, JavaChars);
			Env->DeleteLocalRef(JavaString);
		}
	}
	return Result;
}

void AndroidThunkCpp_SetSustainedPerformanceMode(bool bEnable)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_SetSustainedPerformanceMode, bEnable);
	}
}

void AndroidThunkCpp_ShowHiddenAlertDialog()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ShowHiddenAlertDialog);
	}
}

// call out to JNI to see if the application was packaged for GearVR
bool AndroidThunkCpp_IsGearVRApplication()
{
	static int32 IsGearVRApplication = -1;

	if (IsGearVRApplication == -1)
	{
		IsGearVRApplication = 0;
		if (FJavaWrapper::AndroidThunkJava_IsGearVRApplication)
		{
			if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
			{
				IsGearVRApplication = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_IsGearVRApplication) ? 1 : 0;
			}
		}
	}
	return IsGearVRApplication == 1;
}

// call optional remote notification registration
void AndroidThunkCpp_RegisterForRemoteNotifications()
{
	if (FJavaWrapper::AndroidThunkJava_RegisterForRemoteNotifications)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_RegisterForRemoteNotifications);
		}
	}
}

// call optional remote notification unregistration
void AndroidThunkCpp_UnregisterForRemoteNotifications()
{
	if (FJavaWrapper::AndroidThunkJava_UnregisterForRemoteNotifications)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_UnregisterForRemoteNotifications);
		}
	}
}

void AndroidThunkCpp_ShowConsoleWindow()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// figure out all the possible texture formats that are allowed
		TArray<FString> PossibleTargetPlatforms;
		FPlatformMisc::GetValidTargetPlatforms(PossibleTargetPlatforms);

		// separate the format suffixes with commas
		FString ConsoleText;
		for (int32 FormatIndex = 0; FormatIndex < PossibleTargetPlatforms.Num(); FormatIndex++)
		{
			const FString& Format = PossibleTargetPlatforms[FormatIndex];
			int32 UnderscoreIndex;
			if (Format.FindLastChar('_', UnderscoreIndex))
			{
				if (ConsoleText != TEXT(""))
				{
					ConsoleText += ", ";
				}

				ConsoleText += Format.Mid(UnderscoreIndex + 1);
			}
		}

		// call the java side
		jstring ConsoleTextJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ConsoleText));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ShowConsoleWindow, ConsoleTextJava);
		Env->DeleteLocalRef(ConsoleTextJava);
	}
}

void AndroidThunkCpp_ShowVirtualKeyboardInputDialog(TSharedPtr<IVirtualKeyboardEntry> TextWidget, int32 InputType, const FString& Label, const FString& Contents)
{
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        // remember target widget for contents
        VirtualKeyboardWidget = &(*TextWidget);
        
        // call the java side
        jstring LabelJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Label));
        jstring ContentsJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Contents));
        FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ShowVirtualKeyboardInputDialog, InputType, LabelJava, ContentsJava);
        Env->DeleteLocalRef(ContentsJava);
        Env->DeleteLocalRef(LabelJava);
    }
}

void AndroidThunkCpp_HideVirtualKeyboardInputDialog()
{
    // Make sure virtual keyboard currently open
    if (VirtualKeyboardWidget == NULL)
    {
        return;
    }
    
    if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
    {
        // ignore anything it might return
        VirtualKeyboardWidget = NULL;
        
        // call the java side
        FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_HideVirtualKeyboardInputDialog);

		if (FTaskGraphInterface::IsRunning())
		{
			FGraphEventRef VirtualKeyboardShown = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				FAndroidApplication::Get()->OnVirtualKeyboardHidden().Broadcast();
			}, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}

// This is called from the ViewTreeObserver.OnGlobalLayoutListener in GameActivity
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeVirtualKeyboardShown(JNIEnv* jenv, jobject thiz, jint left, jint top, jint right, jint bottom)
{
	FPlatformRect ScreenRect(left, top, right, bottom);

	if (FTaskGraphInterface::IsRunning())
	{
		FGraphEventRef VirtualKeyboardShown = FFunctionGraphTask::CreateAndDispatchWhenReady([ScreenRect]()
		{
			FAndroidApplication::Get()->OnVirtualKeyboardShown().Broadcast(ScreenRect);
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
}

void AndroidThunkCpp_HideVirtualKeyboardInput()
{
	// Make sure virtual keyboard currently open
	if (VirtualKeyboardWidget == NULL)
	{
		return;
	}

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		// ignore anything it might return
		VirtualKeyboardWidget = NULL;

		// call the java side
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_HideVirtualKeyboardInput);

		if( FTaskGraphInterface::IsRunning() )
		{
			FGraphEventRef VirtualKeyboardShown = FFunctionGraphTask::CreateAndDispatchWhenReady( [&]()
			{
				FAndroidApplication::Get()->OnVirtualKeyboardHidden().Broadcast();
			}, TStatId(), NULL, ENamedThreads::GameThread );
		}
	}
}

void AndroidThunkCpp_ShowVirtualKeyboardInput(TSharedPtr<IVirtualKeyboardEntry> TextWidget, int32 InputType, const FString& Label, const FString& Contents)
{
	// remember target widget for contents
	IVirtualKeyboardEntry * newWidget = &(*TextWidget);
	//#jira UE-49139 Tapping in the same text box doesn't make the virtual keyboard disappear
	if (VirtualKeyboardWidget == newWidget)
	{
		FPlatformMisc::LowLevelOutputDebugString(TEXT("[JNI] - AndroidThunkCpp_ShowVirtualKeyboardInput same control"));
		AndroidThunkCpp_HideVirtualKeyboardInput();
	}
	else if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{

		VirtualKeyboardWidget = newWidget;

		// call the java side
		jstring LabelJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Label));
		jstring ContentsJava = Env->NewStringUTF(TCHAR_TO_UTF8(*Contents));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ShowVirtualKeyboardInput, InputType, LabelJava, ContentsJava);
		Env->DeleteLocalRef(ContentsJava);
		Env->DeleteLocalRef(LabelJava);
	}
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeVirtualKeyboardResult(bool update, String contents);"
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeVirtualKeyboardResult(JNIEnv* jenv, jobject thiz, jboolean update, jstring contents)
{
	// update text widget with new contents if OK pressed
	if (update == JNI_TRUE)
	{
		if (VirtualKeyboardWidget != NULL)
		{
			const char* javaChars = jenv->GetStringUTFChars(contents, 0);

			// call to set the widget text on game thread
			if (FTaskGraphInterface::IsRunning())
			{
				FGraphEventRef SetWidgetText = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
				{
					VirtualKeyboardWidget->SetTextFromVirtualKeyboard(FText::FromString(FString(UTF8_TO_TCHAR(javaChars))), ETextEntryType::TextEntryAccepted);
				}, TStatId(), NULL, ENamedThreads::GameThread);
				FTaskGraphInterface::Get().WaitUntilTaskCompletes(SetWidgetText);
			}
			// release string
			jenv->ReleaseStringUTFChars(contents, javaChars);
		}
	}

	// release reference
	VirtualKeyboardWidget = NULL;
}

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeVirtualKeyboardChanged(String contents);"
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeVirtualKeyboardChanged(JNIEnv* jenv, jobject thiz, jstring contents)
{
	if (VirtualKeyboardWidget != NULL)
	{
		const char* javaChars = jenv->GetStringUTFChars(contents, 0);

		// call to set the widget text on game thread
		if (FTaskGraphInterface::IsRunning())
		{
			FGraphEventRef SetWidgetText = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
			{
				VirtualKeyboardWidget->SetTextFromVirtualKeyboard(FText::FromString(FString(UTF8_TO_TCHAR(javaChars))), ETextEntryType::TextEntryUpdated);
			}, TStatId(), NULL, ENamedThreads::GameThread);
			FTaskGraphInterface::Get().WaitUntilTaskCompletes(SetWidgetText);
		}
		// release string
		jenv->ReleaseStringUTFChars(contents, javaChars);
	}
}

JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeVirtualKeyboardSendKey(JNIEnv* jenv, jobject thiz, jint keyCode)
{
	FDeferredAndroidMessage Message;

	Message.messageType = MessageType_KeyDown;
	Message.KeyEventData.keyId = keyCode;
	FAndroidInputInterface::DeferMessage(Message);
}

void AndroidThunkCpp_LaunchURL(const FString& URL)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring Argument = Env->NewStringUTF(TCHAR_TO_UTF8(*URL));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_LaunchURL, Argument);
		Env->DeleteLocalRef(Argument);
	}
}

void AndroidThunkCpp_ResetAchievements()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_ResetAchievements);
	}
}

void AndroidThunkCpp_ShowAdBanner(const FString& AdUnitID, bool bShowOnBottomOfScreen)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		jstring AdUnitIDArg = Env->NewStringUTF(TCHAR_TO_UTF8(*AdUnitID));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_ShowAdBanner, AdUnitIDArg, bShowOnBottomOfScreen);
		Env->DeleteLocalRef(AdUnitIDArg);
	}
}

void AndroidThunkCpp_HideAdBanner()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_HideAdBanner);
 	}
}

void AndroidThunkCpp_CloseAdBanner()
{
 	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
 	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_CloseAdBanner);
 	}
}

void AndroidThunkCpp_LoadInterstitialAd(const FString& AdUnitID)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring AdUnitIDArg = Env->NewStringUTF(TCHAR_TO_UTF8(*AdUnitID));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_LoadInterstitialAd, AdUnitIDArg);
		Env->DeleteLocalRef(AdUnitIDArg);
	}
}

bool AndroidThunkCpp_IsInterstitialAdAvailable()
{
	bool bIsAdAvailable = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bIsAdAvailable = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IsInterstitialAdAvailable);
	}

	return bIsAdAvailable;
}

bool AndroidThunkCpp_IsInterstitialAdRequested()
{
	bool bIsAdRequested = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bIsAdRequested = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IsInterstitialAdRequested);
	}

	return bIsAdRequested;
}

void AndroidThunkCpp_ShowInterstitialAd()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_ShowInterstitialAd);
	}
}

FString AndroidThunkCpp_GetAdvertisingId()
{
	FString adIdResult = FString("");

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring adId =(jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_GetAdvertisingId);
		if (!Env->IsSameObject(adId, NULL))
		{
			const char *nativeAdIdString = Env->GetStringUTFChars(adId, 0);
			adIdResult = FString(nativeAdIdString);
			Env->ReleaseStringUTFChars(adId, nativeAdIdString);
			Env->DeleteLocalRef(adId);
		}
	}
	return adIdResult;
}

FString AndroidThunkCpp_GetAndroidId()
{
	FString androidIdResult = FString("");

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring androidId = (jstring)FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetAndroidId);
		if (!Env->IsSameObject(androidId, NULL))
		{
			const char *nativeandroidIdString = Env->GetStringUTFChars(androidId, 0);
			androidIdResult = FString(nativeandroidIdString);
			Env->ReleaseStringUTFChars(androidId, nativeandroidIdString);
			Env->DeleteLocalRef(androidId);
		}
	}
	return androidIdResult;
}

void AndroidThunkCpp_GoogleClientConnect()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_GoogleClientConnect);
	}
}

void AndroidThunkCpp_GoogleClientDisconnect()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_GoogleClientDisconnect);
	}
}

namespace
{
	jobject GJavaAssetManager = NULL;
	AAssetManager* GAssetManagerRef = NULL;
}

jobject AndroidJNI_GetJavaAssetManager()
{
	if (!GJavaAssetManager)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			jobject local = FJavaWrapper::CallObjectMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_GetAssetManager);
			GJavaAssetManager = (jobject)Env->NewGlobalRef(local);
			Env->DeleteLocalRef(local);
		}
	}
	return GJavaAssetManager;
}

AAssetManager * AndroidThunkCpp_GetAssetManager()
{
	if (!GAssetManagerRef)
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			jobject JavaAssetMgr = AndroidJNI_GetJavaAssetManager();
			GAssetManagerRef = AAssetManager_fromJava(Env, JavaAssetMgr);
		}
	}

	return GAssetManagerRef;
}

void AndroidThunkCpp_Minimize()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_Minimize);
	}
}

void AndroidThunkCpp_ForceQuit()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_ForceQuit);
	}
}

bool AndroidThunkCpp_IsMusicActive()
{
	bool bIsActive = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bIsActive = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_IsMusicActive);
	}

	return bIsActive;
}

void AndroidThunkCpp_Iap_SetupIapService(const FString& InProductKey)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring ProductKey = Env->NewStringUTF(TCHAR_TO_UTF8(*InProductKey));
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapSetupService, ProductKey);
		Env->DeleteLocalRef(ProductKey);
	}
}

bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>& ProductIDs)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("[JNI] - AndroidThunkCpp_Iap_QueryInAppPurchases"));
	bool bResult = false;

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		CHECK_JNI_METHOD(FJavaWrapper::AndroidThunkJava_IapQueryInAppPurchases);

		// Populate some java types with the provided product information
		jobjectArray ProductIDArray = (jobjectArray)Env->NewObjectArray(ProductIDs.Num(), FJavaWrapper::JavaStringClass, NULL);
		for (uint32 Param = 0; Param < ProductIDs.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*ProductIDs[Param]));
			Env->SetObjectArrayElement(ProductIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);
		}

		// Execute the java code for this operation
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapQueryInAppPurchases, ProductIDArray);
		
		// clean up references
		Env->DeleteLocalRef(ProductIDArray);
	}

	return bResult;
}

bool AndroidThunkCpp_Iap_QueryInAppPurchases(const TArray<FString>& ProductIDs, const TArray<bool>& bConsumable)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidThunkCpp_Iap_QueryInAppPurchases DEPRECATED, won't use consumables array"));
	return AndroidThunkCpp_Iap_QueryInAppPurchases(ProductIDs);
}

bool AndroidThunkCpp_Iap_BeginPurchase(const FString& ProductID)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_BeginPurchase %s"), *ProductID);
	bool bResult = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		CHECK_JNI_METHOD(FJavaWrapper::AndroidThunkJava_IapBeginPurchase);

		jstring ProductIdJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ProductID));
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapBeginPurchase, ProductIdJava);
		Env->DeleteLocalRef(ProductIdJava);
	}

	return bResult;
}

bool AndroidThunkCpp_Iap_BeginPurchase(const FString& ProductID, const bool bConsumable)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("AndroidThunkCpp_Iap_BeginPurchase DEPRECATED, won't use consumable flag"));
	return AndroidThunkCpp_Iap_BeginPurchase(ProductID);
}

bool AndroidThunkCpp_Iap_ConsumePurchase(const FString& ProductToken)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_ConsumePurchase %s"), *ProductToken);
	
	bool bResult = false;
	if (!ProductToken.IsEmpty())
	{
		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			CHECK_JNI_METHOD(FJavaWrapper::AndroidThunkJava_IapConsumePurchase);

			jstring ProductTokenJava = Env->NewStringUTF(TCHAR_TO_UTF8(*ProductToken));
			//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_ConsumePurchase BEGIN"));
			bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapConsumePurchase, ProductTokenJava);
			//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_ConsumePurchase END"));
			Env->DeleteLocalRef(ProductTokenJava);
		}
	}

	return bResult;
}

bool AndroidThunkCpp_Iap_QueryExistingPurchases()
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_QueryExistingPurchases"));
	
	bool bResult = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		CHECK_JNI_METHOD(FJavaWrapper::AndroidThunkJava_IapQueryExistingPurchases);

		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_QueryExistingPurchases BEGIN"));
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapQueryExistingPurchases);
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("[JNI] - AndroidThunkCpp_Iap_QueryExistingPurchases END"));
	}

	return bResult;
}

bool AndroidThunkCpp_Iap_IsAllowedToMakePurchases()
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("[JNI] - AndroidThunkCpp_Iap_IsAllowedToMakePurchases"));
	bool bResult = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		CHECK_JNI_METHOD(FJavaWrapper::AndroidThunkJava_IapIsAllowedToMakePurchases);

		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapIsAllowedToMakePurchases);
	}
	return bResult;
}

bool AndroidThunkCpp_Iap_RestorePurchases(const TArray<FString>& ProductIDs, const TArray<bool>& bConsumable)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("[JNI] - AndroidThunkCpp_Iap_RestorePurchases"));
	bool bResult = false;

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		CHECK_JNI_METHOD(FJavaWrapper::AndroidThunkJava_IapRestorePurchases);

		// Populate some java types with the provided product information
		jobjectArray ProductIDArray = (jobjectArray)Env->NewObjectArray(ProductIDs.Num(), FJavaWrapper::JavaStringClass, NULL);
		jbooleanArray ConsumeArray = (jbooleanArray)Env->NewBooleanArray(ProductIDs.Num());

		jboolean* ConsumeArrayValues = Env->GetBooleanArrayElements(ConsumeArray, 0);
		for (uint32 Param = 0; Param < ProductIDs.Num(); Param++)
		{
			jstring StringValue = Env->NewStringUTF(TCHAR_TO_UTF8(*ProductIDs[Param]));
			Env->SetObjectArrayElement(ProductIDArray, Param, StringValue);
			Env->DeleteLocalRef(StringValue);

			ConsumeArrayValues[Param] = bConsumable[Param];
		}
		Env->ReleaseBooleanArrayElements(ConsumeArray, ConsumeArrayValues, 0);

		// Execute the java code for this operation
		bResult = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GoogleServicesThis, FJavaWrapper::AndroidThunkJava_IapRestorePurchases, ProductIDArray, ConsumeArray);

		// clean up references
		Env->DeleteLocalRef(ProductIDArray);
		Env->DeleteLocalRef(ConsumeArray);
	}

	return bResult;
}

void AndroidThunkCpp_UseSurfaceViewWorkaround()
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_UseSurfaceViewWorkaround);
	}
}

void AndroidThunkCpp_SetDesiredViewSize(int32 Width, int32 Height)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_SetDesiredViewSize, Width, Height);
	}
}

void AndroidThunkCpp_ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent) 
{
	//Convert FireDateTime to yyyy-MM-dd HH:mm:ss in order to pass to java
	FString FireDateTimeFormatted = FString::FromInt(FireDateTime.GetYear()) + "-" + FString::FromInt(FireDateTime.GetMonth()) + "-" + FString::FromInt(FireDateTime.GetDay()) + " " + FString::FromInt(FireDateTime.GetHour()) + ":" + FString::FromInt(FireDateTime.GetMinute()) + ":" + FString::FromInt(FireDateTime.GetSecond());

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env != NULL)
	{
		jstring jFireDateTime = Env->NewStringUTF(TCHAR_TO_UTF8(*FireDateTimeFormatted));
		jstring jTitle = Env->NewStringUTF(TCHAR_TO_UTF8(*Title.ToString()));
		jstring jBody = Env->NewStringUTF(TCHAR_TO_UTF8(*Body.ToString()));
		jstring jAction = Env->NewStringUTF(TCHAR_TO_UTF8(*Action.ToString()));
		jstring jActivationEvent = Env->NewStringUTF(TCHAR_TO_UTF8(*ActivationEvent));

		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_LocalNotificationScheduleAtTime, jFireDateTime, LocalTime, jTitle, jBody, jAction, jActivationEvent);

		Env->DeleteLocalRef(jFireDateTime);
		Env->DeleteLocalRef(jTitle);
		Env->DeleteLocalRef(jBody);
		Env->DeleteLocalRef(jAction);
		Env->DeleteLocalRef(jActivationEvent);
	}
}

void AndroidThunkCpp_GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	bool bIsOptional = false;

	NotificationLaunchedApp = false;
	ActivationEvent = "";
	FireDate = 0;

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env != NULL)
	{
		jobject launchInfo = (jobject)Env->CallObjectMethod(FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_LocalNotificationGetLaunchNotification);
		NotificationLaunchedApp = (bool)Env->GetBooleanField(launchInfo, FJavaWrapper::LaunchNotificationUsed);

		jstring jsName = (jstring)Env->GetObjectField(launchInfo, FJavaWrapper::LaunchNotificationEvent);
		CHECK_JNI_RESULT(jsName);
		const char * nativeName = Env->GetStringUTFChars(jsName, 0);
		ActivationEvent = FString(nativeName);
		Env->ReleaseStringUTFChars(jsName, nativeName);
		Env->DeleteLocalRef(jsName);

		FireDate = (int32)Env->GetIntField(launchInfo, FJavaWrapper::LaunchNotificationFireDate);

		Env->DeleteLocalRef(launchInfo);
	}
}

void AndroidThunkCpp_ClearAllLocalNotifications() 
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		FJavaWrapper::CallVoidMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_LocalNotificationClearAll);
	}
}
/*
bool AndroidThunkCpp_DestroyScheduledNotificationIfExists(int32 NotificationId)
{
	bool Result = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		Result = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_LocalNotificationDestroyIfExists, NotificationId);
	}
	return Result;
}
*/

bool AndroidThunkCpp_HasActiveWiFiConnection()
{
	bool bIsActive = false;
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		bIsActive = FJavaWrapper::CallBooleanMethod(Env, FJavaWrapper::GameActivityThis, FJavaWrapper::AndroidThunkJava_HasActiveWiFiConnection);
	}

	return bIsActive;
}

void AndroidThunkCpp_SetThreadName(const char * name)
{
	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		jstring jname = Env->NewStringUTF(name);
		jobject currentThread = Env->CallStaticObjectMethod(FJavaWrapper::ThreadClass, FJavaWrapper::CurrentThreadMethod, nullptr);
		Env->CallVoidMethod(currentThread, FJavaWrapper::SetNameMethod, jname);
		Env->DeleteLocalRef(jname);
		Env->DeleteLocalRef(currentThread);
	}
}

//The JNI_OnLoad function is triggered by loading the game library from 
//the Java source file.
//	static
//	{
//		System.loadLibrary("MyGame");
//	}
//
// Use the JNI_OnLoad function to map all the class IDs and method IDs to their respective
// variables. That way, later when the Java functions need to be called, the IDs will be ready.
// It is much slower to keep looking up the class and method IDs.

JNIEXPORT jint JNI_OnLoad(JavaVM* InJavaVM, void* InReserved)
{
	FPlatformMisc::LowLevelOutputDebugString(TEXT("In the JNI_OnLoad function"));

	JNIEnv* Env = NULL;
	InJavaVM->GetEnv((void **)&Env, JNI_CURRENT_VERSION);

	// if you have problems with stuff being missing especially in distribution builds then it could be because proguard is stripping things from java
	// check proguard-project.txt and see if your stuff is included in the exceptions
	GJavaVM = InJavaVM;
	FAndroidApplication::InitializeJavaEnv(GJavaVM, JNI_CURRENT_VERSION, FJavaWrapper::GameActivityThis);

	FJavaWrapper::FindClassesAndMethods(Env);

	// hook signals
	if (!FPlatformMisc::IsDebuggerPresent() || GAlwaysReportCrash)
	{
		// disable crash handler.. getting better stack traces from system for now
		//FPlatformMisc::SetCrashHandler(EngineCrashHandler);
	}

	// Cache path to external storage
	jclass EnvClass = Env->FindClass("android/os/Environment");
	jmethodID getExternalStorageDir = Env->GetStaticMethodID(EnvClass, "getExternalStorageDirectory", "()Ljava/io/File;");
	jobject externalStoragePath = Env->CallStaticObjectMethod(EnvClass, getExternalStorageDir, nullptr);
	jmethodID getFilePath = Env->GetMethodID(Env->FindClass("java/io/File"), "getPath", "()Ljava/lang/String;");
	jstring pathString = (jstring)Env->CallObjectMethod(externalStoragePath, getFilePath, nullptr);
	const char *nativePathString = Env->GetStringUTFChars(pathString, 0);
	// Copy that somewhere safe 
	GFilePathBase = FString(nativePathString);
	GOBBFilePathBase = GFilePathBase;

	// then release...
	Env->ReleaseStringUTFChars(pathString, nativePathString);
	Env->DeleteLocalRef(pathString);
	Env->DeleteLocalRef(externalStoragePath);
	Env->DeleteLocalRef(EnvClass);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Path found as '%s'\n"), *GFilePathBase);

	// Get the system font directory
	jstring fontPath = (jstring)Env->CallStaticObjectMethod(FJavaWrapper::GameActivityClassID, FJavaWrapper::AndroidThunkJava_GetFontDirectory);
	const char * nativeFontPathString = Env->GetStringUTFChars(fontPath, 0);
	GFontPathBase = FString(nativeFontPathString);
	Env->ReleaseStringUTFChars(fontPath, nativeFontPathString);
	Env->DeleteLocalRef(fontPath);
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Font Path found as '%s'\n"), *GFontPathBase);

	// Wire up to core delegates, so core code can call out to Java
	DECLARE_DELEGATE_OneParam(FAndroidLaunchURLDelegate, const FString&);
	extern CORE_API FAndroidLaunchURLDelegate OnAndroidLaunchURL;
	OnAndroidLaunchURL = FAndroidLaunchURLDelegate::CreateStatic(&AndroidThunkCpp_LaunchURL);

	FPlatformMisc::LowLevelOutputDebugString(TEXT("In the JNI_OnLoad function 5"));
	
	char mainThreadName[] = "MainThread-UE4";
	AndroidThunkCpp_SetThreadName(mainThreadName);

	return JNI_CURRENT_VERSION;
}

//Native-defined functions

//This function is declared in the Java-defined class, GameActivity.java: "public native void nativeSetGlobalActivity();"
JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeSetGlobalActivity(JNIEnv* jenv, jobject thiz, jboolean bUseExternalFilesDir, jboolean bOBBinAPK, jstring APKFilename /*, jobject googleServices*/)
{
	if (!FJavaWrapper::GameActivityThis)
	{
		FJavaWrapper::GameActivityThis = jenv->NewGlobalRef(thiz);
		if (!FJavaWrapper::GameActivityThis)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("Error setting the global GameActivity activity"));
			check(false);
		}

		// This call is only to set the correct GameActivityThis
		FAndroidApplication::InitializeJavaEnv(GJavaVM, JNI_CURRENT_VERSION, FJavaWrapper::GameActivityThis);

		// @todo split GooglePlay, this needs to be passed in to this function
		FJavaWrapper::GoogleServicesThis = FJavaWrapper::GameActivityThis;
		// FJavaWrapper::GoogleServicesThis = jenv->NewGlobalRef(googleServices);

		// Next we check to see if the OBB file is in the APK
		//jmethodID isOBBInAPKMethod = jenv->GetStaticMethodID(FJavaWrapper::GameActivityClassID, "isOBBInAPK", "()Z");
		//GOBBinAPK = (bool)jenv->CallStaticBooleanMethod(FJavaWrapper::GameActivityClassID, isOBBInAPKMethod, nullptr);
		GOBBinAPK = bOBBinAPK;

		const char *nativeAPKFilenameString = jenv->GetStringUTFChars(APKFilename, 0);
		GAPKFilename = FString(nativeAPKFilenameString);
		jenv->ReleaseStringUTFChars(APKFilename, nativeAPKFilenameString);

		// Cache path to external files directory
		jclass ContextClass = jenv->FindClass("android/content/Context");
		jmethodID getExternalFilesDir = jenv->GetMethodID(ContextClass, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
		jobject externalFilesDirPath = jenv->CallObjectMethod(FJavaWrapper::GameActivityThis, getExternalFilesDir, nullptr);
		jmethodID getFilePath = jenv->GetMethodID(jenv->FindClass("java/io/File"), "getPath", "()Ljava/lang/String;");
		jstring externalFilesPathString = (jstring)jenv->CallObjectMethod(externalFilesDirPath, getFilePath, nullptr);
		const char *nativeExternalFilesPathString = jenv->GetStringUTFChars(externalFilesPathString, 0);
		// Copy that somewhere safe 
		GExternalFilePath = FString(nativeExternalFilesPathString);

		if (bUseExternalFilesDir)
		{
			GFilePathBase = GExternalFilePath;
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("GFilePathBase Path override to'%s'\n"), *GFilePathBase);
		}

		// then release...
		jenv->ReleaseStringUTFChars(externalFilesPathString, nativeExternalFilesPathString);
		jenv->DeleteLocalRef(externalFilesPathString);
		jenv->DeleteLocalRef(externalFilesDirPath);
		jenv->DeleteLocalRef(ContextClass);
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("ExternalFilePath found as '%s'\n"), *GExternalFilePath);
	}
}


JNI_METHOD bool Java_com_epicgames_ue4_GameActivity_nativeIsShippingBuild(JNIEnv* LocalJNIEnv, jobject LocalThiz)
{
#if UE_BUILD_SHIPPING
	return JNI_TRUE;
#else
	return JNI_FALSE;
#endif
}

JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeOnActivityResult(JNIEnv* jenv, jobject thiz, jobject activity, jint requestCode, jint resultCode, jobject data)
{
	FJavaWrapper::OnActivityResultDelegate.Broadcast(jenv, thiz, activity, requestCode, resultCode, data);
}


JNI_METHOD void Java_com_epicgames_ue4_GameActivity_nativeHandleSensorEvents(JNIEnv* jenv, jobject thiz, jfloatArray tilt, jfloatArray rotation_rate, jfloatArray gravity, jfloatArray acceleration)
{
	jfloat* tiltFloatValues = jenv->GetFloatArrayElements(tilt, 0);
	FVector current_tilt(tiltFloatValues[0], tiltFloatValues[1], tiltFloatValues[2]);
	jenv->ReleaseFloatArrayElements(tilt, tiltFloatValues, 0);

	jfloat* rotation_rate_FloatValues = jenv->GetFloatArrayElements(rotation_rate, 0);
	FVector current_rotation_rate(rotation_rate_FloatValues[0], rotation_rate_FloatValues[1], rotation_rate_FloatValues[2]);
	jenv->ReleaseFloatArrayElements(rotation_rate, rotation_rate_FloatValues, 0);

	jfloat* gravity_FloatValues = jenv->GetFloatArrayElements(gravity, 0);
	FVector current_gravity(gravity_FloatValues[0], gravity_FloatValues[1], gravity_FloatValues[2]);
	jenv->ReleaseFloatArrayElements(gravity, gravity_FloatValues, 0);
	
	jfloat* acceleration_FloatValues = jenv->GetFloatArrayElements(acceleration, 0);
	FVector current_acceleration(acceleration_FloatValues[0], acceleration_FloatValues[1], acceleration_FloatValues[2]);
	jenv->ReleaseFloatArrayElements(acceleration, acceleration_FloatValues, 0);

	FAndroidInputInterface::QueueMotionData(current_tilt, current_rotation_rate, current_gravity, current_acceleration);

}