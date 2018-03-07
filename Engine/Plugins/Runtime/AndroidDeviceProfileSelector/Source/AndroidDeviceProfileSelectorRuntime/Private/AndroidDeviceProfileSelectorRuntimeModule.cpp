// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.AndroidDeviceProfileSelector

#include "AndroidDeviceProfileSelectorRuntimeModule.h"
#include "AndroidDeviceProfileSelectorRuntime.h"
#include "Templates/Casts.h"
#include "Regex.h"
#include "Modules/ModuleManager.h"
#include "AndroidDeviceProfileSelectorRuntime.h"
#include "AndroidDeviceProfileSelector.h"
#include "AndroidJavaSurfaceViewDevices.h"

IMPLEMENT_MODULE(FAndroidDeviceProfileSelectorRuntimeModule, AndroidDeviceProfileSelectorRuntime);

void FAndroidDeviceProfileSelectorRuntimeModule::StartupModule()
{
}

void FAndroidDeviceProfileSelectorRuntimeModule::ShutdownModule()
{
}

FString const FAndroidDeviceProfileSelectorRuntimeModule::GetRuntimeDeviceProfileName()
{
	static FString ProfileName; 
	
	if (ProfileName.IsEmpty())
	{
		// Fallback profiles in case we do not match any rules
		ProfileName = FPlatformMisc::GetDefaultDeviceProfileName();
		if (ProfileName.IsEmpty())
		{
			ProfileName = FPlatformProperties::PlatformName();
		}

		FString GPUFamily = FAndroidMisc::GetGPUFamily();
		FString GLVersion = FAndroidMisc::GetGLVersion();

		FString VulkanVersion = FAndroidMisc::GetVulkanVersion();
		FString AndroidVersion = FAndroidMisc::GetAndroidVersion();
		FString DeviceMake = FAndroidMisc::GetDeviceMake();
		FString DeviceModel = FAndroidMisc::GetDeviceModel();

#if !(PLATFORM_ANDROID_X86 || PLATFORM_ANDROID_X64)
		// Not running an Intel libUE4.so with Houdini library present means we're emulated
		bool bUsingHoudini = (access("/system/lib/libhoudini.so", F_OK) != -1);
#else
		bool bUsingHoudini = false;
#endif
		FString UsingHoudini = bUsingHoudini ? "true" : "false";

		UE_LOG(LogAndroid, Log, TEXT("Checking %d rules from DeviceProfile ini file."), FAndroidDeviceProfileSelector::GetNumProfiles() );
		UE_LOG(LogAndroid, Log, TEXT("  Default profile: %s"), *ProfileName);
		UE_LOG(LogAndroid, Log, TEXT("  GpuFamily: %s"), *GPUFamily);
		UE_LOG(LogAndroid, Log, TEXT("  GlVersion: %s"), *GLVersion);
		UE_LOG(LogAndroid, Log, TEXT("  VulkanVersion: %s"), *VulkanVersion);
		UE_LOG(LogAndroid, Log, TEXT("  AndroidVersion: %s"), *AndroidVersion);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceMake: %s"), *DeviceMake);
		UE_LOG(LogAndroid, Log, TEXT("  DeviceModel: %s"), *DeviceModel);
		UE_LOG(LogAndroid, Log, TEXT("  UsingHoudini: %s"), *UsingHoudini);

		CheckForJavaSurfaceViewWorkaround(DeviceMake, DeviceModel);

		ProfileName = FAndroidDeviceProfileSelector::FindMatchingProfile(GPUFamily, GLVersion, AndroidVersion, DeviceMake, DeviceModel, VulkanVersion, UsingHoudini, ProfileName);

		UE_LOG(LogAndroid, Log, TEXT("Selected Device Profile: [%s]"), *ProfileName);
	}

	return ProfileName;
}

extern void AndroidThunkCpp_UseSurfaceViewWorkaround();

void FAndroidDeviceProfileSelectorRuntimeModule::CheckForJavaSurfaceViewWorkaround(const FString& DeviceMake, const FString& DeviceModel) const
{
	// We need to initialize the class early as device profiles need to be evaluated before ProcessNewlyLoadedUObjects can be called.
	extern UClass* Z_Construct_UClass_UAndroidJavaSurfaceViewDevices();
	Z_Construct_UClass_UAndroidJavaSurfaceViewDevices();

	const UAndroidJavaSurfaceViewDevices *const SurfaceViewDevices = Cast<UAndroidJavaSurfaceViewDevices>(UAndroidJavaSurfaceViewDevices::StaticClass()->GetDefaultObject());
	check(SurfaceViewDevices);

	for(const FJavaSurfaceViewDevice& Device : SurfaceViewDevices->SurfaceViewDevices)
	{
		if(Device.Manufacturer == DeviceMake && Device.Model == DeviceModel)
		{
			AndroidThunkCpp_UseSurfaceViewWorkaround();
			return;
		}
	}
}