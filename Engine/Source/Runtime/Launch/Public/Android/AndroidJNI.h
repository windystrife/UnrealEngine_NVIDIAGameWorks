// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <jni.h>
#include <android/log.h>

extern JavaVM* GJavaVM;

DECLARE_MULTICAST_DELEGATE_SixParams(FOnActivityResult, JNIEnv *, jobject, jobject, jint, jint, jobject);

// Define all the Java classes/methods that the game will need to access to
class FJavaWrapper
{
public:

	// Nonstatic methods
	static jclass GameActivityClassID;
	static jobject GameActivityThis;
	static jmethodID AndroidThunkJava_ShowConsoleWindow;
    static jmethodID AndroidThunkJava_ShowVirtualKeyboardInputDialog;
    static jmethodID AndroidThunkJava_HideVirtualKeyboardInputDialog;
	static jmethodID AndroidThunkJava_ShowVirtualKeyboardInput;
	static jmethodID AndroidThunkJava_HideVirtualKeyboardInput;
	static jmethodID AndroidThunkJava_LaunchURL;
	static jmethodID AndroidThunkJava_GetAssetManager;
	static jmethodID AndroidThunkJava_Minimize;
	static jmethodID AndroidThunkJava_ForceQuit;
	static jmethodID AndroidThunkJava_GetFontDirectory;
	static jmethodID AndroidThunkJava_Vibrate;
	static jmethodID AndroidThunkJava_IsMusicActive;
	static jmethodID AndroidThunkJava_KeepScreenOn;
	static jmethodID AndroidThunkJava_InitHMDs;
	static jmethodID AndroidThunkJava_DismissSplashScreen;
	static jmethodID AndroidThunkJava_GetInputDeviceInfo;
	static jmethodID AndroidThunkJava_IsGamepadAttached;
	static jmethodID AndroidThunkJava_HasMetaDataKey;
	static jmethodID AndroidThunkJava_GetMetaDataBoolean;
	static jmethodID AndroidThunkJava_GetMetaDataInt;
	static jmethodID AndroidThunkJava_GetMetaDataString;
	static jmethodID AndroidThunkJava_IsGearVRApplication;
	static jmethodID AndroidThunkJava_ShowHiddenAlertDialog;
	static jmethodID AndroidThunkJava_LocalNotificationScheduleAtTime;
	static jmethodID AndroidThunkJava_LocalNotificationClearAll;
	static jmethodID AndroidThunkJava_LocalNotificationGetLaunchNotification;
	//static jmethodID AndroidThunkJava_LocalNotificationDestroyIfExists; - This is not needed yet but will be soon so just leaving commented out for now
	static jmethodID AndroidThunkJava_HasActiveWiFiConnection;
	static jmethodID AndroidThunkJava_GetAndroidId;
	static jmethodID AndroidThunkJava_SetSustainedPerformanceMode;

	static jmethodID AndroidThunkCpp_VirtualInputIgnoreClick;
	static jmethodID AndroidThunkCpp_IsVirtuaKeyboardShown;

	// InputDeviceInfo member field ids
	static jclass InputDeviceInfoClass;
	static jfieldID InputDeviceInfo_VendorId;
	static jfieldID InputDeviceInfo_ProductId;
	static jfieldID InputDeviceInfo_ControllerId;
	static jfieldID InputDeviceInfo_Name;
	static jfieldID InputDeviceInfo_Descriptor;

	// IDs related to google play services
	static jclass GoogleServicesClassID;
	static jobject GoogleServicesThis;
	static jmethodID AndroidThunkJava_ResetAchievements;
	static jmethodID AndroidThunkJava_ShowAdBanner;
	static jmethodID AndroidThunkJava_HideAdBanner;
	static jmethodID AndroidThunkJava_CloseAdBanner;
	static jmethodID AndroidThunkJava_LoadInterstitialAd;
	static jmethodID AndroidThunkJava_IsInterstitialAdAvailable;
	static jmethodID AndroidThunkJava_IsInterstitialAdRequested;
	static jmethodID AndroidThunkJava_ShowInterstitialAd;
	static jmethodID AndroidThunkJava_GetAdvertisingId;
	static jmethodID AndroidThunkJava_GoogleClientConnect;
	static jmethodID AndroidThunkJava_GoogleClientDisconnect;

	// Optionally added if GCM plugin (or other remote notification system) enabled
	static jmethodID AndroidThunkJava_RegisterForRemoteNotifications;
	static jmethodID AndroidThunkJava_UnregisterForRemoteNotifications;

	// In app purchase functionality
	static jclass JavaStringClass;
	static jmethodID AndroidThunkJava_IapSetupService;
	static jmethodID AndroidThunkJava_IapQueryInAppPurchases;
	static jmethodID AndroidThunkJava_IapBeginPurchase;
	static jmethodID AndroidThunkJava_IapIsAllowedToMakePurchases;
	static jmethodID AndroidThunkJava_IapRestorePurchases;
	static jmethodID AndroidThunkJava_IapQueryExistingPurchases;
	static jmethodID AndroidThunkJava_IapConsumePurchase;

	// SurfaceView functionality for view scaling on some devices
	static jmethodID AndroidThunkJava_UseSurfaceViewWorkaround;
	static jmethodID AndroidThunkJava_SetDesiredViewSize;
	static jmethodID AndroidThunkJava_VirtualInputIgnoreClick;

	// member fields for getting the launch notification
	static jclass LaunchNotificationClass;
	static jfieldID LaunchNotificationUsed;
	static jfieldID LaunchNotificationEvent;
	static jfieldID LaunchNotificationFireDate;

	// method and classes for thread name change
	static jclass ThreadClass;
	static jmethodID CurrentThreadMethod;
	static jmethodID SetNameMethod;

	/**
	 * Find all known classes and methods
	 */
	static void FindClassesAndMethods(JNIEnv* Env);

	/**
	 * Helper wrapper functions around the JNIEnv versions with NULL/error handling
	 */
	static jclass FindClass(JNIEnv* Env, const ANSICHAR* ClassName, bool bIsOptional);
	static jmethodID FindMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional);
	static jmethodID FindStaticMethod(JNIEnv* Env, jclass Class, const ANSICHAR* MethodName, const ANSICHAR* MethodSignature, bool bIsOptional);
	static jfieldID FindField(JNIEnv* Env, jclass Class, const ANSICHAR* FieldName, const ANSICHAR* FieldType, bool bIsOptional);

	static void CallVoidMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);
	static jobject CallObjectMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);
	static int32 CallIntMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);
	static bool CallBooleanMethod(JNIEnv* Env, jobject Object, jmethodID Method, ...);

	// Delegate that can be registered to that is called when an activity is finished
	static FOnActivityResult OnActivityResultDelegate;

private:

	/** Find GooglePlay "game services" classes and methods */
	static void FindGooglePlayMethods(JNIEnv* Env);
	/** Find GooglePlay billing classes and methods */
	static void FindGooglePlayBillingMethods(JNIEnv* Env);
};
